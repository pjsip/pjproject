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
#include <pjlib-util/websock.h>
#include <pjlib-util/base64.h>
#include <pjlib-util/sha1.h>
#include <pj/activesock.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/compat/socket.h>
#include <pj/errno.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/sock.h>
#include <pj/ssl_sock.h>
#include <pj/string.h>
#include <pj/timer.h>

#define THIS_FILE           "websock.c"
#define WS_GUID             "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_KEY_RAW_LEN      16
#define WS_KEY_B64_LEN      24
#define WS_ACCEPT_B64_LEN   28
#define MAX_FRAME_HDR_LEN   14  /* 2 base + 8 ext len + 4 mask */

/*
 * WebSocket instance.
 */
struct pj_websock
{
    pj_pool_t              *pool;
    pj_websock_readystate   state;
    pj_websock_cb           cb;
    void                   *user_data;

    /* Config */
    pj_ioqueue_t           *ioqueue;
    pj_timer_heap_t        *timer_heap;
    pj_size_t               max_rx_msg_size;
    unsigned                ping_interval;
    pj_ssl_sock_param      *ssl_param;

    /* Connect params (set by pj_websock_connect) */
    pj_str_t                subprotocol;
    pj_http_headers         extra_hdr;

    /* Parsed URL */
    pj_bool_t               is_ssl;
    pj_str_t                host;
    pj_uint16_t             port;
    pj_str_t                path;

    /* Transport */
    pj_ssl_sock_t          *ssl_sock;
    pj_activesock_t        *asock;
    pj_ioqueue_op_key_t     send_key;
    pj_bool_t               send_pending;

    /* Handshake */
    char                    ws_key_b64[WS_KEY_B64_LEN + 1];
    pj_bool_t               handshake_done;

    /* RX buffer */
    pj_uint8_t             *rx_buf;
    pj_size_t               rx_buf_size;

    /* Fragment reassembly buffer (for WS-level message fragmentation) */
    pj_uint8_t             *frag_buf;
    pj_size_t               frag_buf_size;
    pj_size_t               frag_len;
    pj_websock_opcode       frag_opcode;

    /* Partial frame accumulation (frame payload spans multiple reads) */
    pj_bool_t               rx_partial;
    pj_websock_opcode       rx_partial_op;
    pj_bool_t               rx_partial_fin;
    pj_size_t               rx_partial_total;
    pj_size_t               rx_partial_got;
    pj_uint8_t             *rx_partial_buf;

    /* TX buffer for data frames */
    pj_uint8_t             *tx_buf;
    pj_size_t               tx_buf_size;

    /* TX buffer for control frames (ping, pong, close).
     * Separate from tx_buf so control frames can be sent even when a
     * data send is in flight.  Max control payload is 125 bytes (RFC 6455).
     */
#   define CTL_BUF_SIZE  (125 + MAX_FRAME_HDR_LEN) /* 139 bytes */
    pj_uint8_t              ctl_buf[125 + MAX_FRAME_HDR_LEN];
    pj_ioqueue_op_key_t     ctl_send_key;
    pj_bool_t               ctl_send_pending;

    /* Ping/pong timer */
    pj_timer_entry          ping_timer;

    /* Group lock for thread safety and ref-counted destruction */
    pj_grp_lock_t          *grp_lock;
    pj_bool_t               destroying;
};


/*
 * Forward declarations.
 */
static pj_status_t send_handshake(pj_websock *ws);
static pj_status_t process_handshake_response(pj_websock *ws,
                                              pj_uint8_t *data,
                                              pj_size_t len,
                                              pj_size_t *parsed_len);
static void process_rx_frame(pj_websock *ws, pj_uint8_t *data,
                             pj_size_t len, pj_size_t *remainder);
static pj_status_t send_raw(pj_websock *ws, const void *data,
                            pj_ssize_t len);
static pj_status_t send_raw_ctl(pj_websock *ws, const void *data,
                                pj_ssize_t len);
static pj_status_t send_close_frame(pj_websock *ws,
                                    pj_uint16_t status_code,
                                    const pj_str_t *reason);
static pj_status_t send_pong(pj_websock *ws, const void *data,
                             pj_size_t len);
static pj_size_t encode_frame(pj_uint8_t *buf, pj_websock_opcode opcode,
                              pj_bool_t fin, const void *payload,
                              pj_size_t payload_len);
static void on_transport_connected(pj_websock *ws, pj_status_t status);
static void on_transport_data(pj_websock *ws, void *data, pj_size_t size,
                              pj_status_t status, pj_size_t *remainder);
static void on_timer(pj_timer_heap_t *th, pj_timer_entry *te);
static void close_transport(pj_websock *ws);
static void websock_on_destroy(void *arg);

#if PJ_HAS_SSL_SOCK
/* SSL callbacks */
static pj_bool_t ssl_on_connect_complete(pj_ssl_sock_t *ssock,
                                         pj_status_t status);
static pj_bool_t ssl_on_data_read(pj_ssl_sock_t *ssock, void *data,
                                  pj_size_t size, pj_status_t status,
                                  pj_size_t *remainder);
static pj_bool_t ssl_on_data_sent(pj_ssl_sock_t *ssock,
                                  pj_ioqueue_op_key_t *op_key,
                                  pj_ssize_t sent);
#endif

/* Activesock callbacks */
static pj_bool_t asock_on_connect_complete(pj_activesock_t *asock,
                                           pj_status_t status);
static pj_bool_t asock_on_data_read(pj_activesock_t *asock, void *data,
                                    pj_size_t size, pj_status_t status,
                                    pj_size_t *remainder);
static pj_bool_t asock_on_data_sent(pj_activesock_t *asock,
                                    pj_ioqueue_op_key_t *op_key,
                                    pj_ssize_t sent);


/*=========================================================================
 * WebSocket frame encode/decode
 */

/**
 * Encode a WebSocket frame into buf. Returns the total frame size.
 * Client frames are always masked.
 */
static pj_size_t encode_frame(pj_uint8_t *buf, pj_websock_opcode opcode,
                              pj_bool_t fin, const void *payload,
                              pj_size_t payload_len)
{
    pj_size_t pos = 0;
    pj_uint8_t mask_key[4];
    pj_size_t i;

    /* Byte 0: FIN + opcode */
    buf[pos++] = (pj_uint8_t)((fin ? 0x80 : 0) | (opcode & 0x0F));

    /* Byte 1: MASK=1 + payload length */
    if (payload_len < 126) {
        buf[pos++] = (pj_uint8_t)(0x80 | payload_len);
    } else if (payload_len <= 0xFFFF) {
        buf[pos++] = (pj_uint8_t)(0x80 | 126);
        buf[pos++] = (pj_uint8_t)((payload_len >> 8) & 0xFF);
        buf[pos++] = (pj_uint8_t)(payload_len & 0xFF);
    } else {
        buf[pos++] = (pj_uint8_t)(0x80 | 127);
        buf[pos++] = 0;
        buf[pos++] = 0;
        buf[pos++] = 0;
        buf[pos++] = 0;
        buf[pos++] = (pj_uint8_t)((payload_len >> 24) & 0xFF);
        buf[pos++] = (pj_uint8_t)((payload_len >> 16) & 0xFF);
        buf[pos++] = (pj_uint8_t)((payload_len >> 8) & 0xFF);
        buf[pos++] = (pj_uint8_t)(payload_len & 0xFF);
    }

    /* Masking key */
    mask_key[0] = (pj_uint8_t)(pj_rand() & 0xFF);
    mask_key[1] = (pj_uint8_t)(pj_rand() & 0xFF);
    mask_key[2] = (pj_uint8_t)(pj_rand() & 0xFF);
    mask_key[3] = (pj_uint8_t)(pj_rand() & 0xFF);
    pj_memcpy(&buf[pos], mask_key, 4);
    pos += 4;

    /* Masked payload */
    for (i = 0; i < payload_len; ++i) {
        buf[pos + i] = ((const pj_uint8_t*)payload)[i] ^ mask_key[i & 3];
    }

    return pos + payload_len;
}

/**
 * Parse a single frame from data. Returns the number of bytes consumed,
 * or 0 if not enough data for a complete frame.
 */
/*
 * Parse WebSocket frame header only (2-14 bytes). Does not require the full
 * payload to be present. Returns header size (bytes consumed for header +
 * mask), or 0 if insufficient data for the header itself.
 */
static pj_size_t parse_frame_header(const pj_uint8_t *data, pj_size_t len,
                                    pj_bool_t *fin,
                                    pj_websock_opcode *opcode,
                                    pj_bool_t *has_mask,
                                    pj_uint8_t mask_key[4],
                                    pj_size_t *payload_len)
{
    pj_size_t pos = 0;
    pj_uint8_t b1;
    pj_size_t plen;

    if (len < 2) return 0;

    /* Byte 0 */
    *fin = (data[0] & 0x80) != 0;
    *opcode = (pj_websock_opcode)(data[0] & 0x0F);

    /* Byte 1 */
    b1 = data[1];
    *has_mask = (b1 & 0x80) != 0;
    plen = b1 & 0x7F;
    pos = 2;

    if (plen == 126) {
        if (len < 4) return 0;
        plen = ((pj_size_t)data[2] << 8) | data[3];
        pos = 4;
    } else if (plen == 127) {
        pj_uint32_t hi, lo;
        if (len < 10) return 0;
        hi = ((pj_uint32_t)data[2] << 24) |
             ((pj_uint32_t)data[3] << 16) |
             ((pj_uint32_t)data[4] << 8) |
             (pj_uint32_t)data[5];
        lo = ((pj_uint32_t)data[6] << 24) |
             ((pj_uint32_t)data[7] << 16) |
             ((pj_uint32_t)data[8] << 8) |
             (pj_uint32_t)data[9];
        /* Reject frames with upper 32 bits set (>4GB) */
        if (hi != 0) return 0;
        /* On 32-bit platforms, lo could still overflow pj_size_t */
        if (sizeof(pj_size_t) < 8 && lo > 0x7FFFFFFFU) return 0;
        plen = (pj_size_t)lo;
        pos = 10;
    }

    if (*has_mask) {
        if (len < pos + 4) return 0;
        pj_memcpy(mask_key, &data[pos], 4);
        pos += 4;
    }

    *payload_len = plen;
    return pos;
}


/*
 * Decode a complete WebSocket frame (header + full payload).
 * Returns total consumed bytes, or 0 if the frame is incomplete.
 */
static pj_size_t decode_frame_header(const pj_uint8_t *data, pj_size_t len,
                                     pj_bool_t *fin,
                                     pj_websock_opcode *opcode,
                                     const pj_uint8_t **payload,
                                     pj_size_t *payload_len)
{
    pj_size_t hdr_len;
    pj_bool_t has_mask;
    pj_uint8_t mask[4];

    hdr_len = parse_frame_header(data, len, fin, opcode, &has_mask,
                                 mask, payload_len);
    if (hdr_len == 0)
        return 0;

    /* Need full payload in buffer */
    if (len < hdr_len + *payload_len)
        return 0;

    /* Explicit cap for static analysis (redundant with check above) */
    if (*payload_len > len)
        return 0;

    *payload = &data[hdr_len];

    if (has_mask) {
        pj_size_t i;
        for (i = 0; i < *payload_len; ++i) {
            ((pj_uint8_t*)(*payload))[i] ^= mask[i & 3];
        }
    }

    return hdr_len + *payload_len;
}


/*=========================================================================
 * Handshake
 */
static pj_status_t send_handshake(pj_websock *ws)
{
    char *buf = (char*)ws->tx_buf;
    pj_size_t buf_size = ws->tx_buf_size;
    int len;
    pj_uint8_t raw_key[WS_KEY_RAW_LEN];
    int b64_len = sizeof(ws->ws_key_b64);
    unsigned i;

    /* Generate random key */
    for (i = 0; i < WS_KEY_RAW_LEN; ++i) {
        raw_key[i] = (pj_uint8_t)(pj_rand() & 0xFF);
    }
    pj_base64_encode(raw_key, WS_KEY_RAW_LEN, ws->ws_key_b64, &b64_len);
    ws->ws_key_b64[b64_len] = '\0';

    /* Build HTTP request */
    len = pj_ansi_snprintf(buf, buf_size,
        "GET %.*s HTTP/1.1\r\n"
        "Host: %.*s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n",
        (int)ws->path.slen, ws->path.ptr,
        (int)ws->host.slen, ws->host.ptr,
        ws->port,
        ws->ws_key_b64);
    if (len < 0 || len >= (int)buf_size)
        return PJ_ETOOSMALL;

    /* Subprotocol */
    if (ws->subprotocol.slen > 0) {
        int n = pj_ansi_snprintf(buf + len, buf_size - len,
            "Sec-WebSocket-Protocol: %.*s\r\n",
            (int)ws->subprotocol.slen, ws->subprotocol.ptr);
        if (n < 0 || n >= (int)(buf_size - len))
            return PJ_ETOOSMALL;
        len += n;
    }

    /* Extra headers */
    for (i = 0; i < ws->extra_hdr.count; ++i) {
        int n = pj_ansi_snprintf(buf + len, buf_size - len,
            "%.*s: %.*s\r\n",
            (int)ws->extra_hdr.header[i].name.slen,
            ws->extra_hdr.header[i].name.ptr,
            (int)ws->extra_hdr.header[i].value.slen,
            ws->extra_hdr.header[i].value.ptr);
        if (n < 0 || n >= (int)(buf_size - len))
            return PJ_ETOOSMALL;
        len += n;
    }

    /* End of headers */
    {
        int n = pj_ansi_snprintf(buf + len, buf_size - len, "\r\n");
        if (n < 0 || n >= (int)(buf_size - len))
            return PJ_ETOOSMALL;
        len += n;
    }

    PJ_LOG(5, (THIS_FILE, "Sending WebSocket handshake (%d bytes)", len));

    return send_raw(ws, buf, len);
}

/* Helper: find a substring in a memory buffer */
static const char *find_substr(const char *buf, pj_size_t buf_len,
                               const char *needle)
{
    pj_size_t needle_len = pj_ansi_strlen(needle);
    pj_size_t i;

    if (needle_len > buf_len) return NULL;
    for (i = 0; i <= buf_len - needle_len; ++i) {
        if (pj_memcmp(buf + i, needle, needle_len) == 0)
            return buf + i;
    }
    return NULL;
}

/* Case-insensitive variant for HTTP header matching */
static const char *find_substr_i(const char *buf, pj_size_t buf_len,
                                 const char *needle)
{
    pj_size_t needle_len = pj_ansi_strlen(needle);
    pj_size_t i;

    if (needle_len > buf_len) return NULL;
    for (i = 0; i <= buf_len - needle_len; ++i) {
        if (pj_ansi_strnicmp(buf + i, needle, needle_len) == 0)
            return buf + i;
    }
    return NULL;
}

static pj_status_t process_handshake_response(pj_websock *ws,
                                              pj_uint8_t *data,
                                              pj_size_t len,
                                              pj_size_t *parsed_len)
{
    const char *hdr_end;
    pj_sha1_context sha_ctx;
    pj_uint8_t sha_hash[20];
    char expected_accept[32];
    int accept_len = sizeof(expected_accept);
    char concat[64 + sizeof(WS_GUID)];
    int concat_len;
    const char *accept_hdr;

    /* Look for end of HTTP headers */
    hdr_end = find_substr((const char*)data, len, "\r\n\r\n");
    if (!hdr_end) {
        /* Not enough data yet */
        *parsed_len = 0;
        return PJ_EPENDING;
    }
    hdr_end += 4; /* include \r\n\r\n */
    *parsed_len = hdr_end - (const char*)data;

    /* Check for "101" status */
    if (len < 12 ||
        pj_memcmp(data, "HTTP/1.1 101", 12) != 0)
    {
        PJ_LOG(2, (THIS_FILE, "WebSocket handshake failed: "
                   "non-101 response"));
        return PJ_EUNKNOWN;
    }

    /* Compute expected Sec-WebSocket-Accept value */
    concat_len = pj_ansi_snprintf(concat, sizeof(concat), "%s%s",
                                  ws->ws_key_b64, WS_GUID);
    if (concat_len < 0 || concat_len >= (int)sizeof(concat))
        return PJ_ETOOBIG;
    pj_sha1_init(&sha_ctx);
    pj_sha1_update(&sha_ctx, (pj_uint8_t*)concat, concat_len);
    pj_sha1_final(&sha_ctx, sha_hash);
    pj_base64_encode(sha_hash, 20, expected_accept, &accept_len);
    expected_accept[accept_len] = '\0';

    /* Find Sec-WebSocket-Accept header (case-insensitive per HTTP spec) */
    accept_hdr = find_substr_i((const char*)data, *parsed_len,
                               "Sec-WebSocket-Accept: ");
    if (!accept_hdr) {
        PJ_LOG(2, (THIS_FILE, "WebSocket handshake: "
                   "missing Sec-WebSocket-Accept"));
        return PJ_EUNKNOWN;
    }
    accept_hdr += 22; /* skip header name + ": " */

    /* Compare */
    if (pj_memcmp(accept_hdr, expected_accept, accept_len) != 0) {
        PJ_LOG(2, (THIS_FILE, "WebSocket handshake: "
                   "Sec-WebSocket-Accept mismatch"));
        return PJ_EUNKNOWN;
    }

    PJ_LOG(4, (THIS_FILE, "WebSocket handshake completed successfully"));
    return PJ_SUCCESS;
}


/*=========================================================================
 * Transport send helper
 */
static pj_status_t send_raw(pj_websock *ws, const void *data,
                            pj_ssize_t len)
{
    pj_status_t status;

    if (ws->send_pending) {
        PJ_LOG(4, (THIS_FILE, "Send still pending, returning EBUSY"));
        return PJ_EBUSY;
    }

#if PJ_HAS_SSL_SOCK
    if (ws->is_ssl && ws->ssl_sock) {
        status = pj_ssl_sock_send(ws->ssl_sock, &ws->send_key,
                                  data, &len, 0);
    } else
#endif
    if (ws->asock) {
        status = pj_activesock_send(ws->asock, &ws->send_key,
                                    data, &len, 0);
    } else {
        return PJ_EINVALIDOP;
    }

    if (status == PJ_EPENDING) {
        ws->send_pending = PJ_TRUE;
    }

    return (status == PJ_EPENDING) ? PJ_SUCCESS : status;
}

/* Send a control frame (ping, pong, close) using the dedicated ctl_buf.
 * This is independent of the data send path so control frames can be sent
 * even when a user data send is in flight.
 * Caller MUST hold ws->grp_lock.
 */
static pj_status_t send_raw_ctl(pj_websock *ws, const void *data,
                                pj_ssize_t len)
{
    pj_status_t status;

    if (ws->ctl_send_pending) {
        PJ_LOG(4, (THIS_FILE, "Control send still pending, returning EBUSY"));
        return PJ_EBUSY;
    }

#if PJ_HAS_SSL_SOCK
    if (ws->is_ssl && ws->ssl_sock) {
        status = pj_ssl_sock_send(ws->ssl_sock, &ws->ctl_send_key,
                                  data, &len, 0);
    } else
#endif
    if (ws->asock) {
        status = pj_activesock_send(ws->asock, &ws->ctl_send_key,
                                    data, &len, 0);
    } else {
        return PJ_EINVALIDOP;
    }

    if (status == PJ_EPENDING) {
        ws->ctl_send_pending = PJ_TRUE;
    }

    return (status == PJ_EPENDING) ? PJ_SUCCESS : status;
}


#if PJ_HAS_SSL_SOCK
/*=========================================================================
 * SSL socket callbacks
 */
static pj_bool_t ssl_on_connect_complete(pj_ssl_sock_t *ssock,
                                         pj_status_t status)
{
    pj_websock *ws = (pj_websock*)pj_ssl_sock_get_user_data(ssock);
    on_transport_connected(ws, status);
    return (status == PJ_SUCCESS) ? PJ_TRUE : PJ_FALSE;
}

static pj_bool_t ssl_on_data_read(pj_ssl_sock_t *ssock, void *data,
                                  pj_size_t size, pj_status_t status,
                                  pj_size_t *remainder)
{
    pj_websock *ws = (pj_websock*)pj_ssl_sock_get_user_data(ssock);
    on_transport_data(ws, data, size, status, remainder);
    return (ws->destroying) ? PJ_FALSE : PJ_TRUE;
}

static pj_bool_t ssl_on_data_sent(pj_ssl_sock_t *ssock,
                                  pj_ioqueue_op_key_t *op_key,
                                  pj_ssize_t sent)
{
    pj_websock *ws = (pj_websock*)pj_ssl_sock_get_user_data(ssock);
    PJ_UNUSED_ARG(sent);
    if (op_key == &ws->ctl_send_key)
        ws->ctl_send_pending = PJ_FALSE;
    else
        ws->send_pending = PJ_FALSE;
    return PJ_TRUE;
}
#endif /* PJ_HAS_SSL_SOCK */


/*=========================================================================
 * Active socket callbacks (plain TCP)
 */
static pj_bool_t asock_on_connect_complete(pj_activesock_t *asock,
                                           pj_status_t status)
{
    pj_websock *ws = (pj_websock*)pj_activesock_get_user_data(asock);
    on_transport_connected(ws, status);
    return (status == PJ_SUCCESS) ? PJ_TRUE : PJ_FALSE;
}

static pj_bool_t asock_on_data_read(pj_activesock_t *asock, void *data,
                                    pj_size_t size, pj_status_t status,
                                    pj_size_t *remainder)
{
    pj_websock *ws = (pj_websock*)pj_activesock_get_user_data(asock);
    on_transport_data(ws, data, size, status, remainder);
    return (ws->destroying) ? PJ_FALSE : PJ_TRUE;
}

static pj_bool_t asock_on_data_sent(pj_activesock_t *asock,
                                    pj_ioqueue_op_key_t *op_key,
                                    pj_ssize_t sent)
{
    pj_websock *ws = (pj_websock*)pj_activesock_get_user_data(asock);
    PJ_UNUSED_ARG(sent);
    if (op_key == &ws->ctl_send_key)
        ws->ctl_send_pending = PJ_FALSE;
    else
        ws->send_pending = PJ_FALSE;
    return PJ_TRUE;
}


/*=========================================================================
 * Common transport event handlers
 */
static void on_transport_connected(pj_websock *ws, pj_status_t status)
{
    pj_grp_lock_acquire(ws->grp_lock);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(2, (THIS_FILE, status, "WebSocket TCP connect failed"));
        ws->state = PJ_WEBSOCK_STATE_CLOSED;
        pj_grp_lock_release(ws->grp_lock);
        if (ws->cb.on_connect)
            ws->cb.on_connect(ws, status);
        return;
    }

    PJ_LOG(4, (THIS_FILE, "TCP connected, sending WebSocket handshake"));

    /* Start reading */
    {
        void *readbuf[1];
        readbuf[0] = ws->rx_buf;
#if PJ_HAS_SSL_SOCK
        if (ws->is_ssl) {
            status = pj_ssl_sock_start_read2(ws->ssl_sock, ws->pool,
                                             (unsigned)ws->rx_buf_size,
                                             readbuf, 0);
        } else
#endif
        {
            status = pj_activesock_start_read2(ws->asock, ws->pool,
                                               (unsigned)ws->rx_buf_size,
                                               readbuf, 0);
        }
    }

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        PJ_PERROR(2, (THIS_FILE, status, "Failed to start reading"));
        ws->state = PJ_WEBSOCK_STATE_CLOSED;
        pj_grp_lock_release(ws->grp_lock);
        if (ws->cb.on_connect)
            ws->cb.on_connect(ws, status);
        return;
    }

    /* Send WebSocket handshake */
    status = send_handshake(ws);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(2, (THIS_FILE, status,
                      "Failed to send WebSocket handshake"));
        ws->state = PJ_WEBSOCK_STATE_CLOSED;
        pj_grp_lock_release(ws->grp_lock);
        if (ws->cb.on_connect)
            ws->cb.on_connect(ws, status);
        return;
    }

    pj_grp_lock_release(ws->grp_lock);
}

static void on_transport_data(pj_websock *ws, void *data, pj_size_t size,
                              pj_status_t status, pj_size_t *remainder)
{
    pj_str_t empty;
    pj_websock_readystate prev_state;

    empty.ptr = NULL;
    empty.slen = 0;
    *remainder = 0;

    pj_grp_lock_acquire(ws->grp_lock);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3, (THIS_FILE, status, "Transport read error"));

        prev_state = ws->state;
        ws->state = PJ_WEBSOCK_STATE_CLOSED;
        pj_grp_lock_release(ws->grp_lock);

        /* Invoke callback without lock */
        if (prev_state == PJ_WEBSOCK_STATE_OPEN ||
            prev_state == PJ_WEBSOCK_STATE_CLOSING)
        {
            if (ws->cb.on_close)
                ws->cb.on_close(ws, 0, &empty);
        } else if (prev_state == PJ_WEBSOCK_STATE_CONNECTING) {
            if (ws->cb.on_connect)
                ws->cb.on_connect(ws, status);
        }
        return;
    }

    if (!ws->handshake_done) {
        /* Still in handshake phase */
        pj_size_t parsed = 0;
        status = process_handshake_response(ws, (pj_uint8_t*)data,
                                            size, &parsed);
        if (status == PJ_EPENDING) {
            /* Need more data - keep everything in buffer */
            *remainder = size;
            pj_grp_lock_release(ws->grp_lock);
            return;
        }
        if (status != PJ_SUCCESS) {
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            pj_grp_lock_release(ws->grp_lock);
            if (ws->cb.on_connect)
                ws->cb.on_connect(ws, status);
            return;
        }

        ws->handshake_done = PJ_TRUE;
        ws->state = PJ_WEBSOCK_STATE_OPEN;

        /* Start ping timer */
        if (ws->ping_interval > 0) {
            pj_time_val delay;
            delay.sec = ws->ping_interval;
            delay.msec = 0;
            pj_timer_heap_schedule_w_grp_lock(ws->timer_heap,
                                              &ws->ping_timer, &delay,
                                              1, ws->grp_lock);
        }

        pj_grp_lock_release(ws->grp_lock);

        /* Invoke callback without lock.  The transport's grp_lock
         * reference keeps the object alive even if the callback calls
         * pj_websock_destroy().
         */
        if (ws->cb.on_connect)
            ws->cb.on_connect(ws, PJ_SUCCESS);

        /* Process remaining data as WebSocket frames, but only if
         * the callback didn't destroy us.
         */
        if (!ws->destroying && parsed < size) {
            pj_size_t rem = size - parsed;
            pj_memmove(data, (pj_uint8_t*)data + parsed, rem);
            process_rx_frame(ws, (pj_uint8_t*)data, rem, remainder);
        }
    } else {
        pj_grp_lock_release(ws->grp_lock);
        /* Process WebSocket frames */
        process_rx_frame(ws, (pj_uint8_t*)data, size, remainder);
    }
}


/*=========================================================================
 * WebSocket frame processing
 */
/*
 * Process a decoded frame payload (shared by normal and partial paths).
 * Returns PJ_FALSE if caller should stop (e.g. after CLOSE), PJ_TRUE
 * to continue processing.
 */
static pj_bool_t handle_frame(pj_websock *ws, pj_bool_t fin,
                               pj_websock_opcode opcode,
                               pj_uint8_t *payload,
                               pj_size_t payload_len)
{
    switch (opcode) {
    case PJ_WEBSOCK_OP_PING:
        PJ_LOG(5, (THIS_FILE, "Received PING (%lu bytes)",
                   (unsigned long)payload_len));
        pj_grp_lock_acquire(ws->grp_lock);
        if (!ws->destroying)
            send_pong(ws, payload, payload_len);
        pj_grp_lock_release(ws->grp_lock);
        break;

    case PJ_WEBSOCK_OP_PONG:
        PJ_LOG(5, (THIS_FILE, "Received PONG"));
        break;

    case PJ_WEBSOCK_OP_CLOSE:
    {
        pj_uint16_t code = 0;
        pj_str_t reason;
        pj_websock_readystate prev_state;

        reason.ptr = NULL;
        reason.slen = 0;

        if (payload_len >= 2) {
            code = (pj_uint16_t)((payload[0] << 8) | payload[1]);
            if (payload_len > 2) {
                reason.ptr = (char*)payload + 2;
                reason.slen = payload_len - 2;
            }
        }

        PJ_LOG(4, (THIS_FILE, "Received CLOSE (code=%d, reason=%.*s)",
                   code,
                   (int)reason.slen,
                   (reason.ptr ? reason.ptr : "")));

        pj_grp_lock_acquire(ws->grp_lock);
        prev_state = ws->state;
        if (prev_state == PJ_WEBSOCK_STATE_OPEN) {
            /* Echo close frame back */
            send_close_frame(ws, code, &reason);
        }
        ws->state = PJ_WEBSOCK_STATE_CLOSED;
        pj_grp_lock_release(ws->grp_lock);

        /* Invoke callback without lock */
        if (!ws->destroying && ws->cb.on_close)
            ws->cb.on_close(ws, code, &reason);
        return PJ_FALSE; /* Stop processing after close */
    }

    case PJ_WEBSOCK_OP_TEXT:
    case PJ_WEBSOCK_OP_BIN:
        if (fin && ws->frag_len == 0) {
            /* Complete message in single frame */
            if (ws->cb.on_rx_msg)
                ws->cb.on_rx_msg(ws, opcode, payload, payload_len);
        } else {
            if (ws->frag_len == 0) {
                ws->frag_opcode = opcode;
            }
            if (ws->frag_len + payload_len > ws->frag_buf_size) {
                PJ_LOG(2, (THIS_FILE, "Message too large, dropping"));
                ws->frag_len = 0;
                break;
            }
            pj_memcpy(ws->frag_buf + ws->frag_len, payload, payload_len);
            ws->frag_len += payload_len;

            if (fin) {
                pj_websock_opcode frag_op = ws->frag_opcode;
                pj_size_t frag_len = ws->frag_len;
                ws->frag_len = 0;
                if (ws->cb.on_rx_msg) {
                    ws->cb.on_rx_msg(ws, frag_op,
                                     ws->frag_buf, frag_len);
                }
            }
        }
        break;

    case PJ_WEBSOCK_OP_CONT:
        if (ws->frag_len == 0) {
            PJ_LOG(2, (THIS_FILE,
                       "Unexpected continuation frame, ignoring"));
            break;
        }
        if (ws->frag_len + payload_len > ws->frag_buf_size) {
            PJ_LOG(2, (THIS_FILE, "Message too large, dropping"));
            ws->frag_len = 0;
            break;
        }
        pj_memcpy(ws->frag_buf + ws->frag_len, payload, payload_len);
        ws->frag_len += payload_len;

        if (fin) {
            pj_websock_opcode frag_op = ws->frag_opcode;
            pj_size_t frag_len = ws->frag_len;
            ws->frag_len = 0;
            if (ws->cb.on_rx_msg) {
                ws->cb.on_rx_msg(ws, frag_op,
                                 ws->frag_buf, frag_len);
            }
        }
        break;

    default:
        PJ_LOG(3, (THIS_FILE, "Unknown opcode 0x%x, ignoring", opcode));
        break;
    }

    return PJ_TRUE;
}

static void process_rx_frame(pj_websock *ws, pj_uint8_t *data,
                             pj_size_t len, pj_size_t *remainder)
{
    *remainder = 0;

    /* Check if we're being destroyed concurrently */
    if (ws->destroying)
        return;

    /* Continue accumulating/discarding a partial frame from previous reads */
    while (ws->rx_partial && len > 0) {
        pj_size_t needed = ws->rx_partial_total - ws->rx_partial_got;
        pj_size_t copy = (len < needed) ? len : needed;

        if (ws->rx_partial_buf) {
            /* Accumulating: copy payload into buffer */
            pj_memcpy(ws->rx_partial_buf + ws->rx_partial_got, data, copy);
        }
        /* else: discarding oversized frame, just consume bytes */

        ws->rx_partial_got += copy;
        data += copy;
        len -= copy;

        if (ws->rx_partial_got < ws->rx_partial_total) {
            /* Still need more data */
            return;
        }

        /* Frame complete (or fully discarded) */
        ws->rx_partial = PJ_FALSE;

        if (ws->rx_partial_buf) {
            /* Note: server->client frames are not masked per RFC 6455 */
            if (!handle_frame(ws, ws->rx_partial_fin, ws->rx_partial_op,
                              ws->rx_partial_buf, ws->rx_partial_total))
            {
                return; /* CLOSE received */
            }
        } else {
            PJ_LOG(5, (THIS_FILE, "Discarded oversized frame (%lu bytes)",
                       (unsigned long)ws->rx_partial_total));
        }
    }

    while (len > 0) {
        pj_bool_t fin;
        pj_websock_opcode opcode;
        const pj_uint8_t *payload;
        pj_size_t payload_len;
        pj_size_t consumed;

        consumed = decode_frame_header(data, len, &fin, &opcode,
                                       &payload, &payload_len);
        if (consumed == 0) {
            /* Full frame not in buffer. Check if we can parse the header
             * to start incremental accumulation for oversized frames.
             */
            pj_bool_t has_mask;
            pj_uint8_t mask[4];
            pj_size_t hdr_len;
            pj_size_t avail;

            hdr_len = parse_frame_header(data, len, &fin, &opcode,
                                         &has_mask, mask, &payload_len);
            if (hdr_len == 0 || hdr_len >= len) {
                /* Incomplete header, keep remainder */
                *remainder = len;
                return;
            }

            /* Header parsed but payload exceeds read buffer. Verify it
             * fits within max message size, then start accumulating.
             */
            if (payload_len > ws->frag_buf_size) {
                pj_size_t avail_discard = len - hdr_len;

                PJ_LOG(2, (THIS_FILE,
                           "Frame payload too large (%lu > %lu), dropping",
                           (unsigned long)payload_len,
                           (unsigned long)ws->frag_buf_size));
                /* Set up discard state: consume available payload bytes
                 * now, skip the rest in subsequent reads.
                 */
                ws->rx_partial = PJ_TRUE;
                ws->rx_partial_op = opcode;
                ws->rx_partial_fin = fin;
                ws->rx_partial_total = payload_len;
                ws->rx_partial_got = avail_discard;
                ws->rx_partial_buf = NULL; /* NULL = discard mode */
                return;
            }

            /* Allocate accumulation buffer from pool if needed */
            if (!ws->rx_partial_buf) {
                ws->rx_partial_buf = (pj_uint8_t*)pj_pool_alloc(
                                         ws->pool,
                                         ws->frag_buf_size);
            }

            avail = len - hdr_len;
            if (has_mask) {
                /* Unmask available payload */
                pj_size_t i;
                for (i = 0; i < avail; ++i) {
                    ((pj_uint8_t*)(data + hdr_len))[i] ^= mask[i & 3];
                }
            }
            pj_memcpy(ws->rx_partial_buf, data + hdr_len, avail);

            ws->rx_partial = PJ_TRUE;
            ws->rx_partial_op = opcode;
            ws->rx_partial_fin = fin;
            ws->rx_partial_total = payload_len;
            ws->rx_partial_got = avail;

            PJ_LOG(6, (THIS_FILE,
                       "Large frame: opcode=%d payload=%lu, "
                       "buffered %lu, need %lu more",
                       opcode, (unsigned long)payload_len,
                       (unsigned long)avail,
                       (unsigned long)(payload_len - avail)));

            /* All data consumed, wait for more reads */
            return;
        }

        if (!handle_frame(ws, fin, opcode, (pj_uint8_t*)payload,
                          payload_len))
        {
            return; /* CLOSE received */
        }

        data += consumed;
        len -= consumed;
    }
}


/*=========================================================================
 * Control frame helpers
 */
/* Control-frame senders use ws->ctl_buf + send_raw_ctl(), which is
 * independent of the data-frame tx_buf + send_raw() path.
 * Callers MUST hold ws->grp_lock.
 */
static pj_status_t send_close_frame(pj_websock *ws,
                                    pj_uint16_t status_code,
                                    const pj_str_t *reason)
{
    pj_uint8_t payload[125];
    pj_size_t plen = 0;
    pj_size_t frame_len;

    /* Encode close payload: 2-byte status code + reason */
    payload[0] = (pj_uint8_t)((status_code >> 8) & 0xFF);
    payload[1] = (pj_uint8_t)(status_code & 0xFF);
    plen = 2;

    if (reason && reason->slen > 0) {
        pj_size_t rlen = reason->slen;
        if (rlen > sizeof(payload) - 2)
            rlen = sizeof(payload) - 2;
        pj_memcpy(payload + 2, reason->ptr, rlen);
        plen += rlen;
    }

    frame_len = encode_frame(ws->ctl_buf, PJ_WEBSOCK_OP_CLOSE, PJ_TRUE,
                             payload, plen);
    return send_raw_ctl(ws, ws->ctl_buf, (pj_ssize_t)frame_len);
}

static pj_status_t send_pong(pj_websock *ws, const void *data,
                             pj_size_t len)
{
    pj_size_t frame_len;

    if (len > 125)
        len = 125;

    frame_len = encode_frame(ws->ctl_buf, PJ_WEBSOCK_OP_PONG, PJ_TRUE,
                             data, len);
    return send_raw_ctl(ws, ws->ctl_buf, (pj_ssize_t)frame_len);
}


/*=========================================================================
 * Ping timer
 */
static void on_timer(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pj_websock *ws = (pj_websock*)te->user_data;
    pj_size_t frame_len;
    pj_time_val delay;

    PJ_UNUSED_ARG(th);

    pj_grp_lock_acquire(ws->grp_lock);

    if (ws->state != PJ_WEBSOCK_STATE_OPEN) {
        pj_grp_lock_release(ws->grp_lock);
        return;
    }

    PJ_LOG(5, (THIS_FILE, "Sending PING"));

    frame_len = encode_frame(ws->ctl_buf, PJ_WEBSOCK_OP_PING, PJ_TRUE,
                             NULL, 0);
    send_raw_ctl(ws, ws->ctl_buf, (pj_ssize_t)frame_len);

    /* Reschedule */
    delay.sec = ws->ping_interval;
    delay.msec = 0;
    pj_timer_heap_schedule_w_grp_lock(ws->timer_heap, &ws->ping_timer,
                                      &delay, 1, ws->grp_lock);

    pj_grp_lock_release(ws->grp_lock);
}


/*=========================================================================
 * Transport close helper
 */
static void close_transport(pj_websock *ws)
{
    if (ws->ping_timer.id != 0) {
        pj_timer_heap_cancel(ws->timer_heap, &ws->ping_timer);
        ws->ping_timer.id = 0;
    }

#if PJ_HAS_SSL_SOCK
    if (ws->ssl_sock) {
        pj_ssl_sock_close(ws->ssl_sock);
        ws->ssl_sock = NULL;
    }
#endif
    if (ws->asock) {
        pj_activesock_close(ws->asock);
        ws->asock = NULL;
    }
}


/*=========================================================================
 * Public API
 */

PJ_DEF(void) pj_websock_param_default(pj_websock_param *param)
{
    pj_bzero(param, sizeof(*param));
    param->max_rx_msg_size = PJ_WEBSOCK_MAX_RX_MSG_SIZE;
    param->ping_interval = PJ_WEBSOCK_DEFAULT_PING_INTERVAL;
}

PJ_DEF(pj_status_t) pj_websock_create(pj_pool_t *pool,
                                      const pj_websock_param *param,
                                      pj_websock **p_ws)
{
    pj_websock *ws;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && param && p_ws, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->ioqueue && param->timer_heap, PJ_EINVAL);

    pool = pj_pool_create(pool->factory, "websock%p", 1024, 1024, NULL);

    ws = PJ_POOL_ZALLOC_T(pool, pj_websock);

    ws->pool = pool;
    ws->state = PJ_WEBSOCK_STATE_CLOSED;
    ws->cb = param->cb;
    ws->user_data = param->user_data;
    ws->ioqueue = param->ioqueue;
    ws->timer_heap = param->timer_heap;
    ws->max_rx_msg_size = param->max_rx_msg_size;
    ws->ping_interval = param->ping_interval;
    ws->ssl_param = param->ssl_param;

    /* Allocate buffers */
    ws->rx_buf_size = PJ_WEBSOCK_RX_BUF_SIZE;
    ws->rx_buf = (pj_uint8_t*)pj_pool_alloc(ws->pool, ws->rx_buf_size);

    ws->frag_buf_size = ws->max_rx_msg_size;
    ws->frag_buf = (pj_uint8_t*)pj_pool_alloc(ws->pool, ws->frag_buf_size);

    ws->tx_buf_size = PJ_WEBSOCK_TX_BUF_SIZE;
    ws->tx_buf = (pj_uint8_t*)pj_pool_alloc(ws->pool, ws->tx_buf_size);

    /* Init timer */
    pj_timer_entry_init(&ws->ping_timer, 0, ws, &on_timer);

    /* Init send keys */
    pj_ioqueue_op_key_init(&ws->send_key, sizeof(ws->send_key));
    pj_ioqueue_op_key_init(&ws->ctl_send_key, sizeof(ws->ctl_send_key));

    /* Create or use supplied group lock */
    if (param->grp_lock) {
        ws->grp_lock = param->grp_lock;
    } else {
        status = pj_grp_lock_create(ws->pool, NULL, &ws->grp_lock);
        if (status != PJ_SUCCESS) return status;
    }
    pj_grp_lock_add_ref(ws->grp_lock);
    pj_grp_lock_add_handler(ws->grp_lock, ws->pool, ws,
                            &websock_on_destroy);

    *p_ws = ws;

    PJ_LOG(4, (THIS_FILE, "WebSocket instance created"));
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_websock_connect_param_default(
                                        pj_websock_connect_param *cparam)
{
    pj_bzero(cparam, sizeof(*cparam));
}

PJ_DEF(pj_status_t) pj_websock_connect(
                                pj_websock *ws,
                                const pj_str_t *url,
                                const pj_websock_connect_param *cparam)
{
    pj_status_t status;
    pj_sockaddr rem_addr;
    pj_sockaddr local_addr;
    pj_addrinfo ai[1];
    unsigned ai_cnt = 1;

    PJ_ASSERT_RETURN(ws && url, PJ_EINVAL);
    PJ_ASSERT_RETURN(ws->state == PJ_WEBSOCK_STATE_CLOSED, PJ_EINVALIDOP);

    ws->state = PJ_WEBSOCK_STATE_CONNECTING;
    ws->handshake_done = PJ_FALSE;
    ws->frag_len = 0;
    ws->send_pending = PJ_FALSE;
    ws->ctl_send_pending = PJ_FALSE;

    /* Copy connect parameters */
    ws->subprotocol.slen = 0;
    ws->extra_hdr.count = 0;
    if (cparam) {
        if (cparam->subprotocol.slen > 0) {
            pj_strdup(ws->pool, &ws->subprotocol, &cparam->subprotocol);
        }
        ws->extra_hdr.count = cparam->extra_hdr.count;
        if (ws->extra_hdr.count > PJ_WEBSOCK_MAX_HEADERS)
            ws->extra_hdr.count = PJ_WEBSOCK_MAX_HEADERS;
        {
            unsigned i;
            for (i = 0; i < ws->extra_hdr.count; ++i) {
                pj_strdup(ws->pool, &ws->extra_hdr.header[i].name,
                          &cparam->extra_hdr.header[i].name);
                pj_strdup(ws->pool, &ws->extra_hdr.header[i].value,
                          &cparam->extra_hdr.header[i].value);
            }
        }
    }

    /* Parse URL */
    {
        pj_http_url hurl;

        status = pj_http_req_parse_url(url, &hurl);
        if (status != PJ_SUCCESS) {
            PJ_LOG(1, (THIS_FILE, "Invalid WebSocket URL"));
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            return status;
        }
        if (pj_stricmp2(&hurl.protocol, "WSS") == 0) {
            ws->is_ssl = PJ_TRUE;
        } else if (pj_stricmp2(&hurl.protocol, "WS") == 0) {
            ws->is_ssl = PJ_FALSE;
        } else {
            PJ_LOG(1, (THIS_FILE, "Invalid WebSocket URL scheme"));
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            return PJ_EINVAL;
        }
        pj_strdup(ws->pool, &ws->host, &hurl.host);
        ws->port = hurl.port;
        pj_strdup(ws->pool, &ws->path, &hurl.path);
    }

    /* Resolve hostname */
    status = pj_getaddrinfo(pj_AF_UNSPEC(), &ws->host, &ai_cnt, ai);
    if (status != PJ_SUCCESS || ai_cnt == 0) {
        PJ_PERROR(2, (THIS_FILE, status, "Failed to resolve %.*s",
                      (int)ws->host.slen, ws->host.ptr));
        ws->state = PJ_WEBSOCK_STATE_CLOSED;
        return status ? status : PJ_ERESOLVE;
    }

    pj_sockaddr_cp(&rem_addr, &ai[0].ai_addr);
    pj_sockaddr_set_port(&rem_addr, ws->port);

    /* Prepare local address */
    pj_sockaddr_init(rem_addr.addr.sa_family == pj_AF_INET6()
                         ? pj_AF_INET6() : pj_AF_INET(),
                     &local_addr, NULL, 0);

    if (ws->is_ssl) {
#if PJ_HAS_SSL_SOCK
        /* SSL connection */
        pj_ssl_sock_param ssl_prm;
        pj_ssl_sock_cb ssl_cb;

        pj_bzero(&ssl_cb, sizeof(ssl_cb));
        ssl_cb.on_connect_complete = &ssl_on_connect_complete;
        ssl_cb.on_data_read = &ssl_on_data_read;
        ssl_cb.on_data_sent = &ssl_on_data_sent;

        if (ws->ssl_param) {
            pj_memcpy(&ssl_prm, ws->ssl_param, sizeof(ssl_prm));
        } else {
            pj_ssl_sock_param_default(&ssl_prm);
        }
        ssl_prm.cb = ssl_cb;
        ssl_prm.user_data = ws;
        ssl_prm.ioqueue = ws->ioqueue;
        ssl_prm.timer_heap = ws->timer_heap;
        ssl_prm.server_name = ws->host;
        ssl_prm.sock_af = rem_addr.addr.sa_family;
        ssl_prm.sock_type = pj_SOCK_STREAM();
        ssl_prm.grp_lock = ws->grp_lock;

        status = pj_ssl_sock_create(ws->pool, &ssl_prm, &ws->ssl_sock);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(2, (THIS_FILE, status,
                          "Failed to create SSL socket"));
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            return status;
        }

        {
            pj_ssl_start_connect_param conn_prm;
            pj_bzero(&conn_prm, sizeof(conn_prm));
            conn_prm.pool = ws->pool;
            conn_prm.localaddr = &local_addr;
            conn_prm.local_port_range = 0;
            conn_prm.remaddr = &rem_addr;
            conn_prm.addr_len = pj_sockaddr_get_len(&rem_addr);
            status = pj_ssl_sock_start_connect2(ws->ssl_sock, &conn_prm);
        }
        if (status != PJ_SUCCESS && status != PJ_EPENDING) {
            PJ_PERROR(2, (THIS_FILE, status, "SSL connect failed"));
            pj_ssl_sock_close(ws->ssl_sock);
            ws->ssl_sock = NULL;
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            return status;
        }
#else  /* PJ_HAS_SSL_SOCK */
        PJ_LOG(1, (THIS_FILE, "wss:// requires SSL support "
                               "(PJ_HAS_SSL_SOCK)"));
        ws->state = PJ_WEBSOCK_STATE_CLOSED;
        return PJ_ENOTSUP;
#endif /* PJ_HAS_SSL_SOCK */
    } else {
        /* Plain TCP connection */
        pj_activesock_cb asock_cb;
        pj_activesock_cfg asock_cfg;
        pj_sock_t sock;

        status = pj_sock_socket(rem_addr.addr.sa_family,
                                pj_SOCK_STREAM(), 0, &sock);
        if (status != PJ_SUCCESS) {
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            return status;
        }

        pj_activesock_cfg_default(&asock_cfg);
        asock_cfg.grp_lock = ws->grp_lock;

        pj_bzero(&asock_cb, sizeof(asock_cb));
        asock_cb.on_connect_complete = &asock_on_connect_complete;
        asock_cb.on_data_read = &asock_on_data_read;
        asock_cb.on_data_sent = &asock_on_data_sent;

        status = pj_activesock_create(ws->pool, sock, pj_SOCK_STREAM(),
                                      &asock_cfg, ws->ioqueue, &asock_cb,
                                      ws, &ws->asock);
        if (status != PJ_SUCCESS) {
            pj_sock_close(sock);
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            return status;
        }

        status = pj_activesock_start_connect(ws->asock, ws->pool,
                                             &rem_addr,
                                             pj_sockaddr_get_len(
                                                 &rem_addr));
        if (status != PJ_SUCCESS && status != PJ_EPENDING) {
            PJ_PERROR(2, (THIS_FILE, status, "TCP connect failed"));
            pj_activesock_close(ws->asock);
            ws->asock = NULL;
            ws->state = PJ_WEBSOCK_STATE_CLOSED;
            return status;
        }
    }

    PJ_LOG(4, (THIS_FILE, "Connecting to %.*s:%d (%s)...",
               (int)ws->host.slen, ws->host.ptr, ws->port,
               ws->is_ssl ? "wss" : "ws"));

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_websock_send(pj_websock *ws,
                                    pj_websock_opcode opcode,
                                    const void *data,
                                    pj_size_t len)
{
    pj_size_t frame_len;
    pj_size_t max_payload;

    PJ_ASSERT_RETURN(ws, PJ_EINVAL);
    PJ_ASSERT_RETURN(ws->state == PJ_WEBSOCK_STATE_OPEN, PJ_EINVALIDOP);

    /* Check that payload fits in tx buffer (header overhead is at most 14) */
    max_payload = ws->tx_buf_size - MAX_FRAME_HDR_LEN;
    if (len > max_payload) {
        PJ_LOG(2, (THIS_FILE, "Send payload too large (%lu > %lu)",
                   (unsigned long)len, (unsigned long)max_payload));
        return PJ_ETOOBIG;
    }

    pj_grp_lock_acquire(ws->grp_lock);

    frame_len = encode_frame(ws->tx_buf, opcode, PJ_TRUE, data, len);

    {
        pj_status_t status;
        status = send_raw(ws, ws->tx_buf, (pj_ssize_t)frame_len);
        pj_grp_lock_release(ws->grp_lock);
        return status;
    }
}

PJ_DEF(pj_status_t) pj_websock_close(pj_websock *ws,
                                     pj_uint16_t status_code,
                                     const pj_str_t *reason)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(ws, PJ_EINVAL);

    pj_grp_lock_acquire(ws->grp_lock);

    if (ws->state != PJ_WEBSOCK_STATE_OPEN) {
        pj_grp_lock_release(ws->grp_lock);
        return PJ_EINVALIDOP;
    }

    ws->state = PJ_WEBSOCK_STATE_CLOSING;

    PJ_LOG(4, (THIS_FILE, "Closing WebSocket (code=%d)", status_code));

    status = send_close_frame(ws, status_code, reason);

    pj_grp_lock_release(ws->grp_lock);
    return status;
}

PJ_DEF(pj_websock_readystate) pj_websock_get_state(const pj_websock *ws)
{
    PJ_ASSERT_RETURN(ws, PJ_WEBSOCK_STATE_CLOSED);
    return ws->state;
}

PJ_DEF(void*) pj_websock_get_user_data(const pj_websock *ws)
{
    PJ_ASSERT_RETURN(ws, NULL);
    return ws->user_data;
}

PJ_DEF(pj_status_t) pj_websock_set_user_data(pj_websock *ws,
                                             void *user_data)
{
    PJ_ASSERT_RETURN(ws, PJ_EINVAL);
    ws->user_data = user_data;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_websock_destroy(pj_websock *ws)
{
    PJ_ASSERT_RETURN(ws, PJ_EINVAL);

    PJ_LOG(4, (THIS_FILE, "Destroying WebSocket instance"));

    pj_grp_lock_acquire(ws->grp_lock);
    ws->destroying = PJ_TRUE;
    ws->state = PJ_WEBSOCK_STATE_CLOSED;
    close_transport(ws);
    pj_grp_lock_release(ws->grp_lock);

    /* Release our ref; actual cleanup in websock_on_destroy() when
     * all references (including those held by in-flight callbacks)
     * have been released. */
    pj_grp_lock_dec_ref(ws->grp_lock);

    return PJ_SUCCESS;
}

/*
 * Group lock destroy handler — called when the last reference is released.
 */
static void websock_on_destroy(void *arg)
{
    pj_websock *ws = (pj_websock*)arg;

    PJ_LOG(4, (THIS_FILE, "WebSocket instance destroyed (ref=0)"));

    pj_pool_release(ws->pool);
}
