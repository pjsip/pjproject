/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_MEDIAMGR_H__
#define __PJMEDIA_MEDIAMGR_H__


/**
 * @file endpoint.h
 * @brief Media endpoint.
 */
/**
 * @defgroup PJMED_ENDPT The Endpoint
 * @{
 *
 * The media endpoint acts as placeholder for endpoint capabilities. Each 
 * media endpoint will have a codec manager to manage list of codecs installed
 * in the endpoint and a sound device factory.
 *
 * A reference to media endpoint instance is required when application wants
 * to create a media session (#pjmedia_session_create()).
 */

#include <pjmedia/codec.h>
#include <pjmedia/sdp.h>
#include <pjmedia/transport.h>
#include <pjmedia-audiodev/audiodev.h>


PJ_BEGIN_DECL

/**
 * This enumeration describes various flags that can be set or retrieved in
 * the media endpoint, by using pjmedia_endpt_set_flag() and
 * pjmedia_endpt_get_flag() respectively.
 */
typedef enum pjmedia_endpt_flag
{
    /**
     * This flag controls whether telephony-event should be offered in SDP.
     * Value is boolean.
     */
    PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG

} pjmedia_endpt_flag;

/**
 * This structure specifies various settings that can be passed when creating
 * audio/video sdp.
 */
typedef struct pjmedia_endpt_create_sdp_param
{
    /**
     * Direction of the media.
     *
     * Default: PJMEDIA_DIR_ENCODING_DECODING
     */
    pjmedia_dir dir;

} pjmedia_endpt_create_sdp_param;

/**
 * Type of callback to register to pjmedia_endpt_atexit().
 */
typedef void (*pjmedia_endpt_exit_callback)(pjmedia_endpt *endpt);


/**
 * Call this function to initialize \a pjmedia_endpt_create_sdp_param with default 
 * values.
 *
 * @param param     The param to be initialized.
 */
PJ_DECL(void)
pjmedia_endpt_create_sdp_param_default(pjmedia_endpt_create_sdp_param *param);

/**
 * Create an instance of media endpoint.
 *
 * @param pf            Pool factory, which will be used by the media endpoint
 *                      throughout its lifetime.
 * @param ioqueue       Optional ioqueue instance to be registered to the 
 *                      endpoint. The ioqueue instance is used to poll all RTP
 *                      and RTCP sockets. If this argument is NULL, the 
 *                      endpoint will create an internal ioqueue instance.
 * @param worker_cnt    Specify the number of worker threads to be created
 *                      to poll the ioqueue.
 * @param p_endpt       Pointer to receive the endpoint instance.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_create2(pj_pool_factory *pf,
                                           pj_ioqueue_t *ioqueue,
                                           unsigned worker_cnt,
                                           pjmedia_endpt **p_endpt);

/**
 * Create an instance of media endpoint and initialize audio subsystem.
 *
 * @param pf            Pool factory, which will be used by the media endpoint
 *                      throughout its lifetime.
 * @param ioqueue       Optional ioqueue instance to be registered to the 
 *                      endpoint. The ioqueue instance is used to poll all RTP
 *                      and RTCP sockets. If this argument is NULL, the 
 *                      endpoint will create an internal ioqueue instance.
 * @param worker_cnt    Specify the number of worker threads to be created
 *                      to poll the ioqueue.
 * @param p_endpt       Pointer to receive the endpoint instance.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_endpt_create(pj_pool_factory *pf,
                                            pj_ioqueue_t *ioqueue,
                                            unsigned worker_cnt,
                                            pjmedia_endpt **p_endpt)
{
    /* This function is inlined to avoid build problem due to circular
     * dependency, i.e: this function prevents pjmedia's dependency on
     * pjmedia-audiodev.
     */

    pj_status_t status;

    /* Sound */
    status = pjmedia_aud_subsys_init(pf);
    if (status != PJ_SUCCESS)
        return status;

    status = pjmedia_endpt_create2(pf, ioqueue, worker_cnt, p_endpt);
    if (status != PJ_SUCCESS) {
        pjmedia_aud_subsys_shutdown();
    }
    
    return status;
}

/**
 * Destroy media endpoint instance.
 *
 * @param endpt         Media endpoint instance.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_destroy2(pjmedia_endpt *endpt);

/**
 * Destroy media endpoint instance and shutdown audio subsystem.
 *
 * @param endpt         Media endpoint instance.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_endpt_destroy(pjmedia_endpt *endpt)
{
    /* This function is inlined to avoid build problem due to circular
     * dependency, i.e: this function prevents pjmedia's dependency on
     * pjmedia-audiodev.
     */
     pj_status_t status = pjmedia_endpt_destroy2(endpt);
     pjmedia_aud_subsys_shutdown();
     return status;
}

/**
 * Change the value of a flag.
 *
 * @param endpt         Media endpoint.
 * @param flag          The flag.
 * @param value         Pointer to the value to be set.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_set_flag(pjmedia_endpt *endpt,
                                            pjmedia_endpt_flag flag,
                                            const void *value);

/**
 *  Retrieve the value of a flag.
 *
 *  @param endpt        Media endpoint.
 *  @param flag         The flag.
 *  @param value        Pointer to store the result.
 *
 *  @return             PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_get_flag(pjmedia_endpt *endpt,
                                            pjmedia_endpt_flag flag,
                                            void *value);

/**
 * Get the ioqueue instance of the media endpoint.
 *
 * @param endpt         The media endpoint instance.
 *
 * @return              The ioqueue instance of the media endpoint.
 */
PJ_DECL(pj_ioqueue_t*) pjmedia_endpt_get_ioqueue(pjmedia_endpt *endpt);


/**
 * Get the number of worker threads on the media endpoint
 *
 * @param endpt         The media endpoint instance.
 * @return              The number of worker threads on the media endpoint
 */
PJ_DECL(unsigned) pjmedia_endpt_get_thread_count(pjmedia_endpt *endpt);

/**
 * Get a reference to one of the worker threads of the media endpoint 
 *
 * @param endpt         The media endpoint instance.
 * @param index         The index of the thread: 0<= index < thread_cnt
 *
 * @return              pj_thread_t or NULL
 */
PJ_DECL(pj_thread_t*) pjmedia_endpt_get_thread(pjmedia_endpt *endpt, 
                                               unsigned index);

/**
 * Stop and destroy the worker threads of the media endpoint
 *
 * @param endpt         The media endpoint instance.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_stop_threads(pjmedia_endpt *endpt);


/**
 * Request the media endpoint to create pool.
 *
 * @param endpt         The media endpoint instance.
 * @param name          Name to be assigned to the pool.
 * @param initial       Initial pool size, in bytes.
 * @param increment     Increment size, in bytes.
 *
 * @return              Memory pool.
 */
PJ_DECL(pj_pool_t*) pjmedia_endpt_create_pool( pjmedia_endpt *endpt,
                                               const char *name,
                                               pj_size_t initial,
                                               pj_size_t increment);

/**
 * Get the codec manager instance of the media endpoint.
 *
 * @param endpt         The media endpoint instance.
 *
 * @return              The instance of codec manager belonging to
 *                      this media endpoint.
 */
PJ_DECL(pjmedia_codec_mgr*) pjmedia_endpt_get_codec_mgr(pjmedia_endpt *endpt);


/**
 * Create a SDP session description that describes the endpoint
 * capability.
 *
 * @param endpt         The media endpoint.
 * @param pool          Pool to use to create the SDP descriptor.
 * @param stream_cnt    Number of elements in the sock_info array. This
 *                      also denotes the maximum number of streams (i.e.
 *                      the "m=" lines) that will be created in the SDP.
 *                      By convention, if this value is greater than one,
 *                      the first media will be audio and the remaining
 *                      media is video.
 * @param sock_info     Array of socket transport information. One 
 *                      transport is needed for each media stream, and
 *                      each transport consists of an RTP and RTCP socket
 *                      pair.
 * @param p_sdp         Pointer to receive SDP session descriptor.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_create_sdp( pjmedia_endpt *endpt,
                                               pj_pool_t *pool,
                                               unsigned stream_cnt,
                                               const pjmedia_sock_info sock_info[],
                                               pjmedia_sdp_session **p_sdp );

/**
 * Create a "blank" SDP session description. The SDP will contain basic SDP
 * fields such as origin, time, and name, but without any media lines.
 *
 * @param endpt         The media endpoint.
 * @param pool          Pool to allocate memory from.
 * @param sess_name     Optional SDP session name, or NULL to use default
 *                      value.
 * @param origin        Address to put in the origin field.
 * @param p_sdp         Pointer to receive the created SDP session.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_create_base_sdp(pjmedia_endpt *endpt,
                                                   pj_pool_t *pool,
                                                   const pj_str_t *sess_name,
                                                   const pj_sockaddr *origin,
                                                   pjmedia_sdp_session **p_sdp);

/**
 * Create SDP media line for audio media.
 *
 * @param endpt         The media endpoint.
 * @param pool          Pool to allocate memory from.
 * @param si            Socket information.
 * @param options       Options parameter, can be NULL. If set to NULL,
 *                      default values will be used.
 * @param p_m           Pointer to receive the created SDP media.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t)
pjmedia_endpt_create_audio_sdp(pjmedia_endpt *endpt,
                               pj_pool_t *pool,
                               const pjmedia_sock_info *si,
                               const pjmedia_endpt_create_sdp_param *options,
                               pjmedia_sdp_media **p_m);

/**
 * Create SDP media line for video media.
 *
 * @param endpt         The media endpoint.
 * @param pool          Pool to allocate memory from.
 * @param si            Socket information.
 * @param options       Options parameter, can be NULL. If set to NULL,
 *                      default values will be used.
 * @param p_m           Pointer to receive the created SDP media.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t)
pjmedia_endpt_create_video_sdp(pjmedia_endpt *endpt,
                               pj_pool_t *pool,
                               const pjmedia_sock_info *si,
                               const pjmedia_endpt_create_sdp_param *options,
                               pjmedia_sdp_media **p_m);

/**
 * Dump media endpoint capabilities.
 *
 * @param endpt         The media endpoint.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_dump(pjmedia_endpt *endpt);


/**
 * Register cleanup function to be called by media endpoint when 
 * #pjmedia_endpt_destroy() is called. Note that application should not
 * use or access any endpoint resource (such as pool, ioqueue) from within
 * the callback as such resource may have been released when the callback
 * function is invoked.
 *
 * @param endpt         The media endpoint.
 * @param func          The function to be registered.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_endpt_atexit(pjmedia_endpt *endpt,
                                          pjmedia_endpt_exit_callback func);



PJ_END_DECL


/**
 * @}
 */



#endif  /* __PJMEDIA_MEDIAMGR_H__ */
