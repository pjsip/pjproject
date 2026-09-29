package org.pjsip.glasses.horizon

/**
 * Interfaces shaped after the v207 Horizon OS SDK surfaces this POC needs.
 * They are deliberately tiny so the real SDK classes can be adapted in one
 * file each without touching the SIP layer. Nothing here is the SDK.
 */

/** Hand Tracking API: one recognised gesture per event. */
enum class HandGesture { PALM_UP, PINCH, SWIPE_LEFT, SWIPE_RIGHT, FIST, NONE }

enum class Hand { LEFT, RIGHT }

data class HandGestureEvent(val hand: Hand, val gesture: HandGesture, val confidence: Float)

interface HandTrackingProvider {
    fun start(listener: (HandGestureEvent) -> Unit)
    fun stop()
}

/** Passthrough API: a 2D panel composited over the camera feed. */
interface PassthroughLayer {
    fun show()
    fun hide()
    fun setStatus(title: String, subtitle: String?)
    fun appendTranscript(side: String, text: String)
    fun clearTranscript()
}

/** Passthrough camera / Device Access Toolkit frame source. */
data class CameraFrame(val jpeg: ByteArray, val width: Int, val height: Int, val timestampNs: Long)

interface CameraFrameSource {
    /** Null on camera-free devices (Luna). */
    suspend fun grab(maxEdgePx: Int = 512): CameraFrame?
}

/** Voice SDK: app-side registration of intents Muse may dispatch. */
interface VoiceIntentRegistry {
    fun register(intentName: String, examples: List<String>)
}

/** Default no-op implementations used on a plain AVD. */
object NoopHandTracking : HandTrackingProvider {
    override fun start(listener: (HandGestureEvent) -> Unit) {}
    override fun stop() {}
}

object NoCamera : CameraFrameSource {
    override suspend fun grab(maxEdgePx: Int): CameraFrame? = null
}
