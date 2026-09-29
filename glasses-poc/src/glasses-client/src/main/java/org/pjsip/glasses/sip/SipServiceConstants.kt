package org.pjsip.glasses.sip

/**
 * Intent contract between the UI/voice/gesture layers and [SipService].
 * Mirrors the SipServiceCommand/SipServiceConstants split used by
 * VoiSmart pjsip-android so the SIP stack stays hidden behind a few
 * actions and extras.
 */
object SipServiceConstants {
    const val PKG = "org.pjsip.glasses"

    /* Commands (Intent actions handled by SipService) */
    const val ACTION_SET_ACCOUNT = "$PKG.action.SET_ACCOUNT"
    const val ACTION_REMOVE_ACCOUNT = "$PKG.action.REMOVE_ACCOUNT"
    const val ACTION_MAKE_CALL = "$PKG.action.MAKE_CALL"
    const val ACTION_ANSWER_CALL = "$PKG.action.ANSWER_CALL"
    const val ACTION_HANGUP_CALL = "$PKG.action.HANGUP_CALL"
    const val ACTION_SET_MUTE = "$PKG.action.SET_MUTE"
    const val ACTION_TRANSFER_CALL = "$PKG.action.TRANSFER_CALL"
    const val ACTION_SEND_DTMF = "$PKG.action.SEND_DTMF"
    const val ACTION_NETWORK_CHANGED = "$PKG.action.NETWORK_CHANGED"
    const val ACTION_STOP_SERVICE = "$PKG.action.STOP_SERVICE"

    /* Events (broadcast by SipService, consumed by overlay / AI bridge) */
    const val EVENT_REGISTRATION = "$PKG.event.REGISTRATION"
    const val EVENT_INCOMING_CALL = "$PKG.event.INCOMING_CALL"
    const val EVENT_CALL_STATE = "$PKG.event.CALL_STATE"
    const val EVENT_CALL_MEDIA = "$PKG.event.CALL_MEDIA"
    const val EVENT_TRANSCRIPT = "$PKG.event.TRANSCRIPT"
    const val EVENT_ERROR = "$PKG.event.ERROR"

    /* Extras */
    const val EXTRA_ACC_ID_URI = "acc_id_uri"        // "Tech <sip:tech1@pbx>"
    const val EXTRA_ACC_REGISTRAR = "acc_registrar"  // "sip:pbx;transport=tls"
    const val EXTRA_ACC_PROXY = "acc_proxy"          // "sip:pbx;lr;transport=tls"
    const val EXTRA_ACC_USER = "acc_user"
    const val EXTRA_ACC_PASS = "acc_pass"
    const val EXTRA_TURN_SERVER = "turn_server"      // "turn.edge:443"
    const val EXTRA_TURN_USER = "turn_user"
    const val EXTRA_TURN_PASS = "turn_pass"
    const val EXTRA_STUN_SERVER = "stun_server"

    const val EXTRA_CALL_ID = "call_id"
    const val EXTRA_DST_URI = "dst_uri"
    const val EXTRA_DISPLAY_NAME = "display_name"
    const val EXTRA_HEADERS = "headers"              // Bundle of String->String
    const val EXTRA_VISION_CONTEXT = "vision_context" // JSON, becomes X-Meta-Vision-Context
    const val EXTRA_MUTE = "mute"
    const val EXTRA_DTMF = "dtmf"
    const val EXTRA_CALL_STATE = "call_state"
    const val EXTRA_CALL_STATE_TEXT = "call_state_text"
    const val EXTRA_REMOTE_URI = "remote_uri"
    const val EXTRA_REG_CODE = "reg_code"
    const val EXTRA_REG_ACTIVE = "reg_active"
    const val EXTRA_ERROR = "error"
    const val EXTRA_TRANSCRIPT_TEXT = "text"
    const val EXTRA_TRANSCRIPT_SIDE = "side"         // "near" | "far"

    /* Custom SIP headers */
    const val HDR_VISION_CONTEXT = "X-Meta-Vision-Context"
    const val HDR_GLASSES_DEVICE = "X-Meta-Glasses-Device"

    /* Local TCP port where PcmTapPort streams raw PCM for the AI bridge */
    const val PCM_TAP_PORT = 7788
}
