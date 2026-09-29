package org.pjsip.glasses.gesture

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.SystemClock
import org.pjsip.glasses.horizon.HandGesture
import org.pjsip.glasses.horizon.HandGestureEvent
import org.pjsip.glasses.horizon.HandTrackingProvider
import org.pjsip.glasses.sip.SipServiceCommand
import org.pjsip.glasses.voice.CallSession
import org.pjsip.glasses.voice.ContactResolver

/**
 * Maps v207 Hand Tracking gestures to SIP actions (VR Glasses):
 *   palm-up      answer incoming call
 *   pinch        toggle mute
 *   swipe right  transfer to the last spoken contact / dispatch
 *   fist (hold)  hang up
 *
 * Debounced: one action per gesture per 800 ms, confidence >= 0.8.
 */
class HandGestureHandler(
    private val ctx: Context,
    private val provider: HandTrackingProvider,
) {
    private var lastActionAt = 0L
    private var muted = false
    var transferTarget: String = "dispatch"

    fun start() = provider.start(::onGesture)
    fun stop() = provider.stop()

    fun onGesture(ev: HandGestureEvent) {
        if (ev.confidence < 0.8f) return
        val now = SystemClock.elapsedRealtime()
        if (now - lastActionAt < 800) return
        lastActionAt = now

        val callId = CallSession.currentCallId
        when (ev.gesture) {
            HandGesture.PALM_UP -> SipServiceCommand.answerCall(ctx, callId)
            HandGesture.PINCH -> { muted = !muted; SipServiceCommand.setMute(ctx, callId, muted) }
            HandGesture.SWIPE_RIGHT -> ContactResolver.resolve(ctx, transferTarget)
                ?.let { SipServiceCommand.transferCall(ctx, callId, it.uri) }
            HandGesture.FIST -> SipServiceCommand.hangupCall(ctx, callId)
            else -> {}
        }
    }
}

/**
 * Lets demo scripts inject gestures without hand tracking hardware:
 *   adb shell am broadcast -a org.pjsip.glasses.action.GESTURE --es gesture PALM_UP
 */
class GestureDebugReceiver : BroadcastReceiver() {
    override fun onReceive(ctx: Context, intent: Intent) {
        val g = runCatching { HandGesture.valueOf(intent.getStringExtra("gesture") ?: "NONE") }
            .getOrDefault(HandGesture.NONE)
        HandGestureHandler(ctx, org.pjsip.glasses.horizon.NoopHandTracking)
            .onGesture(HandGestureEvent(org.pjsip.glasses.horizon.Hand.RIGHT, g, 1f))
    }
}
