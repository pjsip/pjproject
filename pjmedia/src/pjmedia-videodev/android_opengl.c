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
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_ANDROID_OPENGL) && \
    PJMEDIA_VIDEO_DEV_HAS_ANDROID_OPENGL != 0

#include <pjmedia-videodev/opengl_dev.h>
#include <jni.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define THIS_FILE               "android_opengl.cpp"

#define MAX_JOBS 1
/* Define the number of errors before the stream stops trying to do rendering.
 * To disable this feature, put 0.
 */
#define STOP_IF_ERROR_RENDERING 8

typedef struct andgl_fmt_info
{
    pjmedia_format_id   pjmedia_format;
} andgl_fmt_info;

/* Supported formats */
static andgl_fmt_info andgl_fmts[] =
{
    {PJMEDIA_FORMAT_BGRA}
};

typedef pj_status_t (*job_func_ptr)(void *data);

typedef struct job {
    job_func_ptr    func;
    void           *data;
    unsigned        flags;
    pj_status_t     retval;
} job;

typedef struct job_queue {
    job            *jobs[MAX_JOBS];
    pj_sem_t       *job_sem[MAX_JOBS];
    pj_mutex_t     *mutex;
    pj_thread_t    *thread;
    pj_sem_t       *sem;
    
    unsigned        size;
    unsigned        head, tail;
    pj_bool_t       is_quitting;
} job_queue;

/* Video stream. */
struct andgl_stream
{
    pjmedia_vid_dev_stream  base;               /**< Base stream       */
    pjmedia_vid_dev_param   param;              /**< Settings          */
    pj_pool_t              *pool;               /**< Memory pool       */
    
    pjmedia_vid_dev_cb      vid_cb;             /**< Stream callback   */
    void                   *user_data;          /**< Application data  */
    
    pj_timestamp            frame_ts;
    unsigned                ts_inc;
    pjmedia_rect_size       vid_size;
    
    job_queue              *jq;
    pj_bool_t               is_running;
    pj_int32_t              err_rend;
    const pjmedia_frame    *frame;
    
    gl_buffers             *gl_buf;
    EGLDisplay              display;
    EGLSurface              surface;
    EGLContext              context;
    ANativeWindow          *window;
};


/* Prototypes */
static pj_status_t andgl_stream_get_param(pjmedia_vid_dev_stream *strm,
                                          pjmedia_vid_dev_param *param);
static pj_status_t andgl_stream_get_cap(pjmedia_vid_dev_stream *strm,
                                        pjmedia_vid_dev_cap cap,
                                        void *value);
static pj_status_t andgl_stream_set_cap(pjmedia_vid_dev_stream *strm,
                                        pjmedia_vid_dev_cap cap,
                                        const void *value);
static pj_status_t andgl_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t andgl_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                          const pjmedia_frame *frame);
static pj_status_t andgl_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t andgl_stream_destroy(pjmedia_vid_dev_stream *strm);

static pj_status_t init_opengl(void * data);
static pj_status_t deinit_opengl(void * data);

/* Job queue prototypes */
static pj_status_t job_queue_create(pj_pool_t *pool, job_queue **pjq);
static pj_status_t job_queue_post_job(job_queue *jq, job_func_ptr func,
                                      void *data, unsigned flags,
                                      pj_status_t *retval);
static pj_status_t job_queue_destroy(job_queue *jq);

/* Operations */
static pjmedia_vid_dev_stream_op stream_op =
{
    &andgl_stream_get_param,
    &andgl_stream_get_cap,
    &andgl_stream_set_cap,
    &andgl_stream_start,
    NULL,
    &andgl_stream_put_frame,
    &andgl_stream_stop,
    &andgl_stream_destroy
};

int pjmedia_vid_dev_opengl_imp_get_cap(void)
{
    return PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
}

static andgl_fmt_info* get_andgl_format_info(pjmedia_format_id id)
{
    unsigned i;
    
    for (i = 0; i < PJ_ARRAY_SIZE(andgl_fmts); i++) {
        if (andgl_fmts[i].pjmedia_format == id)
            return &andgl_fmts[i];
    }
    
    return NULL;
}

#define EGL_ERR(str) \
    { \
        PJ_LOG(3, (THIS_FILE, "Unable to %s %d", str, eglGetError())); \
        status = PJMEDIA_EVID_SYSERR; \
        goto on_return; \
    }

static pj_status_t init_opengl(void * data)
{
    struct andgl_stream *strm = (struct andgl_stream *)data;
    const EGLint attr[] =
    {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_NONE
    };
    EGLint context_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLConfig config;
    EGLint numConfigs;
    EGLint format;
    EGLint width;
    EGLint height;
    pj_status_t status;
    
    strm->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (strm->display == EGL_NO_DISPLAY ||
        !eglInitialize(strm->display, NULL, NULL))
    {
        EGL_ERR("initialize OpenGL display");
    }

    if (!eglChooseConfig(strm->display, attr, &config, 1, &numConfigs) ||
        (!eglGetConfigAttrib(strm->display, config, EGL_NATIVE_VISUAL_ID,
                             &format)))
    {
        EGL_ERR("configure OpenGL display");
    }
    
    if (ANativeWindow_setBuffersGeometry(strm->window, strm->param.disp_size.w,
                                         strm->param.disp_size.h, format) != 0)
    {
        EGL_ERR("set window geometry");
    }

    strm->surface = eglCreateWindowSurface(strm->display, config,
                                           strm->window, 0);
    if (strm->surface == EGL_NO_SURFACE)
        EGL_ERR("create window surface");
    
    strm->context = eglCreateContext(strm->display, config, EGL_NO_CONTEXT,
                                     context_attr);
    if (strm->context == EGL_NO_CONTEXT)
        EGL_ERR("create OpenGL context");

    if (!eglMakeCurrent(strm->display, strm->surface, strm->surface,
                        strm->context))
    {
        EGL_ERR("make OpenGL as current context");
    }
    
    if (!eglQuerySurface(strm->display, strm->surface, EGL_WIDTH, &width) ||
        !eglQuerySurface(strm->display, strm->surface, EGL_HEIGHT, &height))
    {
        EGL_ERR("query surface");
    }
    
    /* Create GL buffers */
    pjmedia_vid_dev_opengl_create_buffers(strm->pool, PJ_TRUE, &strm->gl_buf);
    
    /* Init GL buffers */
    status = pjmedia_vid_dev_opengl_init_buffers(strm->gl_buf);
    
on_return:
    if (status != PJ_SUCCESS)
        deinit_opengl(strm);

    return status;
}

#undef EGL_ERR

static pj_status_t render(void * data)
{
    struct andgl_stream *stream = (struct andgl_stream *)data;
    
    if (stream->display == EGL_NO_DISPLAY || stream->err_rend == 0)
        return PJ_SUCCESS;
    
    pjmedia_vid_dev_opengl_draw(stream->gl_buf, stream->vid_size.w,
                                stream->vid_size.h, stream->frame->buf);
        
    if (!eglSwapBuffers(stream->display, stream->surface)) {
        if (eglGetError() == EGL_BAD_SURFACE && stream->err_rend > 0) {
            stream->err_rend--;
            if (stream->err_rend == 0) {
                PJ_LOG(3, (THIS_FILE, "Stopping OpenGL rendering due to "
                                      "consecutive errors. If app is in bg,"
                                      "it's advisable to stop the stream."));
            }
        }
        return eglGetError();
    }
    
    return PJ_SUCCESS;
}

static pj_status_t deinit_opengl(void * data)
{
    struct andgl_stream *stream = (struct andgl_stream *)data;

    if (stream->gl_buf) {
        pjmedia_vid_dev_opengl_destroy_buffers(stream->gl_buf);
        stream->gl_buf = NULL;
    }

    if (stream->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(stream->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (stream->context != EGL_NO_CONTEXT)
            eglDestroyContext(stream->display, stream->context);
        if (stream->surface != EGL_NO_SURFACE)
            eglDestroySurface(stream->display, stream->surface);
        eglTerminate(stream->display);
    }
    
    stream->display = EGL_NO_DISPLAY;
    stream->surface = EGL_NO_SURFACE;
    stream->context = EGL_NO_CONTEXT;
    
    return PJ_SUCCESS;
}

/* API: create stream */
pj_status_t
pjmedia_vid_dev_opengl_imp_create_stream(pj_pool_t *pool,
                                         pjmedia_vid_dev_param *param,
                                         const pjmedia_vid_dev_cb *cb,
                                         void *user_data,
                                         pjmedia_vid_dev_stream **p_vid_strm)
{
    struct andgl_stream *strm;
    const pjmedia_video_format_detail *vfd;
    pj_status_t status = PJ_SUCCESS;
    
    strm = PJ_POOL_ZALLOC_T(pool, struct andgl_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    strm->display = EGL_NO_DISPLAY;
    
    vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt, PJ_TRUE);
    strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);
    
    /* Set video format */
    status = andgl_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_FORMAT,
                                  &param->fmt);
    if (status != PJ_SUCCESS)
        goto on_error;

    status = job_queue_create(pool, &strm->jq);
    if (status != PJ_SUCCESS)
        goto on_error;

    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        status = andgl_stream_set_cap(&strm->base,
                                      PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                                      &param->window);
    }
    
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Failed to initialize OpenGL with the specified"
                              " output window"));
        goto on_error;
    }

    PJ_LOG(4, (THIS_FILE, "Android OpenGL ES renderer successfully created"));
                    
    /* Done */
    strm->base.op = &stream_op;
    *p_vid_strm = &strm->base;
    
    return PJ_SUCCESS;
    
on_error:
    andgl_stream_destroy((pjmedia_vid_dev_stream *)strm);
    
    return status;
}

/* API: Get stream info. */
static pj_status_t andgl_stream_get_param(pjmedia_vid_dev_stream *s,
                                          pjmedia_vid_dev_param *pi)
{
    struct andgl_stream *strm = (struct andgl_stream*)s;
    
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    
    pj_memcpy(pi, &strm->param, sizeof(*pi));

    if (andgl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                             &pi->window) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
    }
    
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t andgl_stream_get_cap(pjmedia_vid_dev_stream *s,
                                        pjmedia_vid_dev_cap cap,
                                        void *pval)
{
    struct andgl_stream *strm = (struct andgl_stream*)s;
    
    PJ_UNUSED_ARG(strm);
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    
    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        pjmedia_vid_dev_hwnd *wnd = (pjmedia_vid_dev_hwnd *)pval;
        wnd->info.android.window = strm->window;
        return PJ_SUCCESS;
    } else {
        return PJMEDIA_EVID_INVCAP;
    }
}

/* API: set capability */
static pj_status_t andgl_stream_set_cap(pjmedia_vid_dev_stream *s,
                                        pjmedia_vid_dev_cap cap,
                                        const void *pval)
{
    struct andgl_stream *strm = (struct andgl_stream*)s;
    
    PJ_UNUSED_ARG(strm);
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    
    if (cap==PJMEDIA_VID_DEV_CAP_FORMAT) {
        const pjmedia_video_format_info *vfi;
        pjmedia_video_format_detail *vfd;
        pjmedia_format *fmt = (pjmedia_format *)pval;
        andgl_fmt_info *ifi;
        pj_status_t status = PJ_SUCCESS;
        
        if (!(ifi = get_andgl_format_info(fmt->id)))
            return PJMEDIA_EVID_BADFORMAT;
        
        vfi = pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
                                            fmt->id);
        if (!vfi)
            return PJMEDIA_EVID_BADFORMAT;
        
        /* Re-init OpenGL */
        if (strm->window)
            job_queue_post_job(strm->jq, deinit_opengl, strm, 0, NULL);

        pjmedia_format_copy(&strm->param.fmt, fmt);

        vfd = pjmedia_format_get_video_format_detail(fmt, PJ_TRUE);
        pj_memcpy(&strm->vid_size, &vfd->size, sizeof(vfd->size));
        pj_memcpy(&strm->param.disp_size, &vfd->size, sizeof(vfd->size));
        
        if (strm->window)
            job_queue_post_job(strm->jq, init_opengl, strm, 0, &status);
            
        PJ_PERROR(4,(THIS_FILE, status,
                     "Re-initializing OpenGL due to format change"));
        return status;
        
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        pj_status_t status = PJ_SUCCESS;
        pjmedia_vid_dev_hwnd *wnd = (pjmedia_vid_dev_hwnd *)pval;
        ANativeWindow *native_wnd = (ANativeWindow *)wnd->info.android.window;

        if (strm->window == native_wnd)
            return PJ_SUCCESS;
        
        /* Re-init OpenGL */
        job_queue_post_job(strm->jq, deinit_opengl, strm, 0, NULL);
        if (strm->window)
            ANativeWindow_release(strm->window);

        strm->window = strm->param.window.info.android.window = native_wnd;
        if (strm->window) {
            job_queue_post_job(strm->jq, init_opengl, strm, 0, &status);
        }

        PJ_PERROR(4,(THIS_FILE, status,
                     "Re-initializing OpenGL with native window %p",
                     strm->window));
        return status;
    }
    
    return PJMEDIA_EVID_INVCAP;
}

/* API: Start stream. */
static pj_status_t andgl_stream_start(pjmedia_vid_dev_stream *strm)
{
    struct andgl_stream *stream = (struct andgl_stream*)strm;
    
    stream->err_rend = STOP_IF_ERROR_RENDERING;
    if (!stream->err_rend) stream->err_rend = 0xFFFF;
    stream->is_running = PJ_TRUE;
    PJ_LOG(4, (THIS_FILE, "Starting Android opengl stream"));
    
    return PJ_SUCCESS;
}

/* API: Put frame from stream */
static pj_status_t andgl_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                          const pjmedia_frame *frame)
{
    struct andgl_stream *stream = (struct andgl_stream*)strm;
    pj_status_t status;

    /* Video conference just trying to send heart beat for updating timestamp
     * or keep-alive, this port doesn't need any, just ignore.
     */
    if (frame->size==0 || frame->buf==NULL)
        return PJ_SUCCESS;
        
    if (!stream->is_running || stream->display == EGL_NO_DISPLAY)
        return PJ_EINVALIDOP;
    
    stream->frame = frame;
    job_queue_post_job(stream->jq, render, strm, 0, &status);
    
    return status;
}

/* API: Stop stream. */
static pj_status_t andgl_stream_stop(pjmedia_vid_dev_stream *strm)
{
    struct andgl_stream *stream = (struct andgl_stream*)strm;
    
    stream->is_running = PJ_FALSE;
    PJ_LOG(4, (THIS_FILE, "Stopping Android opengl stream"));

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t andgl_stream_destroy(pjmedia_vid_dev_stream *strm)
{
    struct andgl_stream *stream = (struct andgl_stream*)strm;
    
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);
    
    andgl_stream_stop(strm);
    
    job_queue_post_job(stream->jq, deinit_opengl, strm, 0, NULL);
    
    if (stream->window) {
        ANativeWindow_release(stream->window);
        stream->window = NULL;
    }
    
    if (stream->jq) {
        job_queue_destroy(stream->jq);
        stream->jq = NULL;
    }
    
    pj_pool_release(stream->pool);
    
    return PJ_SUCCESS;
}

static int job_thread(void * data)
{
    job_queue *jq = (job_queue *)data;
    
    while (1) {
        job *jb;
        
        /* Wait until there is a job. */
        pj_sem_wait(jq->sem);
        
        /* Make sure there is no pending jobs before we quit. */
        if (jq->is_quitting && jq->head == jq->tail)
            break;
        
        jb = jq->jobs[jq->head];
        jb->retval = (*jb->func)(jb->data);
        pj_sem_post(jq->job_sem[jq->head]);
        jq->head = (jq->head + 1) % jq->size;
    }
    
    return 0;
}

static pj_status_t job_queue_create(pj_pool_t *pool, job_queue **pjq)
{
    unsigned i;
    pj_status_t status;
    
    job_queue *jq = PJ_POOL_ZALLOC_T(pool, job_queue);
    jq->size = MAX_JOBS;
    status = pj_sem_create(pool, "thread_sem", 0, jq->size + 1, &jq->sem);
    if (status != PJ_SUCCESS)
        goto on_error;

    for (i = 0; i < jq->size; i++) {
        status = pj_sem_create(pool, "job_sem", 0, 1, &jq->job_sem[i]);
        if (status != PJ_SUCCESS)
            goto on_error;
    }
    
    status = pj_mutex_create_recursive(pool, "job_mutex", &jq->mutex);
    if (status != PJ_SUCCESS)
        goto on_error;
    
    status = pj_thread_create(pool, "job_th", job_thread, jq, 0, 0,
                              &jq->thread);
    if (status != PJ_SUCCESS)
        goto on_error;
    
    *pjq = jq;
    return PJ_SUCCESS;
    
on_error:
    job_queue_destroy(jq);
    return status;
}

static pj_status_t job_queue_post_job(job_queue *jq, job_func_ptr func,
                                      void *data, unsigned flags,
                                      pj_status_t *retval)
{
    job jb;
    int tail;
    
    if (jq->is_quitting)
        return PJ_EBUSY;
    
    jb.func = func;
    jb.data = data;
    jb.flags = flags;
    
    pj_mutex_lock(jq->mutex);
    jq->jobs[jq->tail] = &jb;
    tail = jq->tail;
    jq->tail = (jq->tail + 1) % jq->size;

    pj_sem_post(jq->sem);
    /* Wait until our posted job is completed. */
    pj_sem_wait(jq->job_sem[tail]);
    pj_mutex_unlock(jq->mutex);
    
    if (retval) *retval = jb.retval;
    
    return PJ_SUCCESS;
}

static pj_status_t job_queue_destroy(job_queue *jq)
{
    unsigned i;
    
    jq->is_quitting = PJ_TRUE;
    
    if (jq->thread) {
        pj_sem_post(jq->sem);
        pj_thread_join(jq->thread);
        pj_thread_destroy(jq->thread);
    }
    
    if (jq->sem) {
        pj_sem_destroy(jq->sem);
        jq->sem = NULL;
    }
    for (i = 0; i < jq->size; i++) {
        if (jq->job_sem[i]) {
            pj_sem_destroy(jq->job_sem[i]);
            jq->job_sem[i] = NULL;
        }
    }

    if (jq->mutex) {
        pj_mutex_destroy(jq->mutex);
        jq->mutex = NULL;
    }
    
    return PJ_SUCCESS;
}

#endif  /* PJMEDIA_VIDEO_DEV_HAS_ANDROID_OPENGL */
