package org.pjsip.glasses.sip

import java.io.OutputStream
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.TimeUnit

/**
 * Streams tapped PCM to the AI bridge over a local TCP socket
 * (adb forward tcp:7788 tcp:7788 when the bridge runs on a laptop).
 *
 * Wire format per frame: 1 byte side (0=near, 1=far), 2 bytes LE length,
 * then 16-bit LE mono PCM at 16 kHz.
 */
class PcmTapServer(
    private val near: PcmTapPort,
    private val far: PcmTapPort,
    private val port: Int = SipServiceConstants.PCM_TAP_PORT,
) {
    @Volatile private var running = false
    private var server: ServerSocket? = null
    private var thread: Thread? = null

    fun start() {
        if (running) return
        running = true
        thread = Thread({ serve() }, "pcm-tap-server").apply { isDaemon = true; start() }
    }

    fun stop() {
        running = false
        try { server?.close() } catch (_: Exception) {}
        thread?.join(500)
    }

    private fun serve() {
        val ss = ServerSocket(port).also { server = it }
        while (running) {
            val client = try { ss.accept() } catch (e: Exception) { break }
            pump(client)
        }
    }

    private fun pump(client: Socket) {
        client.tcpNoDelay = true
        val out = client.getOutputStream()
        try {
            while (running && client.isConnected) {
                near.queue.poll(ptime(), TimeUnit.MILLISECONDS)?.let { write(out, 0, it) }
                far.queue.poll()?.let { write(out, 1, it) }
            }
        } catch (_: Exception) {
        } finally {
            try { client.close() } catch (_: Exception) {}
        }
    }

    private fun ptime() = 20L

    private fun write(out: OutputStream, side: Int, pcm: ByteArray) {
        val hdr = byteArrayOf(side.toByte(), (pcm.size and 0xff).toByte(), (pcm.size shr 8).toByte())
        out.write(hdr); out.write(pcm); out.flush()
    }
}
