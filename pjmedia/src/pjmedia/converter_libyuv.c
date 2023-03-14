/*
 * Copyright (C) 2010-2011 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia/converter.h>
#include <pj/errno.h>

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0) && \
    defined(PJMEDIA_HAS_LIBYUV) && (PJMEDIA_HAS_LIBYUV != 0)

#include <libyuv.h>

static pj_status_t factory_create_converter(pjmedia_converter_factory *cf,
                                            pj_pool_t *pool,
                                            const pjmedia_conversion_param*prm,
                                            pjmedia_converter **p_cv);

static void factory_destroy_factory(pjmedia_converter_factory *cf);

static pj_status_t libyuv_conv_convert(pjmedia_converter *converter,
                                       pjmedia_frame *src_frame,
                                       pjmedia_frame *dst_frame);

static pj_status_t libyuv_conv_convert2(
                                    pjmedia_converter       *converter,
                                    pjmedia_frame           *src_frame,
                                    const pjmedia_rect_size *src_frame_size,
                                    const pjmedia_coord     *src_pos,
                                    pjmedia_frame           *dst_frame,
                                    const pjmedia_rect_size *dst_frame_size,
                                    const pjmedia_coord     *dst_pos,
                                    void                    *param);

static void libyuv_conv_destroy(pjmedia_converter *converter);

static pjmedia_converter_factory_op libyuv_factory_op =
{
    &factory_create_converter,
    &factory_destroy_factory
};

static pjmedia_converter_op libyuv_converter_op =
{
    &libyuv_conv_convert,
    &libyuv_conv_destroy,
    &libyuv_conv_convert2
};

typedef struct fmt_info
{
    const pjmedia_video_format_info     *vid_fmt_info;
    pjmedia_video_apply_fmt_param        apply_param;
} fmt_info;

typedef enum conv_func_type
{
    CONV_PACK_TO_PACK,
    CONV_PACK_TO_PLANAR,
    CONV_PLANAR_TO_PACK,
    CONV_PLANAR_TO_PLANAR,
    SCALE_PACK,
    SCALE_PLANAR
} conv_func_type;

typedef int (*gen_conv_func)();

typedef int (*conv_pack_to_pack_method)(const uint8* src, int src_stride,
                                        uint8* dst, int dst_stride,
                                        int width, int height); 

typedef int (*conv_pack_to_planar_method)(const uint8* src, int src_stride,
                                          uint8* dst1, int dst_stride1,
                                          uint8* dst2, int dst_stride2,
                                          uint8* dst3, int dst_stride3,
                                          int width, int height);

typedef int (*conv_planar_to_pack_method)(const uint8* src1, int src_stride1,
                                          const uint8* src2, int src_stride2,
                                          const uint8* src3, int src_stride3,
                                          uint8* dst, int dst_stride,
                                          int width, int height);

typedef int (*conv_planar_to_planar_method)(const uint8* src1, int src_stride1,
                                            const uint8* src2, int src_stride2,
                                            const uint8* src3, int src_stride3,
                                            uint8* dst1, int dst_stride1,
                                            uint8* dst2, int dst_stride2,
                                            uint8* dst3, int dst_stride3,
                                            int width, int height);

typedef int  (*scale_pack_method)
             (const uint8* src_argb, int src_stride_argb,
              int src_width, int src_height,
              uint8* dst_argb, int dst_stride_argb,
              int dst_width, int dst_height,
              enum FilterMode filtering);

typedef int (*scale_planar_method)      
            (const uint8* src_y, int src_stride_y,
             const uint8* src_u, int src_stride_u,
             const uint8* src_v, int src_stride_v,
             int src_width, int src_height,
             uint8* dst_y, int dst_stride_y,
             uint8* dst_u, int dst_stride_u,
             uint8* dst_v, int dst_stride_v,
             int dst_width, int dst_height,
             enum FilterMode filtering);

typedef union act_method
{
    conv_pack_to_pack_method        conv_pack_to_pack;
    conv_pack_to_planar_method      conv_pack_to_planar;
    conv_planar_to_pack_method      conv_planar_to_pack;
    conv_planar_to_planar_method    conv_planar_to_planar;
    scale_pack_method               scale_pack;
    scale_planar_method             scale_planar;    
} act_method;

typedef struct fmt_convert_map {
    pj_uint32_t                     src_id;
    pj_uint32_t                     dst_id;
    conv_func_type                  func_type;
    gen_conv_func                   conv_func;
} fmt_convert_map;

/* Maximum number of steps/act needed for the conversion/scale process. */
#define MAXIMUM_ACT 3

/* Define the filter mode for libyuv:
 * 0 : None (fastest)
 * 1 : Linear
 * 2 : Biinear
 * 3 : Filter Box (best quality)
 */
#if !defined(LIBYUV_FILTER_MODE) 
#   define LIBYUV_FILTER_MODE 3
#endif

#define METHOD_IS_SCALE(mtd) mtd>CONV_PLANAR_TO_PLANAR

/* Macro to help define format conversion table. */
#define GET_PJ_FORMAT(fmt) PJMEDIA_FORMAT_##fmt

#define MAP_CONV_PACK_TO_PACK(src,dst,method) GET_PJ_FORMAT(src),\
        GET_PJ_FORMAT(dst),CONV_PACK_TO_PACK,(gen_conv_func)&method
#define MAP_CONV_PACK_TO_PLANAR(src,dst,method) GET_PJ_FORMAT(src),\
        GET_PJ_FORMAT(dst),CONV_PACK_TO_PLANAR,(gen_conv_func)&method
#define MAP_CONV_PLANAR_TO_PACK(src,dst,method) GET_PJ_FORMAT(src),\
        GET_PJ_FORMAT(dst),CONV_PLANAR_TO_PACK,(gen_conv_func)&method
#define MAP_CONV_PLANAR_TO_PLANAR(src,dst,method) GET_PJ_FORMAT(src),\
        GET_PJ_FORMAT(dst),CONV_PLANAR_TO_PLANAR,(gen_conv_func)&method
#define MAP_SCALE_PACK(fmt,method) GET_PJ_FORMAT(fmt),\
        GET_PJ_FORMAT(fmt),SCALE_PACK,(gen_conv_func)&method
#define MAP_SCALE_PLANAR(fmt,method) GET_PJ_FORMAT(fmt),\
        GET_PJ_FORMAT(fmt),SCALE_PLANAR,(gen_conv_func)&method

static fmt_convert_map conv_to_i420[] = 
{
    {MAP_CONV_PACK_TO_PLANAR(RGB24,I420,RGB24ToI420)},
    {MAP_CONV_PACK_TO_PLANAR(RGBA,I420,ABGRToI420)},
    {MAP_CONV_PACK_TO_PLANAR(BGRA,I420,ARGBToI420)},
    {MAP_CONV_PACK_TO_PLANAR(YUY2,I420,YUY2ToI420)},
    {MAP_CONV_PACK_TO_PLANAR(UYVY,I420,UYVYToI420)},
    {MAP_CONV_PLANAR_TO_PLANAR(I422,I420,I422ToI420)}    
};

static fmt_convert_map conv_from_i420[] = 
{
    {MAP_CONV_PLANAR_TO_PACK(I420,RGB24,I420ToRGB24)},
    {MAP_CONV_PLANAR_TO_PACK(I420,RGBA,I420ToABGR)},
    {MAP_CONV_PLANAR_TO_PACK(I420,BGRA,I420ToARGB)},
    {MAP_CONV_PLANAR_TO_PACK(I420,YUY2,I420ToYUY2)},
    {MAP_CONV_PLANAR_TO_PACK(I420,UYVY,I420ToUYVY)},
    {MAP_CONV_PLANAR_TO_PLANAR(I420,I422,I420ToI422)},
    {MAP_SCALE_PLANAR(I420,I420Scale)}
};

static fmt_convert_map conv_to_bgra[] = 
{
    {MAP_CONV_PACK_TO_PACK(RGB24,BGRA,RGB24ToARGB)},
    {MAP_CONV_PACK_TO_PACK(RGBA,BGRA,ABGRToARGB)},    
    {MAP_CONV_PACK_TO_PACK(YUY2,BGRA,YUY2ToARGB)},
    {MAP_CONV_PACK_TO_PACK(UYVY,BGRA,UYVYToARGB)},
    {MAP_CONV_PLANAR_TO_PACK(I422,BGRA,I422ToARGB)},
    {MAP_CONV_PLANAR_TO_PACK(I420,BGRA,I420ToARGB)}
};

static fmt_convert_map conv_from_bgra[] = 
{
    {MAP_CONV_PACK_TO_PACK(BGRA,RGB24,ARGBToRGB24)},
    {MAP_CONV_PACK_TO_PACK(BGRA,RGBA,ARGBToABGR)},
    {MAP_CONV_PACK_TO_PACK(BGRA,YUY2,ARGBToYUY2)},
    {MAP_CONV_PACK_TO_PACK(BGRA,UYVY,ARGBToUYVY)},
    {MAP_CONV_PACK_TO_PLANAR(BGRA,I422,ARGBToI422)},
    {MAP_CONV_PACK_TO_PLANAR(BGRA,I420,ARGBToI420)},
    {MAP_SCALE_PACK(BGRA,ARGBScale)}
};

typedef struct converter_act 
{
    conv_func_type          act_type;
    struct fmt_info         src_fmt_info;
    struct fmt_info         dst_fmt_info;
    act_method              method;
} converter_act;

struct libyuv_converter
{
    pjmedia_converter                    base;      
    int                                  act_num;
    converter_act                        act[MAXIMUM_ACT];
};

/* Find the matched format conversion map. */ 
static pj_status_t get_converter_map(pj_uint32_t src_id, 
                                     pj_uint32_t dst_id,
                                     const pjmedia_rect_size *src_size, 
                                     const pjmedia_rect_size *dst_size,
                                     int act_num,
                                     converter_act *act)
{
    fmt_convert_map *map = NULL;
    unsigned cnt = 0, i = 0;
    unsigned act_idx = act_num - 1;

#   define GET_MAP(src) \
    do { \
        map=src; \
        cnt=PJ_ARRAY_SIZE(src); \
    }while(0)

    if (src_id == PJMEDIA_FORMAT_I420) {
        GET_MAP(conv_from_i420);
    } else if (src_id == PJMEDIA_FORMAT_BGRA) {
        GET_MAP(conv_from_bgra);
    }

    if (!map) {
        if (dst_id == PJMEDIA_FORMAT_I420) {
            GET_MAP(conv_to_i420);
        } else if (dst_id == PJMEDIA_FORMAT_BGRA) {
            GET_MAP(conv_to_bgra);
        }
    }

    if (!map)
        return PJ_ENOTSUP;

    for (;i<cnt;++i) {
        if ((map[i].src_id == src_id) && (map[i].dst_id == dst_id))
            break;
    }

    if (i == cnt)
        return PJ_ENOTSUP;

    act[act_idx].act_type = map[i].func_type;

    switch (act[act_idx].act_type) {
    case CONV_PACK_TO_PACK:
        act[act_idx].method.conv_pack_to_pack = 
                                     (conv_pack_to_pack_method)map[i].conv_func;
        break;
    case CONV_PACK_TO_PLANAR:
        act[act_idx].method.conv_pack_to_planar = 
                                   (conv_pack_to_planar_method)map[i].conv_func;
        break;
    case CONV_PLANAR_TO_PACK:
        act[act_idx].method.conv_planar_to_pack = 
                                   (conv_planar_to_pack_method)map[i].conv_func;
        break;
    case CONV_PLANAR_TO_PLANAR:
        act[act_idx].method.conv_planar_to_planar = 
                                 (conv_planar_to_planar_method)map[i].conv_func;
        break;
    case SCALE_PACK:
        act[act_idx].method.scale_pack = (scale_pack_method)map[i].conv_func;
        break;
    case SCALE_PLANAR:
        act[act_idx].method.scale_planar = 
                                          (scale_planar_method)map[i].conv_func;
        break;
    }    

    act[act_idx].src_fmt_info.vid_fmt_info = pjmedia_get_video_format_info(
                                            pjmedia_video_format_mgr_instance(),
                                            src_id);
    
    act[act_idx].dst_fmt_info.vid_fmt_info = 
              pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
                                            dst_id);

    /* Source buffer size is always the same as the previous destination buffer 
       size, except for the first act. */
    act[act_idx].src_fmt_info.apply_param.size = (!act_idx)?*src_size:
                                   act[act_idx-1].dst_fmt_info.apply_param.size;

    /* Destination buffer size is not the same as source buffer size 
       when scaling. */
    act[act_idx].dst_fmt_info.apply_param.size = 
                                     (METHOD_IS_SCALE(act[act_idx].act_type))?
                                     *dst_size:
                                     act[act_idx].src_fmt_info.apply_param.size;
    
    return PJ_SUCCESS;
}

/* This method will return the prefered conversion format based 
 * on the color model. 
 */
static pjmedia_format_id get_next_conv_fmt(pj_uint32_t src_id) {
    const pjmedia_video_format_info *vid_info = pjmedia_get_video_format_info(
                                            pjmedia_video_format_mgr_instance(),
                                            src_id);

    if (!vid_info)
        return PJMEDIA_FORMAT_BGRA;

    if (vid_info->color_model == PJMEDIA_COLOR_MODEL_YUV) {
        return PJMEDIA_FORMAT_I420;
    } else {
        return PJMEDIA_FORMAT_BGRA;
    }
}

/* This method will find and set all the steps needed for conversion/scale. 
 * More than one step might be needed, since not all format is provided with 
 * a direct conversion/scale method.
 * e.g : Scale YUY2 (240*320) to YUY2 (320*720)
 *       - Step 1: Convert YUY2(240*320) to I420 (240*320)
 *       - Step 2: Scale I420 (320*760)
 *       - Step 3: Convert I420 (320*760) to YUY2 (320*760)
 */
static int set_converter_act(pj_uint32_t src_id, 
                             pj_uint32_t dst_id,
                             const pjmedia_rect_size *src_size, 
                             const pjmedia_rect_size *dst_size,
                             converter_act *act)
{
    unsigned act_num = 0;
    pj_uint32_t current_id = src_id;
    pj_bool_t need_scale = PJ_FALSE;

    /* Convert to I420 or BGRA if needed. */
    if ((src_id != PJMEDIA_FORMAT_I420) && (src_id != PJMEDIA_FORMAT_BGRA)) {
        pj_uint32_t next_id = get_next_conv_fmt(src_id);
        if (get_converter_map(src_id, next_id, src_size, dst_size, ++act_num, 
                              act) != PJ_SUCCESS)
        {
            return 0;                   
        }                                                  
                
        current_id = next_id;
    }

    /* Scale if needed */
    //need_scale = ((src_size->w != dst_size->w) ||
                  //(src_size->h != dst_size->h));

    // Always enable scale, as this can be used for rendering a region of
    // a frame to another region of similar/another frame.
    need_scale = PJ_TRUE;

    if (need_scale) {
        if (get_converter_map(current_id, current_id, src_size, dst_size, 
                              ++act_num, act) != PJ_SUCCESS)
        {
            return 0;                        
        }                              
    }

    /* Convert if needed */
    if (current_id != dst_id) {
        if (get_converter_map(current_id, dst_id, src_size, dst_size, ++act_num,
                              act) != PJ_SUCCESS)
        {
            return 0;
        }                              
    }

    return act_num; 
}

/* Additional buffer might be needed for formats without direct conversion/scale
 * method. This method will allocate and set the destination buffer needed by
 * the conversion/scaling process.
 */
static pj_status_t set_destination_buffer(pj_pool_t *pool, 
                                          struct libyuv_converter *lconv)
{
    int i = 0;

    for (;i<lconv->act_num-1;++i) {
        pj_size_t buffer_size = 0;      
        fmt_info *info = &lconv->act[i].dst_fmt_info;

        /* Get destination buffer size. */
        (*info->vid_fmt_info->apply_fmt)(info->vid_fmt_info, 
                                         &info->apply_param);

        buffer_size = info->apply_param.framebytes;

        /* Allocate buffer. */
        lconv->act[i].dst_fmt_info.apply_param.buffer = 
                                  (pj_uint8_t*)pj_pool_alloc(pool, buffer_size);

        if (!lconv->act[i].dst_fmt_info.apply_param.buffer)
            return PJ_ENOMEM;
    }
    return PJ_SUCCESS;
}

/* Check the act input/output format matched the conversion/scale format. */
static pj_bool_t check_converter_act(const converter_act *act, 
                                     int act_num, 
                                     pj_uint32_t src_id,
                                     const pjmedia_rect_size *src_size,
                                     pj_uint32_t dst_id, 
                                     const pjmedia_rect_size *dst_size)
{
    if (act_num) {
        const struct fmt_info *first_fmt = &act[0].src_fmt_info;
        const struct fmt_info *last_fmt = &act[act_num-1].dst_fmt_info; 

        if ((first_fmt->vid_fmt_info->id == src_id) &&
            (first_fmt->apply_param.size.h == src_size->h) &&
            (first_fmt->apply_param.size.w == src_size->w) &&
            (last_fmt->vid_fmt_info->id == dst_id) && 
            (last_fmt->apply_param.size.h == dst_size->h) &&
            (last_fmt->apply_param.size.w == dst_size->w))
        {
            return PJ_TRUE;
        }
    } 
    return PJ_FALSE;
}

static pj_status_t factory_create_converter(pjmedia_converter_factory *cf,
                                            pj_pool_t *pool,
                                            const pjmedia_conversion_param *prm,
                                            pjmedia_converter **p_cv)
{
    const pjmedia_video_format_detail *src_detail, *dst_detail;
    const pjmedia_video_format_info *src_fmt_info, *dst_fmt_info;
    struct libyuv_converter *lconv = NULL;
    pj_status_t status = PJ_ENOTSUP;

    PJ_UNUSED_ARG(cf);

    /* Only supports video */
    if (prm->src.type != PJMEDIA_TYPE_VIDEO ||
        prm->dst.type != prm->src.type ||
        prm->src.detail_type != PJMEDIA_FORMAT_DETAIL_VIDEO ||
        prm->dst.detail_type != prm->src.detail_type)
    {
        return status;
    }

    /* lookup source format info */
    src_fmt_info = pjmedia_get_video_format_info(
                                            pjmedia_video_format_mgr_instance(),
                                            prm->src.id);

    if (!src_fmt_info)
        return status;

    /* lookup destination format info */
    dst_fmt_info = pjmedia_get_video_format_info(
                                            pjmedia_video_format_mgr_instance(),
                                            prm->dst.id);

    if (!dst_fmt_info)
        return status;

    src_detail = pjmedia_format_get_video_format_detail(&prm->src, PJ_TRUE);
    dst_detail = pjmedia_format_get_video_format_detail(&prm->dst, PJ_TRUE);
    
    lconv = PJ_POOL_ZALLOC_T(pool, struct libyuv_converter);
    lconv->base.op = &libyuv_converter_op;

    lconv->act_num = set_converter_act(src_fmt_info->id, dst_fmt_info->id, 
                                       &src_detail->size, &dst_detail->size,
                                       lconv->act);

    if (!lconv->act_num) {
        return status;
    }

    if (!check_converter_act(lconv->act, lconv->act_num, 
                             src_fmt_info->id, &src_detail->size,
                             dst_fmt_info->id, &dst_detail->size)) 
    {
        return status;
    }

    status = set_destination_buffer(pool, lconv);

    *p_cv = &lconv->base;

    return status;
}

static void factory_destroy_factory(pjmedia_converter_factory *cf)
{
    PJ_UNUSED_ARG(cf);
}

static pj_status_t libyuv_conv_convert(pjmedia_converter *converter,
                                       pjmedia_frame *src_frame,
                                       pjmedia_frame *dst_frame)
{
    struct libyuv_converter *lconv = (struct libyuv_converter*)converter;
    int i = 0;

    /* Set the first act buffer from src frame. */
    lconv->act[0].src_fmt_info.apply_param.buffer = src_frame->buf;

    /* Set the last act buffer from dst frame. */
    lconv->act[lconv->act_num-1].dst_fmt_info.apply_param.buffer = 
                                                                 dst_frame->buf;

    for (;i<lconv->act_num;++i) {       
        /* Use destination info as the source info for the next act. */
        struct fmt_info *src_fmt_info = (i==0)?&lconv->act[i].src_fmt_info: 
                                        &lconv->act[i-1].dst_fmt_info;

        struct fmt_info *dst_fmt_info = &lconv->act[i].dst_fmt_info;    
        
        (*src_fmt_info->vid_fmt_info->apply_fmt)(src_fmt_info->vid_fmt_info, 
                                                 &src_fmt_info->apply_param);

        (*dst_fmt_info->vid_fmt_info->apply_fmt)(dst_fmt_info->vid_fmt_info, 
                                                 &dst_fmt_info->apply_param);

        switch (lconv->act[i].act_type) {
        case CONV_PACK_TO_PACK:
            (*lconv->act[i].method.conv_pack_to_pack)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case CONV_PACK_TO_PLANAR:
            (*lconv->act[i].method.conv_pack_to_planar)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[1], 
                              dst_fmt_info->apply_param.strides[1],
                              dst_fmt_info->apply_param.planes[2], 
                              dst_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case CONV_PLANAR_TO_PACK:
            (*lconv->act[i].method.conv_planar_to_pack)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              (const uint8*)src_fmt_info->apply_param.planes[1],
                              src_fmt_info->apply_param.strides[1],
                              (const uint8*)src_fmt_info->apply_param.planes[2],
                              src_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case CONV_PLANAR_TO_PLANAR:
            (*lconv->act[i].method.conv_planar_to_planar)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              (const uint8*)src_fmt_info->apply_param.planes[1],
                              src_fmt_info->apply_param.strides[1],
                              (const uint8*)src_fmt_info->apply_param.planes[2],
                              src_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[1], 
                              dst_fmt_info->apply_param.strides[1],
                              dst_fmt_info->apply_param.planes[2], 
                              dst_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case SCALE_PACK:
            (*lconv->act[i].method.scale_pack)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              src_fmt_info->apply_param.size.w,
                              src_fmt_info->apply_param.size.h,
                              (uint8*)dst_fmt_info->apply_param.planes[0],
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.size.w,
                              dst_fmt_info->apply_param.size.h,
                              LIBYUV_FILTER_MODE);
            break;      
        case SCALE_PLANAR:
            (*lconv->act[i].method.scale_planar)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              (const uint8*)src_fmt_info->apply_param.planes[1],
                              src_fmt_info->apply_param.strides[1],
                              (const uint8*)src_fmt_info->apply_param.planes[2],
                              src_fmt_info->apply_param.strides[2],
                              src_fmt_info->apply_param.size.w,
                              src_fmt_info->apply_param.size.h,
                              (uint8*)dst_fmt_info->apply_param.planes[0],
                              dst_fmt_info->apply_param.strides[0],
                              (uint8*)dst_fmt_info->apply_param.planes[1],
                              dst_fmt_info->apply_param.strides[1],
                              (uint8*)dst_fmt_info->apply_param.planes[2],
                              dst_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.size.w,
                              dst_fmt_info->apply_param.size.h,
                              LIBYUV_FILTER_MODE);
            break;      
        };      
    }    
    return PJ_SUCCESS;
}

static pj_status_t libyuv_conv_convert2(
                                    pjmedia_converter       *converter,
                                    pjmedia_frame           *src_frame,
                                    const pjmedia_rect_size *src_frame_size,
                                    const pjmedia_coord     *src_pos,
                                    pjmedia_frame           *dst_frame,
                                    const pjmedia_rect_size *dst_frame_size,
                                    const pjmedia_coord     *dst_pos,
                                    pjmedia_converter_convert_setting
                                                            *param)
{
    struct libyuv_converter *lconv = (struct libyuv_converter*)converter;
    int i = 0;
    fmt_info *src_info = &lconv->act[0].src_fmt_info;
    fmt_info *dst_info = &lconv->act[lconv->act_num-1].dst_fmt_info;
    pjmedia_rect_size orig_src_size;
    pjmedia_rect_size orig_dst_size;

    PJ_UNUSED_ARG(param);

    /* Save original conversion sizes */
    orig_src_size = src_info->apply_param.size;
    orig_dst_size = dst_info->apply_param.size;

    /* Set the first act buffer from src frame, and overwrite size. */
    src_info->apply_param.buffer = src_frame->buf;
    src_info->apply_param.size   = *src_frame_size;

    /* Set the last act buffer from dst frame, and overwrite size. */
    dst_info->apply_param.buffer = dst_frame->buf;
    dst_info->apply_param.size   = *dst_frame_size;

    for (i=0;i<lconv->act_num;++i) {
        /* Use destination info as the source info for the next act. */
        struct fmt_info *src_fmt_info = (i==0)? src_info : 
                                        &lconv->act[i-1].dst_fmt_info;

        struct fmt_info *dst_fmt_info = &lconv->act[i].dst_fmt_info;    
        
        (*src_fmt_info->vid_fmt_info->apply_fmt)(src_fmt_info->vid_fmt_info, 
                                                 &src_fmt_info->apply_param);

        (*dst_fmt_info->vid_fmt_info->apply_fmt)(dst_fmt_info->vid_fmt_info, 
                                                 &dst_fmt_info->apply_param);

        /* For first and last acts, apply plane buffer offset and return back
         * the original sizes.
         */
        if (i == 0) {
            pjmedia_video_apply_fmt_param *ap = &src_fmt_info->apply_param;
            unsigned j;
            for (j = 0; j < src_fmt_info->vid_fmt_info->plane_cnt; ++j) {
                int y = src_pos->y * (int)ap->plane_bytes[j] / ap->strides[j] /
                        ap->size.h;
                ap->planes[j] += y * ap->strides[j] + src_pos->x *
                                 ap->strides[j] / ap->size.w;
            }
            ap->size = orig_src_size;
        }
        if (i == lconv->act_num-1) {
            pjmedia_video_apply_fmt_param *ap = &dst_fmt_info->apply_param;
            unsigned j;
            for (j = 0; j < dst_fmt_info->vid_fmt_info->plane_cnt; ++j)
            {
                int y = dst_pos->y * (int)ap->plane_bytes[j] / ap->strides[j] /
                        ap->size.h;
                ap->planes[j] += y * ap->strides[j] + dst_pos->x *
                                 ap->strides[j] / ap->size.w;
            }
            ap->size = orig_dst_size;
        }

        switch (lconv->act[i].act_type) {
        case CONV_PACK_TO_PACK:
            (*lconv->act[i].method.conv_pack_to_pack)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case CONV_PACK_TO_PLANAR:
            (*lconv->act[i].method.conv_pack_to_planar)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[1], 
                              dst_fmt_info->apply_param.strides[1],
                              dst_fmt_info->apply_param.planes[2], 
                              dst_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case CONV_PLANAR_TO_PACK:
            (*lconv->act[i].method.conv_planar_to_pack)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              (const uint8*)src_fmt_info->apply_param.planes[1],
                              src_fmt_info->apply_param.strides[1],
                              (const uint8*)src_fmt_info->apply_param.planes[2],
                              src_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case CONV_PLANAR_TO_PLANAR:
            (*lconv->act[i].method.conv_planar_to_planar)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              (const uint8*)src_fmt_info->apply_param.planes[1],
                              src_fmt_info->apply_param.strides[1],
                              (const uint8*)src_fmt_info->apply_param.planes[2],
                              src_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.planes[0], 
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.planes[1], 
                              dst_fmt_info->apply_param.strides[1],
                              dst_fmt_info->apply_param.planes[2], 
                              dst_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.size.w, 
                              dst_fmt_info->apply_param.size.h);
            break;
        case SCALE_PACK:
            (*lconv->act[i].method.scale_pack)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              src_fmt_info->apply_param.size.w,
                              src_fmt_info->apply_param.size.h,
                              (uint8*)dst_fmt_info->apply_param.planes[0],
                              dst_fmt_info->apply_param.strides[0],
                              dst_fmt_info->apply_param.size.w,
                              dst_fmt_info->apply_param.size.h,
                              LIBYUV_FILTER_MODE);
            break;      
        case SCALE_PLANAR:
            (*lconv->act[i].method.scale_planar)(
                              (const uint8*)src_fmt_info->apply_param.planes[0],
                              src_fmt_info->apply_param.strides[0],
                              (const uint8*)src_fmt_info->apply_param.planes[1],
                              src_fmt_info->apply_param.strides[1],
                              (const uint8*)src_fmt_info->apply_param.planes[2],
                              src_fmt_info->apply_param.strides[2],
                              src_fmt_info->apply_param.size.w,
                              src_fmt_info->apply_param.size.h,
                              (uint8*)dst_fmt_info->apply_param.planes[0],
                              dst_fmt_info->apply_param.strides[0],
                              (uint8*)dst_fmt_info->apply_param.planes[1],
                              dst_fmt_info->apply_param.strides[1],
                              (uint8*)dst_fmt_info->apply_param.planes[2],
                              dst_fmt_info->apply_param.strides[2],
                              dst_fmt_info->apply_param.size.w,
                              dst_fmt_info->apply_param.size.h,
                              LIBYUV_FILTER_MODE);
            break;      
        };      
    }    
    return PJ_SUCCESS;
}

static void libyuv_conv_destroy(pjmedia_converter *converter)
{
    PJ_UNUSED_ARG(converter);
}

static pjmedia_converter_factory libyuv_factory =
{
    NULL, NULL,                                 /* list */
    "libyuv",                                   /* name */
    PJMEDIA_CONVERTER_PRIORITY_NORMAL,          /* priority */
    NULL                                        /* op will be init-ed later  */
};

PJ_DEF(pj_status_t)
pjmedia_libyuv_converter_init(pjmedia_converter_mgr *mgr)
{
    libyuv_factory.op = &libyuv_factory_op;
    return pjmedia_converter_mgr_register_factory(mgr, &libyuv_factory);
}


PJ_DEF(pj_status_t)
pjmedia_libyuv_converter_shutdown(pjmedia_converter_mgr *mgr,
                                  pj_pool_t *pool)
{
    PJ_UNUSED_ARG(pool);
    return pjmedia_converter_mgr_unregister_factory(mgr, &libyuv_factory,
                                                    PJ_TRUE);
}

#endif //#if defined(PJMEDIA_HAS_LIBYUV) && PJMEDIA_HAS_LIBYUV != 0
