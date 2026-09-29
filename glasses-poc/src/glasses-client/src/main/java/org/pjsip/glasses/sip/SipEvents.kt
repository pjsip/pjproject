package org.pjsip.glasses.sip

import android.content.Context
import android.content.Intent
import org.pjsip.glasses.sip.SipServiceConstants as C

/**
 * Emits service events as package-local broadcasts. PJSUA2 callbacks
 * call these while holding PJSUA_LOCK, so they must stay cheap and must
 * not call back into the service.
 */
class SipEvents(private val ctx: Context) {

    fun registration(code: Int, active: Boolean) = emit(
        Intent(C.EVENT_REGISTRATION)
            .putExtra(C.EXTRA_REG_CODE, code)
            .putExtra(C.EXTRA_REG_ACTIVE, active)
    )

    fun incomingCall(callId: Int, remoteUri: String, displayName: String?) = emit(
        Intent(C.EVENT_INCOMING_CALL)
            .putExtra(C.EXTRA_CALL_ID, callId)
            .putExtra(C.EXTRA_REMOTE_URI, remoteUri)
            .putExtra(C.EXTRA_DISPLAY_NAME, displayName)
    )

    fun callState(callId: Int, state: Int, stateText: String, remoteUri: String) = emit(
        Intent(C.EVENT_CALL_STATE)
            .putExtra(C.EXTRA_CALL_ID, callId)
            .putExtra(C.EXTRA_CALL_STATE, state)
            .putExtra(C.EXTRA_CALL_STATE_TEXT, stateText)
            .putExtra(C.EXTRA_REMOTE_URI, remoteUri)
    )

    fun callMediaActive(callId: Int) = emit(
        Intent(C.EVENT_CALL_MEDIA).putExtra(C.EXTRA_CALL_ID, callId)
    )

    fun transcript(callId: Int, side: String, text: String) = emit(
        Intent(C.EVENT_TRANSCRIPT)
            .putExtra(C.EXTRA_CALL_ID, callId)
            .putExtra(C.EXTRA_TRANSCRIPT_SIDE, side)
            .putExtra(C.EXTRA_TRANSCRIPT_TEXT, text)
    )

    fun error(msg: String) = emit(Intent(C.EVENT_ERROR).putExtra(C.EXTRA_ERROR, msg))

    private fun emit(i: Intent) {
        i.setPackage(ctx.packageName)
        ctx.sendBroadcast(i)
    }
}
