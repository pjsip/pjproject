/* 
 * Copyright (C) 2025 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include <pjmedia/avi_stream.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/avi.h>
#include <pjmedia/errno.h>
#include <pjmedia/wave.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/file_access.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#define THIS_FILE   "avi_writer.c"

#define SIGNATURE           PJMEDIA_SIG_PORT_VID_AVI_WRITER

#define SET_TAG(tag) *((pj_uint32_t*)avi_tags[tag])

#if 0
#   define TRACE_(x)    PJ_LOG(4,x)
#else
#   define TRACE_(x)
#endif

typedef struct avi_writer_streams
{
    pjmedia_avi_streams base;
    unsigned         options;
    pjmedia_avi_hdr  avi_hdr;

    pj_oshandle_t    fd;
    pj_size_t        frame_cnt;
    pj_size_t        total;
    pj_size_t        max_size;

    void           (*cb2)(pjmedia_avi_streams *, void *);
    void            *user_data;
    pj_bool_t        subscribed;
} avi_writer_streams;

struct avi_port
{
    pjmedia_port     base;
    unsigned         stream_id;
    pj_size_t        frame_cnt;
    avi_writer_streams *streams;
};

static pj_status_t write_headers(pj_oshandle_t fd, avi_writer_streams *streams)
{
    unsigned i;
    pj_ssize_t size;
    pj_status_t status;

    /* Write AVI header. */
    size = sizeof(riff_hdr_t) + sizeof(avih_hdr_t);
    status = pj_file_write(fd, &streams->avi_hdr, &size);
    if (status != PJ_SUCCESS)
        return status;
    streams->total += size;

    /* Write stream headers. */
    for (i = 0; i < streams->base.num_streams; i++) {
        size = sizeof(strl_hdr_t);
        status = pj_file_write(fd, &streams->avi_hdr.strl_hdr[i],
                               &size);
        if (status != PJ_SUCCESS)
            return status;
        streams->total += size;

        size = streams->base.streams[i]->info.fmt.type == PJMEDIA_TYPE_AUDIO?
               sizeof(strf_audio_hdr_t): sizeof(strf_video_hdr_t);
        status = pj_file_write(fd, &streams->avi_hdr.strf_hdr[i],
                               &size);
        if (status != PJ_SUCCESS)
            return status;
        streams->total += size;
    }

    return PJ_SUCCESS;
}

static void close_avi_file(avi_writer_streams *streams)
{
    pj_off_t file_size;
    unsigned i;
    pj_status_t status;
    pj_oshandle_t fd;

    /* First, reset file handle, best effort in handling race conditions */
    fd = streams->fd;
    streams->fd = (pj_oshandle_t)(pj_ssize_t)-1;

    /* Already closed. */
    if (fd == (pj_oshandle_t)(pj_ssize_t)-1)
        return;

    /* Set AVI header's file length and total frames. */
    status = pj_file_getpos(fd, &file_size);
    if (status != PJ_SUCCESS)
        goto on_return;

    streams->avi_hdr.riff_hdr.file_len = (pj_uint32_t)
                                         (file_size - 8);
    pjmedia_avi_swap_data(&streams->avi_hdr.riff_hdr.file_len,
                          sizeof(pj_uint32_t), 32);
    streams->avi_hdr.avih_hdr.tot_frames = (pj_uint32_t)streams->frame_cnt;
    pjmedia_avi_swap_data(&streams->avi_hdr.avih_hdr.tot_frames,
                            sizeof(pj_uint32_t), 32);

    for (i = 0; i < streams->base.num_streams; i++) {
        pjmedia_avi_swap_data(&streams->avi_hdr.strl_hdr[i].length,
                                sizeof(pj_uint32_t), 32);
    }

    /* Rewrite headers. */
    status = pj_file_setpos(fd, 0, PJ_SEEK_SET);
    if (status != PJ_SUCCESS)
        goto on_return;
    status = write_headers(fd, streams);
    if (status != PJ_SUCCESS)
        goto on_return;

on_return:
    pj_file_close(fd);
    if (status != PJ_SUCCESS) {
        pj_perror(2, THIS_FILE, status,
                  "Error updating length & frame count in AVI header");
    }
}

static pj_status_t file_on_event(pjmedia_event *event,
                                 void *user_data)
{
    avi_writer_streams *streams = (avi_writer_streams *)user_data;

    /* Call callback. */
    if (event->type == PJMEDIA_EVENT_CALLBACK) {
        if (streams->cb2)
            (*streams->cb2)(&streams->base, streams->user_data);
    }
    
    return PJ_SUCCESS;
}

/*
 * Put frame into file.
 */
static pj_status_t avi_put_frame(pjmedia_port *this_port, 
                                 pjmedia_frame *frame)
{
    struct avi_port *fport = (struct avi_port*)this_port;
    pjmedia_avi_subchunk ch;
    pj_ssize_t size;
    pj_status_t status = PJ_SUCCESS;

    pj_assert(fport->base.info.signature == SIGNATURE);

    if (frame->size <= 0)
        return PJ_SUCCESS;

    pj_grp_lock_acquire(fport->base.grp_lock);

    if (fport->streams->fd == (pj_oshandle_t) (pj_ssize_t)-1)
        goto on_return;

    /* Check if we have reached maximum size. */
    if (fport->streams->total + frame->size + sizeof(pjmedia_avi_subchunk) >
        fport->streams->max_size)
    {
        PJ_LOG(4, (THIS_FILE, "AVI writer max size %zu reached",
                              fport->streams->max_size));

        close_avi_file(fport->streams);

        /* Call callback. */
        if (fport->streams->cb2) {
            if (!fport->streams->subscribed) {
                status = pjmedia_event_subscribe(NULL, &file_on_event,
                                                 fport->streams,
                                                 fport->streams);
                fport->streams->subscribed = (status == PJ_SUCCESS)? PJ_TRUE:
                                             PJ_FALSE;
            }

            if (fport->streams->subscribed)  {
                pjmedia_event event;

                pjmedia_event_init(&event, PJMEDIA_EVENT_CALLBACK,
                                   NULL, fport->streams);
                pjmedia_event_publish(NULL, fport->streams, &event,
                                      PJMEDIA_EVENT_PUBLISH_POST_EVENT);
            }
        }

        goto on_return;
    }

    /* Write subchunk header. */
    ch.id = 0;
    ((char *)&ch.id)[0] = '0';
    ((char *)&ch.id)[1] = '0' + (char)fport->stream_id;
    ((char *)&ch.id)[2] = 'd';
    ((char *)&ch.id)[3] = 'b';
    ch.len = (pj_uint32_t)frame->size;
    size = sizeof(ch);
    pjmedia_avi_swap_data(&ch, sizeof(ch), 32);

    status = pj_file_write(fport->streams->fd, &ch, &size);
    if (status != PJ_SUCCESS)
        goto on_return;
    fport->streams->total += size;

    /* Write subchunk data. */
    size = frame->size;
    if (fport->base.info.fmt.type == PJMEDIA_TYPE_AUDIO) {
        pjmedia_avi_swap_data(frame->buf, frame->size,
                              fport->base.info.fmt.det.aud.bits_per_sample);
    }
    status = pj_file_write(fport->streams->fd, frame->buf, &size);
    if (status != PJ_SUCCESS)
        goto on_return;
    fport->streams->total += size;
    TRACE_((THIS_FILE, "Writing %ld total:%zu", size,
                      fport->streams->total));

    /* Increase frame length/count. */
    fport->streams->avi_hdr.strl_hdr[fport->stream_id].length++;
    if (fport->base.info.fmt.type == PJMEDIA_TYPE_VIDEO) {
        fport->streams->frame_cnt++;
    }

on_return:
    pj_grp_lock_release(fport->base.grp_lock);

    return status;
}

static void streams_on_destroy(void *arg)
{
    avi_writer_streams *streams = (avi_writer_streams *)arg;

    if (streams->subscribed) {
        pjmedia_event_unsubscribe(NULL, &file_on_event, streams, streams);
        streams->subscribed = PJ_FALSE;
    }

    close_avi_file(streams);
    pj_pool_safe_release(&streams->base.pool);
}

static pj_status_t avi_on_destroy(pjmedia_port *this_port)
{
    PJ_UNUSED_ARG(this_port);

    return PJ_SUCCESS;
}

/*
 * Create AVI writer streams.
 */
PJ_DEF(pj_status_t)
pjmedia_avi_writer_create_streams(pj_pool_t *pool_,
                                  const char *filename,
                                  pj_uint32_t max_fsize,
                                  unsigned num_streams,
                                  const pjmedia_format format[],
                                  unsigned options,
                                  pjmedia_avi_streams **p_streams)
{
    avi_writer_streams *streams = NULL;
    pjmedia_avi_hdr avi_hdr;
    pj_uint32_t tags[3];
    struct avi_port *fport[PJMEDIA_AVI_MAX_NUM_STREAMS];
    unsigned i;
    pj_ssize_t size;
    pj_pool_t *pool = NULL;
    pj_grp_lock_t *grp_lock = NULL;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool_ && filename && p_streams, PJ_EINVAL);
    PJ_ASSERT_RETURN(num_streams > 0 &&
                     num_streams <= PJMEDIA_AVI_MAX_NUM_STREAMS, PJ_EINVAL);

    /* Create own pool */
    pool = pj_pool_create(pool_->factory, "aviwriter", 500, 500, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    /* Create AVI streams. */
    streams = pj_pool_calloc(pool, 1, sizeof(avi_writer_streams));
    PJ_ASSERT_RETURN(streams, PJ_ENOMEM);
    streams->options = options;
    streams->max_size = max_fsize;
    streams->base.num_streams = num_streams;
    streams->base.streams = pj_pool_calloc(pool, streams->base.num_streams,
                                           sizeof(pjmedia_port *));

    /* Create group lock */
    status = pj_grp_lock_create(pool, NULL, &grp_lock);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Init AVI headers. */
    pj_bzero(&avi_hdr, sizeof(pjmedia_avi_hdr));
    avi_hdr.riff_hdr.riff = SET_TAG(PJMEDIA_AVI_RIFF_TAG);
    avi_hdr.riff_hdr.file_len = 0; /* will be filled later */
    avi_hdr.riff_hdr.avi = SET_TAG(PJMEDIA_AVI_AVI_TAG);

    avi_hdr.avih_hdr.list_tag = SET_TAG(PJMEDIA_AVI_LIST_TAG);
    avi_hdr.avih_hdr.list_sz = sizeof(avih_hdr_t) - 8;
    avi_hdr.avih_hdr.hdrl_tag = SET_TAG(PJMEDIA_AVI_HDRL_TAG);
    avi_hdr.avih_hdr.avih = SET_TAG(PJMEDIA_AVI_AVIH_TAG);
    avi_hdr.avih_hdr.size = 56; /* sizeof MainAVIHeader */
    avi_hdr.avih_hdr.num_streams = num_streams;
    avi_hdr.avih_hdr.tot_frames = 0; /* will be filled later */

    for (i = 0; i < avi_hdr.avih_hdr.num_streams; i++) {
        const pj_str_t name = pj_str("aviw");

        /* Create AVI writer port. */
        fport[i] = PJ_POOL_ZALLOC_T(pool, struct avi_port);
        if (!fport[i]) {
            status = PJ_ENOMEM;
            goto on_error;
        }
        streams->base.streams[i] = &fport[i]->base;

        /* Init AVI writer port. */
        fport[i]->stream_id = i;
        fport[i]->streams = streams;
        fport[i]->base.put_frame = &avi_put_frame;
        fport[i]->base.on_destroy = &avi_on_destroy;
        pjmedia_port_info_init2(&fport[i]->base.info, &name, SIGNATURE,
                                PJMEDIA_DIR_DECODING, &format[i]);
        pjmedia_port_init_grp_lock(&fport[i]->base, pool, grp_lock);

        /* Populate AVI headers. */
        avi_hdr.strl_hdr[i].list_tag = SET_TAG(PJMEDIA_AVI_LIST_TAG);
        avi_hdr.strl_hdr[i].list_sz = sizeof(strl_hdr_t) - 8;
        avi_hdr.strl_hdr[i].strl_tag = SET_TAG(PJMEDIA_AVI_STRL_TAG);
        avi_hdr.strl_hdr[i].strh = SET_TAG(PJMEDIA_AVI_STRH_TAG);
        avi_hdr.strl_hdr[i].strh_size = 56; /* sizeof AVIStreamHeader */

        avi_hdr.avih_hdr.list_sz += sizeof(strl_hdr_t);

        if (format[i].type == PJMEDIA_TYPE_VIDEO) {
            pjmedia_video_format_detail *vfd;
            const pjmedia_video_format_info *vfi;
            strf_video_hdr_t *strf_hdr;

            vfd = pjmedia_format_get_video_format_detail(&format[i], PJ_TRUE);
            vfi = pjmedia_get_video_format_info(
                pjmedia_video_format_mgr_instance(),
                format[i].id);

            avi_hdr.avih_hdr.list_sz += sizeof(strf_video_hdr_t);
            avi_hdr.avih_hdr.usec_per_frame = 1000000 * vfd->fps.denum /
                                              vfd->fps.num;
            avi_hdr.avih_hdr.max_Bps = vfd->max_bps;
            avi_hdr.avih_hdr.buf_size = vfd->size.w * vfd->size.h * 4;
            avi_hdr.avih_hdr.width = vfd->size.w;
            avi_hdr.avih_hdr.height = vfd->size.h;

            avi_hdr.strl_hdr[i].list_sz += sizeof(strf_video_hdr_t);
            avi_hdr.strl_hdr[i].data_type = SET_TAG(PJMEDIA_AVI_VIDS_TAG);
            avi_hdr.strl_hdr[i].codec = format[i].id;
            avi_hdr.strl_hdr[i].rate = vfd->fps.num;
            avi_hdr.strl_hdr[i].scale = vfd->fps.denum;
            avi_hdr.strl_hdr[i].buf_size = avi_hdr.avih_hdr.buf_size;
            avi_hdr.strl_hdr[i].length = 0; /* will be filled later */

            strf_hdr = &avi_hdr.strf_hdr[i].strf_video_hdr;
            strf_hdr->strf = SET_TAG(PJMEDIA_AVI_STRF_TAG);
            strf_hdr->strf_size = sizeof(strf_video_hdr_t) - 8;
            strf_hdr->biSize = strf_hdr->strf_size;
            strf_hdr->biCompression = format[i].id;
            strf_hdr->biWidth = vfd->size.w;
            strf_hdr->biHeight = vfd->size.h;
            strf_hdr->biPlanes = 1;
            strf_hdr->biBitCount = vfi->bpp;
            strf_hdr->biSizeImage = vfd->size.w * vfd->size.h * vfi->bpp / 8;

            /* Normalize header to AVI's little endian. */
            pjmedia_avi_swap_data(&avi_hdr.strl_hdr[i], sizeof(strl_hdr_t), 32);
            pjmedia_avi_swap_data2(strf_hdr, PJ_ARRAY_SIZE(strf_video_hdr_sizes),
                                   strf_video_hdr_sizes);
        } else {
            pjmedia_audio_format_detail *afd;
            strf_audio_hdr_t *strf_hdr;

            afd = pjmedia_format_get_audio_format_detail(&format[i], PJ_TRUE);

            avi_hdr.strl_hdr[i].list_sz += sizeof(strf_audio_hdr_t);
            avi_hdr.strl_hdr[i].data_type = SET_TAG(PJMEDIA_AVI_AUDS_TAG);
            avi_hdr.strl_hdr[i].codec = format[i].id;
            avi_hdr.strl_hdr[i].rate = afd->clock_rate;
            avi_hdr.strl_hdr[i].scale = 1;
            avi_hdr.strl_hdr[i].quality = (pj_uint32_t)-1;
            avi_hdr.strl_hdr[i].buf_size = 0;
            avi_hdr.strl_hdr[i].sample_size = afd->bits_per_sample / 8;
            avi_hdr.strl_hdr[i].length = 0; /* will be filled later */

            strf_hdr = &avi_hdr.strf_hdr[i].strf_audio_hdr;
            strf_hdr->strf = SET_TAG(PJMEDIA_AVI_STRF_TAG);
            strf_hdr->strf_size = sizeof(strf_audio_hdr_t) - 8;
            strf_hdr->fmt_tag = 1; /* 1 for PCM */
            strf_hdr->nchannels = (pj_uint16_t)afd->channel_count;
            strf_hdr->sample_rate = afd->clock_rate;
            strf_hdr->block_align = (pj_uint16_t)(afd->channel_count *
                                                  afd->bits_per_sample / 8);
            strf_hdr->bytes_per_sec = strf_hdr->sample_rate * strf_hdr->block_align;
            strf_hdr->bits_per_sample = (pj_uint16_t)afd->bits_per_sample;

            /* Normalize header to AVI's little endian. */
            pjmedia_avi_swap_data(&avi_hdr.strl_hdr[i], sizeof(strl_hdr_t), 32);
            pjmedia_avi_swap_data2(strf_hdr, PJ_ARRAY_SIZE(strf_audio_hdr_sizes),
                                   strf_audio_hdr_sizes);
        }
    }

    /* Normalize header to AVI's little endian. */
    pjmedia_avi_swap_data(&avi_hdr, sizeof(riff_hdr_t)+sizeof(avih_hdr_t), 32);
    streams->avi_hdr = avi_hdr;

    /* Open file in write mode. */
    status = pj_file_open(pool, filename, PJ_O_WRONLY | PJ_O_CLOEXEC,
                          &streams->fd);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Write headers. */
    status = write_headers(streams->fd, streams);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Write MOVI tag to indicate the beginning of data. */
    tags[0] = SET_TAG(PJMEDIA_AVI_LIST_TAG);
    tags[1] = 4;
    tags[2] = SET_TAG(PJMEDIA_AVI_MOVI_TAG);
    size = sizeof(tags);
    pjmedia_avi_swap_data(tags, size, 32);
    status = pj_file_write(streams->fd, tags, &size);
    if (status != PJ_SUCCESS)
        goto on_error;       
    streams->total += size;

    /* Done. */
    *p_streams = (pjmedia_avi_streams *)streams;
    (*p_streams)->pool = pool;

    status = pj_grp_lock_add_handler(grp_lock, NULL, *p_streams,
                                     &streams_on_destroy);
    if (status != PJ_SUCCESS)
        goto on_error;

    PJ_LOG(4,(THIS_FILE, 
              "AVI file writer '%.*s' created with "
              "%d media ports",
              (int)fport[0]->base.info.name.slen,
              fport[0]->base.info.name.ptr,
              (*p_streams)->num_streams));

    return PJ_SUCCESS;

on_error:
    if (streams && streams->fd) {
        pj_file_close(streams->fd);
        streams->fd = (pj_oshandle_t)(pj_ssize_t)-1;
    }

    if (grp_lock) {
        pjmedia_port_destroy(&fport[0]->base);
        for (i = 1; i < num_streams; i++)
            pjmedia_port_destroy(&fport[i]->base);
    }
    
    pj_pool_release(pool);

    return status;
}

PJ_DEF(pj_status_t) 
pjmedia_avi_streams_set_cb(pjmedia_avi_streams *streams,
                           void *user_data,
                           void (*cb)(pjmedia_avi_streams *streams,
                                      void *usr_data))
{
    avi_writer_streams *aviw = (avi_writer_streams *)streams;

    PJ_ASSERT_RETURN(streams && cb, PJ_EINVAL);

    aviw->cb2 = cb;
    aviw->user_data = user_data;

    return PJ_SUCCESS;
}

#endif /* PJMEDIA_HAS_VIDEO */
