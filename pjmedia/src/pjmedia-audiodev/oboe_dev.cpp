/*
 * Copyright (C) 2021 Teluu Inc. (http://www.teluu.com)
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


/* This file is the implementation of Android Oboe audio device. */

#include <pjmedia-audiodev/audiodev_imp.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pjmedia/circbuf.h>
#include <pjmedia/errno.h>

#if defined(PJMEDIA_AUDIO_DEV_HAS_OBOE) &&  PJMEDIA_AUDIO_DEV_HAS_OBOE != 0

#define THIS_FILE	"oboe_dev.cpp"
#define DRIVER_NAME	"Oboe"

#include <jni.h>
#include <semaphore.h>
#include <android/log.h>
#include <oboe/Oboe.h>

#include <atomic>


/* Device info */
typedef struct aud_dev_info
{
    pjmedia_aud_dev_info	 info;		/**< Base info		*/
    int				 id;		/**< Original dev ID	*/
} aud_dev_info;


/* Oboe factory */
struct oboe_aud_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_factory		*pf;
    pj_pool_t			*pool;

    pj_pool_t			*dev_pool;	/**< Device list pool  */
    unsigned			 dev_count;	/**< Device count      */
    aud_dev_info		*dev_info;	/**< Device info list  */
};

class MyOboeEngine;


/*
 * Sound stream descriptor.
 * This struct may be used for both unidirectional or bidirectional sound
 * streams.
 */
struct oboe_aud_stream
{
    pjmedia_aud_stream  base;
    pj_pool_t          *pool;
    pj_str_t            name;
    pjmedia_dir         dir;
    pjmedia_aud_param   param;
    struct oboe_aud_factory *f;

    int                 bytes_per_sample;
    pj_uint32_t         samples_per_sec;
    unsigned            samples_per_frame;
    int                 channel_count;
    void               *user_data;
    pj_bool_t           running;

    /* Capture/record */
    MyOboeEngine       *rec_engine;
    pjmedia_aud_rec_cb  rec_cb;

    /* Playback */
    MyOboeEngine       *play_engine;
    pjmedia_aud_play_cb play_cb;
};

/* Factory prototypes */
static pj_status_t oboe_init(pjmedia_aud_dev_factory *f);
static pj_status_t oboe_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t oboe_refresh(pjmedia_aud_dev_factory *f);
static unsigned oboe_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t oboe_get_dev_info(pjmedia_aud_dev_factory *f,
                                        unsigned index,
                                        pjmedia_aud_dev_info *info);
static pj_status_t oboe_default_param(pjmedia_aud_dev_factory *f,
                                         unsigned index,
                                         pjmedia_aud_param *param);
static pj_status_t oboe_create_stream(pjmedia_aud_dev_factory *f,
                                         const pjmedia_aud_param *param,
                                         pjmedia_aud_rec_cb rec_cb,
                                         pjmedia_aud_play_cb play_cb,
                                         void *user_data,
                                         pjmedia_aud_stream **p_aud_strm);

/* Stream prototypes */
static pj_status_t strm_get_param(pjmedia_aud_stream *strm,
                                  pjmedia_aud_param *param);
static pj_status_t strm_get_cap(pjmedia_aud_stream *strm,
                                pjmedia_aud_dev_cap cap,
                                void *value);
static pj_status_t strm_set_cap(pjmedia_aud_stream *strm,
                                pjmedia_aud_dev_cap cap,
                                const void *value);
static pj_status_t strm_start(pjmedia_aud_stream *strm);
static pj_status_t strm_stop(pjmedia_aud_stream *strm);
static pj_status_t strm_destroy(pjmedia_aud_stream *strm);

static pjmedia_aud_dev_factory_op oboe_op =
{
    &oboe_init,
    &oboe_destroy,
    &oboe_get_dev_count,
    &oboe_get_dev_info,
    &oboe_default_param,
    &oboe_create_stream,
    &oboe_refresh
};

static pjmedia_aud_stream_op oboe_strm_op =
{
    &strm_get_param,
    &strm_get_cap,
    &strm_set_cap,
    &strm_start,
    &strm_stop,
    &strm_destroy
};


/*
 * Init Android Oboe audio driver.
 */
#ifdef __cplusplus
extern "C"{
#endif
pjmedia_aud_dev_factory* pjmedia_android_oboe_factory(pj_pool_factory *pf)
{
    struct oboe_aud_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "oboe", 256, 256, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct oboe_aud_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &oboe_op;
    f->dev_pool = pj_pool_create(pf, "oboe_dev", 256, 256, NULL);

    return &f->base;
}
#ifdef __cplusplus
}
#endif


/* JNI stuff for enumerating audio devices. This will invoke Java code
 * in pjmedia/src/pjmedia-audiodev/android/PjAudioDevInfo.java.
 */
#define PJ_AUDDEV_INFO_CLASS_PATH	"org/pjsip/PjAudioDevInfo"

static struct jni_objs_t
{
    struct {
	jclass		 cls;
	jmethodID	 m_get_cnt;
	jmethodID	 m_get_info;
	jmethodID	 m_refresh;
	jfieldID	 f_id;
	jfieldID	 f_name;
	jfieldID	 f_direction;
	jfieldID	 f_sup_clockrates;
	jfieldID	 f_sup_channels;
    } dev_info;

    struct {
	jclass		 cls;
	jmethodID	 m_current;
	jmethodID	 m_get_app;
    } activity_thread;

} jobjs;


/* Declare JNI JVM helper from PJLIB OS */
extern "C" {
    pj_bool_t pj_jni_attach_jvm(JNIEnv **jni_env);
    void pj_jni_dettach_jvm(pj_bool_t attached);
}

#define GET_CLASS(class_path, class_name, cls) \
    cls = jni_env->FindClass(class_path); \
    if (cls == NULL || jni_env->ExceptionCheck()) { \
	jni_env->ExceptionClear(); \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find class '" \
			      class_name "'")); \
        status = PJMEDIA_EAUD_SYSERR; \
        goto on_return; \
    } else { \
        jclass tmp = cls; \
	cls = (jclass)jni_env->NewGlobalRef(tmp); \
	jni_env->DeleteLocalRef(tmp); \
	if (cls == NULL) { \
	    PJ_LOG(3, (THIS_FILE, "[JNI] Unable to get global ref for " \
				  "class '" class_name "'")); \
	    status = PJMEDIA_EAUD_SYSERR; \
	    goto on_return; \
	} \
    }
#define GET_METHOD_ID(cls, class_name, method_name, signature, id) \
    id = jni_env->GetMethodID(cls, method_name, signature); \
    if (id == 0) { \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find method '" method_name \
			      "' in class '" class_name "'")); \
        status = PJMEDIA_EAUD_SYSERR; \
        goto on_return; \
    }
#define GET_SMETHOD_ID(cls, class_name, method_name, signature, id) \
    id = jni_env->GetStaticMethodID(cls, method_name, signature); \
    if (id == 0) { \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find static method '" \
			      method_name "' in class '" class_name "'")); \
        status = PJMEDIA_EAUD_SYSERR; \
        goto on_return; \
    }
#define GET_FIELD_ID(cls, class_name, field_name, signature, id) \
    id = jni_env->GetFieldID(cls, field_name, signature); \
    if (id == 0) { \
        PJ_LOG(3, (THIS_FILE, "[JNI] Unable to find field '" field_name \
			      "' in class '" class_name "'")); \
        status = PJMEDIA_EAUD_SYSERR; \
        goto on_return; \
    }

/* Get Java object IDs (via FindClass, GetMethodID, GetFieldID, etc).
 * Note that this function should be called from library-loader thread,
 * otherwise FindClass, etc, may fail, see:
 * http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
 */
static pj_status_t jni_init_ids()
{
    JNIEnv *jni_env;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t with_attach = pj_jni_attach_jvm(&jni_env);

    /* PjAudioDevInfo class info */
    GET_CLASS(PJ_AUDDEV_INFO_CLASS_PATH, "PjAudioDevInfo", jobjs.dev_info.cls);
    GET_SMETHOD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "GetCount", "()I",
		   jobjs.dev_info.m_get_cnt);
    GET_SMETHOD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "GetInfo",
		   "(I)L" PJ_AUDDEV_INFO_CLASS_PATH ";",
		   jobjs.dev_info.m_get_info);
    GET_SMETHOD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "RefreshDevices",
		   "(Landroid/content/Context;)V",
		   jobjs.dev_info.m_refresh);
    GET_FIELD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "id", "I",
		 jobjs.dev_info.f_id);
    GET_FIELD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "name", "Ljava/lang/String;",
		 jobjs.dev_info.f_name);
    GET_FIELD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "direction", "I",
		 jobjs.dev_info.f_direction);
    GET_FIELD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "supportedClockRates", "[I",
		 jobjs.dev_info.f_sup_clockrates);
    GET_FIELD_ID(jobjs.dev_info.cls, "PjAudioDevInfo", "supportedChannelCounts", "[I",
		 jobjs.dev_info.f_sup_channels);

    /* ActivityThread class info */
    GET_CLASS("android/app/ActivityThread", "ActivityThread",
	      jobjs.activity_thread.cls);
    GET_SMETHOD_ID(jobjs.activity_thread.cls, "ActivityThread",
		   "currentActivityThread", "()Landroid/app/ActivityThread;",
		   jobjs.activity_thread.m_current);
    GET_METHOD_ID(jobjs.activity_thread.cls, "ActivityThread",
		   "getApplication", "()Landroid/app/Application;",
		   jobjs.activity_thread.m_get_app);

on_return:
    pj_jni_dettach_jvm(with_attach);
    return status;
}

#undef GET_CLASS_ID
#undef GET_METHOD_ID
#undef GET_SMETHOD_ID
#undef GET_FIELD_ID

static void jni_deinit_ids()
{
    JNIEnv *jni_env;
    pj_bool_t with_attach = pj_jni_attach_jvm(&jni_env);

    if (jobjs.dev_info.cls) {
	jni_env->DeleteGlobalRef(jobjs.dev_info.cls);
	jobjs.dev_info.cls = NULL;
    }

    if (jobjs.activity_thread.cls) {
	jni_env->DeleteGlobalRef(jobjs.activity_thread.cls);
	jobjs.activity_thread.cls = NULL;
    }

    pj_jni_dettach_jvm(with_attach);
}

static jobject get_global_context(JNIEnv *jni_env)
{
    jobject context = NULL;
    jobject cur_at = jni_env->CallStaticObjectMethod(
					jobjs.activity_thread.cls,
					jobjs.activity_thread.m_current);
    if (cur_at==NULL)
	return NULL;

    context = jni_env->CallObjectMethod(cur_at,
					jobjs.activity_thread.m_get_app);
    return context;
}


/* API: Init factory */
static pj_status_t oboe_init(pjmedia_aud_dev_factory *f)
{
    pj_status_t status;

    status = jni_init_ids();
    if (status != PJ_SUCCESS)
	return status;

    status = oboe_refresh(f);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


/* API: refresh the list of devices */
static pj_status_t oboe_refresh(pjmedia_aud_dev_factory *ff)
{
    struct oboe_aud_factory *f = (struct oboe_aud_factory*)ff;
    JNIEnv *jni_env;
    pj_bool_t with_attach;
    int i, dev_count = 0;
    pj_status_t status = PJ_SUCCESS;

    /* Clean up device info and pool */
    f->dev_count = 0;
    pj_pool_reset(f->dev_pool);

    with_attach = pj_jni_attach_jvm(&jni_env);

    jobject context = get_global_context(jni_env);
    if (context == NULL) {
	PJ_LOG(3, (THIS_FILE, "Failed to get context"));
	status = PJMEDIA_EAUD_SYSERR;
	goto on_return;
    }

    /* PjAudioDevInfo::RefreshDevices(Context) */
    jni_env->CallStaticVoidMethod(jobjs.dev_info.cls,
				  jobjs.dev_info.m_refresh, context);

    /* dev_count = PjAudioDevInfo::GetCount() */
    dev_count = jni_env->CallStaticIntMethod(jobjs.dev_info.cls,
					     jobjs.dev_info.m_get_cnt);
    if (dev_count < 0) {
        PJ_LOG(3, (THIS_FILE, "Failed to get camera count"));
        status = PJMEDIA_EAUD_SYSERR;
        goto on_return;
    }

    /* Start querying device info */
    f->dev_info = (aud_dev_info*)
		  pj_pool_calloc(f->dev_pool, dev_count,
				 sizeof(aud_dev_info));

    for (i = 0; i < dev_count; i++) {
	aud_dev_info *adi = &f->dev_info[f->dev_count];
	pjmedia_aud_dev_info *base_adi = &adi->info;
        jobject jdev_info;
	jint jinttmp;

	/* jdev_info = PjAudioDevInfo::GetInfo(i) */
	jdev_info = jni_env->CallStaticObjectMethod(
					    jobjs.dev_info.cls,
					    jobjs.dev_info.m_get_info,
					    i);
	if (jdev_info == NULL)
	    continue;

	/* Get device ID, direction, etc */
	adi->id = jni_env->GetIntField(jdev_info, jobjs.dev_info.f_id);
	jinttmp = jni_env->GetIntField(jdev_info, jobjs.dev_info.f_direction);
	base_adi->input_count = (jinttmp & PJMEDIA_DIR_CAPTURE);
	base_adi->output_count = (jinttmp & PJMEDIA_DIR_PLAYBACK);
	base_adi->caps = 0;

	/* Get name info */
	jstring jstrtmp = (jstring)jni_env->GetObjectField(jdev_info, jobjs.dev_info.f_name);
	const char *strtmp = jni_env->GetStringUTFChars(jstrtmp, NULL);
	pj_ansi_strncpy(base_adi->name, strtmp, sizeof(base_adi->name));

	f->dev_count++;

    on_skip_dev:
	jni_env->DeleteLocalRef(jdev_info);
    }

    PJ_LOG(4, (THIS_FILE,
	       "Oboe audio device initialized with %d device(s):",
	       f->dev_count));

    for (i = 0; i < f->dev_count; i++) {
	aud_dev_info *adi = &f->dev_info[i];
	PJ_LOG(4, (THIS_FILE, "%2d (native id=%d): %s (%s%s%s)",
		   i, adi->id, adi->info.name,
		   adi->info.input_count?"in":"",
		   adi->info.input_count && adi->info.output_count?"+":"",
		   adi->info.output_count?"out":""
		));
    }

on_return:
    if (context)
	jni_env->DeleteLocalRef(context);

    pj_jni_dettach_jvm(with_attach);
    return status;
}


/* API: Destroy factory */
static pj_status_t oboe_destroy(pjmedia_aud_dev_factory *f)
{
    struct oboe_aud_factory *pa = (struct oboe_aud_factory*)f;
    pj_pool_t *pool;

    PJ_LOG(4, (THIS_FILE, "Oboe sound library shutting down.."));

    jni_deinit_ids();

    pool = pa->pool;
    pa->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: Get device count. */
static unsigned oboe_get_dev_count(pjmedia_aud_dev_factory *ff)
{
    struct oboe_aud_factory *f = (struct oboe_aud_factory*)ff;
    return f->dev_count;
}

/* API: Get device info. */
static pj_status_t oboe_get_dev_info(pjmedia_aud_dev_factory *ff,
                                        unsigned index,
                                        pjmedia_aud_dev_info *info)
{
    struct oboe_aud_factory *f = (struct oboe_aud_factory*)ff;

    PJ_ASSERT_RETURN(index < f->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &f->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: fill in with default parameter. */
static pj_status_t oboe_default_param(pjmedia_aud_dev_factory *ff,
                                         unsigned index,
                                         pjmedia_aud_param *param)
{
    struct oboe_aud_factory *f = (struct oboe_aud_factory*)ff;
    pjmedia_aud_dev_info adi;
    pj_status_t status;

    PJ_ASSERT_RETURN(index < f->dev_count, PJMEDIA_EAUD_INVDEV);

    status = oboe_get_dev_info(ff, index, &adi);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(param, sizeof(*param));
    if (adi.input_count && adi.output_count) {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
    } else if (adi.input_count) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (adi.output_count) {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
        return PJMEDIA_EAUD_INVDEV;
    }

    param->clock_rate = adi.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = adi.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}


/* Atomic queue (ring buffer) for single consumer & single producer.
 *
 * Producer invokes 'put(frame)' to put a frame to the back of the queue and
 * consumer invokes 'get(frame)' to get a frame from the head of the queue.
 *
 * For producer, there is write pointer 'ptrWrite' that will be incremented
 * every time a frame is queued to the back of the queue. If the queue is
 * almost full (the write pointer is right before the read pointer) the
 * producer will forcefully discard the oldest frame in the head of the
 * queue by incrementing read pointer.
 *
 * For consumer, there is read pointer 'ptrRead' that will be incremented
 * every time a frame is fetched from the head of the queue, only if the
 * pointer is not modified by producer (in case of queue full).
 */
class AtomicQueue {
public:

    AtomicQueue(unsigned max_frame_cnt, unsigned frame_size,
		const char* name_= "") :
	maxFrameCnt(max_frame_cnt), frameSize(frame_size),
	ptrWrite(0), ptrRead(0),
	buffer(NULL), name(name_)
    {
	buffer = new char[maxFrameCnt * frameSize];

	/* Surpress warning when debugging log is disabled */
	PJ_UNUSED_ARG(name);
    }

    ~AtomicQueue() {
	delete [] buffer;
    }

    /* Get a frame from the head of the queue */
    bool get(void* frame) {
        if (ptrRead == ptrWrite)
            return false;

	unsigned cur_ptr = ptrRead;
	void *p = &buffer[cur_ptr * frameSize];
	pj_memcpy(frame, p, frameSize);
	inc_ptr_read_if_not_yet(cur_ptr);

	//__android_log_print(ANDROID_LOG_INFO, name,
	//		      "GET: ptrRead=%d ptrWrite=%d\n",
	//		      ptrRead.load(), ptrWrite.load());
	return true;
    }

    /* Put a frame to the back of the queue */
    void put(void* frame) {
	unsigned cur_ptr = ptrWrite;
	void *p = &buffer[cur_ptr * frameSize];
	pj_memcpy(p, frame, frameSize);
	unsigned next_ptr = inc_ptr_write(cur_ptr);

	/* Increment read pointer if next write is overlapping (next_ptr == read ptr) */
	unsigned next_read_ptr = (next_ptr == maxFrameCnt-1)? 0 : (next_ptr+1);
	ptrRead.compare_exchange_strong(next_ptr, next_read_ptr);

	//__android_log_print(ANDROID_LOG_INFO, name,
	//		      "PUT: ptrRead=%d ptrWrite=%d\n",
	//		      ptrRead.load(), ptrWrite.load());
    }

private:

    unsigned maxFrameCnt;
    unsigned frameSize;
    std::atomic<unsigned> ptrWrite;
    std::atomic<unsigned> ptrRead;
    char *buffer;
    const char *name;

    /* Increment read pointer, only if producer not incemented it already.
     * Producer may increment the read pointer if the write pointer is
     * right before the read pointer (buffer almost full).
     */
    bool inc_ptr_read_if_not_yet(unsigned old_ptr) {
	unsigned new_ptr = (old_ptr == maxFrameCnt-1)? 0 : (old_ptr+1);
	return ptrRead.compare_exchange_strong(old_ptr, new_ptr);
    }

    /* Increment write pointer */
    unsigned inc_ptr_write(unsigned old_ptr) {
	unsigned new_ptr = (old_ptr == maxFrameCnt-1)? 0 : (old_ptr+1);
	if (ptrWrite.compare_exchange_strong(old_ptr, new_ptr))
	    return new_ptr;

        /* Should never happen */
	pj_assert(!"There is more than one producer!");
	return old_ptr;
    }

    AtomicQueue() {}
};


/* Interface to Oboe */
class MyOboeEngine : oboe::AudioStreamDataCallback,
		     oboe::AudioStreamErrorCallback
{
public:
    MyOboeEngine(struct oboe_aud_stream *stream_, pjmedia_dir dir_)
    : stream(stream_), dir(dir_), oboe_stream(NULL), dir_st(NULL),
      thread(NULL), thread_quit(PJ_TRUE), queue(NULL),
      err_thread_registered(false), mutex(NULL)
    {
	pj_assert(dir == PJMEDIA_DIR_CAPTURE || dir == PJMEDIA_DIR_PLAYBACK);
	dir_st = (dir == PJMEDIA_DIR_CAPTURE? "capture":"playback");
	pj_set_timestamp32(&ts, 0, 0);
    }

    pj_status_t Start() {
	pj_status_t status;

	if (!mutex) {
	    status = pj_mutex_create_recursive(stream->pool, "oboe", &mutex);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(3,(THIS_FILE, status,
			     "Oboe stream %s failed creating mutex", dir_st));
		return status;
	    }
	}

	int dev_id = 0;
	oboe::AudioStreamBuilder sb;

	pj_mutex_lock(mutex);

	if (oboe_stream) {
	    pj_mutex_unlock(mutex);
	    return PJ_SUCCESS;
	}

	if (dir == PJMEDIA_DIR_CAPTURE) {
	    sb.setDirection(oboe::Direction::Input);
	    if (stream->param.rec_id >= 0 &&
		stream->param.rec_id < stream->f->dev_count)
	    {
		dev_id = stream->f->dev_info[stream->param.rec_id].id;
	    }
	} else {
	    sb.setDirection(oboe::Direction::Output);
	    if (stream->param.play_id >= 0 &&
		stream->param.play_id < stream->f->dev_count)
	    {
		dev_id = stream->f->dev_info[stream->param.play_id].id;
	    }
	}
	sb.setDeviceId(dev_id);
	sb.setSampleRate(stream->param.clock_rate);
	sb.setChannelCount(stream->param.channel_count);
	sb.setPerformanceMode(oboe::PerformanceMode::LowLatency);
	sb.setFormat(oboe::AudioFormat::I16);
	sb.setDataCallback(this);
	sb.setErrorCallback(this);
	sb.setFramesPerDataCallback(stream->param.samples_per_frame /
				    stream->param.channel_count);

	/* Somehow mic does not work on Samsung S10 (get no error and
	 * low latency, but callback is never invoked) if sample rate
	 * conversion is specified. If it is not specified (default is None),
	 * mic does not get low latency on, but it works.
	 */
	if (dir == PJMEDIA_DIR_PLAYBACK) {
	    sb.setSampleRateConversionQuality(
				oboe::SampleRateConversionQuality::High);

	    /* Also if mic is Exclusive, it won't reopen after
	     * plug/unplug headset (on Samsung S10).
	     */
	    sb.setSharingMode(oboe::SharingMode::Exclusive);
	}

	/* Create queue */
	unsigned latency = (dir == PJMEDIA_DIR_CAPTURE?
			    stream->param.input_latency_ms :
			    stream->param.output_latency_ms);
	unsigned queue_size = latency * stream->param.clock_rate *
			      stream->param.channel_count / 1000 /
			      stream->param.samples_per_frame;

	/* Normalize queue size to be in range of 3-10 frames */
	if (queue_size < 3) queue_size = 3;
	if (queue_size > 10) queue_size = 10;

	PJ_LOG(3,(THIS_FILE,
		  "Oboe stream %s queue size=%d frames (latency=%d ms)",
		  dir_st, queue_size, latency));

	queue = new AtomicQueue(queue_size, stream->param.samples_per_frame*2,
				dir_st);

	/* Create semaphore */
        if (sem_init(&sem, 0, 0) != 0) {
	    pj_mutex_unlock(mutex);
	    return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
	}

	/* Create thread */
	thread_quit = PJ_FALSE;
        status = pj_thread_create(stream->pool, "android_oboe",
                                  &AudioThread, this, 0, 0, &thread);
        if (status != PJ_SUCCESS) {
	    pj_mutex_unlock(mutex);
            return status;
	}

	/* Open & start oboe stream */
	oboe::Result result = sb.openStream(&oboe_stream);
	if (result != oboe::Result::OK) {
	    PJ_LOG(3,(THIS_FILE,
		      "Oboe stream %s open failed (err=%d/%s)",
		      dir_st, result, oboe::convertToText(result)));
	    pj_mutex_unlock(mutex);
	    return PJMEDIA_EAUD_SYSERR;
	}

	result = oboe_stream->requestStart();
	if (result != oboe::Result::OK) {
	    PJ_LOG(3,(THIS_FILE,
		      "Oboe stream %s start failed (err=%d/%s)",
		      dir_st, result, oboe::convertToText(result)));
	    pj_mutex_unlock(mutex);
	    return PJMEDIA_EAUD_SYSERR;
	}

	PJ_LOG(4, (THIS_FILE,
		"Oboe stream %s started, "
		"id=%d, clock_rate=%d, channel_count=%d, "
		"samples_per_frame=%d (%dms), "
		"API=%d/%s, exclusive=%s, low latency=%s, "
		"size per callback=%d, buffer capacity=%d, burst size=%d",
		dir_st,
		stream->param.play_id,
		stream->param.clock_rate,
		stream->param.channel_count,
		stream->param.samples_per_frame,
		stream->param.samples_per_frame * 1000 /
		       stream->param.clock_rate,
		oboe_stream->getAudioApi(),
		(oboe_stream->usesAAudio()? "AAudio":"other"),
		(oboe_stream->getSharingMode()==
			oboe::SharingMode::Exclusive? "yes":"no"),
		(oboe_stream->getPerformanceMode()==
			oboe::PerformanceMode::LowLatency? "yes":"no"),
		oboe_stream->getFramesPerDataCallback()*
			stream->param.channel_count,
		oboe_stream->getBufferCapacityInFrames(),
		oboe_stream->getFramesPerBurst()
		));

	pj_mutex_unlock(mutex);
	return PJ_SUCCESS;
    }

    void Stop() {
	/* Just return if it has not been started */
	if (!mutex || thread_quit) {
	    PJ_LOG(5, (THIS_FILE, "Oboe stream %s stop request when "
		       "already stopped.", dir_st));
	    return;
	}

	PJ_LOG(5, (THIS_FILE, "Oboe stream %s stop requested.", dir_st));

	pj_mutex_lock(mutex);

	if (thread) {
	    PJ_LOG(5,(THIS_FILE, "Oboe %s stopping thread", dir_st));
	    thread_quit = PJ_TRUE;
            sem_post(&sem);
            pj_thread_join(thread);
            pj_thread_destroy(thread);
            thread = NULL;
	}

	if (oboe_stream) {
	    PJ_LOG(5,(THIS_FILE, "Oboe %s closing stream", dir_st));
	    oboe_stream->close();
	    delete oboe_stream;
	    oboe_stream = NULL;
	}

	if (queue) {
	    PJ_LOG(5,(THIS_FILE, "Oboe %s deleting queue", dir_st));
	    delete queue;
	    queue = NULL;
	}

	sem_destroy(&sem);

	pj_mutex_unlock(mutex);

	PJ_LOG(4, (THIS_FILE, "Oboe stream %s stopped.", dir_st));
    }

    /* Oboe callback, here let's just use Android native mutex & semaphore
     * so we don't need to register the thread to PJLIB.
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream,
					  void *audioData,
					  int32_t numFrames)
    {
	if (dir == PJMEDIA_DIR_CAPTURE) {
	    /* Put the audio frame to queue */
	    queue->put(audioData);
	} else {
	    /* Get audio frame from queue */
	    if (!queue->get(audioData)) {
		pj_bzero(audioData, stream->param.samples_per_frame*2);
		__android_log_write(ANDROID_LOG_WARN, THIS_FILE,
			"Oboe playback got an empty queue");
	    }
	}

	sem_post(&sem);

	return (thread_quit? oboe::DataCallbackResult::Stop :
			     oboe::DataCallbackResult::Continue);
    }

    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result result)
    {
	__android_log_print(ANDROID_LOG_INFO, THIS_FILE,
			    "Oboe %s got onErrorAfterClose(%d)",
			    dir_st, result);

	/* Register callback thread */
	if (!err_thread_registered || !pj_thread_is_registered())
	{
	    pj_thread_t* tmp_thread;
	    pj_bzero(err_thread_desc, sizeof(pj_thread_desc));
	    pj_thread_register("oboe_err_thread", err_thread_desc,
			       &tmp_thread);
	    err_thread_registered = true;
	}

	/* Just try to restart */
	pj_mutex_lock(mutex);

	/* Make sure stop request has not been made */
	if (!thread_quit) {
	    PJ_LOG(3,(THIS_FILE,
		      "Oboe stream %s error (%d/%s), "
		      "trying to restart stream..",
		      dir_st, result, oboe::convertToText(result)));

	    Stop();
	    Start();
	}

	pj_mutex_unlock(mutex);
    }

    ~MyOboeEngine() {
	/* Oboe should have been stopped before destroying the engine.
	 * As stopping it here (below) may cause undefined behaviour when
	 * there is race condition against restart in onErrorAfterClose().
	 */
	pj_assert(thread_quit == PJ_TRUE);

	/* Forcefully stopping Oboe anyway */
	Stop();

	/* Try to trigger context switch in case onErrorAfterClose() is
	 * waiting for mutex.
	 */
	pj_thread_sleep(1);

	if (mutex)
	    pj_mutex_destroy(mutex);
    }

private:

    static int AudioThread(void *arg) {
	MyOboeEngine *this_ = (MyOboeEngine*)arg;
	struct oboe_aud_stream *stream = this_->stream;
	pj_int16_t *tmp_buf;
	unsigned ts_inc;
	pj_status_t status;

	/* Try to bump up the thread priority */
	enum {
	    THREAD_PRIORITY_AUDIO = -16,
	    THREAD_PRIORITY_URGENT_AUDIO = -19
	};
	status = pj_thread_set_prio(NULL, THREAD_PRIORITY_URGENT_AUDIO);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(3,(THIS_FILE, status,
			 "Warning: Oboe %s failed increasing thread priority",
			 this_->dir_st));
	}

	tmp_buf = new pj_int16_t[this_->stream->param.samples_per_frame]();
	ts_inc = stream->param.samples_per_frame/stream->param.channel_count;

	/* Queue a silent frame to playback buffer */
	if (this_->dir == PJMEDIA_DIR_PLAYBACK) {
	    this_->queue->put(tmp_buf);
	}

	while (1) {
	    sem_wait(&this_->sem);
	    if (this_->thread_quit)
		break;

	    if (this_->dir == PJMEDIA_DIR_CAPTURE) {
		unsigned cnt = 0;
		bool stop_stream = false;

		/* Read audio frames from Oboe */
		while (this_->queue->get(tmp_buf)) {
		    /* Send audio frame to app via callback rec_cb() */
		    pjmedia_frame frame;
		    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		    frame.size =  stream->param.samples_per_frame * 2;
		    frame.bit_info = 0;
		    frame.buf = (void *)tmp_buf;
		    frame.timestamp = this_->ts;
		    status = (*stream->rec_cb)(stream->user_data, &frame);
		    if (status != PJ_SUCCESS) {
		        /* App wants to stop audio dev stream */
			stop_stream = true;
		        break;
		    }

		    /* Increment timestamp */
		    pj_add_timestamp32(&this_->ts, ts_inc);
		    ++cnt;
		}

		if (stop_stream)
		    break;

		/* Print log for debugging purpose */
		if (cnt == 0) {
		    PJ_LOG(5,(THIS_FILE, "Oboe %s got an empty queue",
			      this_->dir_st));
		} else if (cnt > 1) {
		    PJ_LOG(5,(THIS_FILE, "Oboe %s got a burst of %d frames",
			      this_->dir_st, cnt));
		}
	    } else {
		/* Get audio frame from app via callback play_cb() */
		pjmedia_frame frame;
		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame.size =  stream->param.samples_per_frame * 2;
		frame.bit_info = 0;
		frame.buf = (void *)tmp_buf;
		frame.timestamp = this_->ts;
		status = (*stream->play_cb)(stream->user_data, &frame);

		/* Send audio frame to Oboe */
		if (status == PJ_SUCCESS) {
		    this_->queue->put(tmp_buf);
		} else {
		    /* App wants to stop audio dev stream */
		    break;
		}

		/* Increment timestamp */
		pj_add_timestamp32(&this_->ts, ts_inc);
	    }
	}

	delete [] tmp_buf;

	PJ_LOG(5,(THIS_FILE, "Oboe %s thread stopped", this_->dir_st));
	return 0;
    }

private:
    struct oboe_aud_stream	*stream;
    pjmedia_dir			 dir;
    oboe::AudioStream		*oboe_stream;
    const char			*dir_st;
    pj_thread_t			*thread;
    volatile pj_bool_t		 thread_quit;
    sem_t			 sem;
    AtomicQueue			*queue;
    pj_timestamp		 ts;
    bool			 err_thread_registered;
    pj_thread_desc		 err_thread_desc;
    pj_mutex_t			*mutex;

};


/* API: create stream */
static pj_status_t oboe_create_stream(pjmedia_aud_dev_factory *f,
				      const pjmedia_aud_param *param,
				      pjmedia_aud_rec_cb rec_cb,
				      pjmedia_aud_play_cb play_cb,
				      void *user_data,
				      pjmedia_aud_stream **p_aud_strm)
{
    struct oboe_aud_factory *pa = (struct oboe_aud_factory*)f;
    pj_pool_t *pool;
    struct oboe_aud_stream *stream;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(param->channel_count >= 1 && param->channel_count <= 2,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(param->bits_per_sample==8 || param->bits_per_sample==16,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(play_cb && rec_cb && p_aud_strm, PJ_EINVAL);

    pool = pj_pool_create(pa->pf, "oboestrm", 1024, 1024, NULL);
    if (!pool)
        return PJ_ENOMEM;

    PJ_LOG(4, (THIS_FILE, "Creating Oboe stream"));

    stream = PJ_POOL_ZALLOC_T(pool, struct oboe_aud_stream);
    stream->pool = pool;
    stream->f = pa;
    pj_strdup2_with_null(pool, &stream->name, "Oboe stream");
    stream->dir = param->dir;
    pj_memcpy(&stream->param, param, sizeof(*param));
    stream->user_data = user_data;
    stream->rec_cb = rec_cb;
    stream->play_cb = play_cb;

    if (param->dir & PJMEDIA_DIR_CAPTURE) {
	stream->rec_engine = new MyOboeEngine(stream, PJMEDIA_DIR_CAPTURE);
	if (!stream->rec_engine) {
	    status = PJ_ENOMEM;
	    goto on_error;
	}
    }

    if (stream->dir & PJMEDIA_DIR_PLAYBACK) {
	stream->play_engine = new MyOboeEngine(stream, PJMEDIA_DIR_PLAYBACK);
	if (!stream->play_engine) {
	    status = PJ_ENOMEM;
	    goto on_error;
	}
    }

    /* Done */
    stream->base.op = &oboe_strm_op;
    *p_aud_strm = &stream->base;

    return PJ_SUCCESS;

on_error:
    strm_destroy(&stream->base);
    return status;
}

/* API: Get stream parameters */
static pj_status_t strm_get_param(pjmedia_aud_stream *s,
                                  pjmedia_aud_param *pi)
{
    struct oboe_aud_stream *strm = (struct oboe_aud_stream*)s;
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    pj_memcpy(pi, &strm->param, sizeof(*pi));

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t strm_get_cap(pjmedia_aud_stream *s,
                                pjmedia_aud_dev_cap cap,
                                void *pval)
{
    struct oboe_aud_stream *strm = (struct oboe_aud_stream*)s;
    pj_status_t status = PJMEDIA_EAUD_INVCAP;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    return status;
}

/* API: set capability */
static pj_status_t strm_set_cap(pjmedia_aud_stream *s,
                                pjmedia_aud_dev_cap cap,
                                const void *value)
{
    struct oboe_aud_stream *strm = (struct oboe_aud_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && value, PJ_EINVAL);

    return PJMEDIA_EAUD_INVCAP;
}

/* API: start stream. */
static pj_status_t strm_start(pjmedia_aud_stream *s)
{
    struct oboe_aud_stream *stream = (struct oboe_aud_stream*)s;
    pj_status_t status;

    if (stream->running)
	return PJ_SUCCESS;

    if (stream->rec_engine) {
	status = stream->rec_engine->Start();
	if (status != PJ_SUCCESS)
	    goto on_error;
    }
    if (stream->play_engine) {
	status = stream->play_engine->Start();
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    stream->running = PJ_TRUE;
    PJ_LOG(4, (THIS_FILE, "Oboe stream started"));

    return PJ_SUCCESS;

on_error:
    if (stream->rec_engine)
	stream->rec_engine->Stop();
    if (stream->play_engine)
	stream->play_engine->Stop();

    PJ_LOG(4, (THIS_FILE, "Failed starting Oboe stream"));

    return status;
}

/* API: stop stream. */
static pj_status_t strm_stop(pjmedia_aud_stream *s)
{
    struct oboe_aud_stream *stream = (struct oboe_aud_stream*)s;

    if (!stream->running)
        return PJ_SUCCESS;

    stream->running = PJ_FALSE;

    if (stream->rec_engine)
	stream->rec_engine->Stop();
    if (stream->play_engine)
	stream->play_engine->Stop();

    PJ_LOG(4,(THIS_FILE, "Oboe stream stopped"));

    return PJ_SUCCESS;
}

/* API: destroy stream. */
static pj_status_t strm_destroy(pjmedia_aud_stream *s)
{
    struct oboe_aud_stream *stream = (struct oboe_aud_stream*)s;

    PJ_LOG(4,(THIS_FILE, "Destroying Oboe stream..."));

    /* Stop the stream */
    strm_stop(s);

    if (stream->rec_engine) {
	delete stream->rec_engine;
	stream->rec_engine = NULL;
    }
    if (stream->play_engine) {
	delete stream->play_engine;
	stream->play_engine = NULL;
    }

    pj_pool_release(stream->pool);
    PJ_LOG(4, (THIS_FILE, "Oboe stream destroyed"));

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_AUDIO_DEV_HAS_OBOE */
