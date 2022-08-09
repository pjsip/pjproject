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
 
#include "util.h"

#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#if defined(PJMEDIA_HAS_LIBYUV) && PJMEDIA_HAS_LIBYUV != 0
    #include  <libyuv.h>
    #define HAS_ROTATION 1
#else
    #define HAS_ROTATION 0
#endif

#define THIS_FILE               "vid_util.c"

pj_status_t
pjmedia_vid_dev_conv_create_converter(pjmedia_vid_dev_conv *conv,
                                      pj_pool_t *pool,
                                      pjmedia_format *fmt,
                                      pjmedia_rect_size src_size,
                                      pjmedia_rect_size dst_size,
                                      pj_bool_t handle_rotation,
                                      pj_bool_t maintain_aspect_ratio)
{
    pj_status_t status;
    pjmedia_conversion_param conv_param;
    const pjmedia_video_format_info *vfi;       

    pj_assert((src_size.w == dst_size.w || src_size.h == dst_size.h) ||
              (src_size.w == dst_size.h || src_size.h == dst_size.w));
              
    if (conv->conv)
        return PJ_SUCCESS;
        
    if (fmt->id != PJMEDIA_FORMAT_I420 && fmt->id != PJMEDIA_FORMAT_BGRA)
        return PJ_EINVAL;
        
    /* Currently, for BGRA format, device must handle the rotation. */
    if (fmt->id == PJMEDIA_FORMAT_BGRA && handle_rotation)
        return PJ_ENOTSUP;

    if (handle_rotation) {
#if !HAS_ROTATION
        return PJ_ENOTSUP;
#endif
    }
    
    conv->src_size = src_size;
    conv->dst_size = dst_size;
    conv->handle_rotation = handle_rotation;
    pjmedia_format_copy(&conv->fmt, fmt);
    pjmedia_format_copy(&conv_param.src, fmt);
    pjmedia_format_copy(&conv_param.dst, fmt);

    /* If we do the rotation, the conversion's source size must be the same
     * as the device's original size. Otherwise, frames that require conversion
     * are the ones of which orientation differ by 90 or 270 degrees from the
     * destination size.
     */
    if (handle_rotation) {
        conv_param.src.det.vid.size = src_size;
    } else {
        conv_param.src.det.vid.size.w = dst_size.h;
        conv_param.src.det.vid.size.h = dst_size.w;
    }

    /* Maintaining aspect ratio requires filling the left&right /
     * top&bottom area with black color.
     * Currently it is only supported for I420.
     * TODO: support BGRA as well
     */
    if (fmt->id != PJMEDIA_FORMAT_I420)
        maintain_aspect_ratio = PJ_FALSE;

    /* Calculate the size after rotation.
     * If aspect ratio doesn't need to be maintained, rot_size is simply equal
     * to the destination size. Otherwise, we need to fit the rotated frame
     * to height or to width.
     */
    conv->maintain_aspect_ratio = maintain_aspect_ratio;
    if (maintain_aspect_ratio) {
        conv->fit_to_h = (dst_size.w >= dst_size.h? PJ_TRUE: PJ_FALSE);
        if (conv->fit_to_h) {   /* Fit to height */
            conv->rot_size.h = dst_size.h;
            conv->rot_size.w = dst_size.h * dst_size.h / dst_size.w;
            /* Make sure the width difference is divisible by four
             * so we can have equal padding left and right.
             */
            conv->rot_size.w += (dst_size.w - conv->rot_size.w) % 4;
            conv->pad = (conv->dst_size.w - conv->rot_size.w) / 2;
        } else {                        /* Fit to width */
            conv->rot_size.w = dst_size.w;
            conv->rot_size.h = dst_size.w * dst_size.w / dst_size.h;
            conv->rot_size.h += (dst_size.h - conv->rot_size.h) % 4;
            conv->pad = (conv->dst_size.h - conv->rot_size.h) / 2;
        }
    } else {
        conv->rot_size = dst_size;
    }
    
    /* Calculate the size after resizing. */
    if (handle_rotation) {
        /* If we do the rotation, conversion is done before rotation. */
        if (maintain_aspect_ratio) {
            /* Since aspect ratio is maintained, the long side after
             * conversion must be the same as before conversion.
             * For example: 352x288 will be converted to 288x236
             */
            pj_size_t long_s  = (conv->rot_size.h > conv->rot_size.w?
                                 conv->rot_size.h: conv->rot_size.w);
            pj_size_t short_s = (conv->rot_size.h > conv->rot_size.w?
                                 conv->rot_size.w: conv->rot_size.h);
            if (src_size.w > src_size.h) {
                conv->res_size.w = long_s;
                conv->res_size.h = short_s;
             } else {
                conv->res_size.w = short_s;
                conv->res_size.h = long_s;
             }
        } else {
            /* We don't need to maintain aspect ratio,
             * so just swap the width and height.
             * For example: 352x288 will be resized to 288x352
             */
            conv->res_size.w = src_size.h;
            conv->res_size.h = src_size.w;
        }
        conv_param.dst.det.vid.size = conv->res_size;
    } else {
        conv->res_size = conv->rot_size;
        conv_param.dst.det.vid.size = conv->rot_size;
    }

    status = pjmedia_converter_create(NULL, pool, &conv_param,
                                      &conv->conv);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Error creating converter"));
        return status;
    }

    vfi = pjmedia_get_video_format_info(NULL, fmt->id);
    pj_assert(vfi);
    
    conv->wxh = conv->dst_size.w * conv->dst_size.h;
    conv->src_frame_size = dst_size.w * dst_size.h * vfi->bpp / 8;
    conv->conv_frame_size = conv->rot_size.w * conv->rot_size.h *
                            vfi->bpp / 8;
    conv->conv_buf = pj_pool_alloc(pool, conv->src_frame_size);
    
    pjmedia_vid_dev_conv_set_rotation(conv, PJMEDIA_ORIENT_NATURAL);

    PJ_LOG(4, (THIS_FILE, "Orientation converter created: %dx%d to %dx%d, "
                          "maintain aspect ratio=%s",
                          conv_param.src.det.vid.size.w,
                          conv_param.src.det.vid.size.h,
                          conv_param.dst.det.vid.size.w,
                          conv_param.dst.det.vid.size.h,
                          maintain_aspect_ratio? "yes": "no"));
                          
    return PJ_SUCCESS;
}

void pjmedia_vid_dev_conv_set_rotation(pjmedia_vid_dev_conv *conv,
                                       pjmedia_orient rotation)
{
    pjmedia_rect_size new_size = conv->src_size;
    
    conv->rotation = rotation;
    
    if (rotation == PJMEDIA_ORIENT_ROTATE_90DEG ||
        rotation == PJMEDIA_ORIENT_ROTATE_270DEG)
    {
        new_size.w = conv->src_size.h;
        new_size.h = conv->src_size.w;
    }
    
    /* Check whether new size (size after rotation) and destination
     * are both portrait or both landscape. If yes, resize will not
     * be required in pjmedia_vid_dev_conv_resize_and_rotate() below.
     * For example, 352x288 frame rotated 270 degrees will fit into
     * a destination frame of 288x352 (no resize needed).
     */
    if ((new_size.w > new_size.h && conv->dst_size.w > conv->dst_size.h) ||
        (new_size.h > new_size.w && conv->dst_size.h > conv->dst_size.w))
    {
        conv->match_src_dst = PJ_TRUE;
    } else {
        conv->match_src_dst = PJ_FALSE;
    }
}

pj_status_t pjmedia_vid_dev_conv_resize_and_rotate(pjmedia_vid_dev_conv *conv,
                                                   void *src_buf,
                                                   void **result)
{
#define swap(a, b) {pj_uint8_t *c = a; a = b; b = c;}

    pj_status_t status;
    pjmedia_frame src_frame, dst_frame;
    pjmedia_rect_size src_size = conv->src_size;
    pj_uint8_t *src = src_buf;
    pj_uint8_t *dst = conv->conv_buf;    

    pj_assert(src_buf);
    
    if (!conv->conv) return PJ_EINVALIDOP;
    
    if (!conv->match_src_dst) {
        /* We need to resize. */
        src_frame.buf = src;
        dst_frame.buf = dst;
        src_frame.size = conv->src_frame_size;
        dst_frame.size = conv->conv_frame_size;
    
        status = pjmedia_converter_convert(conv->conv, &src_frame, &dst_frame);
        if (status != PJ_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Failed to convert frame"));
            return status;
        }
        
        src_size = conv->res_size;
        
        swap(src, dst);
    }
    
    if (conv->handle_rotation && conv->rotation != PJMEDIA_ORIENT_NATURAL) {
        /* We need to do rotation. */
        if (conv->fmt.id == PJMEDIA_FORMAT_I420) {
            pjmedia_rect_size dst_size = src_size;
            pj_size_t p_len = src_size.w * src_size.h;
            
            if (conv->rotation == PJMEDIA_ORIENT_ROTATE_90DEG ||
                conv->rotation == PJMEDIA_ORIENT_ROTATE_270DEG)
            {
                dst_size.w = src_size.h;
                dst_size.h = src_size.w;
            }
            
#if defined(PJMEDIA_HAS_LIBYUV) && PJMEDIA_HAS_LIBYUV != 0
            enum RotationMode mode;
            
            switch (conv->rotation) {
                case PJMEDIA_ORIENT_ROTATE_90DEG:
                    mode = kRotate90;
                    break;
                case PJMEDIA_ORIENT_ROTATE_180DEG:
                    mode = kRotate180;
                    break;
                case PJMEDIA_ORIENT_ROTATE_270DEG:
                    mode = kRotate270;
                    break;
                default:
                    mode = kRotate0;
            }

            I420Rotate(src, src_size.w,
                       src+p_len, src_size.w/2,
                       src+p_len+p_len/4, src_size.w/2,
                       dst, dst_size.w,
                       dst+p_len, dst_size.w/2,
                       dst+p_len+p_len/4, dst_size.w/2,
                       src_size.w, src_size.h, mode);
            
            swap(src, dst);
#else
            PJ_UNUSED_ARG(p_len);
            PJ_UNUSED_ARG(dst_size);
#endif
        }
    }
    
    if (!conv->match_src_dst && conv->maintain_aspect_ratio) {
        /* Center the frame and fill the area with black color */    
        if (conv->fmt.id == PJMEDIA_FORMAT_I420) {
            unsigned i = 0;
            pj_uint8_t *pdst = dst;
            pj_uint8_t *psrc = src;
            pj_size_t p_len_src = 0, p_len_dst = conv->wxh;
            int pad = conv->pad;

            pj_bzero(pdst, p_len_dst);

            if (conv->fit_to_h) {
                /* Fill the left and right with black */
                for (; i < conv->dst_size.h; ++i) {
                    pdst += pad;
                    pj_memcpy(pdst, psrc, conv->rot_size.w);
                    pdst += conv->rot_size.w;
                    psrc += conv->rot_size.w;
                    pdst += pad;
                }
            } else {
                /* Fill the top and bottom with black */
                p_len_src = conv->rot_size.w * conv->rot_size.h;
                pj_memcpy(pdst + conv->rot_size.w * pad, psrc, p_len_src);
                psrc += p_len_src;
                pdst += p_len_dst;
            }

            /* Fill the U&V components with 0x80 to make it black.
             * Bzero-ing will make the area look green instead.
             */
            pj_memset(pdst, 0x80, p_len_dst/2);
            pad /= 2;
            if (conv->fit_to_h) {
                p_len_src = conv->rot_size.w / 2;
                for (i = conv->dst_size.h; i > 0; --i) {
                    pdst += pad;
                    pj_memcpy(pdst, psrc, p_len_src);
                    pdst += p_len_src;
                    psrc += p_len_src;
                    pdst += pad;
                }
            } else {
                pj_uint8_t *U, *V;
                pj_size_t gap = conv->rot_size.w * pad / 2;

                p_len_src /= 4;
                U = pdst;
                V = U + p_len_dst/4;

                pj_memcpy(U + gap, psrc, p_len_src);
                psrc += p_len_src;
                pj_memcpy(V + gap, psrc, p_len_src);
            }
            
            swap(src, dst);
        }
    }
    
    *result = src;
    
    return PJ_SUCCESS;
}

void pjmedia_vid_dev_conv_destroy_converter(pjmedia_vid_dev_conv *conv)
{
    if (conv->conv) {
        pjmedia_converter_destroy(conv->conv);
        conv->conv = NULL;
    }
}

#endif /* PJMEDIA_HAS_VIDEO */
