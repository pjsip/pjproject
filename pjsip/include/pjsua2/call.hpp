/* $Id$ */
/*
 * Copyright (C) 2012-2013 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __PJSUA2_CALL_HPP__
#define __PJSUA2_CALL_HPP__

/**
 * @file pjsua2/call.hpp
 * @brief PJSUA2 Call manipulation
 */
#include <pjsua-lib/pjsua.h>
#include <pjsua2/media.hpp>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_CALL Call
 * @ingroup PJSUA2_Ref
 */

/**
 * @defgroup PJSUA2_Call_Data_Structure Call Related Types
 * @ingroup PJSUA2_DS
 * @{
 */

using std::string;
using std::vector;

//////////////////////////////////////////////////////////////////////////////

/**
 * Media stream, corresponds to pjmedia_stream
 */
typedef void *MediaStream;

/**
 * Media transport, corresponds to pjmedia_transport
 */
typedef void *MediaTransport;

/**
 * This structure describes statistics state.
 */
struct MathStat
{
    int n;      /**< number of samples  */
    int max;    /**< maximum value      */
    int min;    /**< minimum value      */
    int last;   /**< last value         */
    int mean;   /**< mean               */

public:
    /**
     * Default constructor
     */
    MathStat();
    
    /**
     * Convert from pjsip
     */
    void fromPj(const pj_math_stat &prm);
};

/**
 * Types of loss detected.
 */
struct LossType
{
    unsigned        burst;	/**< Burst/sequential packet lost detected  */
    unsigned        random;	/**< Random packet lost detected.	    */
};

/**
 * Unidirectional RTP stream statistics.
 */
struct RtcpStreamStat
{
    TimeVal	    update;	/**< Time of last update.		    */
    unsigned	    updateCount;/**< Number of updates (to calculate avg)   */
    unsigned	    pkt;	/**< Total number of packets		    */
    unsigned	    bytes;	/**< Total number of payload/bytes	    */
    unsigned	    discard;	/**< Total number of discarded packets.	    */
    unsigned	    loss;	/**< Total number of packets lost	    */
    unsigned	    reorder;	/**< Total number of out of order packets   */
    unsigned	    dup;	/**< Total number of duplicates packets	    */
    
    MathStat        lossPeriodUsec; /**< Loss period statistics 	    */

    LossType        lossType;   /**< Types of loss detected.                */
    
    MathStat        jitterUsec;	/**< Jitter statistics                      */
    
public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_rtcp_stream_stat &prm);
};

/**
 * RTCP SDES structure.
 */
struct RtcpSdes
{
    string	cname;		/**< RTCP SDES type CNAME.	*/
    string	name;		/**< RTCP SDES type NAME.	*/
    string	email;		/**< RTCP SDES type EMAIL.	*/
    string	phone;		/**< RTCP SDES type PHONE.	*/
    string	loc;		/**< RTCP SDES type LOC.	*/
    string	tool;		/**< RTCP SDES type TOOL.	*/
    string	note;		/**< RTCP SDES type NOTE.	*/

public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_rtcp_sdes &prm);
};
    
/**
 * Bidirectional RTP stream statistics.
 */
struct RtcpStat
{
    TimeVal		start;          /**< Time when session was created  */
    
    RtcpStreamStat      txStat;         /**< Encoder stream statistics.	    */
    RtcpStreamStat      rxStat;         /**< Decoder stream statistics.	    */
    
    MathStat            rttUsec;        /**< Round trip delay statistic.    */
    
    pj_uint32_t		rtpTxLastTs;    /**< Last TX RTP timestamp.         */
    pj_uint16_t		rtpTxLastSeq;   /**< Last TX RTP sequence.          */

    MathStat            rxIpdvUsec;     /**< Statistics of IP packet delay
                                             variation in receiving
                                             direction. It is only used when
                                             PJMEDIA_RTCP_STAT_HAS_IPDV is
                                             set to non-zero.               */

    MathStat            rxRawJitterUsec;/**< Statistic of raw jitter in
                                             receiving direction. It is only 
                                             used when
                                             PJMEDIA_RTCP_STAT_HAS_RAW_JITTER
                                             is set to non-zero.            */
    
    RtcpSdes            peerSdes;       /**< Peer SDES.			    */

public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_rtcp_stat &prm);
};

/**
 * This structure describes jitter buffer state.
 */
struct JbufState
{
    /* Setting */
    unsigned	frameSize;	    /**< Individual frame size, in bytes.   */
    unsigned	minPrefetch;	    /**< Minimum allowed prefetch, in frms. */
    unsigned	maxPrefetch;	    /**< Maximum allowed prefetch, in frms. */
    
    /* Status */
    unsigned	burst;		    /**< Current burst level, in frames	    */
    unsigned	prefetch;	    /**< Current prefetch value, in frames  */
    unsigned	size;		    /**< Current buffer size, in frames.    */
    
    /* Statistic */
    unsigned	avgDelayMsec;	    /**< Average delay, in ms.		    */
    unsigned	minDelayMsec;	    /**< Minimum delay, in ms.		    */
    unsigned	maxDelayMsec;	    /**< Maximum delay, in ms.		    */
    unsigned	devDelayMsec;	    /**< Standard deviation of delay, in ms.*/
    unsigned	avgBurst;	    /**< Average burst, in frames.	    */
    unsigned	lost;		    /**< Number of lost frames.		    */
    unsigned	discard;	    /**< Number of discarded frames.	    */
    unsigned	empty;		    /**< Number of empty on GET events.	    */
    
public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_jb_state &prm);
};

/**
 * This structure describes SDP session description. It corresponds to the
 * pjmedia_sdp_session structure.
 */
struct SdpSession
{
    /**
     * The whole SDP as a string.
     */
    string  wholeSdp;
    
    /**
     * Pointer to its original pjmedia_sdp_session. Only valid when the struct
     * is converted from PJSIP's pjmedia_sdp_session.
     */
    void   *pjSdpSession;

public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_sdp_session &sdp);
};


/**
 * This structure describes media transport informations. It corresponds to the
 * pjmedia_transport_info structure. The address name field can be empty string
 * if the address in the pjmedia_transport_info is invalid.
 */
struct MediaTransportInfo
{
    /**
     * Address to be advertised as the local address for the RTP socket, 
     * which does not need to be equal as the bound address (for example, 
     * this address can be the address resolved with STUN).
     */
    SocketAddress   localRtpName;

    /**
     * Address to be advertised as the local address for the RTCP socket, 
     * which does not need to be equal as the bound address (for example, 
     * this address can be the address resolved with STUN).
     */
    SocketAddress   localRtcpName;

    /**
     * Remote address where RTP originated from. This can be empty string if 
     * no data is received from the remote.
     */
    SocketAddress   srcRtpName;

    /**
     * Remote address where RTCP originated from. This can be empty string if 
     * no data is recevied from the remote.
     */
    SocketAddress   srcRtcpName;
    
public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_transport_info &info);
};

//////////////////////////////////////////////////////////////////////////////

/** Array of media direction */
typedef IntVector MediaDirVector;


/**
 * Call settings.
 */
struct CallSetting
{
    /**
     * Bitmask of pjsua_call_flag constants.
     *
     * Default: PJSUA_CALL_INCLUDE_DISABLED_MEDIA
     */
    unsigned	    flag;
    
    /**
     * This flag controls what methods to request keyframe are allowed on
     * the call. Value is bitmask of pjsua_vid_req_keyframe_method.
     *
     * Default: PJSUA_VID_REQ_KEYFRAME_SIP_INFO |
     *          PJSUA_VID_REQ_KEYFRAME_RTCP_PLI
     */
    unsigned	    reqKeyframeMethod;
    
    /**
     * Number of simultaneous active audio streams for this call. Setting
     * this to zero will disable audio in this call.
     *
     * Default: 1
     */
    unsigned        audioCount;
    
    /**
     * Number of simultaneous active video streams for this call. Setting
     * this to zero will disable video in this call.
     *
     * Default: 1 (if video feature is enabled, otherwise it is zero)
     */
    unsigned        videoCount;

    /**
     * Media direction. This setting will only be used if the flag
     * PJSUA_CALL_SET_MEDIA_DIR is set, and it will persist for subsequent
     * offers or answers.
     * For example, a media that is set as PJMEDIA_DIR_ENCODING can only
     * mark the stream in the SDP as sendonly or inactive, but will not
     * become sendrecv in subsequent offers and answers.
     * Application can update the media direction in any API or callback
     * that accepts CallSetting as a parameter, such as via
     * Call::reinvite/update() or in onCallRxOffer/Reinvite()
     * callback.
     *
     * The index of the media dir will correspond to the provisional media
     * in CallInfo.provMedia.
     * For offers that involve adding new medias (such as initial offer),
     * the index will correspond to all new audio media first, then video.
     * For example, for a new call with 2 audios and 1 video, mediaDir[0]
     * and mediaDir[1] will be for the audios, and mediaDir[2] video.
     *
     * Default: empty vector
     */
    MediaDirVector mediaDir;

    
public:
    /**
     * Default constructor initializes with empty or default values.
     */
    CallSetting(bool useDefaultValues = false);

    /**
     * Check if the settings are set with empty values.
     *
     * @return      True if the settings are empty.
     */
    bool isEmpty() const;

    /**
     * Convert from pjsip
     */
    void fromPj(const pjsua_call_setting &prm);

    /**
     * Convert to pjsip
     */
    pjsua_call_setting toPj() const;
};

/**
 * Call media information.
 *
 * Application can query conference bridge port of this media using
 * Call::getAudioMedia() if the media type is audio,
 * or Call::getEncodingVideoMedia()/Call::getDecodingVideoMedia()
 * if the media type is video.
 */
struct CallMediaInfo
{
    /**
     * Media index in SDP.
     */
    unsigned                index;
    
    /**
     * Media type.
     */
    pjmedia_type            type;
    
    /**
     * Media direction.
     */
    pjmedia_dir             dir;
    
    /**
     * Call media status.
     */
    pjsua_call_media_status status;
    
    /**
     * Warning: this is deprecated, application can query conference bridge
     * port of this media using Call::getAudioMedia().
     *
     * The conference port number for the call. Only valid if the media type
     * is audio.
     */
    int                     audioConfSlot;
    
    /**
     * The window id for incoming video, if any, or
     * PJSUA_INVALID_ID. Only valid if the media type is video.
     */
    pjsua_vid_win_id	    videoIncomingWindowId;
    
    /**
     * The video window instance for incoming video. Only valid if
     * videoIncomingWindowId is not PJSUA_INVALID_ID and
     * the media type is video.
     */
    VideoWindow	    	    videoWindow;

    /**
     * The video capture device for outgoing transmission, if any,
     * or PJMEDIA_VID_INVALID_DEV. Only valid if the media type is video.
     */
    pjmedia_vid_dev_index   videoCapDev;
    
public:
    /**
     * Default constructor
     */
    CallMediaInfo();
    
    /**
     * Convert from pjsip
     */
    void fromPj(const pjsua_call_media_info &prm);
};
    
/** Array of call media info */
typedef std::vector<CallMediaInfo> CallMediaInfoVector;

/**
 * Call information. Application can query the call information
 * by calling Call::getInfo().
 */
struct CallInfo
{
    /**
     * Call identification.
     */
    pjsua_call_id	id;
    
    /**
     * Initial call role (UAC == caller)
     */
    pjsip_role_e	role;
    
    /**
     * The account ID where this call belongs.
     */
    pjsua_acc_id	accId;
    
    /**
     * Local URI
     */
    string		localUri;
    
    /**
     * Local Contact
     */
    string		localContact;
    
    /**
     * Remote URI
     */
    string		remoteUri;
    
    /**
     * Remote contact
     */
    string		remoteContact;
    
    /**
     * Dialog Call-ID string.
     */
    string		callIdString;
    
    /**
     * Call setting
     */
    CallSetting         setting;
    
    /**
     * Call state
     */
    pjsip_inv_state	state;
    
    /**
     * Text describing the state
     */
    string		stateText;
    
    /**
     * Last status code heard, which can be used as cause code
     */
    pjsip_status_code	lastStatusCode;
    
    /**
     * The reason phrase describing the last status.
     */
    string		lastReason;
    
    /**
     * Array of active media information.
     */
    CallMediaInfoVector media;

    /**
     * Array of provisional media information. This contains the media info
     * in the provisioning state, that is when the media session is being
     * created/updated (SDP offer/answer is on progress).
     */
    CallMediaInfoVector provMedia;
    
    /**
     * Up-to-date call connected duration (zero when call is not
     * established)
     */
    TimeVal		connectDuration;
    
    /**
     * Total call duration, including set-up time
     */
    TimeVal		totalDuration;
    
    /**
     * Flag if remote was SDP offerer
     */
    bool		remOfferer;
    
    /**
     * Number of audio streams offered by remote
     */
    unsigned		remAudioCount;
    
    /**
     * Number of video streams offered by remote
     */
    unsigned		remVideoCount;

public:
    /**
     * Default constructor
     */
    CallInfo() : id(PJSUA_INVALID_ID),
		 role(PJSIP_ROLE_UAC),
		 state(PJSIP_INV_STATE_NULL),
		 lastStatusCode(PJSIP_SC_NULL)
    {}

    /**
     * Convert from pjsip
     */
    void fromPj(const pjsua_call_info &pci);
};

/**
 * Media stream info.
 */
struct StreamInfo
{
    /**
     * Media type of this stream.
     */
    pjmedia_type        type;

    /**
     * Transport protocol (RTP/AVP, etc.)
     */
    pjmedia_tp_proto	proto;
    
    /**
     * Media direction.
     */
    pjmedia_dir		dir;
    
    /**
     * Remote RTP address
     */
    SocketAddress	remoteRtpAddress;
    
    /**
     * Optional remote RTCP address
     */
    SocketAddress	remoteRtcpAddress;
    
    /**
     * Outgoing codec payload type.
     */
    unsigned		txPt;
    
    /**
     * Incoming codec payload type.
     */
    unsigned		rxPt;
    
    /**
     * Codec name.
     */
    string              codecName;
    
    /**
     * Codec clock rate.
     */
    unsigned            codecClockRate;
    
    /**
     * Optional audio codec param.
     */
    CodecParam          audCodecParam;

    /**
     * Optional video codec param.
     */
    VidCodecParam       vidCodecParam;

    /**
     * Jitter buffer init delay in msec.
     */
    int                 jbInit;

    /**
     * Jitter buffer minimum prefetch delay in msec.
     */
    int                 jbMinPre;

    /**
     * Jitter buffer maximum prefetch delay in msec.
     */
    int                 jbMaxPre;

    /**
     * Jitter buffer max delay in msec.
     */
    int                 jbMax;

    /**
     * Jitter buffer discard algorithm.
     */
    pjmedia_jb_discard_algo jbDiscardAlgo;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    /**
     * Stream keep-alive and NAT hole punch (see #PJMEDIA_STREAM_ENABLE_KA) is
     * enabled?
     */
    bool                useKa;
#endif

    /**
     * Disable automatic sending of RTCP SDES and BYE.
     */
    bool                rtcpSdesByeDisabled;

public:
    /**
     * Default constructor
     */
    StreamInfo() : type(PJMEDIA_TYPE_NONE),
		   proto(PJMEDIA_TP_PROTO_NONE),
		   dir(PJMEDIA_DIR_NONE)
    {}

    /**
     * Convert from pjsip
     */
    void fromPj(const pjsua_stream_info &info);
};

/**
 * Media stream statistic.
 */
struct StreamStat
{
    /**
     * RTCP statistic.
     */
    RtcpStat	rtcp;
    
    /**
     * Jitter buffer statistic.
     */
    JbufState	jbuf;

public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjsua_stream_stat &prm);
};

/**
 * This structure contains parameters for Call::onCallState() callback.
 */
struct OnCallStateParam
{
    /**
     * Event which causes the call state to change.
     */
    SipEvent    e;
};

/**
 * This structure contains parameters for Call::onCallTsxState() callback.
 */
struct OnCallTsxStateParam
{
    /**
     * Transaction event that caused the state change.
     */
    SipEvent    e;
};

/**
 * This structure contains parameters for Call::onCallMediaState() callback.
 */
struct OnCallMediaStateParam
{
};

/**
 * This structure contains parameters for Call::onCallSdpCreated() callback.
 */
struct OnCallSdpCreatedParam
{
    /**
     * The SDP has just been created.
     */
    SdpSession sdp;

    /**
     * The remote SDP, will be empty if local is SDP offerer.
     */
    SdpSession remSdp;
};

/**
 * This structure contains parameters for Call::onStreamPreCreate()
 * callback.
 */
struct OnStreamPreCreateParam
{
    /**
     * Stream index in the media session, read-only.
     */
    unsigned    streamIdx;

    /**
     * Parameters that the stream will be created from.
     */
    StreamInfo streamInfo;
};

/**
 * This structure contains parameters for Call::onStreamCreated()
 * callback.
 */
struct OnStreamCreatedParam
{
    /**
     * Audio media stream, read-only.
     */
    MediaStream stream;
    
    /**
     * Stream index in the audio media session, read-only.
     */
    unsigned    streamIdx;
    
    /**
     * Specify if PJSUA2 should take ownership of the port returned in
     * the pPort parameter below. If set to PJ_TRUE,
     * pjmedia_port_destroy() will be called on the port when it is
     * no longer needed.
     *
     * Default: PJ_FALSE
     */
    bool 	destroyPort;

    /**
     * On input, it specifies the audio media port of the stream. Application
     * may modify this pointer to point to different media port to be
     * registered to the conference bridge.
     */
    MediaPort   pPort;
};

/**
 * This structure contains parameters for Call::onStreamDestroyed()
 * callback.
 */
struct OnStreamDestroyedParam
{
    /**
     * Audio media stream.
     */
    MediaStream stream;
    
    /**
     * Stream index in the audio media session.
     */
    unsigned    streamIdx;
};

/**
 * This structure contains parameters for Call::onDtmfDigit()
 * callback.
 */
struct OnDtmfDigitParam
{
    /**
     * DTMF sending method.
     */
    pjsua_dtmf_method	method;

    /**
     * DTMF ASCII digit.
     */
    string		digit;

    /**
     * DTMF signal duration. If the duration is unknown, this value is set to
     * PJSUA_UNKNOWN_DTMF_DURATION.
     */
    unsigned		duration;
};

/**
 * This structure contains parameters for Call::onDtmfEvent()
 * callback.
 */
struct OnDtmfEventParam
{
    /**
     * DTMF sending method.
     */
    pjsua_dtmf_method   method;

    /**
     * The timestamp identifying the begin of the event. Timestamp units are
     * expressed in milliseconds.
     * Note that this value should only be used to compare multiple events
     * received via the same method relatively to each other, as the time-base
     * is randomized.
     */
    unsigned            timestamp;

    /**
     * DTMF ASCII digit.
     */
    string              digit;

    /**
     * DTMF signal duration in milliseconds. Interpretation of the duration
     * depends on the flag PJMEDIA_STREAM_DTMF_IS_END.
     * depends on the method.
     * If the method is PJSUA_DTMF_METHOD_SIP_INFO, this contains the total
     * duration of the DTMF signal or PJSUA_UNKNOWN_DTMF_DURATION if no signal
     * duration was indicated.
     * If the method is PJSUA_DTMF_METHOD_RFC2833, this contains the total
     * duration of the DTMF signal received up to this point in time.
     */
    unsigned            duration;

    /**
     * Flags indicating additional information about the DTMF event.
     * If PJMEDIA_STREAM_DTMF_IS_UPDATE is set, the event was already
     * indicated earlier. The new indication contains an updated event
     * duration.
     * If PJMEDIA_STREAM_DTMF_IS_END is set, the event has ended and this
     * indication contains the final event duration. Note that end
     * indications might get lost. Hence it is not guaranteed to receive
     * an event with PJMEDIA_STREAM_DTMF_IS_END for every event.
     */
    unsigned            flags;
};

/**
 * This structure contains parameters for Call::onCallTransferRequest()
 * callback.
 */
struct OnCallTransferRequestParam
{
    /**
     * The destination where the call will be transferred to.
     */
    string               dstUri;
    
    /**
     * Status code to be returned for the call transfer request. On input,
     * it contains status code 202.
     */
    pjsip_status_code    statusCode;
    
    /**
     * The current call setting, application can update this setting
     * for the call being transferred.
     */
    CallSetting          opt;

    /**
     * New Call derived object instantiated by application when the call
     * transfer is about to be accepted.
     */
    Call		*newCall;
};

/**
 * This structure contains parameters for Call::onCallTransferStatus()
 * callback.
 */
struct OnCallTransferStatusParam
{
    /**
     * Status progress of the transfer request.
     */
    pjsip_status_code   statusCode;
    
    /**
     * Status progress reason.
     */
    string              reason;
    
    /**
     * If true, no further notification will be reported. The statusCode
     * specified in this callback is the final status.
     */
    bool                finalNotify;
    
    /**
     * Initially will be set to true, application can set this to false
     * if it no longer wants to receive further notification (for example,
     * after it hangs up the call).
     */
    bool                cont;
};

/**
 * This structure contains parameters for Call::onCallReplaceRequest()
 * callback.
 */
struct OnCallReplaceRequestParam
{
    /**
     * The incoming INVITE request to replace the call.
     */
    SipRxData           rdata;
    
    /**
     * Status code to be set by application. Application should only
     * return a final status (200-699)
     */
    pjsip_status_code   statusCode;
    
    /**
     * Optional status text to be set by application.
     */
    string              reason;
    
    /**
     * The current call setting, application can update this setting for
     * the call being replaced.
     */
    CallSetting         opt;
};

/**
 * This structure contains parameters for Call::onCallReplaced() callback.
 */
struct OnCallReplacedParam
{
    /**
     * The new call id.
     */
    pjsua_call_id        newCallId;

    /**
     * New Call derived object instantiated by application.
     */
    Call		*newCall;
};

/**
 * This structure contains parameters for Call::onCallRxOffer() callback.
 */
struct OnCallRxOfferParam
{
    /**
     * The new offer received.
     */
    SdpSession          offer;
    
    /**
     * Status code to be returned for answering the offer. On input,
     * it contains status code 200. Currently, valid values are only
     * 200 and 488.
     */
    pjsip_status_code   statusCode;
    
    /**
     * The current call setting, application can update this setting for
     * answering the offer.
     */
    CallSetting         opt;
};

/**
 * This structure contains parameters for Call::onCallRxReinvite() callback.
 */
struct OnCallRxReinviteParam
{
    /**
     * The new offer received.
     */
    SdpSession          offer;

    /**
     * The incoming re-INVITE.
     */
    SipRxData           rdata;
    
    /**
     * On input, it is false. Set to true if app wants to manually answer
     * the re-INVITE.
     */
    bool		isAsync;
    
    /**
     * Status code to be returned for answering the offer. On input,
     * it contains status code 200. Currently, valid values are only
     * 200 and 488.
     */
    pjsip_status_code   statusCode;
    
    /**
     * The current call setting, application can update this setting for
     * answering the offer.
     */
    CallSetting         opt;
};

/**
 * This structure contains parameters for Call::onCallTxOffer() callback.
 */
struct OnCallTxOfferParam
{
    /**
     * The current call setting, application can update this setting for
     * generating the offer. Note that application should maintain any
     * active media to avoid the need for the peer to reject the offer.
     */
    CallSetting         opt;
};

/**
 * This structure contains parameters for Call::onCallRedirected() callback.
 */
struct OnCallRedirectedParam
{
    /**
     * The current target to be tried.
     */
    string          targetUri;
    
    /**
     * The event that caused this callback to be called.
     * This could be the receipt of 3xx response, or 4xx/5xx response
     * received for the INVITE sent to subsequent targets, or empty
     * (e.type == PJSIP_EVENT_UNKNOWN)
     * if this callback is called from within Call::processRedirect()
     * context.
     */
    SipEvent        e;
};

/**
 * This structure contains parameters for Call::onCallMediaEvent() callback.
 */
struct OnCallMediaEventParam
{
    /**
     * The media stream index.
     */
    unsigned        medIdx;
    
    /**
     * The media event.
     */
    MediaEvent      ev;
};

/**
 * This structure contains parameters for Call::onCallMediaTransportState()
 * callback.
 */
struct OnCallMediaTransportStateParam
{
    /**
     * The media index.
     */
    unsigned        medIdx;
    
    /**
     * The media transport state
     */
    pjsua_med_tp_st state;
    
    /**
     * The last error code related to the media transport state.
     */
    pj_status_t     status;
    
    /**
     * Optional SIP error code.
     */
    int             sipErrorCode;
};

/**
 * This structure contains parameters for Call::onCreateMediaTransport()
 * callback.
 */
struct OnCreateMediaTransportParam
{
    /**
     * The media index in the SDP for which this media transport will be used.
     */
    unsigned        mediaIdx;
    
    /**
     * The media transport which otherwise will be used by the call has this
     * callback not been implemented. Application can change this to its own
     * instance of media transport to be used by the call.
     */
    MediaTransport  mediaTp;
    
    /**
     * Bitmask from pjsua_create_media_transport_flag.
     */
    unsigned        flags;
};

/**
 * This structure contains parameters for Call::onCreateMediaTransportSrtp()
 * callback.
 */
struct OnCreateMediaTransportSrtpParam
{
    /**
     * The media index in the SDP for which the SRTP media transport
     * will be used.
     */
    unsigned        		mediaIdx;
    
    /**
     * Specify whether secure media transport should be used. Application
     * can modify this only for initial INVITE.
     * Valid values are PJMEDIA_SRTP_DISABLED, PJMEDIA_SRTP_OPTIONAL, and
     * PJMEDIA_SRTP_MANDATORY.
     */
    pjmedia_srtp_use		srtpUse;
    
    /**
     * Application can modify this to specify the cryptos and keys
     * which are going to be used.
     */
    SrtpCryptoVector		cryptos;
};

/**
 * @}  // PJSUA2_Call_Data_Structure
 */

/**
 * @addtogroup PJSUA2_CALL
 * @{
 */

/**
 * This structure contains parameters for Call::answer(), Call::hangup(),
 * Call::reinvite(), Call::update(), Call::xfer(), Call::xferReplaces(),
 * Call::setHold().
 */
struct CallOpParam
{
    /**
     * The call setting.
     */
    CallSetting         opt;
    
    /**
     * Status code.
     */
    pjsip_status_code   statusCode;
    
    /**
     * Reason phrase.
     */
    string              reason;
    
    /**
     * Options.
     */
    unsigned            options;
    
    /**
     * List of headers etc to be added to outgoing response message.
     * Note that this message data will be persistent in all next
     * answers/responses for this INVITE request.
     */
    SipTxOption         txOption;

    /**
     * SDP answer. Currently only used for Call::answer().
     */
    SdpSession		sdp;
    
public:
    /**
     * Default constructor initializes with zero/empty values.
     * Setting useDefaultCallSetting to true will initialize opt with default
     * call setting values.
     */
    CallOpParam(bool useDefaultCallSetting = false);
};

/**
 * This structure contains parameters for Call::sendRequest()
 */
struct CallSendRequestParam
{
    /**
     * SIP method of the request.
     */
    string       method;
    
    /**
     * Message body and/or list of headers etc to be included in
     * outgoing request.
     */
    SipTxOption  txOption;
    
public:
    /**
     * Default constructor initializes with zero/empty values.
     */
    CallSendRequestParam();
};

/**
 * This structure contains parameters for Call::vidSetStream()
 */
struct CallVidSetStreamParam
{
    /**
     * Specify the media stream index. This can be set to -1 to denote
     * the default video stream in the call, which is the first active
     * video stream or any first video stream if none is active.
     *
     * This field is valid for all video stream operations, except
     * PJSUA_CALL_VID_STRM_ADD.
     *
     * Default: -1 (first active video stream, or any first video stream
     *              if none is active)
     */
    int                     medIdx;
    
    /**
     * Specify the media stream direction.
     *
     * This field is valid for the following video stream operations:
     * PJSUA_CALL_VID_STRM_ADD and PJSUA_CALL_VID_STRM_CHANGE_DIR.
     *
     * Default: PJMEDIA_DIR_ENCODING_DECODING
     */
    pjmedia_dir             dir;
    
    /**
     * Specify the video capture device ID. This can be set to
     * PJMEDIA_VID_DEFAULT_CAPTURE_DEV to specify the default capture
     * device as configured in the account.
     *
     * This field is valid for the following video stream operations:
     * PJSUA_CALL_VID_STRM_ADD and PJSUA_CALL_VID_STRM_CHANGE_CAP_DEV.
     *
     * Default: PJMEDIA_VID_DEFAULT_CAPTURE_DEV.
     */
    pjmedia_vid_dev_index   capDev;
    
public:
    /**
     * Default constructor
     */
    CallVidSetStreamParam();
};

/**
 * This structure contains parameters for Call::sendDtmf()
 */
struct CallSendDtmfParam
{
    /**
     * The method used to send DTMF.
     * 
     * Default: PJSUA_DTMF_METHOD_RFC2833
     */
    pjsua_dtmf_method method;    

    /**
     * The signal duration used for the DTMF.
     *
     * Default: PJSUA_CALL_SEND_DTMF_DURATION_DEFAULT
     */
    unsigned duration;

    /**
     * The DTMF digits to be sent.
     */
    string digits;

public:
    /**
     * Default constructor initialize with default value.     
     */
    CallSendDtmfParam();

    /**
     * Convert to pjsip.
     */
    pjsua_call_send_dtmf_param toPj() const;

    /**
     * Convert from pjsip.
     */
    void fromPj(const pjsua_call_send_dtmf_param &param);
};

/**
 * Call.
 */
class Call
{
public:
    /**
     * Constructor.
     */
    Call(Account& acc, int call_id = PJSUA_INVALID_ID);

    /**
     * Destructor.
     */
    virtual ~Call();

    /**
     * Obtain detail information about this call.
     *
     * @return              Call info.
     */
    CallInfo getInfo() const PJSUA2_THROW(Error);
    
    /**
     * Check if this call has active INVITE session and the INVITE
     * session has not been disconnected.
     *
     * @return              True if call is active.
     */
    bool isActive() const;

    /**
     * Get PJSUA-LIB call ID or index associated with this call.
     *
     * @return              Integer greater than or equal to zero.
     */
    int getId() const;
    
    /**
     * Get the Call class for the specified call Id.
     *
     * @param call_id       The call ID to lookup
     *
     * @return              The Call instance or NULL if not found.
     */
    static Call *lookup(int call_id);

    /**
     * Check if call has an active media session.
     *
     * @return              True if yes.
     */
    bool hasMedia() const;
    
    /**
     * Warning: deprecated, use getAudioMedia() instead. This function is not
     * safe in multithreaded environment.
     *
     * Get media for the specified media index.
     *
     * @param med_idx       Media index.
     *
     * @return              The media or NULL if invalid or inactive.
     */
    Media *getMedia(unsigned med_idx) const;

    /**
     * Get audio media for the specified media index. If the specified media
     * index is not audio or invalid or inactive, exception will be thrown.
     *
     * @param med_idx       Media index, or -1 to specify any first audio
     *                      media registered in the conference bridge.
     *
     * @return              The audio media.
     */
    AudioMedia getAudioMedia(int med_idx) const PJSUA2_THROW(Error);

    /**
     * Get video media in encoding direction for the specified media index.
     * If the specified media index is not video or invalid or the direction
     * is receive only, exception will be thrown.
     *
     * @param med_idx       Media index, or -1 to specify any first video
     *			    media with encoding direction registered in the
     *			    conference bridge.
     *
     * @return              The video media.
     */
    VideoMedia getEncodingVideoMedia(int med_idx) const PJSUA2_THROW(Error);

    /**
     * Get video media in decoding direction for the specified media index.
     * If the specified media index is not video or invalid or the direction
     * is send only, exception will be thrown.
     *
     * @param med_idx       Media index, or -1 to specify any first video
     *			    media with decoding direction registered in the
     *			    conference bridge.
     *
     * @return              The video media.
     */
    VideoMedia getDecodingVideoMedia(int med_idx) const PJSUA2_THROW(Error);

    /**
     * Check if remote peer support the specified capability.
     *
     * @param htype         The header type (pjsip_hdr_e) to be checked, which
     *                      value may be:
     *                      - PJSIP_H_ACCEPT
     *                      - PJSIP_H_ALLOW
     *                      - PJSIP_H_SUPPORTED
     * @param hname         If htype specifies PJSIP_H_OTHER, then the header
     *                      name must be supplied in this argument. Otherwise
     *                      the value must be set to empty string ("").
     * @param token         The capability token to check. For example, if \a
     *                      htype is PJSIP_H_ALLOW, then \a token specifies the
     *                      method names; if \a htype is PJSIP_H_SUPPORTED, then
     *                      \a token specifies the extension names such as
     *                      "100rel".
     *
     * @return              PJSIP_DIALOG_CAP_SUPPORTED if the specified
     *                      capability is explicitly supported, see
     *                      pjsip_dialog_cap_status for more info.
     */
    pjsip_dialog_cap_status remoteHasCap(int htype,
                                         const string &hname,
                                         const string &token) const;
    
    /**
     * Attach application specific data to the call. Application can then
     * inspect this data by calling getUserData().
     *
     * @param user_data     Arbitrary data to be attached to the call.
     */
    void setUserData(Token user_data);
    
    /**
     * Get user data attached to the call, which has been previously set with
     * setUserData().
     *
     * @return              The user data.
     */
    Token getUserData() const;
    
    /**
     * Get the NAT type of remote's endpoint. This is a proprietary feature
     * of PJSUA-LIB which sends its NAT type in the SDP when \a natTypeInSdp
     * is set in UaConfig.
     *
     * This function can only be called after SDP has been received from remote,
     * which means for incoming call, this function can be called as soon as
     * call is received as long as incoming call contains SDP, and for outgoing
     * call, this function can be called only after SDP is received (normally in
     * 200/OK response to INVITE). As a general case, application should call
     * this function after or in \a onCallMediaState() callback.
     *
     * @return              The NAT type.
     *
     * @see Endpoint::natGetType(), natTypeInSdp
     */
    pj_stun_nat_type getRemNatType() PJSUA2_THROW(Error);

    /**
     * Make outgoing call to the specified URI.
     *
     * @param dst_uri       URI to be put in the To header (normally is the same
     *                      as the target URI).
     * @param prm.opt       Optional call setting.
     * @param prm.txOption  Optional headers etc to be added to outgoing INVITE
     *                      request.
     */
    void makeCall(const string &dst_uri, const CallOpParam &prm)
		  PJSUA2_THROW(Error);

    /**
     * Send response to incoming INVITE request with call setting param.
     * Depending on the status code specified as parameter, this function may
     * send provisional response, establish the call, or terminate the call.
     * Notes about call setting:
     *  - if call setting is changed in the subsequent call to this function,
     *    only the first call setting supplied will applied. So normally
     *    application will not supply call setting before getting confirmation
     *    from the user.
     *  - if no call setting is supplied when SDP has to be sent, i.e: answer
     *    with status code 183 or 2xx, the default call setting will be used,
     *    check CallSetting for its default values.
     *
     * @param prm.opt       Optional call setting.
     * @param prm.statusCode   Status code, (100-699).
     * @param prm.reason    Optional reason phrase. If empty, default text
     *                      will be used.
     * @param prm.txOption  Optional list of headers etc to be added to outgoing
     *                      response message. Note that this message data will
     *                      be persistent in all next answers/responses for this
     *                      INVITE request.
     */
    void answer(const CallOpParam &prm) PJSUA2_THROW(Error);
    
    /**
     * Hangup call by using method that is appropriate according to the
     * call state. This function is different than answering the call with
     * 3xx-6xx response (with answer()), in that this function
     * will hangup the call regardless of the state and role of the call,
     * while answer() only works with incoming calls on EARLY
     * state.
     *
     * @param prm.statusCode
     *                      Optional status code to be sent when we're rejecting
     *                      incoming call. If the value is zero, "603/Decline"
     *                      will be sent.
     * @param prm.reason    Optional reason phrase to be sent when we're
     *                      rejecting incoming call. If empty, default text
     *                      will be used.
     * @param prm.txOption  Optional list of headers etc to be added to outgoing
     *                      request/response message.
     */
    void hangup(const CallOpParam &prm) PJSUA2_THROW(Error);
    
    /**
     * Put the specified call on hold. This will send re-INVITE with the
     * appropriate SDP to inform remote that the call is being put on hold.
     * The final status of the request itself will be reported on the
     * \a onCallMediaState() callback, which inform the application that
     * the media state of the call has changed.
     *
     * @param prm.options   Bitmask of pjsua_call_flag constants. Currently,
     *                      only the flag PJSUA_CALL_UPDATE_CONTACT can be used.
     * @param prm.txOption  Optional message components to be sent with
     *                      the request.
     */
    void setHold(const CallOpParam &prm) PJSUA2_THROW(Error);
    
    /**
     * Send re-INVITE.
     * The final status of the request itself will be reported on the
     * \a onCallMediaState() callback, which inform the application that
     * the media state of the call has changed.
     *
     * @param prm.opt       Optional call setting, if empty, the current call
     *                      setting will remain unchanged.
     * @param prm.opt.flag  Bitmask of pjsua_call_flag constants. Specifying
     *                      PJSUA_CALL_UNHOLD here will release call hold.
     * @param prm.txOption  Optional message components to be sent with
     *                      the request.
     */
    void reinvite(const CallOpParam &prm) PJSUA2_THROW(Error);
    
    /**
     * Send UPDATE request.
     *
     * @param prm.opt       Optional call setting, if empty, the current call
     *                      setting will remain unchanged.
     * @param prm.txOption  Optional message components to be sent with
     *                      the request.
     */
    void update(const CallOpParam &prm) PJSUA2_THROW(Error);
    
    /**
     * Initiate call transfer to the specified address. This function will send
     * REFER request to instruct remote call party to initiate a new INVITE
     * session to the specified destination/target.
     *
     * If application is interested to monitor the successfulness and
     * the progress of the transfer request, it can implement
     * \a onCallTransferStatus() callback which will report the progress
     * of the call transfer request.
     *
     * @param dest          URI of new target to be contacted. The URI may be
     *                      in name address or addr-spec format.
     * @param prm.txOption  Optional message components to be sent with
     *                      the request.
     */
    void xfer(const string &dest, const CallOpParam &prm) PJSUA2_THROW(Error);

    /**
     * Initiate attended call transfer. This function will send REFER request
     * to instruct remote call party to initiate new INVITE session to the URL
     * of \a destCall. The party at \a dest_call then should "replace"
     * the call with us with the new call from the REFER recipient.
     *
     * @param dest_call     The call to be replaced.
     * @param prm.options   Application may specify
     *                      PJSUA_XFER_NO_REQUIRE_REPLACES to suppress the 
     *                      inclusion of "Require: replaces" in
     *                      the outgoing INVITE request created by the REFER
     *                      request.
     * @param prm.txOption  Optional message components to be sent with
     *                      the request.
     */
    void xferReplaces(const Call& dest_call,
                      const CallOpParam &prm) PJSUA2_THROW(Error);
    
    /**
     * Accept or reject redirection response. Application MUST call this
     * function after it signaled PJSIP_REDIRECT_PENDING in the 
     * \a onCallRedirected() callback,
     * to notify the call whether to accept or reject the redirection
     * to the current target. Application can use the combination of
     * PJSIP_REDIRECT_PENDING command in \a onCallRedirected() callback and
     * this function to ask for user permission before redirecting the call.
     *
     * Note that if the application chooses to reject or stop redirection (by
     * using PJSIP_REDIRECT_REJECT or PJSIP_REDIRECT_STOP respectively), the
     * call disconnection callback will be called before this function returns.
     * And if the application rejects the target, the \a onCallRedirected()
     * callback may also be called before this function returns if there is
     * another target to try.
     *
     * @param cmd           Redirection operation to be applied to the current
     *                      target. The semantic of this argument is similar
     *                      to the description in the \a onCallRedirected()
     *                      callback, except that the PJSIP_REDIRECT_PENDING is
     *                      not accepted here.
     */
    void processRedirect(pjsip_redirect_op cmd) PJSUA2_THROW(Error);

    /**
     * Send DTMF digits to remote using RFC 2833 payload formats.
     *
     * @param digits        DTMF string digits to be sent.
     */
    void dialDtmf(const string &digits) PJSUA2_THROW(Error);

    /**
     * Send DTMF digits to remote.
     *
     * @param param	The send DTMF parameter.
     */
    void sendDtmf(const CallSendDtmfParam &param) PJSUA2_THROW(Error);
    
    /**
     * Send instant messaging inside INVITE session.
     *
     * @param prm.contentType
     *                      MIME type.
     * @param prm.content   The message content.
     * @param prm.txOption  Optional list of headers etc to be included in
     *                      outgoing request. The body descriptor in the
     *                      txOption is ignored.
     * @param prm.userData  Optional user data, which will be given back when
     *                      the IM callback is called.
     */
    void sendInstantMessage(const SendInstantMessageParam& prm)
			    PJSUA2_THROW(Error);
    
    /**
     * Send IM typing indication inside INVITE session.
     *
     * @param prm.isTyping  True to indicate to remote that local person is
     *                      currently typing an IM.
     * @param prm.txOption  Optional list of headers etc to be included in
     *                      outgoing request.
     */
    void sendTypingIndication(const SendTypingIndicationParam &prm)
         PJSUA2_THROW(Error);
    
    /**
     * Send arbitrary request with the call. This is useful for example to send
     * INFO request. Note that application should not use this function to send
     * requests which would change the invite session's state, such as
     * re-INVITE, UPDATE, PRACK, and BYE.
     *
     * @param prm.method    SIP method of the request.
     * @param prm.txOption  Optional message body and/or list of headers to be
     *                      included in outgoing request.
     */
    void sendRequest(const CallSendRequestParam &prm) PJSUA2_THROW(Error);
    
    /**
     * Dump call and media statistics to string.
     *
     * @param with_media    True to include media information too.
     * @param indent        Spaces for left indentation.
     *
     * @return              Call dump and media statistics string.
     */
    string dump(bool with_media, const string indent) PJSUA2_THROW(Error);
    
    /**
     * Get the media stream index of the default video stream in the call.
     * Typically this will just retrieve the stream index of the first
     * activated video stream in the call. If none is active, it will return
     * the first inactive video stream.
     *
     * @return              The media stream index or -1 if no video stream
     *                      is present in the call.
     */
    int vidGetStreamIdx() const;
    
    /**
     * Determine if video stream for the specified call is currently running
     * (i.e. has been created, started, and not being paused) for the specified
     * direction.
     *
     * @param med_idx       Media stream index, or -1 to specify default video
     *                      media.
     * @param dir           The direction to be checked.
     *
     * @return              True if stream is currently running for the
     *                      specified direction.
     */
    bool vidStreamIsRunning(int med_idx, pjmedia_dir dir) const;
    
    /**
     * Add, remove, modify, and/or manipulate video media stream for the
     * specified call. This may trigger a re-INVITE or UPDATE to be sent
     * for the call.
     *
     * @param op            The video stream operation to be performed,
     *                      possible values are pjsua_call_vid_strm_op.
     * @param param         The parameters for the video stream operation
     *                      (see CallVidSetStreamParam).
     */
    void vidSetStream(pjsua_call_vid_strm_op op,
                      const CallVidSetStreamParam &param) PJSUA2_THROW(Error);

    /**
     * Get media stream info for the specified media index.
     *
     * @param med_idx       Media stream index.
     *
     * @return              The stream info.
     */
    StreamInfo getStreamInfo(unsigned med_idx) const PJSUA2_THROW(Error);
    
    /**
     * Get media stream statistic for the specified media index.
     *
     * @param med_idx       Media stream index.
     *
     * @return              The stream statistic.
     */
    StreamStat getStreamStat(unsigned med_idx) const PJSUA2_THROW(Error);
    
    /**
     * Get media transport info for the specified media index.
     *
     * @param med_idx       Media stream index.
     *
     * @return              The transport info.
     */
    MediaTransportInfo getMedTransportInfo(unsigned med_idx) const
					   PJSUA2_THROW(Error);

    /**
     * Internal function (callled by Endpoint( to process update to call
     * medias when call media state changes.
     */
    void processMediaUpdate(OnCallMediaStateParam &prm);

    /**
     * Internal function (called by Endpoint) to process call state change.
     */
    void processStateChange(OnCallStateParam &prm);
    
public:
    /*
     * Callbacks
     */
    /**
     * Notify application when call state has changed.
     * Application may then query the call info to get the
     * detail call states by calling getInfo() function.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallState(OnCallStateParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * This is a general notification callback which is called whenever
     * a transaction within the call has changed state. Application can
     * implement this callback for example to monitor the state of
     * outgoing requests, or to answer unhandled incoming requests
     * (such as INFO) with a final response.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallTsxState(OnCallTsxStateParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application when media state in the call has changed.
     * Normal application would need to implement this callback, e.g.
     * to connect the call's media to sound device. When ICE is used,
     * this callback will also be called to report ICE negotiation
     * failure.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallMediaState(OnCallMediaStateParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application when a call has just created a local SDP (for
     * initial or subsequent SDP offer/answer). Application can implement
     * this callback to modify the SDP, before it is being sent and/or
     * negotiated with remote SDP, for example to apply per account/call
     * basis codecs priority or to add custom/proprietary SDP attributes.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallSdpCreated(OnCallSdpCreatedParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application when an audio media session is about to be created
     * (as opposed to #on_stream_created() and #on_stream_created2() which are
     * called *after* the session has been created). The application may change
     * some stream info parameter values, i.e: jbInit, jbMinPre, jbMaxPre,
     * jbMax, useKa, rtcpSdesByeDisabled, jbDiscardAlgo (audio),
     * vidCodecParam.encFmt (video).
     *
     * @param prm       Callback parameter.
     */
    virtual void onStreamPreCreate(OnStreamPreCreateParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application when audio media session is created and before it is
     * registered to the conference bridge. Application may return different
     * audio media port if it has added media processing port to the stream.
     * This media port then will be added to the conference bridge instead.
     *
     * @param prm	Callback parameter.
     */
    virtual void onStreamCreated(OnStreamCreatedParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application when audio media session has been unregistered from
     * the conference bridge and about to be destroyed.
     *
     * @param prm	Callback parameter.
     */
    virtual void onStreamDestroyed(OnStreamDestroyedParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application upon incoming DTMF digits.
     *
     * @param prm	Callback parameter.
     */
    virtual void onDtmfDigit(OnDtmfDigitParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application upon incoming DTMF events.
     *
     * @param prm	Callback parameter.
     */
    virtual void onDtmfEvent(OnDtmfEventParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application on call being transferred (i.e. REFER is received).
     * Application can decide to accept/reject transfer request by setting
     * the code (default is 202). When this callback is not implemented,
     * the default behavior is to accept the transfer.
     *
     * If application decides to accept the transfer request, it must also
     * instantiate the new Call object for the transfer operation and return
     * this new Call object to prm.newCall.
     * 
     * If application does not specify new Call object, library will reuse the
     * existing Call object for initiating the new call (to the transfer
     * destination). In this case, any events from both calls (transferred and
     * transferring) will be delivered to the same Call object, where the call
     * ID will be switched back and forth between callbacks. Application must
     * be careful to not destroy the Call object when receiving disconnection
     * event of the transferred call after the transfer process is completed.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallTransferRequest(OnCallTransferRequestParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application of the status of previously sent call
     * transfer request. Application can monitor the status of the
     * call transfer request, for example to decide whether to
     * terminate existing call.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallTransferStatus(OnCallTransferStatusParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application about incoming INVITE with Replaces header.
     * Application may reject the request by setting non-2xx code.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallReplaceRequest(OnCallReplaceRequestParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application that an existing call has been replaced with
     * a new call. This happens when PJSUA-API receives incoming INVITE
     * request with Replaces header.
     *
     * After this callback is called, normally PJSUA-API will disconnect
     * this call and establish a new call. To be able to control the call,
     * e.g: hold, transfer, change media parameters, application must
     * instantiate a new Call object for the new call using call ID
     * specified in prm.newCallId, and return the Call object via
     * prm.newCall.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallReplaced(OnCallReplacedParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application when call has received new offer from remote
     * (i.e. re-INVITE/UPDATE with SDP is received). Application can
     * decide to accept/reject the offer by setting the code (default
     * is 200). If the offer is accepted, application can update the
     * call setting to be applied in the answer. When this callback is
     * not implemented, the default behavior is to accept the offer using
     * current call setting.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallRxOffer(OnCallRxOfferParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application when call has received a re-INVITE offer from
     * the peer. It allows more fine-grained control over the response to
     * a re-INVITE. If application sets async to PJ_TRUE, it can send
     * the reply manually using the function #Call::answer() and setting
     * the SDP answer. Otherwise, by default the re-INVITE will be
     * answered automatically after the callback returns.
     *
     * Currently, this callback is only called for re-INVITE with
     * SDP, but app should be prepared to handle the case of re-INVITE
     * without SDP.
     *
     * Remarks: If manually answering at a later timing, application may
     * need to monitor onCallTsxState() callback to check whether
     * the re-INVITE is already answered automatically with 487 due to
     * being cancelled.
     *
     * Note: onCallRxOffer() will still be called after this callback,
     * but only if prm.async is false and prm.code is 200. 
     */
    virtual void onCallRxReinvite(OnCallRxReinviteParam &prm)
    { PJ_UNUSED_ARG(prm); }


    /**
     * Notify application when call has received INVITE with no SDP offer.
     * Application can update the call setting (e.g: add audio/video), or
     * enable/disable codecs, or update other media session settings from
     * within the callback, however, as mandated by the standard (RFC3261
     * section 14.2), it must ensure that the update overlaps with the
     * existing media session (in codecs, transports, or other parameters)
     * that require support from the peer, this is to avoid the need for
     * the peer to reject the offer.
     *
     * When this callback is not implemented, the default behavior is to send
     * SDP offer using current active media session (with all enabled codecs
     * on each media type).
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallTxOffer(OnCallTxOfferParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application on incoming MESSAGE request.
     *
     * @param prm	Callback parameter.
     */
    virtual void onInstantMessage(OnInstantMessageParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application about the delivery status of outgoing MESSAGE
     * request.
     *
     * @param prm	Callback parameter.
     */
    virtual void onInstantMessageStatus(OnInstantMessageStatusParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notify application about typing indication.
     *
     * @param prm	Callback parameter.
     */
    virtual void onTypingIndication(OnTypingIndicationParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * This callback is called when the call is about to resend the
     * INVITE request to the specified target, following the previously
     * received redirection response.
     *
     * Application may accept the redirection to the specified target,
     * reject this target only and make the session continue to try the next
     * target in the list if such target exists, stop the whole
     * redirection process altogether and cause the session to be
     * disconnected, or defer the decision to ask for user confirmation.
     *
     * This callback is optional,
     * the default behavior is to NOT follow the redirection response.
     *
     * @param prm	Callback parameter.
     *
     * @return		Action to be performed for the target. Set this
     *			parameter to one of the value below:
     *			- PJSIP_REDIRECT_ACCEPT: immediately accept the
     *			  redirection. When set, the call will immediately
     *			  resend INVITE request to the target.
     *			- PJSIP_REDIRECT_ACCEPT_REPLACE: immediately accept
     *			  the redirection and replace the To header with the
     *			  current target. When set, the call will immediately
     *			  resend INVITE request to the target.
     *			- PJSIP_REDIRECT_REJECT: immediately reject this
     *			  target. The call will continue retrying with
     *			  next target if present, or disconnect the call
     *			  if there is no more target to try.
     *			- PJSIP_REDIRECT_STOP: stop the whole redirection
     *			  process and immediately disconnect the call. The
     *			  onCallState() callback will be called with
     *			  PJSIP_INV_STATE_DISCONNECTED state immediately
     *			  after this callback returns.
     *			- PJSIP_REDIRECT_PENDING: set to this value if
     *			  no decision can be made immediately (for example
     *			  to request confirmation from user). Application
     *			  then MUST call processRedirect()
     *			  to either accept or reject the redirection upon
     *			  getting user decision.
     */
    virtual pjsip_redirect_op onCallRedirected(OnCallRedirectedParam &prm)
    {
	PJ_UNUSED_ARG(prm);
        return PJSIP_REDIRECT_STOP;
    }
    
    /**
     * This callback is called when media transport state is changed.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallMediaTransportState(OnCallMediaTransportStateParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * Notification about media events such as video notifications. This
     * callback will most likely be called from media threads, thus
     * application must not perform heavy processing in this callback.
     * Especially, application must not destroy the call or media in this
     * callback. If application needs to perform more complex tasks to
     * handle the event, it should post the task to another thread.
     *
     * @param prm	Callback parameter.
     */
    virtual void onCallMediaEvent(OnCallMediaEventParam &prm)
    { PJ_UNUSED_ARG(prm); }
    
    /**
     * This callback can be used by application to implement custom media
     * transport adapter for the call, or to replace the media transport
     * with something completely new altogether.
     *
     * This callback is called when a new call is created. The library has
     * created a media transport for the call, and it is provided as the
     * \a mediaTp argument of this callback. The callback may change it
     * with the instance of media transport to be used by the call.
     *
     * @param prm	Callback parameter.
     */
    virtual void
    onCreateMediaTransport(OnCreateMediaTransportParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Warning: deprecated and may be removed in future release.
     * Application can set SRTP crypto settings (including keys) and
     * keying methods via AccountConfig.mediaConfig.srtpOpt.
     * See also ticket #2100.
     *
     * This callback is called when SRTP media transport is created.
     * Application can modify the SRTP setting \a srtpOpt to specify
     * the cryptos and keys which are going to be used. Note that
     * application should not modify the field
     * \a pjmedia_srtp_setting.close_member_tp and can only modify
     * the field \a pjmedia_srtp_setting.use for initial INVITE.
     *
     * @param prm	Callback parameter.
     */
    virtual void
    onCreateMediaTransportSrtp(OnCreateMediaTransportSrtpParam &prm)
    { PJ_UNUSED_ARG(prm); }

private:
    friend class Endpoint;

    Account             &acc;
    pjsua_call_id 	 id;
    Token                userData;
    std::vector<Media *> medias;
    pj_pool_t		*sdp_pool;
    Call		*child;	    /* New outgoing call in call transfer.  */
};

/**
 * @}  // PJSUA2_CALL
 */

} // namespace pj

#endif	/* __PJSUA2_CALL_HPP__ */

