package org.pjsip.glasses.sip

import org.pjsip.pjsua2.Account
import org.pjsip.pjsua2.CallOpParam
import org.pjsip.pjsua2.OnIncomingCallParam
import org.pjsip.pjsua2.OnRegStateParam
import org.pjsip.pjsua2.pjsip_status_code

/**
 * PJSUA2 Account. Incoming calls are not auto-answered: the service rings
 * (180) and waits for a hand gesture / voice command via
 * [SipServiceCommand.answerCall].
 */
class GlassesAccount(private val svc: SipService) : Account() {

    override fun onRegState(prm: OnRegStateParam) {
        val active = try { info.regIsActive } catch (e: Exception) { false }
        svc.events.registration(prm.code.swigValue(), active)
    }

    override fun onIncomingCall(prm: OnIncomingCallParam) {
        val call = GlassesCall(svc, this, prm.callId)
        val ci = try { call.info } catch (e: Exception) { null }
        val remote = ci?.remoteUri ?: "unknown"

        if (!svc.registerCall(call)) {
            /* One call at a time on glasses: reject a second one politely. */
            try {
                call.answer(CallOpParam(true).apply {
                    statusCode = pjsip_status_code.PJSIP_SC_BUSY_HERE
                })
            } catch (e: Exception) {
                svc.events.error("busy reject failed: ${e.message}")
            }
            return
        }

        try {
            call.answer(CallOpParam(true).apply {
                statusCode = pjsip_status_code.PJSIP_SC_RINGING
            })
        } catch (e: Exception) {
            svc.events.error("180 failed: ${e.message}")
        }
        svc.events.incomingCall(prm.callId, remote, displayNameOf(remote))
    }

    private fun displayNameOf(uri: String): String? {
        val lt = uri.indexOf('<')
        return if (lt > 0) uri.substring(0, lt).trim().trim('"') else null
    }
}
