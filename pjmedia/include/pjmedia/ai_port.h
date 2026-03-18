/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_AI_PORT_H__
#define __PJMEDIA_AI_PORT_H__

/**
 * @file ai_port.h
 * @brief AI media port for real-time AI service connectivity.
 */
#include <pjmedia/port.h>
#include <pjlib-util/websock.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJMEDIA_AI_PORT AI Media Port
 * @ingroup PJMEDIA_PORT
 * @brief Media port that bridges conference bridge audio to real-time AI
 * services over WebSocket.
 * @{
 *
 * This is an @b experimental media port that connects the PJMEDIA conference
 * bridge to real-time AI services (e.g. OpenAI Realtime API) via WebSocket.
 * It uses a pluggable backend abstraction since each vendor uses proprietary
 * JSON events over RFC 6455.
 *
 * Audio from the conference bridge (put_frame) is encoded (e.g. base64 JSON)
 * and sent over WebSocket. Audio received from the AI service is decoded
 * and returned via get_frame. The port operates at the backend's native
 * clock rate; the conference bridge handles any resampling.
 */

/** Forward declarations. */
typedef struct pjmedia_ai_port pjmedia_ai_port;
typedef struct pjmedia_ai_backend pjmedia_ai_backend;

/**
 * AI port event types.
 */
typedef enum pjmedia_ai_event_type
{
    /** AI service connection established. */
    PJMEDIA_AI_EVENT_CONNECTED,

    /** AI service connection lost or closed. */
    PJMEDIA_AI_EVENT_DISCONNECTED,

    /** Transcript text received from the AI service. */
    PJMEDIA_AI_EVENT_TRANSCRIPT,

    /** AI response generation started. */
    PJMEDIA_AI_EVENT_RESPONSE_START,

    /** AI response generation completed. */
    PJMEDIA_AI_EVENT_RESPONSE_DONE,

    /** Speech detected in input audio (VAD). */
    PJMEDIA_AI_EVENT_SPEECH_STARTED,

    /** End of speech detected in input audio (VAD). */
    PJMEDIA_AI_EVENT_SPEECH_STOPPED

} pjmedia_ai_event_type;

/**
 * AI port event data delivered to the application callback.
 */
typedef struct pjmedia_ai_event
{
    /**
     * Specify the event type.
     */
    pjmedia_ai_event_type   type;

    /**
     * Specify the status code. PJ_SUCCESS for informational events,
     * error code for DISCONNECTED.
     */
    pj_status_t             status;

    /**
     * Text payload (transcript, etc). Only valid for TRANSCRIPT events.
     * The pointer is only valid for the duration of the callback.
     */
    pj_str_t                text;

} pjmedia_ai_event;

/**
 * AI port application callback.
 */
typedef struct pjmedia_ai_port_cb
{
    /**
     * Called when an AI event occurs. This may be called from the
     * ioqueue worker thread.
     *
     * @param ai_port   The AI port instance.
     * @param event     The event data.
     */
    void (*on_event)(pjmedia_ai_port *ai_port,
                     const pjmedia_ai_event *event);

} pjmedia_ai_port_cb;

/**
 * AI port creation parameters.
 */
typedef struct pjmedia_ai_port_param
{
    /**
     * Specify the ioqueue to use for WebSocket async I/O. Required.
     */
    pj_ioqueue_t            *ioqueue;

    /**
     * Specify the timer heap to use for WebSocket timers. Required.
     */
    pj_timer_heap_t         *timer_heap;

    /**
     * Specify the application callback, see #pjmedia_ai_port_cb.
     */
    pjmedia_ai_port_cb       cb;

    /**
     * Specify application user data.
     */
    void                    *user_data;

    /**
     * Specify the AI backend instance. Required. Created by a backend
     * factory (e.g. pjmedia_ai_openai_backend_create()). The port clock
     * rate, channel count, and bits per sample are taken from the
     * backend's native settings.
     *
     * The AI port takes ownership of the backend. The backend will be
     * destroyed when the port is destroyed via pjmedia_port_destroy().
     * The caller must not use or destroy the backend after passing it.
     */
    pjmedia_ai_backend      *backend;

    /**
     * Specify the ptime in milliseconds.
     *
     * Default value is 20.
     */
    unsigned                 ptime_msec;

    /**
     * Specify whether to enable client-side VAD on the TX (microphone)
     * path. When enabled, silence frames are not sent over WebSocket,
     * reducing bandwidth. The AI service's server-side VAD (if any)
     * still handles turn detection independently.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t                vad_enabled;

    /**
     * Specify the SSL/TLS parameters for wss:// connections. Set to NULL
     * to use defaults. Ignored for ws:// connections.
     *
     * The AI port makes an internal copy of this structure, so the
     * caller's pointer does not need to remain valid after
     * pjmedia_ai_port_create() returns.
     */
    pj_ssl_sock_param       *ssl_param;

} pjmedia_ai_port_param;

/**
 * Backend operation vtable.
 */
typedef struct pjmedia_ai_backend_op
{
    /**
     * Prepare connect parameters (extra headers, subprotocol).
     *
     * @param be            The backend instance.
     * @param pool          Pool for allocations.
     * @param auth_token    Authentication token (e.g. API key).
     * @param cparam        Connect parameters to fill.
     *
     * @return              PJ_SUCCESS on success.
     */
    pj_status_t (*prepare_connect)(pjmedia_ai_backend *be,
                                   pj_pool_t *pool,
                                   const pj_str_t *auth_token,
                                   pj_websock_connect_param *cparam);

    /**
     * Called when WebSocket connection is established. The backend
     * should send any session initialization messages.
     *
     * @param be            The backend instance.
     * @param ws            The connected WebSocket.
     *
     * @return              PJ_SUCCESS on success.
     */
    pj_status_t (*on_ws_connected)(pjmedia_ai_backend *be,
                                   pj_websock *ws);

    /**
     * Encode PCM audio samples into the backend's wire format
     * (e.g. base64 JSON). The output is a text message to be sent
     * via WebSocket.
     *
     * @param be            The backend instance.
     * @param samples       PCM samples at backend native rate.
     * @param sample_count  Number of samples.
     * @param buf           Output buffer for the encoded message.
     * @param buf_len       On entry, buffer capacity. On return,
     *                      actual message length.
     *
     * @return              PJ_SUCCESS on success.
     */
    pj_status_t (*encode_audio)(pjmedia_ai_backend *be,
                                const pj_int16_t *samples,
                                unsigned sample_count,
                                char *buf,
                                int *buf_len);

    /**
     * Parse a received WebSocket message. Extract audio samples
     * and/or events. Optionally return a reply message to send back.
     *
     * @param be            The backend instance.
     * @param pool          Temporary pool for parsing.
     * @param data          Received message data.
     * @param len           Message length.
     * @param audio_out     Buffer for decoded PCM samples (may be NULL
     *                      if no audio in this message).
     * @param sample_count  On entry, capacity in samples. On return,
     *                      number of decoded samples.
     * @param event         Filled with event data if a non-audio event
     *                      was received. Type set to -1 if no event.
     * @param reply         If not NULL and the backend sets *reply to
     *                      a non-NULL value, the AI port will send this
     *                      string as a text WebSocket message. The data
     *                      must remain valid until on_rx_msg returns.
     * @param reply_len     Length of the reply message. Set to 0 if
     *                      no reply is needed.
     *
     * @return              PJ_SUCCESS on success.
     */
    pj_status_t (*on_rx_msg)(pjmedia_ai_backend *be,
                             pj_pool_t *pool,
                             const void *data,
                             pj_size_t len,
                             pj_int16_t *audio_out,
                             unsigned *sample_count,
                             pjmedia_ai_event *event,
                             const char **reply,
                             pj_size_t *reply_len);

    /**
     * Destroy the backend and release resources.
     *
     * @param be            The backend instance.
     *
     * @return              PJ_SUCCESS on success.
     */
    pj_status_t (*destroy)(pjmedia_ai_backend *be);

} pjmedia_ai_backend_op;

/**
 * AI backend base structure. Backend implementations embed this
 * as the first member.
 */
struct pjmedia_ai_backend
{
    /**
     * Specify the backend operation vtable.
     */
    const pjmedia_ai_backend_op *op;

    /**
     * Specify the native clock rate of the AI service
     * (e.g. 24000 for OpenAI).
     */
    unsigned                 native_clock_rate;

    /**
     * Specify the native channel count.
     */
    unsigned                 native_channel_count;

    /**
     * Specify the native bits per sample.
     */
    unsigned                 native_bits_per_sample;

    /**
     * Specify opaque backend-specific data.
     */
    void                    *backend_data;
};

/**
 * Initialize AI port parameters with default values.
 *
 * @param param     The parameters to initialize.
 */
PJ_DECL(void) pjmedia_ai_port_param_default(
                                        pjmedia_ai_port_param *param);

/**
 * Create an AI media port.
 *
 * @param pool      Pool for initial allocations. The AI port will create
 *                  its own internal pool.
 * @param param     Creation parameters.
 * @param p_ai_port Pointer to receive the AI port instance.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_ai_port_create(
                                        pj_pool_t *pool,
                                        const pjmedia_ai_port_param *param,
                                        pjmedia_ai_port **p_ai_port);

/**
 * Get the pjmedia_port interface for connecting to the conference bridge.
 *
 * @param ai_port   The AI port instance.
 *
 * @return          The media port, or NULL on error.
 */
PJ_DECL(pjmedia_port*) pjmedia_ai_port_get_port(pjmedia_ai_port *ai_port);

/**
 * Connect to the AI service asynchronously. The on_event callback will
 * be called with CONNECTED or DISCONNECTED when complete.
 *
 * @param ai_port       The AI port instance.
 * @param url           WebSocket URL (ws:// or wss://).
 * @param auth_token    Authentication token (e.g. API key).
 *
 * @return              PJ_SUCCESS if the connect was initiated.
 */
PJ_DECL(pj_status_t) pjmedia_ai_port_connect(
                                        pjmedia_ai_port *ai_port,
                                        const pj_str_t *url,
                                        const pj_str_t *auth_token);

/**
 * Disconnect from the AI service gracefully.
 *
 * @param ai_port   The AI port instance.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_ai_port_disconnect(pjmedia_ai_port *ai_port);

/**
 * Get the user data associated with the AI port.
 *
 * @param ai_port   The AI port instance.
 *
 * @return          The user data pointer.
 */
PJ_DECL(void*) pjmedia_ai_port_get_user_data(pjmedia_ai_port *ai_port);

/**
 * Create an OpenAI Realtime API backend.
 *
 * @param pool          Pool for allocations.
 * @param p_backend     Pointer to receive the backend instance.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_ai_openai_backend_create(
                                        pj_pool_t *pool,
                                        pjmedia_ai_backend **p_backend);

/**
 * @}
 */

PJ_END_DECL

#endif /* __PJMEDIA_AI_PORT_H__ */
