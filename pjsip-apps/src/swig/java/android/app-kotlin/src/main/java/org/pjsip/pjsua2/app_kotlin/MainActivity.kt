package org.pjsip.pjsua2.app_kotlin

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.widget.Button
import org.pjsip.pjsua2.*

/* Global objects */
internal object g {
    /* Maintain reference to avoid auto garbage collecting */
    lateinit var logWriter: MyLogWriter

    val ep = Endpoint()
    val acc = MyAccount()
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
        val ansPrm = CallOpParam()
        ansPrm.statusCode = pjsip_status_code.PJSIP_SC_OK
        try {
            call.answer(ansPrm)
        } catch (e: Exception) {
            println(e)
        }
    }
}

/* Call implementation */
internal class MyCall(acc: Account, call_id: Int) : Call(acc, call_id) {
    override fun onCallState(prm: OnCallStateParam?) {
        val ci : CallInfo
        try {
            ci = info
        } catch (e: Exception) {
            println(e)
            return
        }

        g.ep.utilLogWrite(3, "MyCall", "Call state changed to: " + ci.getStateText())
        if (ci.state == pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED) {
            g.ep.utilLogWrite(3, "MyCall", this.dump(true, ""))
        }
    }

    override fun onCallMediaState(prm: OnCallMediaStateParam?) {
        val ci : CallInfo
        try {
            ci = info
        } catch (e: Exception) {
            println(e)
            return
        }

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
                    continue
                }
            }
        }
    }
}

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

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
                sipTpConfig.port = 5060
                g.ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP,
                        sipTpConfig)

                val accCfg = AccountConfig()
                accCfg.idUri = "sip:localhost"
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
        }

        /* Setup Call button */
        val buttonCall = findViewById<Button>(R.id.button_call)
        buttonCall.setOnClickListener {
            if (g.ep.libGetState() != pjsua_state.PJSUA_STATE_RUNNING)
                return@setOnClickListener

            try {
                /* Setup null audio (good for emulator) */
                g.ep.audDevManager().setNullDev()

                /* Make call (to itself) */
                val call = MyCall(g.acc, -1)
                call.makeCall("sip:localhost", CallOpParam(true))
            } catch (e: Exception) {
                println(e)
            }
        }

        /* Setup Stop button */
        val buttonStop = findViewById<Button>(R.id.button_stop)
        buttonStop.setOnClickListener {
            /* Destroy PJSUA2 */
            try {
                g.ep.libDestroy()
            } catch (e: Exception) {
                println(e)
            }
        }
    }
}
