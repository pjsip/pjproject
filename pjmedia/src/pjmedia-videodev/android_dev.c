/*
 * Copyright (C) 2015 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>


#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_ANDROID) && \
    PJMEDIA_VIDEO_DEV_HAS_ANDROID != 0

#include <jni.h>

#define THIS_FILE               "android_dev.c"

/* Default video params */
#define DEFAULT_CLOCK_RATE      90000
#define DEFAULT_WIDTH           352
#define DEFAULT_HEIGHT          288
#define DEFAULT_FPS             15
#define ALIGN16(x)              ((((x)+15) >> 4) << 4)

/* Define whether we should maintain the aspect ratio when rotating the image.
 * For more details, please refer to util.h.
 */
#define MAINTAIN_ASPECT_RATIO   PJ_TRUE

/* Format map info */
typedef struct and_fmt_map
{
    pjmedia_format_id   fmt_id;
    pj_uint32_t         and_fmt_id;
} and_fmt_map;


/* Format map.
 * Note: it seems that most of Android devices don't support I420, while
 * unfortunately, our converter (libyuv based) only support I420 & RGBA,
 * so in this case, we'd just pretend that we support I420 and we'll do
 * the NV21/YV12 -> I420 conversion here.
 */
static and_fmt_map fmt_map[] =
{
    {PJMEDIA_FORMAT_NV21, 0x00000011},
    {PJMEDIA_FORMAT_YV12, 0x32315659},
    {PJMEDIA_FORMAT_I420, 0x00000023}, /* YUV_420_888 */
};


/* Device info */
typedef struct and_dev_info
{
    pjmedia_vid_dev_info         info;          /**< Base info         */
    unsigned                     dev_idx;       /**< Original dev ID   */
    pj_bool_t                    facing;        /**< Front/back camera?*/
    unsigned                     sup_size_cnt;  /**< # of supp'd size  */
    pjmedia_rect_size           *sup_size;      /**< Supported size    */
    unsigned                     sup_fps_cnt;   /**< # of supp'd FPS   */
    pjmedia_rect_size           *sup_fps;       /**< Supported FPS     */
    pj_bool_t                    has_yv12;      /**< Support YV12?     */
    pj_bool_t                    has_nv21;      /**< Support NV21?     */
    pj_bool_t                    forced_i420;   /**< Support I420 with
                                                     conversion         */
} and_dev_info;


/* Video factory */
typedef struct and_factory
{
    pjmedia_vid_dev_factory      base;          /**< Base factory      */
    pj_pool_t                   *pool;          /**< Memory pool       */
    pj_pool_factory             *pf;            /**< Pool factory      */

    pj_pool_t                   *dev_pool;      /**< Device list pool  */
    unsigned                     dev_count;     /**< Device count      */
    and_dev_info                *dev_info;      /**< Device info list  */
} and_factory;


/* Video stream. */
typedef struct and_stream
{
    pjmedia_vid_dev_stream  base;               /**< Base stream       */
    pjmedia_vid_dev_param   param;              /**< Settings          */
    pj_pool_t              *pool;               /**< Memory pool       */
    and_factory            *factory;            /**< Factory           */
    
    pjmedia_vid_dev_cb      vid_cb;             /**< Stream callback   */
    void                   *user_data;          /**< Application data  */
    pj_bool_t               is_running;         /**< Stream running?   */
    
    jobject                 jcam;               /**< PjCamera instance */

    pj_timestamp            frame_ts;           /**< Current timestamp */
    unsigned                ts_inc;             /**< Timestamp interval*/
    unsigned                convert_to_i420;    /**< Need to convert to I420?
                                                     0: no
                                                     1: from NV21
                                                     2: from YV12       */
    
    /** Capture thread info */
    pj_bool_t               thread_initialized;
    pj_thread_desc          thread_desc;
    pj_thread_t            *thread;

    /** NV21/YV12 -> I420 Conversion buffer  */
    pj_uint8_t             *convert_buf;
    pjmedia_rect_size       cam_size;
    
    /** Converter to rotate frame  */
    pjmedia_vid_dev_conv    conv;
    
    /** Frame format param for NV21/YV12 -> I420 conversion */
    pjmedia_video_apply_fmt_param vafp;
} and_stream;


/* Prototypes */
static pj_status_t and_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t and_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t and_factory_refresh(pjmedia_vid_dev_factory *f); 
static unsigned    and_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t and_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                            unsigned index,
                                            pjmedia_vid_dev_info *info);
static pj_status_t and_factory_default_param(pj_pool_t *pool,
                                             pjmedia_vid_dev_factory *f,
                                             unsigned index,
                                             pjmedia_vid_dev_param *param);
static pj_status_t and_factory_create_stream(
                                        pjmedia_vid_dev_factory *f,
                                        pjmedia_vid_dev_param *param,
                                        const pjmedia_vid_dev_cb *cb,
                                        void *user_data,
                                        pjmedia_vid_dev_stream **p_vid_strm);


static pj_status_t and_stream_get_param(pjmedia_vid_dev_stream *strm,
                                        pjmedia_vid_dev_param *param);
static pj_status_t and_stream_get_cap(pjmedia_vid_dev_stream *strm,
                                      pjmedia_vid_dev_cap cap,
                                      void *value);
static pj_status_t and_stream_set_cap(pjmedia_vid_dev_stream *strm,
                                      pjmedia_vid_dev_cap cap,
                                      const void *value);
static pj_status_t and_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t and_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t and_stream_destroy(pjmedia_vid_dev_stream *strm);


/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &and_factory_init,
    &and_factory_destroy,
    &and_factory_get_dev_count,
    &and_factory_get_dev_info,
    &and_factory_default_param,
    &and_factory_create_stream,
    &and_factory_refresh
};

static pjmedia_vid_dev_stream_op stream_op =
{
    &and_stream_get_param,
    &and_stream_get_cap,
    &and_stream_set_cap,
    &and_stream_start,
    NULL,
    NULL,
    &and_stream_stop,
    &and_stream_destroy
};


/****************************************************************************
 * JNI stuff
 */
extern JavaVM *pj_jni_jvm;

/* Use camera2 (since Android API level 21) */
#define USE_CAMERA2     1

#if USE_CAMERA2
#define PJ_CAMERA                       "PjCamera2"
#define PJ_CAMERA_INFO                  "PjCameraInfo2"
#else
#define PJ_CAMERA                       "PjCamera"
#define PJ_CAMERA_INFO                  "PjCameraInfo"
#endif

#define PJ_CLASS_PATH                   "org/pjsip/"
#define PJ_CAMERA_CLASS_PATH            PJ_CLASS_PATH PJ_CAMERA
#define PJ_CAMERA_INFO_CLASS_PATH       PJ_CLASS_PATH PJ_CAMERA_INFO


static struct jni_objs_t
{
    struct {
        jclass           cls;
        jmethodID        m_init;
        jmethodID        m_start;
        jmethodID        m_stop;
        jmethodID        m_switch;
    } cam;

    struct {
        jclass           cls;
        jmethodID        m_get_cnt;
        jmethodID        m_get_info;
        jfieldID         f_facing;
        jfieldID         f_orient;
        jfieldID         f_sup_size;
        jfieldID         f_sup_fmt;
        jfieldID         f_sup_fps;
    } cam_info;

} jobjs;


#if USE_CAMERA2
static void JNICALL OnGetFrame2(JNIEnv *env, jobject obj,
                                jlong user_data,
                                jobject plane0, jint rowStride0, jint pixStride0,
                                jobject plane1, jint rowStride1, jint pixStride1,
                                jobject plane2, jint rowStride2, jint pixStride2);
#else
static void JNICALL OnGetFrame(JNIEnv *env, jobject obj,
                               jbyteArray data, jint length,
                               jlong user_data);
#endif

#define jni_get_env(jni_env)     pj_jni_attach_jvm((void **)jni_env)
#define jni_detach_env(attached) pj_jni_detach_jvm(attached)


/* Get Java object IDs (via FindClass, GetMethodID, GetFieldID, etc).
 * Note that this function should be called from library-loader thread,
 * otherwise FindClass, etc, may fail, see:
 * http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
 */
static pj_status_t jni_init_ids()
{
    JNIEnv *jni_env;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t with_attach = jni_get_env(&jni_env);

#define GET_CLASS(class_path, class_name, cls) \
    cls = (*jni_env)->FindClass(jni_env, class_path); \
    if (cls == NULL || (*jni_env)->ExceptionCheck(jni_env)) { \
        (*jni_env)->ExceptionClear(jni_env); \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find class '" \
                              class_name "'")); \
        status = PJMEDIA_EVID_SYSERR; \
        goto on_return; \
    } else { \
        jclass tmp = cls; \
        cls = (jclass)(*jni_env)->NewGlobalRef(jni_env, tmp); \
        (*jni_env)->DeleteLocalRef(jni_env, tmp); \
        if (cls == NULL) { \
            PJ_LOG(3, (THIS_FILE, "[JNI] Unable to get global ref for " \
                                  "class '" class_name "'")); \
            status = PJMEDIA_EVID_SYSERR; \
            goto on_return; \
        } \
    }
#define GET_METHOD_ID(cls, class_name, method_name, signature, id) \
    id = (*jni_env)->GetMethodID(jni_env, cls, method_name, signature); \
    if (id == 0) { \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find method '" method_name \
                              "' in class '" class_name "'")); \
        status = PJMEDIA_EVID_SYSERR; \
        goto on_return; \
    }
#define GET_SMETHOD_ID(cls, class_name, method_name, signature, id) \
    id = (*jni_env)->GetStaticMethodID(jni_env, cls, method_name, signature); \
    if (id == 0) { \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find static method '" \
                              method_name "' in class '" class_name "'")); \
        status = PJMEDIA_EVID_SYSERR; \
        goto on_return; \
    }
#define GET_FIELD_ID(cls, class_name, field_name, signature, id) \
    id = (*jni_env)->GetFieldID(jni_env, cls, field_name, signature); \
    if (id == 0) { \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find field '" field_name \
                              "' in class '" class_name "'")); \
        status = PJMEDIA_EVID_SYSERR; \
        goto on_return; \
    }

    /* PjCamera class info */
    GET_CLASS(PJ_CAMERA_CLASS_PATH, PJ_CAMERA, jobjs.cam.cls);
    GET_METHOD_ID(jobjs.cam.cls, PJ_CAMERA, "<init>",
                  "(IIIIIJLandroid/view/SurfaceView;)V",
                  jobjs.cam.m_init);
    GET_METHOD_ID(jobjs.cam.cls, PJ_CAMERA, "Start", "()I",
                  jobjs.cam.m_start);
    GET_METHOD_ID(jobjs.cam.cls, PJ_CAMERA, "Stop", "()V",
                  jobjs.cam.m_stop);
    GET_METHOD_ID(jobjs.cam.cls, PJ_CAMERA, "SwitchDevice", "(I)I",
                  jobjs.cam.m_switch);

    /* PjCameraInfo class info */
    GET_CLASS(PJ_CAMERA_INFO_CLASS_PATH, PJ_CAMERA_INFO, jobjs.cam_info.cls);
    GET_SMETHOD_ID(jobjs.cam_info.cls, PJ_CAMERA_INFO, "GetCameraCount", "()I",
                   jobjs.cam_info.m_get_cnt);
    GET_SMETHOD_ID(jobjs.cam_info.cls, PJ_CAMERA_INFO, "GetCameraInfo",
                   "(I)L" PJ_CAMERA_INFO_CLASS_PATH ";",
                   jobjs.cam_info.m_get_info);
    GET_FIELD_ID(jobjs.cam_info.cls, PJ_CAMERA_INFO, "facing", "I",
                 jobjs.cam_info.f_facing);
    GET_FIELD_ID(jobjs.cam_info.cls, PJ_CAMERA_INFO, "orient", "I",
                 jobjs.cam_info.f_orient);
    GET_FIELD_ID(jobjs.cam_info.cls, PJ_CAMERA_INFO, "supportedSize", "[I",
                 jobjs.cam_info.f_sup_size);
    GET_FIELD_ID(jobjs.cam_info.cls, PJ_CAMERA_INFO, "supportedFormat", "[I",
                 jobjs.cam_info.f_sup_fmt);
    GET_FIELD_ID(jobjs.cam_info.cls, PJ_CAMERA_INFO, "supportedFps1000", "[I",
                 jobjs.cam_info.f_sup_fps);

#undef GET_CLASS_ID
#undef GET_METHOD_ID
#undef GET_SMETHOD_ID
#undef GET_FIELD_ID

    /* Register native function */
    {
#if USE_CAMERA2
        JNINativeMethod m = { "PushFrame2", "(JLjava/nio/ByteBuffer;IILjava/nio/ByteBuffer;IILjava/nio/ByteBuffer;II)V", (void*)&OnGetFrame2 };
#else
        JNINativeMethod m = { "PushFrame", "([BIJ)V", (void*)&OnGetFrame };
#endif
        if ((*jni_env)->RegisterNatives(jni_env, jobjs.cam.cls, &m, 1)) {
            PJ_LOG(3, (THIS_FILE, "[JNI] Failed in registering native "
                                  "function 'OnGetFrame()'"));
            status = PJMEDIA_EVID_SYSERR;
        }
    }

on_return:
    jni_detach_env(with_attach);
    return status;
}


static void jni_deinit_ids()
{
    JNIEnv *jni_env;
    pj_bool_t with_attach = jni_get_env(&jni_env);

    if (jobjs.cam.cls) {
        (*jni_env)->DeleteGlobalRef(jni_env, jobjs.cam.cls);
        jobjs.cam.cls = NULL;
    }

    if (jobjs.cam_info.cls) {
        (*jni_env)->DeleteGlobalRef(jni_env, jobjs.cam_info.cls);
        jobjs.cam_info.cls = NULL;
    }

    jni_detach_env(with_attach);
}


/****************************************************************************
 * Helper functions
 */
static pjmedia_format_id and_fmt_to_pj(pj_uint32_t fmt)
{
    unsigned i;
    for (i = 0; i < PJ_ARRAY_SIZE(fmt_map); i++) {
        if (fmt_map[i].and_fmt_id == fmt)
            return fmt_map[i].fmt_id;
    }
    return 0;
}

static pj_uint32_t pj_fmt_to_and(pjmedia_format_id fmt)
{
    unsigned i;
    for (i = 0; i < PJ_ARRAY_SIZE(fmt_map); i++) {
        if (fmt_map[i].fmt_id == fmt)
            return fmt_map[i].and_fmt_id;
    }
    return 0;
}


/****************************************************************************
 * Factory operations
 */
/*
 * Init and_ video driver.
 */
pjmedia_vid_dev_factory* pjmedia_and_factory(pj_pool_factory *pf)
{
    and_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "and_video", 512, 512, NULL);
    f = PJ_POOL_ZALLOC_T(pool, and_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;
    f->dev_pool = pj_pool_create(pf, "and_video_dev", 512, 512, NULL);

    return &f->base;
}


/* API: init factory */
static pj_status_t and_factory_init(pjmedia_vid_dev_factory *ff)
{
    pj_status_t status;

    status = jni_init_ids();
    if (status != PJ_SUCCESS)
        return status;

    status = and_factory_refresh(ff);
    if (status != PJ_SUCCESS)
        return status;

    return PJ_SUCCESS;
}


/* API: destroy factory */
static pj_status_t and_factory_destroy(pjmedia_vid_dev_factory *ff)
{
    and_factory *f = (and_factory*)ff;

    jni_deinit_ids();
    pj_pool_safe_release(&f->dev_pool);
    pj_pool_safe_release(&f->pool);

    return PJ_SUCCESS;
}


/* API: refresh the list of devices */
static pj_status_t and_factory_refresh(pjmedia_vid_dev_factory *ff)
{
    and_factory *f = (and_factory*)ff;
    pj_status_t status = PJ_SUCCESS;

    JNIEnv *jni_env;
    pj_bool_t with_attach, found_front = PJ_FALSE;
    int i, dev_count = 0;

    /* Clean up device info and pool */
    f->dev_count = 0;
    pj_pool_reset(f->dev_pool);
    
    with_attach = jni_get_env(&jni_env);
    
    /* dev_count = PjCameraInfo::GetCameraCount() */
    dev_count = (*jni_env)->CallStaticIntMethod(jni_env, jobjs.cam_info.cls,
                                                jobjs.cam_info.m_get_cnt);
    if (dev_count < 0) {
        PJ_LOG(3, (THIS_FILE, "Failed to get camera count"));
        status = PJMEDIA_EVID_SYSERR;
        goto on_return;
    }

    /* Start querying device info */
    f->dev_info = (and_dev_info*)
                  pj_pool_calloc(f->dev_pool, dev_count,
                                 sizeof(and_dev_info));

    for (i = 0; i < dev_count; i++) {
        and_dev_info *adi = &f->dev_info[f->dev_count];
        pjmedia_vid_dev_info *vdi = &adi->info;
        jobject jdev_info;
        jobject jtmp;
        int facing, max_fmt_cnt = PJMEDIA_VID_DEV_INFO_FMT_CNT;

        /* jdev_info = PjCameraInfo::GetCameraInfo(i) */
        jdev_info = (*jni_env)->CallStaticObjectMethod(
                                            jni_env,
                                            jobjs.cam_info.cls,
                                            jobjs.cam_info.m_get_info,
                                            i);
        if (jdev_info == NULL)
            continue;

        /* Get camera facing: 0=back 1=front */
        facing = (*jni_env)->GetIntField(jni_env, jdev_info,
                                         jobjs.cam_info.f_facing);
        if (facing < 0)
            goto on_skip_dev;
        
        /* Set device ID, direction, and has_callback info */
        adi->dev_idx = i;
        vdi->id = f->dev_count;
        vdi->dir = PJMEDIA_DIR_CAPTURE;
        vdi->has_callback = PJ_TRUE;
        vdi->caps = PJMEDIA_VID_DEV_CAP_SWITCH |
                    PJMEDIA_VID_DEV_CAP_ORIENTATION;

        /* Set driver & name info */
        pj_ansi_strxcpy(vdi->driver, "Android", sizeof(vdi->driver));
        adi->facing = facing;
        if (facing == 0) {
            pj_ansi_strxcpy(vdi->name, "Back camera", sizeof(vdi->name));
        } else {
            pj_ansi_strxcpy(vdi->name, "Front camera", sizeof(vdi->name));
        }

        /* Get supported sizes */
        jtmp = (*jni_env)->GetObjectField(jni_env, jdev_info,
                                          jobjs.cam_info.f_sup_size);
        if (jtmp) {
            jintArray jiarray = (jintArray*)jtmp;
            jint *sizes;
            jsize cnt, j;

            cnt = (*jni_env)->GetArrayLength(jni_env, jiarray);
            sizes = (*jni_env)->GetIntArrayElements(jni_env, jiarray, 0);
            
            adi->sup_size_cnt = cnt/2;
            adi->sup_size = pj_pool_calloc(f->dev_pool, adi->sup_size_cnt,
                                           sizeof(adi->sup_size[0]));
            for (j = 0; j < adi->sup_size_cnt; j++) {
                adi->sup_size[j].w = sizes[j*2];
                adi->sup_size[j].h = sizes[j*2+1];
            }
            (*jni_env)->ReleaseIntArrayElements(jni_env, jiarray, sizes, 0);
            (*jni_env)->DeleteLocalRef(jni_env, jtmp);
        } else {
            goto on_skip_dev;
        }

        /* Get supported formats */
        jtmp = (*jni_env)->GetObjectField(jni_env, jdev_info,
                                          jobjs.cam_info.f_sup_fmt);
        if (jtmp) {
            jintArray jiarray = (jintArray*)jtmp;
            jint *fmts;
            jsize cnt, j;
            pj_bool_t has_i420 = PJ_FALSE;
            int k;

            cnt = (*jni_env)->GetArrayLength(jni_env, jiarray);
            fmts = (*jni_env)->GetIntArrayElements(jni_env, jiarray, 0);
            for (j = 0; j < cnt; j++) {
                pjmedia_format_id fmt = and_fmt_to_pj((pj_uint32_t)fmts[j]);

                /* Make sure we recognize this format */
                if (fmt == 0)
                    continue;

                /* Check formats for I420 conversion */
                if (fmt == PJMEDIA_FORMAT_I420) has_i420 = PJ_TRUE;
                else if (fmt == PJMEDIA_FORMAT_YV12) adi->has_yv12 = PJ_TRUE;
                else if (fmt == PJMEDIA_FORMAT_NV21) adi->has_nv21 = PJ_TRUE;
            }
            (*jni_env)->ReleaseIntArrayElements(jni_env, jiarray, fmts,
                                                JNI_ABORT);
            (*jni_env)->DeleteLocalRef(jni_env, jtmp);

            /* Always put I420/IYUV and in the first place, for better
             * compatibility.
             */
            adi->forced_i420 = !has_i420;
            for (k = 0; k < adi->sup_size_cnt &&
                        vdi->fmt_cnt < max_fmt_cnt-1; k++)
            {
                /* Landscape video */
                pjmedia_format_init_video(&vdi->fmt[vdi->fmt_cnt++],
                                          PJMEDIA_FORMAT_I420,
                                          adi->sup_size[k].w,
                                          adi->sup_size[k].h,
                                          DEFAULT_FPS, 1);
                /* Portrait video */
                pjmedia_format_init_video(&vdi->fmt[vdi->fmt_cnt++],
                                          PJMEDIA_FORMAT_I420,
                                          adi->sup_size[k].h,
                                          adi->sup_size[k].w,
                                          DEFAULT_FPS, 1);
            }

/* Camera2 supports only I420 for now */
#if !USE_CAMERA2
            /* YV12 */
            if (adi->has_yv12) {
                for (k = 0; k < adi->sup_size_cnt &&
                            vdi->fmt_cnt < max_fmt_cnt-1; k++)
                {
                    /* Landscape video */
                    pjmedia_format_init_video(&vdi->fmt[vdi->fmt_cnt++],
                                              PJMEDIA_FORMAT_YV12,
                                              adi->sup_size[k].w,
                                              adi->sup_size[k].h,
                                              DEFAULT_FPS, 1);
                    /* Portrait video */
                    pjmedia_format_init_video(&vdi->fmt[vdi->fmt_cnt++],
                                              PJMEDIA_FORMAT_YV12,
                                              adi->sup_size[k].h,
                                              adi->sup_size[k].w,
                                              DEFAULT_FPS, 1);
                }
            }
            
            /* NV21 */
            if (adi->has_nv21) {
                for (k = 0; k < adi->sup_size_cnt &&
                            vdi->fmt_cnt < max_fmt_cnt-1; k++)
                {
                    /* Landscape video */
                    pjmedia_format_init_video(&vdi->fmt[vdi->fmt_cnt++],
                                              PJMEDIA_FORMAT_NV21,
                                              adi->sup_size[k].w,
                                              adi->sup_size[k].h,
                                              DEFAULT_FPS, 1);
                    /* Portrait video */
                    pjmedia_format_init_video(&vdi->fmt[vdi->fmt_cnt++],
                                              PJMEDIA_FORMAT_NV21,
                                              adi->sup_size[k].h,
                                              adi->sup_size[k].w,
                                              DEFAULT_FPS, 1);
                }
            }
#endif
            
        } else {
            goto on_skip_dev;
        }
        
        /* If this is front camera, set it as first/default (if not yet) */
        if (facing == 1) {
            if (!found_front && f->dev_count > 0) {
                /* Swap this front cam info with one whose idx==0 */
                and_dev_info tmp_adi;
                pj_memcpy(&tmp_adi, &f->dev_info[0], sizeof(tmp_adi));
                pj_memcpy(&f->dev_info[0], adi, sizeof(tmp_adi));
                pj_memcpy(adi, &tmp_adi, sizeof(tmp_adi));
                f->dev_info[0].info.id = 0;
                f->dev_info[f->dev_count].info.id = f->dev_count;
            }
            found_front = PJ_TRUE;
        }
        
        f->dev_count++;

    on_skip_dev:
        (*jni_env)->DeleteLocalRef(jni_env, jdev_info);
    }

    PJ_LOG(4, (THIS_FILE,
               "Android video capture initialized with %d device(s):",
               f->dev_count));
    for (i = 0; i < f->dev_count; i++) {
        and_dev_info *adi = &f->dev_info[i];
        char tmp_str[2048], *p;
        int j, plen, slen;
        PJ_LOG(4, (THIS_FILE, "%2d: %s", i, f->dev_info[i].info.name));

        /* Print supported formats */
        p = tmp_str;
        plen = sizeof(tmp_str);
        for (j = 0; j < adi->info.fmt_cnt; j++) {
            char tmp_str2[5];
            const pjmedia_video_format_detail *vfd =
                pjmedia_format_get_video_format_detail(&adi->info.fmt[j], 0);
            pjmedia_fourcc_name(adi->info.fmt[j].id, tmp_str2);
            slen = pj_ansi_snprintf(p, plen, "%s/%dx%d ",
                                    tmp_str2, vfd->size.w, vfd->size.h);
            if (slen < 0 || slen >= plen) break;
            plen -= slen;
            p += slen;
        }
        PJ_LOG(4, (THIS_FILE, "     supported format = %s", tmp_str));
    }

on_return:
    jni_detach_env(with_attach);
    return status;
}


/* API: get number of devices */
static unsigned and_factory_get_dev_count(pjmedia_vid_dev_factory *ff)
{
    and_factory *f = (and_factory*)ff;
    return f->dev_count;
}


/* API: get device info */
static pj_status_t and_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                            unsigned index,
                                            pjmedia_vid_dev_info *info)
{
    and_factory *cf = (and_factory*)f;

    PJ_ASSERT_RETURN(index < cf->dev_count, PJMEDIA_EVID_INVDEV);

    pj_memcpy(info, &cf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}


/* API: create default device parameter */
static pj_status_t and_factory_default_param(pj_pool_t *pool,
                                             pjmedia_vid_dev_factory *f,
                                             unsigned index,
                                             pjmedia_vid_dev_param *param)
{
    and_factory *cf = (and_factory*)f;
    and_dev_info *di = &cf->dev_info[index];

    PJ_ASSERT_RETURN(index < cf->dev_count, PJMEDIA_EVID_INVDEV);

    PJ_UNUSED_ARG(pool);

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_CAPTURE;
    param->cap_id = index;
    param->rend_id = PJMEDIA_VID_INVALID_DEV;
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->clock_rate = DEFAULT_CLOCK_RATE;
    pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));

    return PJ_SUCCESS;
}


/* API: create stream */
static pj_status_t and_factory_create_stream(
                                        pjmedia_vid_dev_factory *ff,
                                        pjmedia_vid_dev_param *param,
                                        const pjmedia_vid_dev_cb *cb,
                                        void *user_data,
                                        pjmedia_vid_dev_stream **p_vid_strm)
{
    and_factory *f = (and_factory*)ff;
    pj_pool_t *pool;
    and_stream *strm;
    and_dev_info *adi;
    const pjmedia_video_format_detail *vfd;
    const pjmedia_video_format_info *vfi;
    pjmedia_video_apply_fmt_param vafp;
    pj_uint32_t and_fmt = 0;
    unsigned convert_to_i420 = 0;
    pj_status_t status = PJ_SUCCESS;

    JNIEnv *jni_env;
    pj_bool_t with_attach;
    jobject jcam;

    PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.type == PJMEDIA_TYPE_VIDEO &&
                     param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO &&
                     param->dir == PJMEDIA_DIR_CAPTURE,
                     PJ_EINVAL);

/* Camera2 supports only I420 for now */
#if USE_CAMERA2
    if (param->fmt.id != PJMEDIA_FORMAT_I420)
        return PJMEDIA_EVID_BADFORMAT;
#endif

    pj_bzero(&vafp, sizeof(vafp));
    adi = &f->dev_info[param->cap_id];
    vfd = pjmedia_format_get_video_format_detail(&param->fmt, PJ_TRUE);
    vfi = pjmedia_get_video_format_info(NULL, param->fmt.id);

    if (param->fmt.id == PJMEDIA_FORMAT_I420 && adi->forced_i420) {
        /* Not really support I420, need to convert it from YV12/NV21 */
        if (adi->has_nv21) {
            and_fmt = pj_fmt_to_and(PJMEDIA_FORMAT_NV21);
            convert_to_i420 = 1;
        } else if (adi->has_yv12) {
            and_fmt = pj_fmt_to_and(PJMEDIA_FORMAT_YV12);
            convert_to_i420 = 2;
        } else
            pj_assert(!"Bug!");
    } else {
        and_fmt = pj_fmt_to_and(param->fmt.id);
    }
    if (!vfi || !and_fmt)
        return PJMEDIA_EVID_BADFORMAT;

    vafp.size = vfd->size;
    if (vfi->apply_fmt(vfi, &vafp) != PJ_SUCCESS)
        return PJMEDIA_EVID_BADFORMAT;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(f->pf, "and-dev", 512, 512, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, and_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    strm->factory = f;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    pj_memcpy(&strm->vafp, &vafp, sizeof(vafp));
    strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);

    /* Allocate buffer for YV12 -> I420 conversion.
     * The camera2 is a bit tricky with format, for example it reports
     * for I420 support (and no NV21 support), however the incoming frame
     * buffers are actually in NV21 format (e.g: pixel stride is 2), so
     * we should always check and conversion buffer may be needed.
     */
    if (USE_CAMERA2 || convert_to_i420) {
        pj_assert(vfi->plane_cnt > 1);
        strm->convert_to_i420 = convert_to_i420;
        strm->convert_buf = pj_pool_alloc(pool, vafp.plane_bytes[1]);
    }

    /* Native preview */
    if (param->flags & PJMEDIA_VID_DEV_CAP_INPUT_PREVIEW) {
    }

    with_attach = jni_get_env(&jni_env);

    /* Instantiate PjCamera */
    strm->cam_size.w = (vfd->size.w > vfd->size.h? vfd->size.w: vfd->size.h);
    strm->cam_size.h = (vfd->size.w > vfd->size.h? vfd->size.h: vfd->size.w);
    jcam = (*jni_env)->NewObject(jni_env, jobjs.cam.cls, jobjs.cam.m_init,
                                 adi->dev_idx,          /* idx */
                                 strm->cam_size.w,      /* w */
                                 strm->cam_size.h,      /* h */
                                 and_fmt,               /* fmt */
#if USE_CAMERA2
                                 vfd->fps.num/
#else
                                 vfd->fps.num*1000/
#endif
                                 vfd->fps.denum,        /* fps */
                                 (jlong)(intptr_t)strm, /* user data */
                                 NULL                   /* SurfaceView */
                                 );        
    if (jcam == NULL) {
        PJ_LOG(3, (THIS_FILE, "Unable to create PjCamera instance"));
        status = PJMEDIA_EVID_SYSERR;
        goto on_return;
    }
    strm->jcam = (jobject)(*jni_env)->NewGlobalRef(jni_env, jcam);
    (*jni_env)->DeleteLocalRef(jni_env, jcam);
    if (strm->jcam == NULL) {
        PJ_LOG(3, (THIS_FILE, "Unable to create global ref to PjCamera"));
        status = PJMEDIA_EVID_SYSERR;
        goto on_return;
    }
    
    /* Video orientation.
     * If we send in portrait, we need to set up orientation converter
     * as well.
     */
    if ((param->flags & PJMEDIA_VID_DEV_CAP_ORIENTATION) ||
        (vfd->size.h > vfd->size.w))
    {
        if (param->orient == PJMEDIA_ORIENT_UNKNOWN)
            param->orient = PJMEDIA_ORIENT_NATURAL;
        and_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_ORIENTATION,
                           &param->orient);
    }

on_return:
    jni_detach_env(with_attach);

    /* Success */
    if (status == PJ_SUCCESS) {
        strm->base.op = &stream_op;
        *p_vid_strm = &strm->base;
    }

    return status;
}


/****************************************************************************
 * Stream operations
 */


/* API: Get stream info. */
static pj_status_t and_stream_get_param(pjmedia_vid_dev_stream *s,
                                        pjmedia_vid_dev_param *pi)
{
    and_stream *strm = (and_stream*)s;
    
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    
    pj_memcpy(pi, &strm->param, sizeof(*pi));

    if (and_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                             &pi->window) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
    }
    
    return PJ_SUCCESS;
}


/* API: get capability */
static pj_status_t and_stream_get_cap(pjmedia_vid_dev_stream *s,
                                      pjmedia_vid_dev_cap cap,
                                      void *pval)
{
    and_stream *strm = (and_stream*)s;
    
    PJ_UNUSED_ARG(strm);
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    
    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        //pjmedia_vid_dev_hwnd *wnd = (pjmedia_vid_dev_hwnd *)pval;
        //wnd->info.android.window = strm->window;
        //return PJ_SUCCESS;
    }

    return PJMEDIA_EVID_INVCAP;
}


/* API: set capability */
static pj_status_t and_stream_set_cap(pjmedia_vid_dev_stream *s,
                                      pjmedia_vid_dev_cap cap,
                                      const void *pval)
{
    and_stream *strm = (and_stream*)s;
    JNIEnv *jni_env;
    pj_bool_t with_attach;
    pj_status_t status = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    switch (cap) {
        case PJMEDIA_VID_DEV_CAP_SWITCH:
        {
            pjmedia_vid_dev_switch_param *p = (pjmedia_vid_dev_switch_param*)
                                              pval;
            and_dev_info *adi;
            int res;
            
            /* Just return if current and target device are the same */
            if (strm->param.cap_id == p->target_id)
                return PJ_SUCCESS;

            /* Verify target capture ID */
            if (p->target_id < 0 || p->target_id >= strm->factory->dev_count)
                return PJ_EINVAL;

            /* Ok, let's do the switch */
            adi = &strm->factory->dev_info[p->target_id];
            PJ_LOG(4, (THIS_FILE, "Switching camera to %s..", adi->info.name));

            /* Call PjCamera::Start() method */
            with_attach = jni_get_env(&jni_env);
            res = (*jni_env)->CallIntMethod(jni_env, strm->jcam,
                                            jobjs.cam.m_switch, adi->dev_idx);
            if (res < 0) {
                PJ_LOG(3, (THIS_FILE, "Failed to switch camera (err=%d)",
                           res));
                status = PJMEDIA_EVID_SYSERR;
            } else {
                strm->param.cap_id = p->target_id;
                
                /* If successful, set the orientation as well */
                and_stream_set_cap(s, PJMEDIA_VID_DEV_CAP_ORIENTATION,
                                   &strm->param.orient);
            }
            jni_detach_env(with_attach);
            break;
        }

        case PJMEDIA_VID_DEV_CAP_ORIENTATION:
        {
            pjmedia_orient orient = *(pjmedia_orient *)pval;
            pjmedia_orient eff_ori;
            and_dev_info *adi;

            pj_assert(orient >= PJMEDIA_ORIENT_UNKNOWN &&
                      orient <= PJMEDIA_ORIENT_ROTATE_270DEG);

            if (orient == PJMEDIA_ORIENT_UNKNOWN)
                return PJ_EINVAL;

            pj_memcpy(&strm->param.orient, pval,
                      sizeof(strm->param.orient));

            if (!strm->conv.conv) {
                status = pjmedia_vid_dev_conv_create_converter(
                                                 &strm->conv, strm->pool,
                                                 &strm->param.fmt,
                                                 strm->cam_size,
                                                 strm->param.fmt.det.vid.size,
                                                 PJ_TRUE,
                                                 MAINTAIN_ASPECT_RATIO);
                
                if (status != PJ_SUCCESS)
                    return status;
            }
            
            eff_ori = strm->param.orient;
            adi = &strm->factory->dev_info[strm->param.cap_id];
            /* Normalize the orientation for back-facing camera */
            if (!adi->facing) {
                if (eff_ori == PJMEDIA_ORIENT_ROTATE_90DEG)
                    eff_ori = PJMEDIA_ORIENT_ROTATE_270DEG;
                else if (eff_ori == PJMEDIA_ORIENT_ROTATE_270DEG)
                    eff_ori = PJMEDIA_ORIENT_ROTATE_90DEG;
            }
            pjmedia_vid_dev_conv_set_rotation(&strm->conv, eff_ori);
            
            PJ_LOG(4, (THIS_FILE, "Video capture orientation set to %d",
                                  strm->param.orient));

            break;
        }

        default:
            status = PJMEDIA_EVID_INVCAP;
            break;
    }
    
    return status;
}


/* API: Start stream. */
static pj_status_t and_stream_start(pjmedia_vid_dev_stream *s)
{
    and_stream *strm = (and_stream*)s;
    JNIEnv *jni_env;
    pj_bool_t with_attach;
    jint res;
    pj_status_t status = PJ_SUCCESS;

    PJ_LOG(4, (THIS_FILE, "Starting Android camera stream"));

    with_attach = jni_get_env(&jni_env);

    /* Call PjCamera::Start() method */
    res = (*jni_env)->CallIntMethod(jni_env, strm->jcam, jobjs.cam.m_start);
    if (res < 0) {
        PJ_LOG(3, (THIS_FILE, "Failed to start camera (err=%d)", res));
        status = PJMEDIA_EVID_SYSERR;
        goto on_return;
    }

    strm->is_running = PJ_TRUE;

on_return:
    jni_detach_env(with_attach);
    return status;
}


/* API: Stop stream. */
static pj_status_t and_stream_stop(pjmedia_vid_dev_stream *s)
{
    and_stream *strm = (and_stream*)s;
    JNIEnv *jni_env;
    pj_bool_t with_attach;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);
    
    PJ_LOG(4, (THIS_FILE, "Stopping Android camera stream"));

    with_attach = jni_get_env(&jni_env);

    /* Call PjCamera::Stop() method */
    (*jni_env)->CallVoidMethod(jni_env, strm->jcam, jobjs.cam.m_stop);

    strm->is_running = PJ_FALSE;

    jni_detach_env(with_attach);
    
    return status;
}


/* API: Destroy stream. */
static pj_status_t and_stream_destroy(pjmedia_vid_dev_stream *s)
{
    and_stream *strm = (and_stream*)s;
    JNIEnv *jni_env;
    pj_bool_t with_attach;
    
    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);
    
    with_attach = jni_get_env(&jni_env);

    if (strm->is_running)
        and_stream_stop(s);

    if (strm->jcam) {
        (*jni_env)->DeleteGlobalRef(jni_env, strm->jcam);
        strm->jcam = NULL;
    }
    
    jni_detach_env(with_attach);
    
    pjmedia_vid_dev_conv_destroy_converter(&strm->conv);
    
    if (strm->pool)
        pj_pool_release(strm->pool);

    PJ_LOG(4, (THIS_FILE, "Android camera stream destroyed"));

    return PJ_SUCCESS;
}

#if USE_CAMERA2

PJ_INLINE(void) strip_padding(void *dst, void *src, int w, int h, int stride)
{
    int i;
    for (i = 0; i < h; ++i) {
        pj_memmove(dst, src, w);
        src += stride;
        dst += w;
    }
}

static void JNICALL OnGetFrame2(JNIEnv *env, jobject obj,
                                jlong user_data,
                                jobject plane0, jint rowStride0, jint pixStride0,
                                jobject plane1, jint rowStride1, jint pixStride1,
                                jobject plane2, jint rowStride2, jint pixStride2)
{
    and_stream *strm = (and_stream*)(intptr_t)user_data;
    pjmedia_frame f;
    pj_uint8_t *p0, *p1, *p2, *p0_end;
    pj_uint8_t *Y, *U, *V;
    pj_status_t status;
    void *frame_buf, *data_buf;
    
    strm->frame_ts.u64 += strm->ts_inc;
    if (!strm->vid_cb.capture_cb)
        return;

    if (strm->thread_initialized == 0 || !pj_thread_is_registered()) {
        pj_status_t status;
        pj_bzero(strm->thread_desc, sizeof(pj_thread_desc));
        status = pj_thread_register("and_cam", strm->thread_desc,
                                    &strm->thread);
        if (status != PJ_SUCCESS)
            return;
        strm->thread_initialized = 1;
        PJ_LOG(5,(THIS_FILE, "Android camera thread registered"));
    }

    p0 = (pj_uint8_t*)(*env)->GetDirectBufferAddress(env, plane0);
    p1 = (pj_uint8_t*)(*env)->GetDirectBufferAddress(env, plane1);
    p2 = (pj_uint8_t*)(*env)->GetDirectBufferAddress(env, plane2);
    
    /* Assuming the buffers are originally a large contigue buffer,
     * minimum check for now: plane 1 or 2 must be after plane 0.
     */
    p0_end = p0 + strm->cam_size.h * rowStride0;
    pj_assert(p1 >= p0_end || p2 >= p0_end);

    f.type = PJMEDIA_FRAME_TYPE_VIDEO;
    f.size = strm->vafp.framebytes;
    f.timestamp.u64 = strm->frame_ts.u64;
    f.buf = data_buf = p0;

    /* In this implementation, we only return I420 frames, so here we need to
     * convert other formats and strip any padding.
     */

    Y = (pj_uint8_t*)f.buf;
    U = Y + strm->vafp.plane_bytes[0];
    V = U + strm->vafp.plane_bytes[1];

    /* Check if we need conversion here, this is the tricky part of camera2.
     * When we request I420, the returned buffer may not be actually I420,
     * for example NV21. The camera2 'cheats' us via it's Plane type which
     * has pixel stride attribute. For example, NV21 buffer structure will
     * be represented as I420 using Plane array with the following attributes:
     * - Plane[1], or U plane, points to address of (Plane[0] + Ysize + 1).
     * - Plane[2], or V plane, points to address of (Plane[0] + Ysize).
     * - Pixel stride is set to 2 for U & V planes, and 1 for Y plane.
     */

    /* Already I420 without padding, nothing to do */
    if (p1 == U && p2 == V) {}

    /* I420 with padding, remove padding */
    else if (pixStride1==1 && pixStride2==1 && p2 > p1 && p1 > p0)
    {
        /* Strip out Y padding */
        if (rowStride0 > strm->cam_size.w) {
            strip_padding(Y, p0, strm->cam_size.w, strm->cam_size.h,
                          rowStride0);
        }

        /* Get U & V planes */

        if (rowStride1 == strm->cam_size.w/2) {
            /* No padding, simply bulk memmove U & V */
            pj_memmove(U, p1, strm->vafp.plane_bytes[1]);
            pj_memmove(V, p2, strm->vafp.plane_bytes[2]);
        } else if (rowStride1 > strm->cam_size.w/2) {
            /* Strip padding */
            strip_padding(U, p1, strm->cam_size.w/2, strm->cam_size.h/2,
                          rowStride1);
            strip_padding(V, p2, strm->cam_size.w/2, strm->cam_size.h/2,
                          rowStride2);
        }
    }

    /* The buffer may be originally NV21/NV12, i.e: VU/UV is interleaved */
    else if ((p1-p2==1 || p2-p1==1) &&
             pixStride0==1 &&  pixStride1==2 && pixStride2==2)
    {
        pj_bool_t nv21 = p1 > p2;

        /* Strip out Y padding */
        if (rowStride0 > strm->cam_size.w) {
            strip_padding(Y, p0, strm->cam_size.w, strm->cam_size.h,
                          rowStride0);
        }

        /* Get U & V, and strip if needed */
        {
            pj_uint8_t *dst_u = U;
            pj_uint8_t *dst_v = strm->convert_buf;
            int diff = rowStride1 - strm->cam_size.w;
            int i, j;

            if (nv21) {
                pj_uint8_t *src = p2;
                for (i = 0; i < strm->cam_size.h/2; ++i) {
                    for (j = 0; j < strm->cam_size.w/2; ++j) {
                        *dst_v++ = *src++;
                        *dst_u++ = *src++;
                    }
                    src += diff; /* stripping any padding */
                }
            } else {
                pj_uint8_t *src = p1;
                for (i = 0; i < strm->cam_size.h/2; ++i) {
                    for (j = 0; j < strm->cam_size.w/2; ++j) {
                        *dst_u++ = *src++;
                        *dst_v++ = *src++;
                    }
                    src += diff; /* stripping any padding */
                }
            }
            pj_memcpy(V, strm->convert_buf, strm->vafp.plane_bytes[2]);
        }
    }
    
    /* The buffer may be originally YV12, i.e: U & V planes are swapped.
     * We also need to strip out padding, if any.
     */
    else if (pixStride1==1 && pixStride2==1 && p1 > p2 && p2 > p0)
    {
        /* Strip out Y padding */
        if (rowStride0 > strm->cam_size.w) {
            strip_padding(Y, p0, strm->cam_size.w, strm->cam_size.h,
                          rowStride0);
        }

        /* Swap U & V planes */
        if (rowStride1 == strm->cam_size.w/2) {

            /* No padding, note Y plane should be no padding too! */
            pj_assert(rowStride0 == strm->cam_size.w);
            pj_memcpy(strm->convert_buf, p1, strm->vafp.plane_bytes[1]);
            pj_memmove(U, p1, strm->vafp.plane_bytes[1]);
            pj_memcpy(V, strm->convert_buf, strm->vafp.plane_bytes[1]);

        } else if (rowStride1 > strm->cam_size.w/2) {

            /* Strip padding */
            strip_padding(strm->convert_buf, p1, strm->cam_size.w/2,
                          strm->cam_size.h/2, rowStride1);
            strip_padding(V, p2, strm->cam_size.w/2, strm->cam_size.h/2,
                          rowStride2);

            /* Get V plane data from conversion buffer */
            pj_memcpy(V, strm->convert_buf, strm->vafp.plane_bytes[2]);

        }
    }
    
    /* Else, let's just print log for now */
    else {
        jlong p0_len, p1_len, p2_len;

        p0_len = (*env)->GetDirectBufferCapacity(env, plane0);
        p1_len = (*env)->GetDirectBufferCapacity(env, plane1);
        p2_len = (*env)->GetDirectBufferCapacity(env, plane2);

        PJ_LOG(1,(THIS_FILE, "Unrecognized image format from Android camera2, "
                             "please report the following plane format:"));
        PJ_LOG(1,(THIS_FILE, " Planes (buf/len/row_stride/pix_stride):"
                             " p0=%p/%ld/%d/%d p1=%p/%ld/%d/%d "
                             "p2=%p/%ld/%d/%d",
                             p0, p0_len, rowStride0, pixStride0,
                             p1, p1_len, rowStride1, pixStride1,
                             p2, p2_len, rowStride2, pixStride2));

#if 1
        /* Generic converter to I420, based on row stride & pixel stride */

        /* Strip out Y padding */
        if (rowStride0 > strm->cam_size.w) {
            strip_padding(Y, p0, strm->cam_size.w, strm->cam_size.h,
                          rowStride0);
        }

        /* Get U & V, and strip if needed */
        {
            pj_uint8_t *src_u = p1;
            pj_uint8_t *src_v = p2;
            pj_uint8_t *dst_u = U;
            pj_uint8_t *dst_v = strm->convert_buf;
            int i;

            /* Note, we use convert buffer for V, just in case U & V are
             * swapped.
             */
            for (i = 0; i < strm->cam_size.h/2; ++i) {
                int j;
                for (j = 0; j < strm->cam_size.w/2; ++j) {
                    *dst_v++ = *(src_v + j*pixStride2);
                    *dst_u++ = *(src_u + j*pixStride1);
                }
                src_u += rowStride1;
                src_v += rowStride2;
            }
            pj_memcpy(V, strm->convert_buf, strm->vafp.plane_bytes[2]);
        }
#endif

    }

    status = pjmedia_vid_dev_conv_resize_and_rotate(&strm->conv, 
                                                    f.buf,
                                                    &frame_buf);
    if (status == PJ_SUCCESS) {
        f.buf = frame_buf;
    }

    (*strm->vid_cb.capture_cb)(&strm->base, strm->user_data, &f);
}

#else

static void JNICALL OnGetFrame(JNIEnv *env, jobject obj,
                               jbyteArray data, jint length,
                               jlong user_data)
{
    and_stream *strm = (and_stream*)(intptr_t)user_data;
    pjmedia_frame f;
    pj_uint8_t *Y, *U, *V;
    pj_status_t status; 
    void *frame_buf, *data_buf;
    
    strm->frame_ts.u64 += strm->ts_inc;
    if (!strm->vid_cb.capture_cb)
        return;

    if (strm->thread_initialized == 0 || !pj_thread_is_registered()) {
        pj_status_t status;
        pj_bzero(strm->thread_desc, sizeof(pj_thread_desc));
        status = pj_thread_register("and_cam", strm->thread_desc,
                                    &strm->thread);
        if (status != PJ_SUCCESS)
            return;
        strm->thread_initialized = 1;
        PJ_LOG(5,(THIS_FILE, "Android camera thread registered"));
    }

    f.type = PJMEDIA_FRAME_TYPE_VIDEO;
    f.size = length;
    f.timestamp.u64 = strm->frame_ts.u64;
    f.buf = data_buf = (*env)->GetByteArrayElements(env, data, 0);

    Y = (pj_uint8_t*)f.buf;
    U = Y + strm->vafp.plane_bytes[0];
    V = U + strm->vafp.plane_bytes[1];

    /* Convert NV21 -> I420, i.e: separate V/U interleaved data plane
     * into U & V planes.
     */
    if (strm->convert_to_i420 == 1) {
        pj_uint8_t *src = U;
        pj_uint8_t *dst_u = U;
        pj_uint8_t *end_u = U + strm->vafp.plane_bytes[1];
        pj_uint8_t *dst_v = strm->convert_buf;
        while (dst_u < end_u) {
            *dst_v++ = *src++;
            *dst_u++ = *src++;
        }
        pj_memcpy(V, strm->convert_buf, strm->vafp.plane_bytes[2]);
    }

    /* Convert YV12 -> I420, i.e: swap U & V planes. We also need to
     * strip out padding, if any.
     */
    else if (strm->convert_to_i420 == 2) {
        int y_stride  = ALIGN16(strm->vafp.size.w);
        int uv_stride = ALIGN16(strm->vafp.size.w/2);

        /* Strip out Y padding */
        if (y_stride > strm->vafp.size.w) {
            int i;
            pj_uint8_t *src = Y + y_stride;
            pj_uint8_t *dst = Y + strm->vafp.size.w;

            for (i = 1; i < strm->vafp.size.h; ++i) {
                memmove(dst, src, strm->vafp.size.w);
                src += y_stride;
                dst += strm->vafp.size.w;
            }
        }

        /* Swap U & V planes */
        if (uv_stride == strm->vafp.size.w/2) {

            /* No padding, note Y plane should be no padding too! */
            pj_assert(y_stride == strm->vafp.size.w);
            pj_memcpy(strm->convert_buf, U, strm->vafp.plane_bytes[1]);
            pj_memmove(U, V, strm->vafp.plane_bytes[1]);
            pj_memcpy(V, strm->convert_buf, strm->vafp.plane_bytes[1]);

        } else if (uv_stride > strm->vafp.size.w/2) {

            /* Strip & copy V plane into conversion buffer */
            pj_uint8_t *src = Y + y_stride*strm->vafp.size.h;
            pj_uint8_t *dst = strm->convert_buf;
            unsigned dst_stride = strm->vafp.size.w/2;
            int i;
            for (i = 0; i < strm->vafp.size.h/2; ++i) {
                memmove(dst, src, dst_stride);
                src += uv_stride;
                dst += dst_stride;
            }

            /* Strip U plane */
            dst = U;
            for (i = 0; i < strm->vafp.size.h/2; ++i) {
                memmove(dst, src, dst_stride);
                src += uv_stride;
                dst += dst_stride;
            }

            /* Get V plane data from conversion buffer */
            pj_memcpy(V, strm->convert_buf, strm->vafp.plane_bytes[2]);

        }
    }
    
    status = pjmedia_vid_dev_conv_resize_and_rotate(&strm->conv, 
                                                    f.buf,
                                                    &frame_buf);
    if (status == PJ_SUCCESS) {
        f.buf = frame_buf;
    }

    (*strm->vid_cb.capture_cb)(&strm->base, strm->user_data, &f);
    (*env)->ReleaseByteArrayElements(env, data, data_buf, JNI_ABORT);
}

#endif /* USE_CAMERA2 */

#endif  /* PJMEDIA_VIDEO_DEV_HAS_ANDROID */
