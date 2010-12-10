#include "test.h"
#include <pjmedia-codec/ffmpeg_codecs.h>
#include <pjmedia-videodev/videodev.h>
#include <pjmedia/vid_codec.h>
#include <pjmedia/port.h>

#define THIS_FILE "vid_codec.c"

#define BYPASS_CODEC 0

typedef struct codec_port_data_t
{
    pjmedia_vid_codec   *codec;
    pjmedia_port        *dn_port;
    pj_uint8_t          *enc_buf;
    pj_size_t            enc_buf_size;
} codec_port_data_t;

static pj_status_t codec_put_frame(pjmedia_port *port,
			           pjmedia_frame *frame)
{
    codec_port_data_t *port_data = (codec_port_data_t*)port->port_data.pdata;
    pjmedia_vid_codec *codec = port_data->codec;
    pjmedia_frame enc_frame;
    pj_status_t status;

    enc_frame.buf = port_data->enc_buf;
    enc_frame.size = port_data->enc_buf_size;

#if !BYPASS_CODEC
    status = codec->op->encode(codec, frame, enc_frame.size, &enc_frame);
    if (status != PJ_SUCCESS) goto on_error;
    status = codec->op->decode(codec, &enc_frame, frame->size, frame);
    if (status != PJ_SUCCESS) goto on_error;
#endif

    status = pjmedia_port_put_frame(port_data->dn_port, frame);
    if (status != PJ_SUCCESS) goto on_error;

    return PJ_SUCCESS;

on_error:
    pj_perror(3, THIS_FILE, status, "codec_put_frame() error");
    return status;
}

static const char* dump_codec_info(const pjmedia_vid_codec_info *info)
{
    static char str[80];
    unsigned i;
    char *p = str;

    /* Raw format ids */
    for (i=0; (i<info->dec_fmt_id_cnt) && (p-str+5<sizeof(str)); ++i) {
        pj_memcpy(p, &info->dec_fmt_id[i], 4);
        p += 4;
        *p++ = ' ';
    }
    *p = '\0';

    return str;
}

static int enum_codecs()
{
    unsigned i, cnt;
    pjmedia_vid_codec_info info[PJMEDIA_CODEC_MGR_MAX_CODECS];
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  codec enums"));
    cnt = PJ_ARRAY_SIZE(info);
    status = pjmedia_vid_codec_mgr_enum_codecs(NULL, &cnt, info, NULL);
    if (status != PJ_SUCCESS)
        return 100;

    for (i = 0; i < cnt; ++i) {
        PJ_LOG(3, (THIS_FILE, "  %16.*s %c%c %s",
                   info[i].encoding_name.slen, info[i].encoding_name.ptr,
                   (info[i].dir & PJMEDIA_DIR_ENCODING? 'E' : ' '),
                   (info[i].dir & PJMEDIA_DIR_DECODING? 'D' : ' '),
                   dump_codec_info(&info[i])));
    }

    return PJ_SUCCESS;
}

static int encode_decode_test(pj_pool_t *pool, const char *codec_id,
                              pjmedia_format_id raw_fmt_id)
{

    pjmedia_vid_codec *codec=NULL;
    pjmedia_port codec_port;
    codec_port_data_t codec_port_data;
    pjmedia_vid_codec_param codec_param;
    const pjmedia_vid_codec_info *codec_info;

    pjmedia_vid_dev_index cap_idx, rdr_idx;
    pjmedia_vid_port *capture=NULL, *renderer=NULL;
    pjmedia_vid_port_param vport_param;
    pjmedia_video_format_detail *vfd;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "  encode decode test"));

    /* Lookup codec */
    {
        pj_str_t codec_id_st;
        unsigned info_cnt = 1;

        /* Lookup codec */
        pj_cstr(&codec_id_st, codec_id);
        status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL, &codec_id_st, 
                                                         &info_cnt, 
                                                         &codec_info, NULL);
        if (status != PJ_SUCCESS) {
            rc = 205; goto on_return;
        }
    }

    /* Lookup colorbar source */
    status = pjmedia_vid_dev_lookup("Colorbar", "Colorbar generator", &cap_idx);
    if (status != PJ_SUCCESS) {
	rc = 206; goto on_return;
    }

    /* Lookup SDL renderer */
    status = pjmedia_vid_dev_lookup("SDL", "SDL renderer", &rdr_idx);
    if (status != PJ_SUCCESS) {
	rc = 207; goto on_return;
    }

    /* Raw format ID "not specified", lets find common format among the codec
     * and the video devices
     */
    if (raw_fmt_id == 0) {
        pjmedia_vid_dev_info cap_info, rdr_info;
        unsigned i, j, k;

        pjmedia_vid_dev_get_info(cap_idx, &cap_info);
        pjmedia_vid_dev_get_info(rdr_idx, &rdr_info);

        for (i=0; i<codec_info->dec_fmt_id_cnt && !raw_fmt_id; ++i) {
            for (j=0; j<cap_info.fmt_cnt && !raw_fmt_id; ++j) {
                if (codec_info->dec_fmt_id[i]==(int)cap_info.fmt[j].id) {
                    for (k=0; k<rdr_info.fmt_cnt && !raw_fmt_id; ++k) {
                        if (codec_info->dec_fmt_id[i]==(int)rdr_info.fmt[k].id)
                        {
                            raw_fmt_id = codec_info->dec_fmt_id[i];
                        }
                    }
                }
            }
        }

        if (raw_fmt_id == 0) {
            PJ_LOG(3, (THIS_FILE, "  No common format ID among the codec "
                       "and the video devices"));
            status = PJ_ENOTFOUND;
            rc = 210;
            goto on_return;
        }
    }

    pjmedia_vid_port_param_default(&vport_param);

    /* Create capture, set it to active (master) */
    status = pjmedia_vid_dev_default_param(pool, cap_idx,
					   &vport_param.vidparam);
    if (status != PJ_SUCCESS) {
	rc = 220; goto on_return;
    }
    vport_param.vidparam.fmt.id = raw_fmt_id;
    vport_param.vidparam.dir = PJMEDIA_DIR_CAPTURE;
    vport_param.active = PJ_TRUE;

    if (vport_param.vidparam.fmt.detail_type != PJMEDIA_FORMAT_DETAIL_VIDEO) {
	rc = 221; goto on_return;
    }

    vfd = pjmedia_format_get_video_format_detail(&vport_param.vidparam.fmt,
						 PJ_TRUE);
    if (vfd == NULL) {
	rc = 225; goto on_return;
    }

    status = pjmedia_vid_port_create(pool, &vport_param, &capture);
    if (status != PJ_SUCCESS) {
	rc = 226; goto on_return;
    }

    /* Create renderer, set it to passive (slave)  */
    vport_param.active = PJ_FALSE;
    vport_param.vidparam.dir = PJMEDIA_DIR_RENDER;
    vport_param.vidparam.rend_id = rdr_idx;
    vport_param.vidparam.disp_size = vfd->size;

    status = pjmedia_vid_port_create(pool, &vport_param, &renderer);
    if (status != PJ_SUCCESS) {
	rc = 230; goto on_return;
    }

    /* Prepare codec */
    {
        pj_str_t codec_id_st;
        unsigned info_cnt = 1;
        const pjmedia_vid_codec_info *codec_info;
        pj_str_t port_name = {"codec", 5};
        pj_uint8_t *enc_buf = NULL;
        pj_size_t enc_buf_size = 0;


        /* Lookup codec */
        pj_cstr(&codec_id_st, codec_id);
        status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL, &codec_id_st, 
                                                         &info_cnt, 
                                                         &codec_info, NULL);
        if (status != PJ_SUCCESS) {
            rc = 245; goto on_return;
        }
        status = pjmedia_vid_codec_mgr_get_default_param(NULL, codec_info,
                                                         &codec_param);
        if (status != PJ_SUCCESS) {
            rc = 246; goto on_return;
        }

        pjmedia_format_copy(&codec_param.dec_fmt, &vport_param.vidparam.fmt);

#if !BYPASS_CODEC

        /* Open codec */
        status = pjmedia_vid_codec_mgr_alloc_codec(NULL, codec_info,
                                                   &codec);
        if (status != PJ_SUCCESS) {
	    rc = 250; goto on_return;
        }

        status = codec->op->init(codec, pool);
        if (status != PJ_SUCCESS) {
	    rc = 251; goto on_return;
        }

        status = codec->op->open(codec, &codec_param);
        if (status != PJ_SUCCESS) {
	    rc = 252; goto on_return;
        }

        /* Alloc encoding buffer */
        enc_buf_size =  codec_param.dec_fmt.det.vid.size.w *
                        codec_param.dec_fmt.det.vid.size.h * 4
                        + 16; /*< padding, just in case */
        enc_buf = pj_pool_alloc(pool,enc_buf_size);

#endif /* !BYPASS_CODEC */

        /* Init codec port */
        pj_bzero(&codec_port, sizeof(codec_port));
        status = pjmedia_port_info_init2(&codec_port.info, &port_name, 0x1234,
                                         PJMEDIA_DIR_ENCODING, 
                                         &codec_param.dec_fmt);
        if (status != PJ_SUCCESS) {
	    rc = 260; goto on_return;
        }
        codec_port_data.codec = codec;
        codec_port_data.dn_port = pjmedia_vid_port_get_passive_port(renderer);
        codec_port_data.enc_buf = enc_buf;
        codec_port_data.enc_buf_size = enc_buf_size;

        codec_port.put_frame = &codec_put_frame;
        codec_port.port_data.pdata = &codec_port_data;
    }


    /* Connect capture to codec port */
    status = pjmedia_vid_port_connect(capture,
				      &codec_port,
				      PJ_FALSE);
    if (status != PJ_SUCCESS) {
	rc = 270; goto on_return;
    }

    PJ_LOG(3, (THIS_FILE, "  starting codec test:  %c%c%c%c<->%s %dx%d",
        ((codec_param.dec_fmt.id & 0x000000FF) >> 0),
        ((codec_param.dec_fmt.id & 0x0000FF00) >> 8),
        ((codec_param.dec_fmt.id & 0x00FF0000) >> 16),
        ((codec_param.dec_fmt.id & 0xFF000000) >> 24),
        codec_id, 
        codec_param.dec_fmt.det.vid.size.w,
        codec_param.dec_fmt.det.vid.size.h
        ));

    /* Start streaming.. */
    status = pjmedia_vid_port_start(renderer);
    if (status != PJ_SUCCESS) {
	rc = 275; goto on_return;
    }
    status = pjmedia_vid_port_start(capture);
    if (status != PJ_SUCCESS) {
	rc = 280; goto on_return;
    }

    /* Sleep while the video is being displayed... */
    pj_thread_sleep(10000);

on_return:
    if (status != PJ_SUCCESS) {
        PJ_PERROR(3, (THIS_FILE, status, "  error"));
    }
    if (capture) {
        pjmedia_vid_port_stop(capture);
	pjmedia_vid_port_destroy(capture);
    }
    if (renderer) {
        pjmedia_vid_port_stop(renderer);
	pjmedia_vid_port_destroy(renderer);
    }
    if (codec) {
        codec->op->close(codec);
        pjmedia_vid_codec_mgr_dealloc_codec(NULL, codec);
    }

    return rc;
}

int vid_codec_test(void)
{
    pj_pool_t *pool;
    int rc = 0;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "Performing video codec tests.."));

    pool = pj_pool_create(mem, "Vid codec test", 256, 256, 0);

    status = pjmedia_vid_subsys_init(mem);
    if (status != PJ_SUCCESS)
        return -10;

    status = pjmedia_codec_ffmpeg_init(NULL, mem);
    if (status != PJ_SUCCESS)
        return -20;

    rc = enum_codecs();
    if (rc != 0)
	goto on_return;

    rc = encode_decode_test(pool, "mjpeg", 0);
    if (rc != 0)
	goto on_return;

on_return:
    pjmedia_codec_ffmpeg_deinit();
    pjmedia_vid_subsys_shutdown();
    pj_pool_release(pool);

    return rc;
}


