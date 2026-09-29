package org.pjsip.glasses.sip

import org.pjsip.pjsua2.AudioMediaPort
import org.pjsip.pjsua2.MediaFormatAudio
import org.pjsip.pjsua2.MediaFrame
import org.pjsip.pjsua2.pjmedia_format_id
import java.util.concurrent.ArrayBlockingQueue

/**
 * Custom AudioMedia port that taps raw PCM off the conference bridge for
 * the AI bridge (STT, Llama, translation). One instance per direction.
 *
 * onFrameReceived() runs on the conference clock thread every ptime ms:
 * it copies the frame and returns. Delivery to consumers happens on
 * [PcmTapServer]'s thread. Frames are dropped, never blocked on, when the
 * consumer is slow.
 *
 * Note: MediaFrame.buf is a SWIG ByteVector; copying it element-wise
 * through JNI is the cost of doing this in Kotlin. For production, tap in
 * C++ (pjmedia_port with put_frame) or use pjmedia_ai_port and only ship
 * text events up to Kotlin.
 */
class PcmTapPort(
    val side: String,                  // "near" | "far"
    private val clockRate: Int = 16000,
    private val ptimeMs: Int = 20,
) : AudioMediaPort() {

    val queue = ArrayBlockingQueue<ByteArray>(50)  // ~1 s of audio

    fun create() {
        val fmt = MediaFormatAudio()
        fmt.init(
            pjmedia_format_id.PJMEDIA_FORMAT_PCM.swigValue().toLong(),
            clockRate.toLong(), 1, (ptimeMs * 1000).toLong(), 16,
        )
        createPort("glasses-tap-$side", fmt)
    }

    override fun onFrameReceived(frame: MediaFrame) {
        val n = frame.size.toInt()
        if (n <= 0) return
        val buf = frame.buf
        val out = ByteArray(n)
        for (i in 0 until n) out[i] = buf[i]
        queue.offer(out)
    }

    /* Tap only listens; leave onFrameRequested() as silence. */
}
