/*
 * Copyright (C) 2014-2015 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_VIDEODEV_UTIL_H__
#define __PJMEDIA_VIDEODEV_UTIL_H__

#include <pjmedia/converter.h>
#include <pjmedia/format.h>
#include <pjmedia/types.h>

/*
 * Video device utility functions to resize and rotate video frames.
 */

typedef struct pjmedia_vid_dev_conv
{
    pjmedia_converter      *conv;
    pjmedia_format          fmt;
    pjmedia_rect_size       src_size;
    pjmedia_rect_size       dst_size;
    pjmedia_rect_size       res_size;           /* Size after resizing   */
    pjmedia_orient          rotation;
    pjmedia_rect_size       rot_size;           /* Size after rotation   */ 
    
    void                   *conv_buf;
    pj_size_t               src_frame_size;
    pj_size_t               conv_frame_size;
    
    pj_bool_t               fit_to_h;
    pj_bool_t               handle_rotation;
    pj_bool_t               maintain_aspect_ratio;
    pj_bool_t               match_src_dst;
    pj_int32_t              pad;
    pj_size_t               wxh;
} pjmedia_vid_dev_conv;

/**
 * Create converter.
 * The process: 
 * frame --> resize -->        rotate       -->           center
 *                      (if handle_rotation      (if maintain_aspect_ratio
 *                          == PJ_TRUE)                 == PJ_TRUE)
 *
 * handle_rotation will specify whether the converter will need to do the
 * rotation as well. If PJ_FALSE, the video device will handle the rotation
 * and pass the already-rotated frame.
 *
 * maintain_aspect_ratio defines whether aspect ratio should be maintained
 * when rotating the image.
 * If PJ_TRUE, a frame of size w x h will be resized and rotated to
 * a new frame of size new_w x new_h (new_h and new_h have the same
 * aspect ratio as w x h). Then the new frame will be centered-fit into
 * the original frame with black area inserted to fill the gaps.
 * Disabling this setting will only resize the frame of size w x h to h x w,
 * and then rotate it to fit the original size of w x h. It will achieve
 * a slightly faster performance but the resulting image will be stretched.
 * The feature to maintain aspect ratio is only supported for certain formats
 * (currently, only if fmt.id equals to I420).
 */
pj_status_t
pjmedia_vid_dev_conv_create_converter(pjmedia_vid_dev_conv *conv,
                                      pj_pool_t *pool,
                                      pjmedia_format *fmt,
                                      pjmedia_rect_size src_size,
                                      pjmedia_rect_size dst_size,
                                      pj_bool_t handle_rotation,
                                      pj_bool_t maintain_aspect_ratio);

/* Set rotation */
void pjmedia_vid_dev_conv_set_rotation(pjmedia_vid_dev_conv *conv,
                                       pjmedia_orient rotation);

/* Resize the buffer and rotate it, if necessary */
pj_status_t pjmedia_vid_dev_conv_resize_and_rotate(pjmedia_vid_dev_conv *conv,
                                                    void *src_buf,
                                                    void **result);

/* Destroy converter */
void pjmedia_vid_dev_conv_destroy_converter(pjmedia_vid_dev_conv *conv);

#endif    /* __PJMEDIA_VIDEODEV_UTIL_H__ */
