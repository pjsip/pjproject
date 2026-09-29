package org.pjsip.glasses.sip

import org.pjsip.pjsua2.Account
import org.pjsip.pjsua2.AudioMedia
import org.pjsip.pjsua2.Call
import org.pjsip.pjsua2.OnCallMediaStateParam
import org.pjsip.pjsua2.OnCallStateParam
import org.pjsip.pjsua2.pjmedia_type
import org.pjsip.pjsua2.pjsip_inv_state
import org.pjsip.pjsua2.pjsua_call_media_status

/**
 * PJSUA2 Call. On media activation it wires the sound device to the call
 * and both directions of audio into [PcmTapPort]s for the AI bridge.
 */
class GlassesCall(
    private val svc: SipService,
    acc: Account,
    callId: Int = org.pjsip.pjsua2.pjsua2.INVALID_ID,
) : Call(acc, callId) {

    @Volatile var muted = false
        private set
    private var callAudio: AudioMedia? = null

    override fun onCallState(prm: OnCallStateParam) {
        val ci = try { info } catch (e: Exception) { return }
        svc.events.callState(ci.id, ci.state.swigValue(), ci.stateText, ci.remoteUri)
        if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
            callAudio = null
            svc.onCallEnded(this)
        }
    }

    override fun onCallMediaState(prm: OnCallMediaStateParam) {
        val ci = try { info } catch (e: Exception) { return }
        for (i in ci.media.indices) {
            val cmi = ci.media[i]
            if (cmi.type != pjmedia_type.PJMEDIA_TYPE_AUDIO) continue
            if (cmi.status != pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE &&
                cmi.status != pjsua_call_media_status.PJSUA_CALL_MEDIA_REMOTE_HOLD) continue
            try {
                val am = getAudioMedia(i)
                callAudio = am
                val adm = svc.ep.audDevManager()
                if (!muted) adm.captureDevMedia.startTransmit(am)
                am.startTransmit(adm.playbackDevMedia)
                /* AI taps: far end (remote speaker) and near end (glasses mic). */
                svc.farTap?.let { am.startTransmit(it) }
                svc.nearTap?.let { adm.captureDevMedia.startTransmit(it) }
                svc.events.callMediaActive(ci.id)
            } catch (e: Exception) {
                svc.events.error("media wiring failed: ${e.message}")
            }
        }
    }

    /** Must be called on the service worker thread. */
    fun setMute(mute: Boolean) {
        val am = callAudio ?: run { muted = mute; return }
        val cap = svc.ep.audDevManager().captureDevMedia
        if (mute) cap.stopTransmit(am) else cap.startTransmit(am)
        muted = mute
    }
}
