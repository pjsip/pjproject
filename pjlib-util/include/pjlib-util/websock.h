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
#ifndef __PJLIB_UTIL_WEBSOCK_H__
#define __PJLIB_UTIL_WEBSOCK_H__

/**
 * @file websock.h
 * @brief WebSocket client (RFC 6455)
 */
#include <pjlib-util/http_client.h>
#include <pjlib-util/types.h>
#include <pj/activesock.h>
#include <pj/lock.h>
#include <pj/ssl_sock.h>
#include <pj/timer.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_WEBSOCK WebSocket Client
 * @ingroup PJ_PROTOCOLS
 * @{
 *
 * @section websock_intro Introduction
 *
 * This is an @b experimental WebSocket client implementation (RFC 6455),
 * created as initial work toward real-time AI service connectivity
 * (e.g. speech-to-text, LLM streaming). It supports both ws:// (plain)
 * and wss:// (TLS) connections.
 *
 * @section websock_limits Current Limitations
 *
 * - Client mode only (no server/accept).
 * - DNS resolution uses blocking pj_getaddrinfo().
 * - No automatic reconnection.
 * - Single pending send at a time (returns PJ_EBUSY if busy).
 */

/** Maximum number of custom HTTP headers for the handshake. */
#define PJ_WEBSOCK_MAX_HEADERS      16

/** Default maximum receive message size (256 KB). */
#define PJ_WEBSOCK_MAX_RX_MSG_SIZE  (256 * 1024)

/** Default receive buffer size (16 KB). */
#define PJ_WEBSOCK_RX_BUF_SIZE      (16 * 1024)

/** Default transmit buffer size (64 KB). */
#define PJ_WEBSOCK_TX_BUF_SIZE      (64 * 1024)

/** Default ping interval in seconds. 0 to disable. */
#define PJ_WEBSOCK_DEFAULT_PING_INTERVAL  30

/**
 * WebSocket opcodes.
 */
typedef enum pj_websock_opcode
{
    PJ_WEBSOCK_OP_CONT      = 0x0,  /**< Continuation frame     */
    PJ_WEBSOCK_OP_TEXT      = 0x1,  /**< Text frame             */
    PJ_WEBSOCK_OP_BIN       = 0x2,  /**< Binary frame           */
    PJ_WEBSOCK_OP_CLOSE     = 0x8,  /**< Connection close       */
    PJ_WEBSOCK_OP_PING      = 0x9,  /**< Ping                   */
    PJ_WEBSOCK_OP_PONG      = 0xA   /**< Pong                   */
} pj_websock_opcode;

/**
 * WebSocket ready states.
 */
typedef enum pj_websock_readystate
{
    PJ_WEBSOCK_STATE_CONNECTING,     /**< Connecting/handshaking */
    PJ_WEBSOCK_STATE_OPEN,           /**< Connected and ready    */
    PJ_WEBSOCK_STATE_CLOSING,        /**< Close handshake        */
    PJ_WEBSOCK_STATE_CLOSED          /**< Connection closed      */
} pj_websock_readystate;

/** Forward declaration. */
typedef struct pj_websock pj_websock;

/**
 * WebSocket callbacks.
 */
typedef struct pj_websock_cb
{
    /**
     * Called when the WebSocket connection is established (handshake
     * completed successfully) or when the connection attempt fails.
     *
     * @param ws        The WebSocket instance.
     * @param status    PJ_SUCCESS on success, or error code on failure.
     */
    void (*on_connect)(pj_websock *ws, pj_status_t status);

    /**
     * Called when a complete message (possibly reassembled from fragments)
     * has been received.
     *
     * @param ws        The WebSocket instance.
     * @param opcode    Message type (PJ_WEBSOCK_OP_TEXT or PJ_WEBSOCK_OP_BIN).
     * @param data      Pointer to the message payload.
     * @param len       Length of the message payload.
     */
    void (*on_rx_msg)(pj_websock *ws, pj_websock_opcode opcode,
                      const void *data, pj_size_t len);

    /**
     * Called when the WebSocket connection has been closed, either by
     * the remote peer, locally, or due to a transport error.
     *
     * @param ws            The WebSocket instance.
     * @param status_code   WebSocket close code (1000=normal), or 0 if
     *                      closed due to transport error.
     * @param reason        Close reason string, may be empty.
     */
    void (*on_close)(pj_websock *ws, pj_uint16_t status_code,
                     const pj_str_t *reason);

} pj_websock_cb;

/**
 * Definition of WebSocket creation parameters.
 */
typedef struct pj_websock_param
{
    /**
     * Specify the ioqueue to use for asynchronous socket operations.
     */
    pj_ioqueue_t        *ioqueue;

    /**
     * Specify the timer heap to use for ping and timeout timers.
     */
    pj_timer_heap_t     *timer_heap;

    /**
     * Specify WebSocket callbacks, see #pj_websock_cb.
     */
    pj_websock_cb        cb;

    /**
     * Specify WebSocket user data.
     */
    void                *user_data;

    /**
     * Specify the maximum receive message size in bytes. Incoming messages
     * larger than this will cause a protocol error.
     *
     * Default value is PJ_WEBSOCK_MAX_RX_MSG_SIZE (256 KB).
     */
    pj_size_t            max_rx_msg_size;

    /**
     * Specify the SSL/TLS parameters for wss:// connections. Set to NULL
     * to use defaults. Ignored for ws:// connections. Only SSL-specific
     * fields (ciphers, certificates, verify mode, etc.) are used;
     * infrastructure fields (ioqueue, timer_heap, cb, user_data) are
     * overridden internally by the WebSocket layer.
     */
    pj_ssl_sock_param   *ssl_param;

    /**
     * Specify the ping interval in seconds. Set to 0 to disable automatic
     * ping.
     *
     * Default value is PJ_WEBSOCK_DEFAULT_PING_INTERVAL (30 seconds).
     */
    unsigned             ping_interval;

    /**
     * Optional group lock to be assigned to the WebSocket instance. If
     * NULL, the WebSocket will create its own group lock. If supplied,
     * the WebSocket will add a reference and register a destroy handler.
     * The application may supply a shared group lock to coordinate
     * lifetime with other objects (e.g. a media port).
     */
    pj_grp_lock_t       *grp_lock;

} pj_websock_param;

/**
 * Definition of WebSocket connect parameters. These parameters are
 * specific to the client-side connect operation.
 */
typedef struct pj_websock_connect_param
{
    /**
     * Specify additional HTTP headers to include in the WebSocket
     * handshake request (e.g. Authorization, custom headers).
     */
    pj_http_headers      extra_hdr;

    /**
     * Specify the subprotocol to request via the Sec-WebSocket-Protocol
     * header. Set to empty string to omit.
     */
    pj_str_t             subprotocol;

} pj_websock_connect_param;

/**
 * Initialize WebSocket parameters with default values.
 *
 * @param param     The parameters to initialize.
 */
PJ_DECL(void) pj_websock_param_default(pj_websock_param *param);

/**
 * Initialize WebSocket connect parameters with default values.
 *
 * @param cparam    The connect parameters to initialize.
 */
PJ_DECL(void) pj_websock_connect_param_default(
                                        pj_websock_connect_param *cparam);

/**
 * Create a WebSocket instance. The WebSocket will create its own memory
 * pool from the pool factory obtained from the supplied pool.
 *
 * @param pool      Pool to use for obtaining the pool factory.
 * @param param     Creation parameters.
 * @param p_ws      Pointer to receive the WebSocket instance.
 *
 * @return          PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_websock_create(pj_pool_t *pool,
                                       const pj_websock_param *param,
                                       pj_websock **p_ws);

/**
 * Connect to a WebSocket server. The URL scheme determines whether
 * a plain (ws://) or TLS (wss://) connection is used. The handshake
 * is performed asynchronously; the on_connect callback will be called
 * when complete.
 *
 * @param ws        The WebSocket instance.
 * @param url       WebSocket URL (ws:// or wss://).
 * @param cparam    Optional connect parameters (extra headers,
 *                  subprotocol). May be NULL.
 *
 * @return          PJ_SUCCESS if the connection attempt was started
 *                  successfully, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_websock_connect(
                                pj_websock *ws,
                                const pj_str_t *url,
                                const pj_websock_connect_param *cparam);

/**
 * Send a WebSocket message.
 *
 * @param ws        The WebSocket instance.
 * @param opcode    Frame type: PJ_WEBSOCK_OP_TEXT or PJ_WEBSOCK_OP_BIN.
 * @param data      Pointer to the payload data.
 * @param len       Length of the payload data.
 *
 * @return          PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_websock_send(pj_websock *ws,
                                     pj_websock_opcode opcode,
                                     const void *data,
                                     pj_size_t len);

/**
 * Initiate a graceful close of the WebSocket connection.
 *
 * @param ws            The WebSocket instance.
 * @param status_code   WebSocket close status code (e.g. 1000 for normal
 *                      closure). See RFC 6455 Section 7.4.
 * @param reason        Optional close reason string. May be NULL.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_websock_close(pj_websock *ws,
                                      pj_uint16_t status_code,
                                      const pj_str_t *reason);

/**
 * Get the current ready state of the WebSocket.
 *
 * @param ws        The WebSocket instance.
 *
 * @return          The current ready state.
 */
PJ_DECL(pj_websock_readystate) pj_websock_get_state(const pj_websock *ws);

/**
 * Get the user data associated with the WebSocket instance.
 *
 * @param ws        The WebSocket instance.
 *
 * @return          The user data pointer.
 */
PJ_DECL(void*) pj_websock_get_user_data(const pj_websock *ws);

/**
 * Set the user data for the WebSocket instance.
 *
 * @param ws        The WebSocket instance.
 * @param user_data The user data pointer.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_websock_set_user_data(pj_websock *ws,
                                              void *user_data);

/**
 * Destroy the WebSocket instance and release resources. If the connection
 * is still open, it will be closed abruptly (no close handshake).
 *
 * @param ws        The WebSocket instance.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_websock_destroy(pj_websock *ws);

/**
 * @}
 */

PJ_END_DECL

#endif /* __PJLIB_UTIL_WEBSOCK_H__ */
