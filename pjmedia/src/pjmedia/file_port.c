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
#include <pjmedia/file_port.h>
#include <pjmedia/errno.h>
#include <pjmedia/wave.h>
#include <pj/assert.h>
#include <pj/file_access.h>
#include <pj/file_io.h>
#include <pj/pool.h>
#include <pj/string.h>


#define SIGNATURE	('F'<<24|'I'<<16|'L'<<8|'E')

struct file_port
{
    pjmedia_port     base;
    pj_size_t	     bufsize;
    char	    *buf;
    char	    *readpos;
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
    port->base.info.sample_rate = 8000;
    port->base.info.bits_per_sample = 16;
    port->base.info.samples_per_frame = 160;
    port->base.info.bytes_per_frame = 320;

    return port;
}

/*
 * Create WAVE player port.
 */
PJ_DEF(pj_status_t) pjmedia_file_player_port_create( pj_pool_t *pool,
						     const char *filename,
						     unsigned flags,
						     pj_ssize_t buff_size,
						     void *user_data,
						     pjmedia_port **p_port )
{
    pj_off_t file_size;
    pj_oshandle_t fd = NULL;
    pjmedia_wave_hdr wave_hdr;
    pj_ssize_t size_read;
    struct file_port *file_port;
    pj_status_t status;


    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(buff_size);

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && filename && p_port, PJ_EINVAL);

    /* Check the file really exists. */
    if (!pj_file_exists(filename)) {
	return PJ_ENOTFOUND;
    }

    /* Get the file size. */
    file_size = pj_file_size(filename);

    /* Size must be more than WAVE header size */
    if (file_size <= sizeof(pjmedia_wave_hdr)) {
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Open file. */
    status = pj_file_open( pool, filename, PJ_O_RDONLY, &fd);
    if (status != PJ_SUCCESS)
	return status;

    /* Read the WAVE header. */
    size_read = sizeof(wave_hdr);
    status = pj_file_read( fd, &wave_hdr, &size_read);
    if (status != PJ_SUCCESS) {
	pj_file_close(fd);
	return status;
    }
    if (size_read != sizeof(wave_hdr)) {
	pj_file_close(fd);
	return PJMEDIA_ENOTVALIDWAVE;
    }

    /* Validate WAVE file. */
    if (wave_hdr.riff_hdr.riff != PJMEDIA_RIFF_TAG ||
	wave_hdr.riff_hdr.wave != PJMEDIA_WAVE_TAG ||
	wave_hdr.fmt_hdr.fmt != PJMEDIA_FMT_TAG)
    {
	pj_file_close(fd);
	return PJMEDIA_ENOTVALIDWAVE;
    }

    if (wave_hdr.fmt_hdr.fmt_tag != 1 ||
	wave_hdr.fmt_hdr.nchan != 1 ||
	wave_hdr.fmt_hdr.bits_per_sample != 16 ||
	wave_hdr.fmt_hdr.block_align != 2)
    {
	pj_file_close(fd);
	return PJMEDIA_EWAVEUNSUPP;
    }

    /* Validate length. */
    if (wave_hdr.data_hdr.len != file_size-sizeof(pjmedia_wave_hdr)) {
	pj_file_close(fd);
	return PJMEDIA_EWAVEUNSUPP;
    }
    if (wave_hdr.data_hdr.len < 400) {
	pj_file_close(fd);
	return PJMEDIA_EWAVETOOSHORT;
    }

    /* It seems like we have a valid WAVE file. */

    /* Create file_port instance. */
    file_port = create_file_port(pool);
    if (!file_port) {
	pj_file_close(fd);
	return PJ_ENOMEM;
    }

    /* Initialize */
    file_port->base.user_data = user_data;

    /* Update port info. */
    file_port->base.info.sample_rate = wave_hdr.fmt_hdr.sample_rate;
    file_port->base.info.bits_per_sample = wave_hdr.fmt_hdr.bits_per_sample;
    file_port->base.info.samples_per_frame = file_port->base.info.sample_rate *
					     20 / 1000;
    file_port->base.info.bytes_per_frame = 
	file_port->base.info.samples_per_frame * 
	file_port->base.info.bits_per_sample / 8;


    /* For this version, we only support reading the whole
     * contents of the file.
     */
    file_port->bufsize = wave_hdr.data_hdr.len - 8;

    /* Create buffer. */
    file_port->buf = pj_pool_alloc(pool, file_port->bufsize);
    if (!file_port->buf) {
	pj_file_close(fd);
	return PJ_ENOMEM;
    }

    file_port->readpos = file_port->buf;

    /* Read the the file. */
    size_read = file_port->bufsize;
    status = pj_file_read(fd, file_port->buf, &size_read);
    if (status != PJ_SUCCESS) {
	pj_file_close(fd);
	return status;
    }

    if (size_read != (pj_ssize_t)file_port->bufsize) {
	pj_file_close(fd);
	return PJMEDIA_ENOTVALIDWAVE;
    }


    /* Done. */
    pj_file_close(fd);

    *p_port = &file_port->base;

    return PJ_SUCCESS;
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
    struct file_port *port = (struct file_port*)this_port;
    unsigned frame_size;
    pj_assert(port->base.info.signature == SIGNATURE);

    frame_size = port->base.info.bytes_per_frame;

    /* Copy frame from buffer. */
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = frame_size;
    frame->timestamp.u64 = 0;

    if (port->readpos + frame_size <= port->buf + port->bufsize) {
	pj_memcpy(frame->buf, port->readpos, frame_size);
	port->readpos += frame_size;
	if (port->readpos == port->buf + port->bufsize)
	    port->readpos = port->buf;
    } else {
	unsigned endread;

	endread = (port->buf+port->bufsize) - port->readpos;
	pj_memcpy(frame->buf, port->readpos, endread);
	pj_memcpy(((char*)frame->buf)+endread, port->buf, frame_size-endread);
	port->readpos = port->buf + (frame_size - endread);
    }

    return PJ_SUCCESS;
}

/*
 * Destroy port.
 */
static pj_status_t file_on_destroy(pjmedia_port *this_port)
{
    PJ_UNUSED_ARG(this_port);

    pj_assert(this_port->info.signature == SIGNATURE);

    return PJ_SUCCESS;
}
