package org.pjsip.pjsua2.app_kotlin

import android.Manifest
import android.hardware.camera2.CameraManager
import android.os.Bundle
import android.os.Handler
import android.os.Message
import android.support.v4.app.ActivityCompat
import android.support.v7.app.AppCompatActivity
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.Button
import android.widget.RelativeLayout
import android.widget.TextView
import org.pjsip.PjCameraInfo2
import org.pjsip.pjsua2.*
import java.lang.ref.WeakReference

/* Configs */

// Account ID
const val ACC_ID_URI    = "sip:localhost"

// Peer to call
const val CALL_DST_URI  = "sip:192.168.1.9:6000"

// Camera ID used for video call.
// Use VidDevManager::enumDev2() to get available cameras & IDs.
const val VIDEO_CAPTURE_DEVICE_ID = -1

// SIP transport listening port
const val SIP_LISTENING_PORT = 6000

/* Constants */
const val MSG_UPDATE_CALL_INFO      = 1
const val MSG_SHOW_REMOTE_VIDEO     = 2
const val MSG_SHOW_LOCAL_VIDEO      = 3


/* Global objects */
internal object g {
    /* Maintain reference to avoid auto garbage collecting */
    lateinit var logWriter: MyLogWriter

    /* Message handler for updating UI */
    lateinit var uiHandler: Handler

    val ep = Endpoint()
    val acc = MyAccount()
    var call: MyCall? = null

    val localVideoHandler = VideoSurfaceHandler()
    val remoteVideoHandler = VideoSurfaceHandler()

    var previewStarted = false
}

/* Log writer, redirects logs to stdout */
internal class MyLogWriter : LogWriter() {
    override fun write(entry: LogEntry) {
        println(entry.msg)
    }
}

/* Account implementation */
internal class MyAccount : Account() {
    override fun onIncomingCall(prm: OnIncomingCallParam) {
        /* Auto answer with 200 for incoming calls  */
        val call = MyCall(g.acc, prm.callId)
        val ansPrm = CallOpParam(true)
        ansPrm.statusCode = if (g.call == null) pjsip_status_code.PJSIP_SC_OK else pjsip_status_code.PJSIP_SC_BUSY_HERE
        try {
            call.answer(ansPrm)
            if (g.call == null)
                g.call = call
        } catch (e: Exception) {
            println(e)
        }
    }
}

private fun getCallInfo(call: Call): CallInfo? {
    var ci : CallInfo? = null
    try {
        ci = call.info
    } catch (e: Exception) {
        println("Failed getting call info: $e")
    }
    return ci
}

/* Call implementation */
internal class MyCall(acc: Account, call_id: Int) : Call(acc, call_id) {

    override fun onCallState(prm: OnCallStateParam?) {
        val ci : CallInfo = getCallInfo(this) ?: return

        g.ep.utilLogWrite(3, "MyCall", "Call state changed to: " + ci.stateText)
        val m = Message.obtain(g.uiHandler, MSG_UPDATE_CALL_INFO, ci)
        m.sendToTarget()

        if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
            g.call = null
            g.localVideoHandler.resetVideoWindow()
            g.remoteVideoHandler.resetVideoWindow()
            if (g.previewStarted) {
                try {
                    VideoPreview(VIDEO_CAPTURE_DEVICE_ID).stop()
                } catch (e: Exception) {
                    println("Failed stopping video preview" + e.message)
                }
                g.previewStarted = false
            }
            g.ep.utilLogWrite(3, "MyCall", this.dump(true, ""))
        }
    }

    override fun onCallMediaState(prm: OnCallMediaStateParam?) {
        val ci : CallInfo = getCallInfo(this) ?: return
        val cmiv = ci.media
        for (i in cmiv.indices) {
            val cmi = cmiv[i]
            if (cmi.type == pjmedia_type.PJMEDIA_TYPE_AUDIO &&
                (cmi.status == pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE ||
                 cmi.status == pjsua_call_media_status.PJSUA_CALL_MEDIA_REMOTE_HOLD))
            {
                /* Connect ports */
                try {
                    val am = getAudioMedia(i)
                    g.ep.audDevManager().captureDevMedia.startTransmit(am)
                    am.startTransmit(g.ep.audDevManager().playbackDevMedia)
                } catch (e: Exception) {
                    println("Failed connecting media ports" + e.message)
                }
            } else if ((cmi.type == pjmedia_type.PJMEDIA_TYPE_VIDEO) &&
                       (cmi.status == pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE) &&
                       (cmi.dir and pjmedia_dir.PJMEDIA_DIR_ENCODING) != 0)
            {
                val m = Message.obtain(g.uiHandler, MSG_SHOW_LOCAL_VIDEO, cmi)
                m.sendToTarget()
            }
        }
    }

    override fun onCallMediaEvent(prm: OnCallMediaEventParam?) {
        if (prm!!.ev.type == pjmedia_event_type.PJMEDIA_EVENT_FMT_CHANGED) {
            val ci : CallInfo = getCallInfo(this) ?: return
            if (prm.medIdx < 0 || prm.medIdx >= ci.media.size)
                return

            /* Check if this event is from incoming video */
            val cmi = ci.media[prm.medIdx.toInt()]
            if (cmi.type != pjmedia_type.PJMEDIA_TYPE_VIDEO ||
                    cmi.status != pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE ||
                    cmi.videoIncomingWindowId == pjsua2.INVALID_ID)
                return

            /* Currently this is a new incoming video */
            println("Got remote video format change = " +prm.ev.data.fmtChanged.newWidth + "x" + prm.ev.data.fmtChanged.newHeight)
            val m = Message.obtain(g.uiHandler, MSG_SHOW_REMOTE_VIDEO, cmi)
            m.sendToTarget()
        }
    }

}

class VideoSurfaceHandler : SurfaceHolder.Callback {
    lateinit var holder: SurfaceHolder
    private var videoWindow : WeakReference<VideoWindow>? = null
    private var active = false

    fun setVideoWindow(vw: VideoWindow) {
        videoWindow = WeakReference<VideoWindow>(vw);
        active = true
        setSurfaceHolder(holder)
    }

    fun resetVideoWindow() {
        active = false
        videoWindow = null
    }

    private fun setSurfaceHolder(holder: SurfaceHolder?) {
        if (!active) return

        try {
            val wh = VideoWindowHandle()
            wh.handle.setWindow(holder?.surface)
            videoWindow?.get()?.setWindow(wh)
        } catch (e: java.lang.Exception) {
            println(e)
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) {
        setSurfaceHolder(holder)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {}

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        try {
            setSurfaceHolder(null)
        } catch (e: java.lang.Exception) {
            println(e)
        }
    }
}


class MainActivity : AppCompatActivity(), android.os.Handler.Callback {

    private fun checkPermissions() {
        val permissions = arrayOf(
                Manifest.permission.CAMERA,
                Manifest.permission.RECORD_AUDIO
        )
        ActivityCompat.requestPermissions(this@MainActivity, permissions, 0)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        /* Check permissions */
        checkPermissions()

        /* Set Camera Manager for PJMEDIA video */
        val cm = getSystemService(CAMERA_SERVICE) as CameraManager
        PjCameraInfo2.SetCameraManager(cm)

        /* Set surface view callback */
        val svLocalVideo = findViewById<View>(R.id.svLocalVideo) as SurfaceView
        val svRemoteVideo = findViewById<View>(R.id.svRemoteVideo) as SurfaceView
        g.localVideoHandler.holder = svLocalVideo.holder
        g.remoteVideoHandler.holder = svRemoteVideo.holder
        svLocalVideo.holder.addCallback(g.localVideoHandler)
        svRemoteVideo.holder.addCallback(g.remoteVideoHandler)

        /* Setup UI handler */
        g.uiHandler = Handler(this.mainLooper, this)

        /* Setup Start button */
        val buttonStart = findViewById<Button>(R.id.button_start)
        buttonStart.setOnClickListener {

            if (g.ep.libGetState() > pjsua_state.PJSUA_STATE_NULL)
                return@setOnClickListener

            val epConfig = EpConfig()

            /* Setup our log writer */
            val logCfg = epConfig.logConfig
            g.logWriter = MyLogWriter()
            logCfg.writer = g.logWriter
            logCfg.decor = logCfg.decor and
                    (pj_log_decoration.PJ_LOG_HAS_CR or
                            pj_log_decoration.PJ_LOG_HAS_NEWLINE).inv().toLong()

            /* Create & init PJSUA2 */
            try {
                g.ep.libCreate()
                g.ep.libInit(epConfig)
            } catch (e: Exception) {
                println(e)
            }

            /* Create transports and account. */
            try {
                val sipTpConfig = TransportConfig()
                sipTpConfig.port = SIP_LISTENING_PORT.toLong()
                g.ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP,
                        sipTpConfig)

                val accCfg = AccountConfig()
                accCfg.idUri = ACC_ID_URI
                accCfg.videoConfig.autoShowIncoming = true
                accCfg.videoConfig.autoTransmitOutgoing = true
                accCfg.videoConfig.defaultCaptureDevice = VIDEO_CAPTURE_DEVICE_ID
                g.acc.create(accCfg, true)
            } catch (e: Exception) {
                println(e)
            }

            /* Start PJSUA2 */
            try {
                g.ep.libStart()
            } catch (e: Exception) {
                println(e)
            }
            findViewById<TextView>(R.id.text_info).text = "Library started"

            /* Fix camera orientation to portrait mode (for front camera) */
            try {
                g.ep.vidDevManager().setCaptureOrient(VIDEO_CAPTURE_DEVICE_ID,
                        pjmedia_orient.PJMEDIA_ORIENT_ROTATE_270DEG, true)

                /* Also adjust size in H264 encoding param */
                var codecs = g.ep.videoCodecEnum2()
                var codecId = "H264/"
                for (c in codecs) {
                    if (c.codecId.startsWith(codecId)) {
                        codecId = c.codecId
                        break
                    }
                }
                var vcp = g.ep.getVideoCodecParam(codecId)
                vcp.encFmt.width = 240
                vcp.encFmt.height = 320
                g.ep.setVideoCodecParam(codecId, vcp)
            } catch (e: Exception) {
                println(e)
            }

        }

        /* Setup Call button */
        val buttonCall = findViewById<Button>(R.id.button_call)
        buttonCall.setOnClickListener {
            if (g.ep.libGetState() != pjsua_state.PJSUA_STATE_RUNNING)
                return@setOnClickListener

            if (g.call == null) {
                try {
                    /* Setup null audio (good for emulator) */
                    g.ep.audDevManager().setNullDev()

                    /* Make call (to itself) */
                    val call = MyCall(g.acc, -1)
                    val prm = CallOpParam(true)
                    call.makeCall(CALL_DST_URI, prm)
                    g.call = call
                } catch (e: Exception) {
                    println(e)
                }
            } else {
                try {
                    g.ep.hangupAllCalls()
                } catch (e: Exception) {
                    println(e)
                }
            }
        }

        /* Setup Stop button */
        val buttonStop = findViewById<Button>(R.id.button_stop)
        buttonStop.setOnClickListener {
            if (g.ep.libGetState() == pjsua_state.PJSUA_STATE_NULL)
                return@setOnClickListener

            /* Destroy PJSUA2 */
            try {
                g.ep.hangupAllCalls()
                g.ep.libDestroy()
            } catch (e: Exception) {
                println(e)
            }
            findViewById<TextView>(R.id.text_info).text = "Library stopped"
        }
    }

    override fun handleMessage(m: Message): Boolean {
        if (m.what == MSG_UPDATE_CALL_INFO) {
            val ci = m.obj as CallInfo

            /* Update button text */
            val buttonCall = findViewById<Button>(R.id.button_call)
            if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED)
                buttonCall.text = "Call"
            else if (ci.state > pjsip_inv_state.PJSIP_INV_STATE_NULL)
                buttonCall.text = "Hangup"

            /* Update call state text */
            val textInfo = findViewById<TextView>(R.id.text_info)
            textInfo.text = ci.stateText

            /* Hide video windows upon disconnection */
            if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
                val svLocalVideo = findViewById<View>(R.id.svLocalVideo) as SurfaceView
                val svRemoteVideo = findViewById<View>(R.id.svRemoteVideo) as SurfaceView
                svLocalVideo.visibility = View.INVISIBLE
                svRemoteVideo.visibility = View.INVISIBLE
            }

        } else if (m.what == MSG_SHOW_LOCAL_VIDEO) {
            try {
                if (g.previewStarted)
                    return false;

                /* Start preview, position it in the bottom right of the screen */
                val vp = VideoPreview(VIDEO_CAPTURE_DEVICE_ID)
                vp.start(VideoPreviewOpParam())
                g.previewStarted = true

                val videoLayout = findViewById<View>(R.id.video_layout) as RelativeLayout
                val vwi = vp.videoWindow.info
                var w = vwi.size.w.toInt()
                var h = vwi.size.h.toInt()

                /* Adjust width to match the parent layout */
                h = (videoLayout.measuredWidth.toDouble() / 2 / w * h).toInt()
                w = videoLayout.measuredWidth / 2

                /* Also adjust height to match the parent layout */
                if (h > videoLayout.measuredHeight / 2) {
                    w = (videoLayout.measuredHeight.toDouble() / 2 / h * w).toInt()
                    h = videoLayout.measuredHeight / 2
                }
                println("Layout size=" + videoLayout.measuredWidth + "x" + videoLayout.measuredHeight)
                println("Local video size=" + vwi.size.w + "x" + vwi.size.h + " -> " + w + "x" + h)

                /* Resize the preview surface */
                val svLocalVideo = findViewById<View>(R.id.svLocalVideo) as SurfaceView
                var params = RelativeLayout.LayoutParams(w, h)
                params.leftMargin = videoLayout.measuredWidth - w
                params.topMargin = videoLayout.measuredHeight - h
                svLocalVideo.layoutParams = params
                svLocalVideo.visibility = View.VISIBLE

                /* Link the video window to the surface view */
                g.localVideoHandler.setVideoWindow(vp.videoWindow)
            } catch (e: Exception) {
                println("Failed showing local video" + e.message)
            }

        } else if (m.what == MSG_SHOW_REMOTE_VIDEO) {
            val cmi = m.obj as CallMediaInfo
            val si: StreamInfo = g.call!!.getStreamInfo(g.call!!.vidGetStreamIdx().toLong())
            try {
                val videoLayout = findViewById<View>(R.id.video_layout) as RelativeLayout
                var w = si.vidCodecParam.decFmt.width.toInt()
                var h = si.vidCodecParam.decFmt.height.toInt()

                /* Adjust width to match the parent layout */
                h = (videoLayout.measuredWidth.toDouble() / w * h).toInt()
                w = videoLayout.measuredWidth

                /* Also adjust height to match the parent layout */
                if (h > videoLayout.measuredHeight) {
                    w = (videoLayout.measuredHeight.toDouble() / h * w).toInt()
                    h = videoLayout.measuredHeight
                }
                println("Remote video size=" + si.vidCodecParam.decFmt.width + "x" + si.vidCodecParam.decFmt.height + " -> " + w + "x" + h)

                /* Resize the remote video surface */
                val svRemoteVideo = findViewById<View>(R.id.svRemoteVideo) as SurfaceView
                var params = RelativeLayout.LayoutParams(w, h)
                params.leftMargin = if (w == videoLayout.measuredWidth) 0 else (videoLayout.measuredWidth-w)/2
                params.topMargin = 0
                svRemoteVideo.layoutParams = params
                svRemoteVideo.visibility = View.VISIBLE

                /* Put local preview always on top */
                val svLocalVideo = findViewById<View>(R.id.svLocalVideo) as SurfaceView
                svLocalVideo.visibility = View.VISIBLE

                /* Link the video window to the surface view */
                g.remoteVideoHandler.setVideoWindow(cmi.videoWindow)
            } catch (e: Exception) {
                println("Failed showing remote video" + e.message)
            }

        } else {

            /* Message not handled */
            return false
        }
        return true
    }

}
