/* Copyright (C) 2002 Jean-Marc Valin */
/**
   @file speex_jitter.h
   @brief Adaptive jitter buffer for Speex
*/
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef SPEEX_JITTER_H
#define SPEEX_JITTER_H
/** @defgroup JitterBuffer JitterBuffer: Adaptive jitter buffer
 *  This is the jitter buffer that reorders UDP/RTP packets and adjusts the buffer size
 * to maintain good quality and low latency.
 *  @{
 */

#include "speex.h"
#include "speex_bits.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Generic adaptive jitter buffer state */
struct JitterBuffer_;

/** Generic adaptive jitter buffer state */
typedef struct JitterBuffer_ JitterBuffer;

/** Definition of an incoming packet */
typedef struct _JitterBufferPacket JitterBufferPacket;

/** Definition of an incoming packet */
struct _JitterBufferPacket {
   char        *data;       /**< Data bytes contained in the packet */
   spx_uint32_t len;        /**< Length of the packet in bytes */
   spx_uint32_t timestamp;  /**< Timestamp for the packet */
   spx_uint32_t span;       /**< Time covered by the packet (same units as timestamp) */
};

/** Packet has been retrieved */
#define JITTER_BUFFER_OK 0
/** Packet is missing */
#define JITTER_BUFFER_MISSING 1
/** Packet is incomplete (does not cover the entive tick */
#define JITTER_BUFFER_INCOMPLETE 2
/** There was an error in the jitter buffer */
#define JITTER_BUFFER_INTERNAL_ERROR -1
/** Invalid argument */
#define JITTER_BUFFER_BAD_ARGUMENT -2


/** Set minimum amount of extra buffering required (margin) */
#define JITTER_BUFFER_SET_MARGIN 0
/** Get minimum amount of extra buffering required (margin) */
#define JITTER_BUFFER_GET_MARGIN 1
/* JITTER_BUFFER_SET_AVALIABLE_COUNT wouldn't make sense */
/** Get the amount of avaliable packets currently buffered */
#define JITTER_BUFFER_GET_AVALIABLE_COUNT 3

#define JITTER_BUFFER_ADJUST_INTERPOLATE -1
#define JITTER_BUFFER_ADJUST_OK 0
#define JITTER_BUFFER_ADJUST_DROP 1

/** Initialises jitter buffer 
 * 
 * @param tick Number of samples per "tick", i.e. the time period of the elements that will be retrieved
 * @return Newly created jitter buffer state
 */
JitterBuffer *jitter_buffer_init(int tick);

/** Restores jitter buffer to its original state 
 * 
 * @param jitter Jitter buffer state
 */
void jitter_buffer_reset(JitterBuffer *jitter);

/** Destroys jitter buffer 
 * 
 * @param jitter Jitter buffer state
 */
void jitter_buffer_destroy(JitterBuffer *jitter);

/** Put one packet into the jitter buffer
 * 
 * @param jitter Jitter buffer state
 * @param packet Incoming packet
*/
void jitter_buffer_put(JitterBuffer *jitter, const JitterBufferPacket *packet);

/** Get one packet from the jitter buffer
 * 
 * @param jitter Jitter buffer state
 * @param packet Returned packet
 * @param current_timestamp Timestamp for the returned packet 
*/
int jitter_buffer_get(JitterBuffer *jitter, JitterBufferPacket *packet, spx_int32_t *start_offset);

/** Get pointer timestamp of jitter buffer
 * 
 * @param jitter Jitter buffer state
*/
int jitter_buffer_get_pointer_timestamp(JitterBuffer *jitter);

/** Advance by one tick
 * 
 * @param jitter Jitter buffer state
*/
void jitter_buffer_tick(JitterBuffer *jitter);

/** Used like the ioctl function to control the jitter buffer parameters
 * 
 * @param jitter Jitter buffer state
 * @param request ioctl-type request (one of the JITTER_BUFFER_* macros)
 * @param ptr Data exchanged to-from function
 * @return 0 if no error, -1 if request in unknown
*/
int jitter_buffer_ctl(JitterBuffer *jitter, int request, void *ptr);

int jitter_buffer_update_delay(JitterBuffer *jitter, JitterBufferPacket *packet, spx_int32_t *start_offset);

/* @} */

/** @defgroup SpeexJitter SpeexJitter: Adaptive jitter buffer specifically for Speex
 *  This is the jitter buffer that reorders UDP/RTP packets and adjusts the buffer size
 * to maintain good quality and low latency. This is a simplified version that works only
 * with Speex, but is much easier to use.
 *  @{
*/

/** Speex jitter-buffer state. Never use it directly! */
typedef struct SpeexJitter {
   SpeexBits current_packet;         /**< Current Speex packet */
   int valid_bits;                   /**< True if Speex bits are valid */
   JitterBuffer *packets;            /**< Generic jitter buffer state */
   void *dec;                        /**< Pointer to Speex decoder */
   spx_int32_t frame_size;           /**< Frame size of Speex decoder */
} SpeexJitter;

/** Initialise jitter buffer 
 * 
 * @param jitter State of the Speex jitter buffer
 * @param decoder Speex decoder to call
 * @param sampling_rate Sampling rate used by the decoder
*/
void speex_jitter_init(SpeexJitter *jitter, void *decoder, int sampling_rate);

/** Destroy jitter buffer */
void speex_jitter_destroy(SpeexJitter *jitter);

/** Put one packet into the jitter buffer */
void speex_jitter_put(SpeexJitter *jitter, char *packet, int len, int timestamp);

/** Get one packet from the jitter buffer */
void speex_jitter_get(SpeexJitter *jitter, spx_int16_t *out, int *start_offset);

/** Get pointer timestamp of jitter buffer */
int speex_jitter_get_pointer_timestamp(SpeexJitter *jitter);

#ifdef __cplusplus
}
#endif

/* @} */
#endif
