package org.pjsip.glasses.voice

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import org.pjsip.glasses.sip.SipServiceCommand

/**
 * Entry point for "Hey Meta, call warehouse".
 *
 * Muse (glasses OS agent) resolves the utterance to an intent and hands it
 * to the app. Until the v207 Voice SDK / Muse intent contract is public we
 * accept a generic broadcast:
 *
 *   action  org.pjsip.glasses.action.VOICE_INTENT
 *   extras  intent      = "call" | "answer" | "hangup" | "mute" | "transfer"
 *           slot_name   = "warehouse"            (contact / queue name)
 *           slot_uri    = "sip:2001@pbx"         (optional, skips resolver)
 *           vision      = true|false             (attach camera context)
 *           vision_ctx  = "<json>"               (from ai-vision-call)
 *
 * The same receiver also accepts android.intent.action.CALL with a
 * tel:/sip: URI so the plain Android assistant path works on an AVD.
 */
class VoiceTriggerReceiver : BroadcastReceiver() {

    companion object {
        const val ACTION_VOICE_INTENT = "org.pjsip.glasses.action.VOICE_INTENT"
        private const val TAG = "VoiceTrigger"
    }

    override fun onReceive(ctx: Context, intent: Intent) {
        when (intent.action) {
            ACTION_VOICE_INTENT -> handleMuseIntent(ctx, intent)
            Intent.ACTION_CALL, Intent.ACTION_VOICE_COMMAND -> {
                val uri = intent.dataString ?: return
                SipServiceCommand.makeCall(ctx, ContactResolver.normalize(uri))
            }
        }
    }

    private fun handleMuseIntent(ctx: Context, i: Intent) {
        val kind = i.getStringExtra("intent") ?: return
        val callId = CallSession.currentCallId
        Log.i(TAG, "muse intent=$kind name=${i.getStringExtra("slot_name")}")
        when (kind) {
            "call" -> {
                val uri = i.getStringExtra("slot_uri")
                    ?: ContactResolver.resolve(ctx, i.getStringExtra("slot_name") ?: return)
                    ?: run { Log.w(TAG, "unresolved contact"); return }
                val vision = if (i.getBooleanExtra("vision", false))
                    i.getStringExtra("vision_ctx") else null
                SipServiceCommand.makeCall(ctx, uri.uri, uri.displayName, vision)
            }
            "answer" -> SipServiceCommand.answerCall(ctx, callId)
            "hangup" -> SipServiceCommand.hangupCall(ctx, callId)
            "mute" -> SipServiceCommand.setMute(ctx, callId, true)
            "unmute" -> SipServiceCommand.setMute(ctx, callId, false)
            "transfer" -> {
                val to = ContactResolver.resolve(ctx, i.getStringExtra("slot_name") ?: return) ?: return
                SipServiceCommand.transferCall(ctx, callId, to.uri)
            }
        }
    }
}
