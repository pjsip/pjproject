package org.pjsip.glasses.sip

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.os.IBinder
import android.util.Log
import org.pjsip.glasses.audio.AudioRouter
import org.pjsip.glasses.sip.SipServiceConstants as C
import org.pjsip.pjsua2.AccountConfig
import org.pjsip.pjsua2.AuthCredInfo
import org.pjsip.pjsua2.CallOpParam
import org.pjsip.pjsua2.Endpoint
import org.pjsip.pjsua2.SipHeader
import org.pjsip.pjsua2.StringVector
import org.pjsip.pjsua2.TransportConfig
import org.pjsip.pjsua2.pj_turn_tp_type
import org.pjsip.pjsua2.pjsip_transport_type_e

/**
 * Foreground service that owns the PJSUA2 [Endpoint].
 *
 * Contract:
 *  - Every PJSUA2 call happens on [worker]; it is the thread that ran
 *    libCreate(), so it is registered with PJLIB.
 *  - Commands arrive as Intents (see [SipServiceCommand]); events leave
 *    as broadcasts (see [SipEvents]).
 *  - PJSUA2 callbacks run on PJSUA's worker thread holding PJSUA_LOCK and
 *    only emit events.
 */
class SipService : Service() {

    companion object {
        private const val TAG = "SipService"
        private const val CHANNEL_ID = "glasses_sip"
        private const val NOTIF_ID = 1001

        init {
            System.loadLibrary("pjsua2")
        }
    }

    lateinit var ep: Endpoint
        private set
    val events by lazy { SipEvents(this) }
    var nearTap: PcmTapPort? = null
        private set
    var farTap: PcmTapPort? = null
        private set

    private val worker = HandlerThread("pjsua2-worker")
    private lateinit var handler: Handler
    private lateinit var audioRouter: AudioRouter
    private var tapServer: PcmTapServer? = null
    private var account: GlassesAccount? = null
    @Volatile private var activeCall: GlassesCall? = null
    private var started = false

    /* ---------------------------------------------------------------- */
    /* Service lifecycle                                                */
    /* ---------------------------------------------------------------- */

    override fun onCreate() {
        super.onCreate()
        audioRouter = AudioRouter(this)
        worker.start()
        handler = Handler(worker.looper)
        handler.post { startStack() }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        promoteToForeground("Glasses SIP ready")
        if (intent != null) handler.post { dispatch(intent) }
        return START_STICKY
    }

    override fun onDestroy() {
        handler.post { stopStack() }
        worker.quitSafely()
        worker.join(3000)
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    /* ---------------------------------------------------------------- */
    /* Stack init / teardown (worker thread)                            */
    /* ---------------------------------------------------------------- */

    private fun startStack() {
        if (started) return
        try {
            ep = Endpoint()
            ep.libCreate()
            ep.libInit(GlassesEpConfig.build())

            /* TLS first (keepalive-friendly, carries big vision headers), UDP as fallback. */
            ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_TLS,
                TransportConfig().apply { port = 0 })
            ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP,
                TransportConfig().apply { port = 0 })

            ep.libStart()
            GlassesEpConfig.applyCodecs(ep)

            nearTap = PcmTapPort("near", GlassesEpConfig.CLOCK_RATE, GlassesEpConfig.PTIME_MS).also { it.create() }
            farTap = PcmTapPort("far", GlassesEpConfig.CLOCK_RATE, GlassesEpConfig.PTIME_MS).also { it.create() }
            tapServer = PcmTapServer(nearTap!!, farTap!!).also { it.start() }

            started = true
            Log.i(TAG, "PJSUA2 started: ${ep.libVersion().full}")
        } catch (e: Exception) {
            Log.e(TAG, "PJSUA2 start failed", e)
            events.error("stack start failed: ${e.message}")
        }
    }

    private fun stopStack() {
        if (!started) return
        try {
            tapServer?.stop()
            activeCall?.let { try { it.hangup(CallOpParam()) } catch (_: Exception) {} }
            activeCall = null
            account?.delete(); account = null
            nearTap?.delete(); farTap?.delete()
            nearTap = null; farTap = null
            ep.libDestroy()
            ep.delete()
        } catch (e: Exception) {
            Log.w(TAG, "stop error", e)
        }
        started = false
    }

    /* ---------------------------------------------------------------- */
    /* Command dispatch (worker thread)                                 */
    /* ---------------------------------------------------------------- */

    private fun dispatch(i: Intent) {
        if (!started && i.action != C.ACTION_STOP_SERVICE) {
            events.error("stack not started; ignoring ${i.action}")
            return
        }
        try {
            when (i.action) {
                C.ACTION_SET_ACCOUNT -> setAccount(i)
                C.ACTION_REMOVE_ACCOUNT -> { account?.delete(); account = null }
                C.ACTION_MAKE_CALL -> makeCall(i)
                C.ACTION_ANSWER_CALL -> callFor(i)?.answer(CallOpParam(true).apply {
                    statusCode = org.pjsip.pjsua2.pjsip_status_code.PJSIP_SC_OK
                })
                C.ACTION_HANGUP_CALL -> callFor(i)?.hangup(CallOpParam())
                C.ACTION_SET_MUTE -> callFor(i)?.setMute(i.getBooleanExtra(C.EXTRA_MUTE, false))
                C.ACTION_TRANSFER_CALL -> callFor(i)?.xfer(i.getStringExtra(C.EXTRA_DST_URI)!!, CallOpParam())
                C.ACTION_SEND_DTMF -> callFor(i)?.dialDtmf(i.getStringExtra(C.EXTRA_DTMF) ?: "")
                C.ACTION_NETWORK_CHANGED -> onNetworkChanged()
                C.ACTION_STOP_SERVICE -> { stopForeground(STOP_FOREGROUND_REMOVE); stopSelf() }
                else -> Log.w(TAG, "unknown action ${i.action}")
            }
        } catch (e: Exception) {
            Log.e(TAG, "command ${i.action} failed", e)
            events.error("${i.action}: ${e.message}")
        }
    }

    private fun setAccount(i: Intent) {
        account?.delete()
        val cfg = AccountConfig()
        cfg.idUri = i.getStringExtra(C.EXTRA_ACC_ID_URI)
        cfg.regConfig.registrarUri = i.getStringExtra(C.EXTRA_ACC_REGISTRAR)
        cfg.regConfig.registerOnAdd = true
        cfg.regConfig.timeoutSec = 600          // fewer REGISTERs = less radio wake
        cfg.regConfig.retryIntervalSec = 30
        i.getStringExtra(C.EXTRA_ACC_PROXY)?.let { cfg.sipConfig.proxies.add(it) }
        cfg.sipConfig.authCreds.add(AuthCredInfo("digest", "*",
            i.getStringExtra(C.EXTRA_ACC_USER), 0, i.getStringExtra(C.EXTRA_ACC_PASS)))

        val nat = cfg.natConfig
        nat.contactRewriteUse = 1
        nat.viaRewriteUse = 1
        nat.sdpNatRewriteUse = 1
        nat.udpKaIntervalSec = 0                // TLS keepalive does the job
        i.getStringExtra(C.EXTRA_STUN_SERVER)?.let { stun ->
            ep.natUpdateStunServers(StringVector().apply { add(stun) }, false)
        }
        i.getStringExtra(C.EXTRA_TURN_SERVER)?.let { turn ->
            nat.iceEnabled = true
            nat.turnEnabled = true
            nat.turnServer = turn
            nat.turnConnType = pj_turn_tp_type.PJ_TURN_TP_TLS
            nat.turnUserName = i.getStringExtra(C.EXTRA_TURN_USER) ?: ""
            nat.turnPasswordType = 0
            nat.turnPassword = i.getStringExtra(C.EXTRA_TURN_PASS) ?: ""
        }

        cfg.mediaConfig.srtpUse = org.pjsip.pjsua2.pjmedia_srtp_use.PJMEDIA_SRTP_OPTIONAL
        cfg.mediaConfig.srtpSecureSignaling = 0

        account = GlassesAccount(this).also { it.create(cfg) }
    }

    private fun makeCall(i: Intent) {
        val acc = account ?: run { events.error("no account"); return }
        if (activeCall != null) { events.error("call already active"); return }
        val dst = i.getStringExtra(C.EXTRA_DST_URI) ?: run { events.error("no dst"); return }

        val prm = CallOpParam(true)
        prm.txOption.headers.add(SipHeader().apply {
            hName = C.HDR_GLASSES_DEVICE
            hValue = Build.MODEL
        })
        i.getStringExtra(C.EXTRA_VISION_CONTEXT)?.let { ctx ->
            prm.txOption.headers.add(SipHeader().apply {
                hName = C.HDR_VISION_CONTEXT
                hValue = ctx.take(1024)
            })
        }
        (i.getBundleExtra(C.EXTRA_HEADERS))?.let { b ->
            for (k in b.keySet()) prm.txOption.headers.add(SipHeader().apply {
                hName = k; hValue = b.getString(k) ?: ""
            })
        }
        /* adb cannot pass a Bundle: accept --es hdr_<Name> <value> too. */
        i.extras?.let { b ->
            for (k in b.keySet()) if (k.startsWith("hdr_")) prm.txOption.headers.add(SipHeader().apply {
                hName = k.removePrefix("hdr_"); hValue = b.getString(k) ?: ""
            })
        }

        audioRouter.enterCallMode()
        val call = GlassesCall(this, acc)
        activeCall = call
        promoteToForeground("Calling ${i.getStringExtra(C.EXTRA_DISPLAY_NAME) ?: dst}")
        call.makeCall(dst, prm)
    }

    private fun onNetworkChanged() {
        /* Re-REGISTER with the new address, then re-INVITE the live call so
         * ICE picks a new path. Cheap on TLS because the socket is reopened
         * by PJSIP on the next send. */
        account?.setRegistration(true)
        activeCall?.let { try { it.reinvite(CallOpParam(true)) } catch (_: Exception) {} }
    }

    private fun callFor(i: Intent): GlassesCall? {
        val id = i.getIntExtra(C.EXTRA_CALL_ID, -1)
        val c = activeCall
        return if (c != null && (id < 0 || c.id == id)) c else null.also {
            events.error("no call with id $id")
        }
    }

    /* ---------------------------------------------------------------- */
    /* Called from PJSUA2 callbacks (PJSUA worker thread)               */
    /* ---------------------------------------------------------------- */

    /** Returns false if a call is already active (caller should reject). */
    fun registerCall(call: GlassesCall): Boolean {
        if (activeCall != null) return false
        activeCall = call
        audioRouter.enterCallMode()
        return true
    }

    fun onCallEnded(call: GlassesCall) {
        if (activeCall === call) activeCall = null
        audioRouter.leaveCallMode()
        /* Defer delete: we are inside the call's own callback. */
        handler.post { call.delete(); promoteToForeground("Glasses SIP ready") }
    }

    /* ---------------------------------------------------------------- */
    /* Foreground plumbing                                              */
    /* ---------------------------------------------------------------- */

    private fun promoteToForeground(text: String) {
        val nm = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            nm.createNotificationChannel(NotificationChannel(CHANNEL_ID, "Glasses SIP",
                NotificationManager.IMPORTANCE_LOW))
        }
        val n: Notification = Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("Glasses SIP")
            .setContentText(text)
            .setSmallIcon(android.R.drawable.sym_action_call)
            .setOngoing(true)
            .build()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIF_ID, n,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_PHONE_CALL or
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE)
        } else {
            startForeground(NOTIF_ID, n)
        }
    }
}
