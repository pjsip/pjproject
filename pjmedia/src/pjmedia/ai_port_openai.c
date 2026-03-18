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
#include <pjmedia/ai_port.h>
#include <pjlib-util/base64.h>
#include <pjlib-util/http_client.h>
#include <pjlib-util/json.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE   "ai_port_openai.c"

/* OpenAI Realtime API native audio format: 24kHz mono PCM16 */
#define OPENAI_CLOCK_RATE       24000
#define OPENAI_CHANNEL_COUNT    1
#define OPENAI_BITS_PER_SAMPLE  16

/* JSON message templates */
static const char SESSION_UPDATE_FMT[] =
    "{"
        "\"type\":\"session.update\","
        "\"session\":{"
            "\"modalities\":[\"text\",\"audio\"],"
            "\"voice\":\"alloy\","
            "\"instructions\":\"You are a helpful AI assistant. "
                "Respond to the user naturally.\","
            "\"input_audio_format\":\"pcm16\","
            "\"output_audio_format\":\"pcm16\","
            "\"turn_detection\":{"
                "\"type\":\"server_vad\""
            "}"
        "}"
    "}";

/* Audio append template: {"type":"input_audio_buffer.append","audio":""}
 * The base64 payload goes between the last quotes. */
static const char AUDIO_APPEND_PREFIX[] =
    "{\"type\":\"input_audio_buffer.append\",\"audio\":\"";
static const char AUDIO_APPEND_SUFFIX[] = "\"}";


/**
 * Find a child element by name in a JSON object.
 */
static pj_json_elem* json_find(pj_json_elem *obj, const char *name)
{
    pj_json_elem *child;

    if (!obj || (obj->type != PJ_JSON_VAL_OBJ &&
                 obj->type != PJ_JSON_VAL_ARRAY))
    {
        return NULL;
    }

    child = obj->value.children.next;
    while (child != (pj_json_elem*)&obj->value.children) {
        if (pj_stricmp2(&child->name, name) == 0)
            return child;
        child = child->next;
    }
    return NULL;
}


/*
 * Prepare WebSocket connect parameters with OpenAI auth headers.
 */
static pj_status_t openai_prepare_connect(pjmedia_ai_backend *be,
                                          pj_pool_t *pool,
                                          const pj_str_t *auth_token,
                                          pj_websock_connect_param *cparam)
{
    char *auth_val;
    pj_status_t status;

    PJ_UNUSED_ARG(be);

    if (!auth_token || auth_token->slen == 0)
        return PJ_EINVAL;

    /* Authorization: Bearer <token> */
    auth_val = (char*)pj_pool_alloc(pool,
                                    7 + (pj_size_t)auth_token->slen + 1);
    pj_ansi_snprintf(auth_val, 7 + (pj_size_t)auth_token->slen + 1,
                     "Bearer %.*s",
                     (int)auth_token->slen, auth_token->ptr);

    status = pj_http_headers_add_elmt2(&cparam->extra_hdr,
                                       "Authorization", auth_val);
    if (status != PJ_SUCCESS)
        return status;

    status = pj_http_headers_add_elmt2(&cparam->extra_hdr,
                                       "OpenAI-Beta", "realtime=v1");
    if (status != PJ_SUCCESS)
        return status;

    return PJ_SUCCESS;
}


/*
 * Called when WebSocket connection is established.
 * We defer sending session.update until the server sends session.created.
 */
static pj_status_t openai_on_ws_connected(pjmedia_ai_backend *be,
                                          pj_websock *ws)
{
    PJ_UNUSED_ARG(be);
    PJ_UNUSED_ARG(ws);

    PJ_LOG(4, (THIS_FILE, "OpenAI WebSocket connected, "
               "waiting for session.created"));

    return PJ_SUCCESS;
}


/*
 * Encode PCM samples as base64 JSON for input_audio_buffer.append.
 */
static pj_status_t openai_encode_audio(pjmedia_ai_backend *be,
                                       const pj_int16_t *samples,
                                       unsigned sample_count,
                                       char *buf,
                                       int *buf_len)
{
    int prefix_len = (int)(sizeof(AUDIO_APPEND_PREFIX) - 1);
    int suffix_len = (int)(sizeof(AUDIO_APPEND_SUFFIX) - 1);
    int b64_len;
    int total;
    pj_status_t status;

    PJ_UNUSED_ARG(be);

    /* Calculate base64 output size */
    b64_len = PJ_BASE256_TO_BASE64_LEN(sample_count * 2);

    total = prefix_len + b64_len + suffix_len;
    if (total > *buf_len)
        return PJ_ETOOBIG;

    /* Copy prefix */
    pj_memcpy(buf, AUDIO_APPEND_PREFIX, prefix_len);

    /* Base64 encode the PCM data */
    b64_len = *buf_len - prefix_len - suffix_len;
    status = pj_base64_encode((const pj_uint8_t*)samples,
                              (int)(sample_count * 2),
                              buf + prefix_len,
                              &b64_len);
    if (status != PJ_SUCCESS)
        return status;

    /* Copy suffix */
    pj_memcpy(buf + prefix_len + b64_len, AUDIO_APPEND_SUFFIX,
              suffix_len);

    *buf_len = prefix_len + b64_len + suffix_len;
    return PJ_SUCCESS;
}


/*
 * Parse incoming WebSocket message from OpenAI.
 */
static pj_status_t openai_on_rx_msg(pjmedia_ai_backend *be,
                                    pj_pool_t *pool,
                                    const void *data,
                                    pj_size_t len,
                                    pj_int16_t *audio_out,
                                    unsigned *sample_count,
                                    pjmedia_ai_event *event,
                                    const char **reply,
                                    pj_size_t *reply_len)
{
    char *buf;
    unsigned buf_size;
    pj_json_elem *root;
    pj_json_elem *type_elem;
    pj_str_t type_str;

    unsigned max_samples = *sample_count;

    PJ_UNUSED_ARG(be);

    *sample_count = 0;
    *reply = NULL;
    *reply_len = 0;
    event->type = (pjmedia_ai_event_type)-1;

    /* Copy to mutable buffer for JSON parser (needs null termination) */
    buf_size = (unsigned)len + 1;
    buf = (char*)pj_pool_alloc(pool, buf_size);
    pj_memcpy(buf, data, len);
    buf[len] = '\0';

    root = pj_json_parse(pool, buf, &buf_size, NULL);
    if (!root) {
        PJ_LOG(4, (THIS_FILE, "Failed to parse JSON from OpenAI"));
        return PJ_EINVAL;
    }

    /* Get "type" field */
    type_elem = json_find(root, "type");
    if (!type_elem || type_elem->type != PJ_JSON_VAL_STRING) {
        return PJ_SUCCESS;
    }
    type_str = type_elem->value.str;

    /* response.audio.delta -> decode audio */
    if (pj_stricmp2(&type_str, "response.audio.delta") == 0) {
        pj_json_elem *delta_elem = json_find(root, "delta");

        if (delta_elem && delta_elem->type == PJ_JSON_VAL_STRING) {
            pj_str_t b64_in = delta_elem->value.str;
            int max_out = (int)(max_samples * 2);
            int max_b64 = (max_out / 3) * 4;
            int out_len;

            /* Truncate base64 input to fit output buffer. Each 4 base64
             * chars decode to 3 bytes, so limit input to a multiple of 4
             * that will decode within max_samples*2 bytes.
             */
            if ((int)b64_in.slen > max_b64) {
                PJ_LOG(4, (THIS_FILE,
                           "Audio delta truncated: b64_len=%d, "
                           "max_b64=%d (buf=%d samples)",
                           (int)b64_in.slen, max_b64, max_samples));
                b64_in.slen = max_b64;
            }
            out_len = (int)PJ_BASE64_TO_BASE256_LEN(b64_in.slen);

            if (pj_base64_decode(&b64_in,
                                 (pj_uint8_t*)audio_out,
                                 &out_len) == PJ_SUCCESS)
            {
                *sample_count = (unsigned)(out_len / 2);
            }
        }
    }
    /* response.audio_transcript.delta -> transcript event */
    else if (pj_stricmp2(&type_str,
                          "response.audio_transcript.delta") == 0)
    {
        pj_json_elem *delta_elem = json_find(root, "delta");
        if (delta_elem && delta_elem->type == PJ_JSON_VAL_STRING) {
            event->type = PJMEDIA_AI_EVENT_TRANSCRIPT;
            event->status = PJ_SUCCESS;
            event->text = delta_elem->value.str;
        }
    }
    /* response.done */
    else if (pj_stricmp2(&type_str, "response.done") == 0) {
        pj_json_elem *resp = json_find(root, "response");
        pj_json_elem *st = resp ? json_find(resp, "status") : NULL;
        if (st && st->type == PJ_JSON_VAL_STRING) {
            PJ_LOG(4, (THIS_FILE, "Response done: status=%.*s",
                       (int)st->value.str.slen, st->value.str.ptr));

            if (pj_stricmp2(&st->value.str, "failed") == 0 ||
                pj_stricmp2(&st->value.str, "cancelled") == 0)
            {
                pj_json_elem *sd = json_find(resp, "status_details");
                pj_json_elem *reason = sd ? json_find(sd, "reason")
                                          : NULL;
                pj_json_elem *err = sd ? json_find(sd, "error") : NULL;
                if (reason && reason->type == PJ_JSON_VAL_STRING) {
                    PJ_LOG(3, (THIS_FILE, "  reason: %.*s",
                               (int)reason->value.str.slen,
                               reason->value.str.ptr));
                }
                if (err) {
                    pj_json_elem *msg = json_find(err, "message");
                    if (msg && msg->type == PJ_JSON_VAL_STRING) {
                        PJ_LOG(3, (THIS_FILE, "  error: %.*s",
                                   (int)msg->value.str.slen,
                                   msg->value.str.ptr));
                    }
                }
            }
        }
        event->type = PJMEDIA_AI_EVENT_RESPONSE_DONE;
        event->status = PJ_SUCCESS;
    }
    /* input_audio_buffer.speech_started */
    else if (pj_stricmp2(&type_str,
                          "input_audio_buffer.speech_started") == 0)
    {
        event->type = PJMEDIA_AI_EVENT_SPEECH_STARTED;
        event->status = PJ_SUCCESS;
    }
    /* input_audio_buffer.speech_stopped */
    else if (pj_stricmp2(&type_str,
                          "input_audio_buffer.speech_stopped") == 0)
    {
        event->type = PJMEDIA_AI_EVENT_SPEECH_STOPPED;
        event->status = PJ_SUCCESS;
    }
    /* session.created -> reply with session.update */
    else if (pj_stricmp2(&type_str, "session.created") == 0) {
        PJ_LOG(4, (THIS_FILE, "OpenAI session created, "
                   "sending session.update"));
        *reply = SESSION_UPDATE_FMT;
        *reply_len = sizeof(SESSION_UPDATE_FMT) - 1;
    }
    /* response.created */
    else if (pj_stricmp2(&type_str, "response.created") == 0) {
        event->type = PJMEDIA_AI_EVENT_RESPONSE_START;
        event->status = PJ_SUCCESS;
    }
    /* session.updated - confirmation */
    else if (pj_stricmp2(&type_str, "session.updated") == 0) {
        PJ_LOG(4, (THIS_FILE, "OpenAI session updated successfully"));
    }
    /* error from OpenAI */
    else if (pj_stricmp2(&type_str, "error") == 0) {
        pj_json_elem *err = json_find(root, "error");
        pj_json_elem *msg = err ? json_find(err, "message") : NULL;
        if (msg && msg->type == PJ_JSON_VAL_STRING) {
            PJ_LOG(2, (THIS_FILE, "OpenAI error: %.*s",
                       (int)msg->value.str.slen, msg->value.str.ptr));
        } else {
            PJ_LOG(2, (THIS_FILE, "OpenAI error (no message)"));
        }
    }
    /* Log unhandled types at level 5 for debugging */
    else {
        PJ_LOG(5, (THIS_FILE, "Unhandled OpenAI event: %.*s",
                   (int)type_str.slen, type_str.ptr));
    }

    return PJ_SUCCESS;
}


/*
 * Destroy OpenAI backend.
 */
static pj_status_t openai_destroy(pjmedia_ai_backend *be)
{
    PJ_UNUSED_ARG(be);
    /* Backend is pool-allocated, nothing to free */
    return PJ_SUCCESS;
}


/* OpenAI backend vtable */
static const pjmedia_ai_backend_op openai_backend_op =
{
    &openai_prepare_connect,
    &openai_on_ws_connected,
    &openai_encode_audio,
    &openai_on_rx_msg,
    &openai_destroy
};


/*
 * Create OpenAI Realtime API backend.
 */
PJ_DEF(pj_status_t) pjmedia_ai_openai_backend_create(
                                    pj_pool_t *pool,
                                    pjmedia_ai_backend **p_backend)
{
    pjmedia_ai_backend *be;

    PJ_ASSERT_RETURN(pool && p_backend, PJ_EINVAL);

    be = PJ_POOL_ZALLOC_T(pool, pjmedia_ai_backend);
    be->op = &openai_backend_op;
    be->native_clock_rate = OPENAI_CLOCK_RATE;
    be->native_channel_count = OPENAI_CHANNEL_COUNT;
    be->native_bits_per_sample = OPENAI_BITS_PER_SAMPLE;

    *p_backend = be;
    return PJ_SUCCESS;
}
