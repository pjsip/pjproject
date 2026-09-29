package org.pjsip.glasses.sip

import org.pjsip.pjsua2.CodecOpusConfig
import org.pjsip.pjsua2.Endpoint
import org.pjsip.pjsua2.EpConfig
import org.pjsip.pjsua2.pjmedia_echo_flag
import org.pjsip.pjsua2.pj_log_decoration

/**
 * PJSUA2 endpoint tuning for glasses: 16 kHz narrow pipeline, low-bitrate
 * Opus, WebRTC AEC3 for open-ear speakers, small thread footprint.
 * Keep in sync with src/pjsip-native/config_site.h.
 */
object GlassesEpConfig {
    const val CLOCK_RATE = 16000
    const val PTIME_MS = 20
    const val OPUS_BITRATE = 16000
    const val EC_TAIL_MS = 200

    fun build(): EpConfig {
        val cfg = EpConfig()

        cfg.logConfig.level = 4
        cfg.logConfig.consoleLevel = 4
        cfg.logConfig.decor = cfg.logConfig.decor and
            (pj_log_decoration.PJ_LOG_HAS_CR.swigValue() or
             pj_log_decoration.PJ_LOG_HAS_NEWLINE.swigValue()).inv().toLong()

        cfg.uaConfig.userAgent = "PJSUA2-Glasses/0.1 (HorizonOS)"
        cfg.uaConfig.threadCnt = 1
        cfg.uaConfig.mainThreadOnly = false

        val med = cfg.medConfig
        med.clockRate = CLOCK_RATE.toLong()
        med.sndClockRate = CLOCK_RATE.toLong()
        med.audioFramePtime = PTIME_MS.toLong()
        med.channelCount = 1
        med.quality = 4          // cheaper resampler if the HAL forces 48 kHz
        med.noVad = false
        med.sndAutoCloseTime = 2 // close mic/speaker 2 s after last call
        med.hasIoqueue = true
        med.threadCnt = 1
        med.ecTailLen = EC_TAIL_MS.toLong()
        med.ecOptions = (pjmedia_echo_flag.PJMEDIA_ECHO_WEBRTC_AEC3.swigValue() or
                pjmedia_echo_flag.PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR.swigValue() or
                pjmedia_echo_flag.PJMEDIA_ECHO_USE_GAIN_CONTROLLER.swigValue()).toLong()
        med.jbInit = -1; med.jbMinPre = -1; med.jbMaxPre = -1; med.jbMax = -1
        return cfg
    }

    /** Call after libInit(): codec priorities and Opus profile. */
    fun applyCodecs(ep: Endpoint) {
        ep.codecSetPriority("opus/48000/2", 255.toShort())
        ep.codecSetPriority("PCMU/8000/1", 128.toShort())
        ep.codecSetPriority("PCMA/8000/1", 127.toShort())
        ep.codecSetPriority("G722/16000/1", 0.toShort())
        ep.codecSetPriority("speex/16000/1", 0.toShort())
        ep.codecSetPriority("iLBC/8000/1", 0.toShort())

        val opus = CodecOpusConfig().apply {
            sample_rate = CLOCK_RATE.toLong()
            channel_cnt = 1
            frm_ptime = PTIME_MS.toLong()
            frm_ptime_denum = 1
            bit_rate = OPUS_BITRATE.toLong()
            packet_loss = 10
            complexity = 4
            cbr = false
        }
        try { ep.setCodecOpusConfig(opus) } catch (_: Exception) { /* Opus not built in */ }
    }
}
