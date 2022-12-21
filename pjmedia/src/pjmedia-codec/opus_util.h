#ifndef OPUS_UTIL_H__
#define OPUS_UTIL_H__

enum OpusMode {
    OPUS_MODE_UNKNOWN = 0x00,
    OPUS_MODE_SILK    = 0x01,
    OPUS_MODE_HYBRID  = 0x02,
    OPUS_MODE_CELT    = 0x03,
};

enum OpusMode Opus_GetMode(const uint8_t* payload, size_t payload_length_bytes);
const char *Opus_GetModeName(enum OpusMode mode);
int Opus_NumSilkFrames(const uint8_t* payload, size_t payload_length_bytes);
int Opus_PacketHasFec1(const uint8_t* payload, size_t payload_length_bytes); // by FreeSwitch
int Opus_PacketHasFec2(const uint8_t* payload, size_t payload_length_bytes); // by WebRTC

#if defined(ENABLE_OPUS_FEC_WEBRTC) && ENABLE_OPUS_FEC_WEBRTC==1
# define Opus_PacketHasFec Opus_PacketHasFec1
#else
# define Opus_PacketHasFec Opus_PacketHasFec2
#endif

#endif /* OPUS_UTIL_H__ */
