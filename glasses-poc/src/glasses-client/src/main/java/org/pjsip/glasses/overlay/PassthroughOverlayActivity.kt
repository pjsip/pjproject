package org.pjsip.glasses.overlay

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import org.pjsip.glasses.horizon.PassthroughLayer
import org.pjsip.glasses.sip.SipServiceConstants as C
import org.pjsip.glasses.voice.CallSession
import org.pjsip.glasses.voice.CrmClient

/**
 * Minimal passthrough overlay: on Horizon OS this Activity is shown as a
 * translucent panel over the Passthrough layer; on a plain AVD it is just a
 * translucent Activity. It renders call status, CRM caller ID and the
 * live transcript coming from the AI bridge (EVENT_TRANSCRIPT).
 */
class PassthroughOverlayActivity : Activity(), PassthroughLayer {

    private lateinit var status: TextView
    private lateinit var subtitle: TextView
    private lateinit var transcript: TextView

    private val rx = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, i: Intent) {
            when (i.action) {
                C.EVENT_INCOMING_CALL -> {
                    CallSession.currentCallId = i.getIntExtra(C.EXTRA_CALL_ID, -1)
                    val uri = i.getStringExtra(C.EXTRA_REMOTE_URI) ?: ""
                    val name = CrmClient.lookupByUri(uri) ?: i.getStringExtra(C.EXTRA_DISPLAY_NAME) ?: uri
                    setStatus("Incoming: $name", "palm-up to answer · fist to reject")
                    clearTranscript()
                }
                C.EVENT_CALL_STATE -> {
                    CallSession.currentCallId = i.getIntExtra(C.EXTRA_CALL_ID, -1)
                    val st = i.getStringExtra(C.EXTRA_CALL_STATE_TEXT) ?: ""
                    setStatus(st, i.getStringExtra(C.EXTRA_REMOTE_URI))
                    if (st == "DISCONNECTED") CallSession.currentCallId = -1
                }
                C.EVENT_TRANSCRIPT -> appendTranscript(
                    i.getStringExtra(C.EXTRA_TRANSCRIPT_SIDE) ?: "?",
                    i.getStringExtra(C.EXTRA_TRANSCRIPT_TEXT) ?: "")
                C.EVENT_REGISTRATION -> subtitle.text =
                    if (i.getBooleanExtra(C.EXTRA_REG_ACTIVE, false)) "registered" else "not registered"
                C.EVENT_ERROR -> subtitle.text = "error: ${i.getStringExtra(C.EXTRA_ERROR)}"
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(32, 32, 32, 32) }
        status = TextView(this).apply { textSize = 28f; text = "Glasses SIP" }
        subtitle = TextView(this).apply { textSize = 18f }
        transcript = TextView(this).apply { textSize = 16f }
        root.addView(status); root.addView(subtitle)
        root.addView(ScrollView(this).apply { addView(transcript) })
        setContentView(root)
        show()
    }

    override fun onStart() {
        super.onStart()
        val f = IntentFilter().apply {
            listOf(C.EVENT_INCOMING_CALL, C.EVENT_CALL_STATE, C.EVENT_TRANSCRIPT,
                   C.EVENT_REGISTRATION, C.EVENT_ERROR).forEach { addAction(it) }
        }
        if (android.os.Build.VERSION.SDK_INT >= 33)
            registerReceiver(rx, f, Context.RECEIVER_NOT_EXPORTED)
        else
            registerReceiver(rx, f)
    }

    override fun onStop() { unregisterReceiver(rx); super.onStop() }

    /* PassthroughLayer */
    override fun show() { /* Horizon: PassthroughLayer.enable(); panel is visible by default here */ }
    override fun hide() { finish() }
    override fun setStatus(title: String, subtitle: String?) {
        status.text = title; this.subtitle.text = subtitle ?: ""
    }
    override fun appendTranscript(side: String, text: String) {
        transcript.append("${if (side == "near") "you" else "them"}: $text\n")
    }
    override fun clearTranscript() { transcript.text = "" }
}
