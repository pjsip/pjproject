/*
 * Copyright (C) 2013-2014 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_VIDEODEV_OPENGL_DEV_H__
#define __PJMEDIA_VIDEODEV_OPENGL_DEV_H__

#include <pjmedia-videodev/videodev_imp.h>

/* OpenGL implementation on each platform needs to implement these functions
 * and stream operations.
 */
/* Get capabilities of the implementation */
int pjmedia_vid_dev_opengl_imp_get_cap(void);

/* Create OpenGL stream */
pj_status_t
pjmedia_vid_dev_opengl_imp_create_stream(pj_pool_t *pool,
                                         pjmedia_vid_dev_param *param,
                                         const pjmedia_vid_dev_cb *cb,
                                         void *user_data,
                                         pjmedia_vid_dev_stream **p_vid_strm);

/****************************************************************************/
/* OpenGL buffers opaque structure. */
typedef struct gl_buffers gl_buffers;

/* Create OpenGL buffers. */
void        pjmedia_vid_dev_opengl_create_buffers(pj_pool_t *pool, pj_bool_t direct,
                                                  gl_buffers **glb);
/* Initialize OpenGL buffers. */
pj_status_t pjmedia_vid_dev_opengl_init_buffers(gl_buffers *glb);
/* Render a texture. */
pj_status_t pjmedia_vid_dev_opengl_draw(gl_buffers *glb,unsigned int width,
                                        unsigned int height, void *pixels);
/* Destroy OpenGL buffers. */
void        pjmedia_vid_dev_opengl_destroy_buffers(gl_buffers *glb);

#endif    /* __PJMEDIA_VIDEODEV_OPENGL_DEV_H__ */
