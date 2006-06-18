/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/wav_port.h>
#include <pjmedia/errno.h>
#include <pjmedia/wave.h>
#include <pj/assert.h>
#include <pj/file_access.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE   "wav_player.c"


#define SIGNATURE	    ('F'<<24|'P'<<16|'L'<<8|'Y')
#define BYTES_PER_SAMPLE    2


#if 1
#   define TRACE_(x)	PJ_LOG(4,x)
#else
#   define TRACE_(x)
#endif

#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    static void samples_to_host(pj_int16_t *samples, unsigned count)
    {
	unsigned i;
	for (i=0; i<count; ++i) {
	    samples[i] = pj_swap16(samples[i]);
	}
    }
#else
#   define samples_to_host(samples,count)
#endif

struct file_port
{
    pjmedia_port     base;
    unsigned	     options;
    pj_bool_t	     eof;
    pj_size_t	     bufsize;
    char	    *buf;
    char	    *readpos;

    pj_off_t	     fsize;
    pj_off_t	     fpos;
    pj_oshandle_t    fd;

};


static pj_status_t file_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame);
static pj_status_t file_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t file_on_destroy(pjmedia_port *this_port);

static struct file_port *create_file_port(pj_pool_t *pool)
{
    struct file_port *port;

    port = pj_pool_zalloc(pool, sizeof(struct file_port));
    if (!port)
	return NULL;

    port->base.info.name = pj_str("file");
    port->base.info.signature = SIGNATURE;
    port->base.info.type = PJMEDIA_TYPE_AUDIO;
    port->base.info.has_info = PJ_TRUE;
    port->base.info.need_info = PJ_FALSE;
    port->base.info.pt = 0xFF;
    port->base.info.encoding_name = pj_str("pcm");

    port->base.put_frame = &file_put_frame;
    port->base.get_frame = &file_get_frame;
    port->base.on_destroy = &file_on_destroy;


    /* Put in default values.
     * These will be overriden once the file is read.
     */
    port->base.info.clock_rate = 8000;
    port->base.info.bits_per_sample = 16;
    port->base.info.samples_per_frame = 160;
    port->base.info.bytes_per_frame = 320;

    return port;
}

/*
 * Fill buffer.
 */
static pj_status_t fill_buffer(struct file_port *fport)
{
    pj_ssize_t size_left = fport->bufsize;
    unsigned size_to_read;
    pj_ssize_t size;
    pj_status_t status;

    if (fport->eof) {
	return PJ_EEOF;
    }

    while (size_left > 0) {

	/* Calculate how many bytes to read in this run. */
	size = size_to_read = size_left;
	status = pj_file_read(fport->fd, 
			      &fport->buf[fport->bufsize-size_left], 
			      &size);
	if (status != PJ_SUCCESS)
	    return status;
	if (size < 0) {
	    /* Should return more appropriate error code here.. */
	    return PJ_ECANCELLED;
	}

	size_left -= size;
	fport->fpos += size;

	/* If size is less than size_to_read, it indicates that we've
	 * encountered EOF. Rewind the file.
	 */
	if (size < (pj_ssize_t)size_to_read) {
	    if (fport->options & PJMEDIA_FILE_NO_LOOP) {
		PJ_LOG(5,(THIS_FILE, "File port %.*s EOF, stopping..",
			  (int)fport->base.info.name.slen,
			  fport->base.info.name.ptr));
		fport->eof = PJ_TRUE;
		return PJ_EEOF;
	    } else {
		PJ_LOG(5,(THIS_FILE, "File port %.*s EOF, rewinding..",
			  (int)fport->base.info.name.slen,
			  fport->base.info.name.ptr));
		fport->fpos = sizeof(struct pjmedia_wave_hdr);
		pj_file_setpos( fport->fd, fport->fpos, PJ_SEEK_SET);
	    }
	}
    }

    /* Convert samples to host rep */
    samples_to_host((pj_int16_t*)fport->buf, fport->bufsize/BYTES_PER_SAMPLE);

    return PJ_SUCCESS;
}


/*
 * Create WAVE player port.
 */
PJ_DEF(pj_status_t) pjmedia_wav_player_port_create( pj_pool_t *pool,
						     const char *filename,
						     unsigned ptime,
						     unsigned options,
						     pj_ssize_t buff_size,
						     void *user_data,
						     pjmedia_port **p_port )
{
    pjmedia_wave_hdr wave_hdr;
    pj_ssize_t size_read;
    struct file_port *fport;
    pj_status_t status;


    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && filename && p_port, PJ_EINVAL);

    /* Check the file really exists. */
    if (!pj_file_exists(filename)) {
	return PJ_ENOTFOUND;
    }

    /* Normalize ptime */
    if (ptime == 0)
	ptime = 20;

    /* Create fport instance. */
    fport = create_file_port(pool);
    if (!fport) {
	return PJ_ENOMEM;
    }


    /* Get the file size. */
    fport->fsize = pj_file_size(filename);

    /* Size must be more than WAVE header size */
    if (fport->fsize <= sizeof(pjmedia_wave_hdr)) {
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Open file. */
    status = pj_file_open( pool, filename, PJ_O_RDONLY, &fport->fd);
    if (status != PJ_SUCCESS)
	return status;

    /* Read the WAVE header. */
    size_read = sizeof(wave_hdr);
    status = pj_file_read( fport->fd, &wave_hdr, &size_read);
    if (status != PJ_SUCCESS) {
	pj_file_close(fport->fd);
	return status;
    }
    if (size_read != sizeof(wave_hdr)) {
	pj_file_close(fport->fd);
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Normalize WAVE header fields values from little-endian to host
     * byte order.
     */
    pjmedia_wave_hdr_file_to_host(&wave_hdr);
    
    /* Validate WAVE file. */
    if (wave_hdr.riff_hdr.riff != PJMEDIA_RIFF_TAG ||
	wave_hdr.riff_hdr.wave != PJMEDIA_WAVE_TAG ||
	wave_hdr.fmt_hdr.fmt != PJMEDIA_FMT_TAG)
    {
	pj_file_close(fport->fd);
	TRACE_((THIS_FILE, 
		"actual value|expected riff=%x|%x, wave=%x|%x fmt=%x|%x",
		wave_hdr.riff_hdr.riff, PJMEDIA_RIFF_TAG,
		wave_hdr.riff_hdr.wave, PJMEDIA_WAVE_TAG,
		wave_hdr.fmt_hdr.fmt, PJMEDIA_FMT_TAG));
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Must be PCM with 16bits per sample */
    if (wave_hdr.fmt_hdr.fmt_tag != 1 ||
	wave_hdr.fmt_hdr.bits_per_sample != 16)
    {
	pj_file_close(fport->fd);
	return PJMEDIA_EWAVEUNSUPP;
    }

    /* Block align must be 2*nchannels */
    if (wave_hdr.fmt_hdr.block_align != wave_hdr.fmt_hdr.nchan*BYTES_PER_SAMPLE) {
	pj_file_close(fport->fd);
	return PJMEDIA_EWAVEUNSUPP;
    }

    /* Validate length. */
    if (wave_hdr.data_hdr.len != fport->fsize-sizeof(pjmedia_wave_hdr)) {
	pj_file_close(fport->fd);
	return PJMEDIA_EWAVEUNSUPP;
    }
    if (wave_hdr.data_hdr.len < 400) {
	pj_file_close(fport->fd);
	return PJMEDIA_EWAVETOOSHORT;
    }

    /* It seems like we have a valid WAVE file. */

    /* Initialize */
    fport->base.user_data = user_data;
    fport->options = options;

    /* Update port info. */
    fport->base.info.channel_count = wave_hdr.fmt_hdr.nchan;
    fport->base.info.clock_rate = wave_hdr.fmt_hdr.sample_rate;
    fport->base.info.bits_per_sample = wave_hdr.fmt_hdr.bits_per_sample;
    fport->base.info.samples_per_frame = fport->base.info.clock_rate *
					 wave_hdr.fmt_hdr.nchan *
					 ptime / 1000;
    fport->base.info.bytes_per_frame = 
	fport->base.info.samples_per_frame * 
	fport->base.info.bits_per_sample / 8;

    pj_strdup2(pool, &fport->base.info.name, filename);

    /* Create file buffer.
     */
    if (buff_size < 1) buff_size = PJMEDIA_FILE_PORT_BUFSIZE;
    fport->bufsize = buff_size;


    /* Create buffer. */
    fport->buf = pj_pool_alloc(pool, fport->bufsize);
    if (!fport->buf) {
	pj_file_close(fport->fd);
	return PJ_ENOMEM;
    }

    fport->readpos = fport->buf;

    /* Set initial position of the file. */
    fport->fpos = sizeof(struct pjmedia_wave_hdr);

    /* Fill up the buffer. */
    status = fill_buffer(fport);
    if (status != PJ_SUCCESS) {
	pj_file_close(fport->fd);
	return status;
    }

    /* Done. */

    *p_port = &fport->base;


    PJ_LOG(4,(THIS_FILE, 
	      "File player '%.*s' created: samp.rate=%d, ch=%d, bufsize=%uKB, "
	      "filesize=%luKB",
	      (int)fport->base.info.name.slen,
	      fport->base.info.name.ptr,
	      fport->base.info.clock_rate,
	      fport->base.info.channel_count,
	      fport->bufsize / 1000,
	      (unsigned long)(fport->fsize / 1000)));

    return PJ_SUCCESS;
}


/*
 * Set position.
 */
PJ_DEF(pj_status_t) pjmedia_wav_player_port_set_pos(pjmedia_port *port,
						    pj_uint32_t samples )
{
    struct file_port *fport;

    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    fport = (struct file_port*) port;

    PJ_ASSERT_RETURN(samples*BYTES_PER_SAMPLE < fport->fsize -
		      sizeof(pjmedia_wave_hdr), PJ_EINVAL);

    fport->fpos = sizeof(struct pjmedia_wave_hdr) + 
		    samples * BYTES_PER_SAMPLE;
    pj_file_setpos( fport->fd, fport->fpos, PJ_SEEK_SET);

    fport->eof = PJ_FALSE;
    return fill_buffer(fport);
}


/*
 * Put frame to file.
 */
static pj_status_t file_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_EINVALIDOP;
}

/*
 * Get frame from file.
 */
static pj_status_t file_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct file_port *fport = (struct file_port*)this_port;
    unsigned frame_size;
    pj_status_t status;

    pj_assert(fport->base.info.signature == SIGNATURE);

    //frame_size = fport->base.info.bytes_per_frame;
    //pj_assert(frame->size == frame_size);
    frame_size = frame->size;

    /* Copy frame from buffer. */
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = frame_size;
    frame->timestamp.u64 = 0;

    if (fport->readpos + frame_size <= fport->buf + fport->bufsize) {

	/* Read contiguous buffer. */
	pj_memcpy(frame->buf, fport->readpos, frame_size);

	/* Fill up the buffer if all has been read. */
	fport->readpos += frame_size;
	if (fport->readpos == fport->buf + fport->bufsize) {
	    fport->readpos = fport->buf;

	    status = fill_buffer(fport);
	    if (status != PJ_SUCCESS) {
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		frame->size = 0;
		return status;
	    }
	}
    } else {
	unsigned endread;

	/* Split read.
	 * First stage: read until end of buffer. 
	 */
	endread = (fport->buf+fport->bufsize) - fport->readpos;
	pj_memcpy(frame->buf, fport->readpos, endread);

	/* Second stage: fill up buffer, and read from the start of buffer. */
	status = fill_buffer(fport);
	if (status != PJ_SUCCESS) {
	    pj_memset(((char*)frame->buf)+endread, 0, frame_size-endread);
	    return status;
	}

	pj_memcpy(((char*)frame->buf)+endread, fport->buf, frame_size-endread);
	fport->readpos = fport->buf + (frame_size - endread);
    }

    return PJ_SUCCESS;
}

/*
 * Destroy port.
 */
static pj_status_t file_on_destroy(pjmedia_port *this_port)
{
    struct file_port *fport = (struct file_port*) this_port;

    pj_assert(this_port->info.signature == SIGNATURE);

    pj_file_close(fport->fd);
    return PJ_SUCCESS;
}
