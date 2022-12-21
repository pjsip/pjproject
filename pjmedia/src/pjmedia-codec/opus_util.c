
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC!=0)

#define THIS_FILE "opus_util.c"

#include <opus/opus.h>

#include "opus_util.h"

enum OpusMode Opus_GetMode(const uint8_t* payload, size_t payload_length_bytes) {
    uint8_t toc = payload[0];
    uint8_t config = toc >> 3;
    if (config < 12) {
        return OPUS_MODE_SILK;
    } else if (config < 16) {
        return OPUS_MODE_HYBRID;
    } else if (config < 32) {
        return OPUS_MODE_CELT;
    }
    return OPUS_MODE_UNKNOWN;
}

const char *Opus_GetModeName(enum OpusMode mode) {
    if (OPUS_MODE_SILK == mode) {
        return "OPUS_MODE_SILK";
    } else if (OPUS_MODE_HYBRID == mode) {
        return "OPUS_MODE_HYBRID";
    } else if (OPUS_MODE_CELT == mode) {
        return "OPUS_MODE_CELT";
    } else {
        return "OPUS_MODE_UNKNOWN";
    }
}

int Opus_NumSilkFrames(const uint8_t* payload, size_t payload_length_bytes) {
    // For computing the payload length in ms, the sample rate is not important
    // since it cancels out. We use 48 kHz, but any valid sample rate would work.
    int payload_length_ms = opus_packet_get_samples_per_frame(payload, 48000) / 48;
    if (payload_length_ms < 10) {
        payload_length_ms = 10;
    }

    int silk_frames;
    switch (payload_length_ms) {
        case 10:
        case 20:
            silk_frames = 1;
            break;
        case 40:
            silk_frames = 2;
            break;
        case 60:
            silk_frames = 3;
            break;
        default:
            return 0;  // It is actually even an invalid packet.
    }
    return silk_frames;
}

/**
 * Check FEC by FreeSwitch
 *
**/
int Opus_PacketHasFec1(const uint8_t* payload, size_t payload_length_bytes) {
    /* nb_silk_frames: number of silk-frames (10 or 20 ms) in an opus frame:  0, 1, 2 or 3 */
    /* computed from the 5 MSB (configuration) of the TOC byte (payload[0]) */
    /* nb_opus_frames: number of opus frames in the packet */
    /* computed from the 2 LSB (p0p1) of the TOC byte */
    /* p0p1 = 0  => nb_opus_frames = 1 */
    /* p0p1 = 1 or 2  => nb_opus_frames = 2 */
    /* p0p1 = 3  =>  given by the 6 LSB of payload[1] */
    int nb_silk_frames, nb_opus_frames, n, i;
    opus_int16 frame_sizes[48];
    const unsigned char *frame_data[48];

    if ((payload == NULL) || (payload_length_bytes <= 0)) {
        fprintf(stdout, "warning: corrupted packet (invalid size)\n");
        return 0;
    }
    if (payload[0] & 0x80) {
        fprintf(stdout, "warning: No FEC in CELT_ONLY mode.\n");
        return 0;
    }

    if ((nb_opus_frames = opus_packet_parse(payload, payload_length_bytes, NULL, frame_data, frame_sizes, NULL)) <= 0) {
        fprintf(stdout, "warning: OPUS_INVALID_PACKET ! nb_opus_frames: %d\n", nb_opus_frames);
        return 0;
    }

    if ((payload[0] >> 3 ) < 12) { /* config in silk-only : NB,MB,WB */
        nb_silk_frames = (payload[0] >> 3) & 0x3;
        if(nb_silk_frames  == 0) {
            nb_silk_frames = 1;
        }
        if ((nb_silk_frames == 1) && (nb_opus_frames == 1)) {
            for (n = 0; n <= (payload[0]&0x4) ; n++) { /* mono or stereo: 10,20 ms */
                if (frame_data[0][0] & (0x80 >> ((n + 1) * (nb_silk_frames + 1) - 1))) {
                    return 1; /* frame has FEC */
                }
            }
        } else {
            opus_int16 LBRR_flag = 0 ;
            for (i=0 ; i < nb_opus_frames; i++ ) { /* only mono Opus frames */
                LBRR_flag = (frame_data[i][0] >> (7 - nb_silk_frames)) & 0x1;
                if (LBRR_flag) {
                    return 1; /* one of the silk frames has FEC */
                }
            }
        }

        return 0;
    }

    return 0;
}

/**
 * Check FEC by WebRTC
 *
 * This method is based on Definition of the Opus Audio Codec
 * (https://tools.ietf.org/html/rfc6716). Basically, this method is based on
 * parsing the LP layer of an Opus packet, particularly the LBRR flag.
 *
 */
int Opus_PacketHasFec2(const uint8_t* payload, size_t payload_length_bytes) {
    if (payload == NULL || payload_length_bytes == 0) {
        return 0;
    }

    /* In CELT_ONLY mode, packets should not have FEC */
    if (payload[0] & 0x80) {
        return 0;
    }

    int silk_frames = Opus_NumSilkFrames(payload, payload_length_bytes);
    if (silk_frames == 0) {
        return 0;  // Not valid.
    }

    const int channels = opus_packet_get_nb_channels(payload);
    assert((channels == 1) || (channels == 2));

    // Max number of frames in an Opus packet is 48.
    int16_t frame_sizes[48];
    const unsigned char* frame_data[48];

    // Parse packet to get the frames. But we only care about the first frame,
    // since we can only decode the FEC from the first one.
    if (opus_packet_parse(payload, payload_length_bytes, NULL, frame_data, frame_sizes, NULL) < 0) {
        return 0;
    }

    if (frame_sizes[0] < 1) {
        return 0;
    }

    // A frame starts with the LP layer. The LP layer begins with two to eight
    // header bits.These consist of one VAD bit per SILK frame (up to 3),
    // followed by a single flag indicating the presence of LBRR frames.
    // For a stereo packet, these first flags correspond to the mid channel, and
    // a second set of flags is included for the side channel. Because these are
    // the first symbols decoded by the range coder and because they are coded
    // as binary values with uniform probability, they can be extracted directly
    // from the most significant bits of the first byte of compressed data.
    for (int n = 0; n < channels; n++) {
        // The LBRR bit for channel 1 is on the (`silk_frames` + 1)-th bit, and
        // that of channel 2 is on the |(`silk_frames` + 1) * 2 + 1|-th bit.
        if (frame_data[0][0] & (0x80 >> ((n + 1) * (silk_frames + 1) - 1))) {
            return 1;
        }
    }

    return 0;
}

#endif /* PJMEDIA_HAS_OPUS_CODEC */
