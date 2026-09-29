package org.pjsip.glasses.sip

import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import org.pjsip.glasses.sip.SipServiceConstants as C

/**
 * Static facade that hides the SIP stack from the rest of the app.
 * Every method builds an Intent and hands it to [SipService]; nothing
 * here touches PJSUA2. Voice intents, hand gestures and the AI bridge
 * all go through this class.
 */
object SipServiceCommand {

    data class AccountParams(
        val idUri: String,          // "Tech <sip:tech1@pbx.example>"
        val registrar: String,      // "sip:pbx.example;transport=tls"
        val proxy: String?,         // "sip:pbx.example;lr;transport=tls"
        val user: String,
        val password: String,
        val turnServer: String?,    // "turn.edge.example:443"
        val turnUser: String?,
        val turnPassword: String?,
        val stunServer: String? = null,
    )

    fun setAccount(ctx: Context, p: AccountParams) {
        send(ctx, Intent(C.ACTION_SET_ACCOUNT).apply {
            putExtra(C.EXTRA_ACC_ID_URI, p.idUri)
            putExtra(C.EXTRA_ACC_REGISTRAR, p.registrar)
            putExtra(C.EXTRA_ACC_PROXY, p.proxy)
            putExtra(C.EXTRA_ACC_USER, p.user)
            putExtra(C.EXTRA_ACC_PASS, p.password)
            putExtra(C.EXTRA_TURN_SERVER, p.turnServer)
            putExtra(C.EXTRA_TURN_USER, p.turnUser)
            putExtra(C.EXTRA_TURN_PASS, p.turnPassword)
            putExtra(C.EXTRA_STUN_SERVER, p.stunServer)
        })
    }

    fun removeAccount(ctx: Context) = send(ctx, Intent(C.ACTION_REMOVE_ACCOUNT))

    /**
     * Place a call. [visionContext] is a compact JSON string produced by
     * the AI bridge; it is carried as X-Meta-Vision-Context on the INVITE.
     */
    fun makeCall(
        ctx: Context,
        dstUri: String,
        displayName: String? = null,
        visionContext: String? = null,
        extraHeaders: Map<String, String> = emptyMap(),
    ) {
        send(ctx, Intent(C.ACTION_MAKE_CALL).apply {
            putExtra(C.EXTRA_DST_URI, dstUri)
            putExtra(C.EXTRA_DISPLAY_NAME, displayName)
            putExtra(C.EXTRA_VISION_CONTEXT, visionContext)
            if (extraHeaders.isNotEmpty()) {
                val b = Bundle()
                extraHeaders.forEach { (k, v) -> b.putString(k, v) }
                putExtra(C.EXTRA_HEADERS, b)
            }
        })
    }

    fun answerCall(ctx: Context, callId: Int) =
        send(ctx, Intent(C.ACTION_ANSWER_CALL).putExtra(C.EXTRA_CALL_ID, callId))

    fun hangupCall(ctx: Context, callId: Int) =
        send(ctx, Intent(C.ACTION_HANGUP_CALL).putExtra(C.EXTRA_CALL_ID, callId))

    fun setMute(ctx: Context, callId: Int, mute: Boolean) =
        send(ctx, Intent(C.ACTION_SET_MUTE)
            .putExtra(C.EXTRA_CALL_ID, callId)
            .putExtra(C.EXTRA_MUTE, mute))

    /** Blind transfer (REFER). */
    fun transferCall(ctx: Context, callId: Int, dstUri: String) =
        send(ctx, Intent(C.ACTION_TRANSFER_CALL)
            .putExtra(C.EXTRA_CALL_ID, callId)
            .putExtra(C.EXTRA_DST_URI, dstUri))

    fun sendDtmf(ctx: Context, callId: Int, digits: String) =
        send(ctx, Intent(C.ACTION_SEND_DTMF)
            .putExtra(C.EXTRA_CALL_ID, callId)
            .putExtra(C.EXTRA_DTMF, digits))

    /** Call from a ConnectivityManager callback when Wi-Fi/tether changes. */
    fun networkChanged(ctx: Context) = send(ctx, Intent(C.ACTION_NETWORK_CHANGED))

    fun stop(ctx: Context) = send(ctx, Intent(C.ACTION_STOP_SERVICE))

    private fun send(ctx: Context, intent: Intent) {
        intent.setClass(ctx, SipService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ctx.startForegroundService(intent)
        } else {
            ctx.startService(intent)
        }
    }
}
