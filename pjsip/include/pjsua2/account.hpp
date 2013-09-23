/* $Id$ */
/*
 * Copyright (C) 2008-2012 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSUA2_ACCOUNT_HPP__
#define __PJSUA2_ACCOUNT_HPP__

/**
 * @file pjsua2/account.hpp
 * @brief PJSUA2 Account operations
 */
#include <pjsua-lib/pjsua.h>

/**
 * @defgroup PJSUA2_ACC Account
 * @ingroup PJSUA2_Ref
 */

/**
 * @defgroup PJSUA2_Acc_Data_Structure Data Structure
 * @ingroup PJSUA2_Ref
 * @{
 */

/**
 * Account registration config. This will be specified in AccountConfig.
 */
struct AccountRegConfig
{
    /**
     * This is the URL to be put in the request URI for the registration,
     * and will look something like "sip:serviceprovider".
     *
     * This field should be specified if registration is desired. If the
     * value is empty, no account registration will be performed.
     */
    String		registrarUri;

    /**
     * Specify whether the account should register as soon as it is
     * added to the UA. Application can set this to PJ_FALSE and control
     * the registration manually with pjsua_acc_set_registration().
     *
     * Default: True
     */
    bool		registerOnAdd;

    /**
     * The optional custom SIP headers to be put in the registration
     * request.
     */
    StringVector	headers;

    /**
     * Optional interval for registration, in seconds. If the value is zero,
     * default interval will be used (PJSUA_REG_INTERVAL, 300 seconds).
     */
    unsigned		timeoutSec;

    /**
     * Specify interval of auto registration retry upon registration failure
     * (including caused by transport problem), in second. Set to 0 to
     * disable auto re-registration. Note that if the registration retry
     * occurs because of transport failure, the first retry will be done
     * after \a firstRetryIntervalSec seconds instead. Also note that
     * the interval will be randomized slightly by approximately +/- ten
     * seconds to avoid all clients re-registering at the same time.
     *
     * See also \a firstRetryIntervalSec setting.
     *
     * Default: #PJSUA_REG_RETRY_INTERVAL
     */
    unsigned		retryIntervalSec;

    /**
     * This specifies the interval for the first registration retry. The
     * registration retry is explained in \a retryIntervalSec. Note that
     * the value here will also be randomized by +/- ten seconds.
     *
     * Default: 0
     */
    unsigned		firstRetryIntervalSec;

    /**
     * Specify the number of seconds to refresh the client registration
     * before the registration expires.
     *
     * Default: PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH, 5 seconds
     */
    unsigned		delayBeforeRefresh;

    /**
     * Specify whether calls of the configured account should be dropped
     * after registration failure and an attempt of re-registration has
     * also failed.
     *
     * Default: FALSE (disabled)
     */
    bool		dropCallsOnFail;

    /**
     * Specify the maximum time to wait for unregistration requests to
     * complete during library shutdown sequence.
     *
     * Default: PJSUA_UNREG_TIMEOUT
     */
    unsigned		unregWaitSec;

    /**
     * Specify whether REGISTER requests will use the proxy settings
     * of this account. If zero, the REGISTER request will not have any
     * Route headers.
     *
     * Default: 1 (use account proxy)
     */
    unsigned		proxyUse;
};


/**
 * Various SIP settings for the account. This will be specified in
 * AccountConfig.
 */
struct AccountSipConfig
{
    /**
     * Array of credentials. If registration is desired, normally there should
     * be at least one credential specified, to successfully authenticate
     * against the service provider. More credentials can be specified, for
     * example when the requests are expected to be challenged by the
     * proxies in the route set.
     */
    AuthCredInfoVector	authCreds;

    /**
     * Array of proxy servers to visit for outgoing requests. Each of the
     * entry is translated into one Route URI.
     */
    StringVector	proxies;

    /**
     * Optional URI to be put as Contact for this account. It is recommended
     * that this field is left empty, so that the value will be calculated
     * automatically based on the transport address.
     */
    string		contactForced;

    /**
     * Additional parameters that will be appended in the Contact header
     * for this account. This will affect the Contact header in all SIP
     * messages sent on behalf of this account, including but not limited to
     * REGISTER, INVITE, and SUBCRIBE requests or responses.
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *	 ";my-param=X;another-param=Hi%20there"
     */
    string		contactParams;

    /**
     * Additional URI parameters that will be appended in the Contact URI
     * for this account. This will affect the Contact URI in all SIP
     * messages sent on behalf of this account, including but not limited to
     * REGISTER, INVITE, and SUBCRIBE requests or responses.
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *	 ";my-param=X;another-param=Hi%20there"
     */
    string		contactUriParams;


    /**
     * If this flag is set, the authentication client framework will
     * send an empty Authorization header in each initial request.
     * Default is no.
     */
    bool		authInitialEmpty;

    /**
     * Specify the algorithm to use when empty Authorization header
     * is to be sent for each initial request (see above)
     */
    string		authInitialAlgorithm;

    /**
     * Optionally bind this account to specific transport. This normally is
     * not a good idea, as account should be able to send requests using
     * any available transports according to the destination. But some
     * application may want to have explicit control over the transport to
     * use, so in that case it can set this field.
     *
     * Default: -1 (PJSUA_INVALID_ID)
     *
     * @see Account::setTransport()
     */
    TransportId		transportId;
};

/**
 * Account's call settings. This will be specified in AccountConfig.
 */
struct AccountCallConfig
{
    /**
     * Specify how to offer call hold to remote peer. Please see the
     * documentation on #pjsua_call_hold_type for more info.
     *
     * Default: PJSUA_CALL_HOLD_TYPE_DEFAULT
     */
    pjsua_call_hold_type holdType;

    /**
     * Specify how support for reliable provisional response (100rel/
     * PRACK) should be used for all sessions in this account. See the
     * documentation of pjsua_100rel_use enumeration for more info.
     *
     * Default: PJSUA_100REL_NOT_USED
     */
    pjsua_100rel_use	prackUse;

    /**
     * Specify the usage of Session Timers for all sessions. See the
     * #pjsua_sip_timer_use for possible values.
     *
     * Default: PJSUA_SIP_TIMER_OPTIONAL
     */
    pjsua_sip_timer_use	timerUse;
};

/**
 * Account presence config. This will be specified in AccountConfig.
 */
struct AccountPresConfig
{
    /**
     * The optional custom SIP headers to be put in the presence
     * subscription request.
     */
    StringVector	subHeaders;

    /**
     * If this flag is set, the presence information of this account will
     * be PUBLISH-ed to the server where the account belongs.
     *
     * Default: PJ_FALSE
     */
    bool		publishEnabled;

    /**
     * Specify whether the client publication session should queue the
     * PUBLISH request should there be another PUBLISH transaction still
     * pending. If this is set to false, the client will return error
     * on the PUBLISH request if there is another PUBLISH transaction still
     * in progress.
     *
     * Default: PJSIP_PUBLISHC_QUEUE_REQUEST (TRUE)
     */
    bool		publishQueue;

    /**
     * Maximum time to wait for unpublication transaction(s) to complete
     * during shutdown process, before sending unregistration. The library
     * tries to wait for the unpublication (un-PUBLISH) to complete before
     * sending REGISTER request to unregister the account, during library
     * shutdown process. If the value is set too short, it is possible that
     * the unregistration is sent before unpublication completes, causing
     * unpublication request to fail.
     *
     * Value is in milliseconds.
     *
     * Default: PJSUA_UNPUBLISH_MAX_WAIT_TIME_MSEC (2000)
     */
    unsigned		publishShutdownWaitMsec;

    /**
     * Optional PIDF tuple ID for outgoing PUBLISH and NOTIFY. If this value
     * is not specified, a random string will be used.
     */
    string		pidfTupleId;
};

/**
 * Account MWI (Message Waiting Indication) settings. This will be specified
 * in AccountConfig.
 */
struct AccountMwiConfig
{
    /**
     * Subscribe to message waiting indication events (RFC 3842).
     *
     * See also UaConfig.mwiUnsolicitedEnabled setting.
     *
     * Default: FALSE
     */
    bool		enabled;

    /**
     * Specify the default expiration time (in seconds) for Message
     * Waiting Indication (RFC 3842) event subscription. This must not
     * be zero.
     *
     * Default: PJSIP_MWI_DEFAULT_EXPIRES (3600)
     */
    unsigned		expirationSec;
};

/**
 * Account's NAT (Network Address Translation) settings. This will be
 * specified in AccountConfig.
 */
struct AccountNatConfig
{
    /**
     * Control the use of STUN for the SIP signaling.
     *
     * Default: PJSUA_STUN_USE_DEFAULT
     */
    pjsua_stun_use 	sipStunUse;

    /**
     * Control the use of STUN for the media transports.
     *
     * Default: PJSUA_STUN_USE_DEFAULT
     */
    pjsua_stun_use 	mediaStunUse;

    /**
     * Enable ICE for the media transport.
     *
     * Default: False
     */
    bool		iceEnabled;

    /**
     * Set the maximum number of ICE host candidates.
     *
     * Default: -1 (maximum not set)
     */
    int			iceMaxHostCands;

    /**
     * Specify whether to use aggressive nomination.
     *
     * Default: True
     */
    bool		iceAggressiveNomination;

    /**
     * For controlling agent if it uses regular nomination, specify the delay
     * to perform nominated check (connectivity check with USE-CANDIDATE
     * attribute) after all components have a valid pair.
     *
     * Default value is PJ_ICE_NOMINATED_CHECK_DELAY.
     */
    unsigned		iceNominatedCheckDelayMsec;

    /**
     * For a controlled agent, specify how long it wants to wait (in
     * milliseconds) for the controlling agent to complete sending
     * connectivity check with nominated flag set to true for all components
     * after the controlled agent has found that all connectivity checks in
     * its checklist have been completed and there is at least one successful
     * (but not nominated) check for every component.
     *
     * Default value for this option is
     * ICE_CONTROLLED_AGENT_WAIT_NOMINATION_TIMEOUT. Specify -1 to disable
     * this timer.
     */
    int			iceWaitNominationTimeoutMsec;

    /**
     * Disable RTCP component.
     *
     * Default: False
     */
    bool		iceNoRtcp;

    /**
     * Enable TURN candidate in ICE.
     */
    bool		turnEnabled;

    /**
     * Specify TURN domain name or host name, in in "DOMAIN:PORT" or
     * "HOST:PORT" format.
     */
    string		turnServer;

    /**
     * Specify the connection type to be used to the TURN server. Valid
     * values are PJ_TURN_TP_UDP or PJ_TURN_TP_TCP.
     *
     * Default: PJ_TURN_TP_UDP
     */
    pj_turn_tp_type	turnConnType;

    /**
     * Specify the username to authenticate with the TURN server.
     */
    string		turnUserName;

    /**
     * Specify the type of password. Currently this must be zero to
     * indicate plain-text password will be used in the password.
     */
    int			turnPasswordType;

    /**
     * Specify the password to authenticate with the TURN server.
     */
    string		turnPassword;

    /**
     * This option is used to update the transport address and the Contact
     * header of REGISTER request. When this option is  enabled, the library
     * will keep track of the public IP address from the response of REGISTER
     * request. Once it detects that the address has changed, it will
     * unregister current Contact, update the Contact with transport address
     * learned from Via header, and register a new Contact to the registrar.
     * This will also update the public name of UDP transport if STUN is
     * configured.
     *
     * See also contactRewriteMethod field.
     *
     * Default: TRUE
     */
    bool		contactRewriteEnabled;

    /**
     * Specify how Contact update will be done with the registration, if
     * contactRewriteEnabled is enabled.
     *
     * If set to 1, the Contact update will be done by sending unregistration
     * to the currently registered Contact, while simultaneously sending new
     * registration (with different Call-ID) for the updated Contact.
     *
     * If set to 2, the Contact update will be done in a single, current
     * registration session, by removing the current binding (by setting its
     * Contact's expires parameter to zero) and adding a new Contact binding,
     * all done in a single request.
     *
     * Value 1 is the legacy behavior.
     *
     * Default value: PJSUA_CONTACT_REWRITE_METHOD (2)
     */
    int			contactRewriteMethod;

    /**
     * This option is used to overwrite the "sent-by" field of the Via header
     * for outgoing messages with the same interface address as the one in
     * the REGISTER request, as long as the request uses the same transport
     * instance as the previous REGISTER request.
     *
     * Default: TRUE
     */
    bool		viaRewriteEnabled;

    /**
     * Control the use of SIP outbound feature. SIP outbound is described in
     * RFC 5626 to enable proxies or registrar to send inbound requests back
     * to UA using the same connection initiated by the UA for its
     * registration. This feature is highly useful in NAT-ed deployemtns,
     * hence it is enabled by default.
     *
     * Note: currently SIP outbound can only be used with TCP and TLS
     * transports. If UDP is used for the registration, the SIP outbound
     * feature will be silently ignored for the account.
     *
     * Default: TRUE
     */
    bool		sipOutboundEnabled;

    /**
     * Specify SIP outbound (RFC 5626) instance ID to be used by this
     * account. If empty, an instance ID will be generated based on
     * the hostname of this agent. If application specifies this parameter, the
     * value will look like "<urn:uuid:00000000-0000-1000-8000-AABBCCDDEEFF>"
     * without the double-quotes.
     *
     * Default: empty
     */
    string		sipOutboundInstanceId;

    /**
     * Specify SIP outbound (RFC 5626) registration ID. The default value
     * is empty, which would cause the library to automatically generate
     * a suitable value.
     *
     * Default: empty
     */
    string		sipOutboundRegId;

    /**
     * Set the interval for periodic keep-alive transmission for this account.
     * If this value is zero, keep-alive will be disabled for this account.
     * The keep-alive transmission will be sent to the registrar's address,
     * after successful registration.
     *
     * Default: 15 (seconds)
     */
    unsigned		udpKaIntervalSec;

    /**
     * Specify the data to be transmitted as keep-alive packets.
     *
     * Default: CR-LF
     */
    string		udpKaData;
};

/**
 * Account media config (applicable for both audio and video). This will be
 * specified in AccountConfig.
 */
struct AccountMediaConfig
{
    /**
     * Media transport configuration.
     */
    TransportConfig	transportConfig;

    /**
     * If remote sends SDP answer containing more than one format or codec in
     * the media line, send re-INVITE or UPDATE with just one codec to lock
     * which codec to use.
     *
     * Default: True (Yes).
     */
    bool		lockCodecEnabled;

    /**
     * Specify whether stream keep-alive and NAT hole punching with
     * non-codec-VAD mechanism (see @ref PJMEDIA_STREAM_ENABLE_KA) is enabled
     * for this account.
     *
     * Default: False
     */
    bool		streamKaEnabled;

    /**
     * Specify whether secure media transport should be used for this account.
     * Valid values are PJMEDIA_SRTP_DISABLED, PJMEDIA_SRTP_OPTIONAL, and
     * PJMEDIA_SRTP_MANDATORY.
     *
     * Default: #PJSUA_DEFAULT_USE_SRTP
     */
    pjmedia_srtp_use	srtpUse;

    /**
     * Specify whether SRTP requires secure signaling to be used. This option
     * is only used when \a use_srtp option above is non-zero.
     *
     * Valid values are:
     *	0: SRTP does not require secure signaling
     *	1: SRTP requires secure transport such as TLS
     *	2: SRTP requires secure end-to-end transport (SIPS)
     *
     * Default: #PJSUA_DEFAULT_SRTP_SECURE_SIGNALING
     */
    int			srtpSecureSignaling;
};

/**
 * Video stream rate control config. This will be specified in
 * AccountVideoConfig.
 */
struct VidRateControlConfig
{
    /**
     * Rate control method.
     *
     * Default: PJMEDIA_VID_STREAM_RC_SIMPLE_BLOCKING.
     */
    pjmedia_vid_stream_rc_method    method;

    /**
     * Upstream/outgoing bandwidth. If this is set to zero, the video stream
     * will use codec maximum bitrate setting.
     *
     * Default: 0 (follow codec maximum bitrate).
     */
    unsigned			    bandwidth;

    /** Default constructor */
    VidRateControlConfig();

    /** Convert to pj */
    pjmedia_vid_stream_rc_config toPj() const;

    /** Convert from pj */
    void fromPj(const pjmedia_vid_stream_rc_config &prm);
};

/**
 * Account video config. This will be specified in AccountConfig.
 */
struct AccountVideoConfig
{
    /**
     * Specify whether incoming video should be shown to screen by default.
     * This applies to incoming call (INVITE), incoming re-INVITE, and
     * incoming UPDATE requests.
     *
     * Regardless of this setting, application can detect incoming video
     * by implementing \a on_call_media_state() callback and enumerating
     * the media stream(s) with #pjsua_call_get_info(). Once incoming
     * video is recognised, application may retrieve the window associated
     * with the incoming video and show or hide it with
     * #pjsua_vid_win_set_show().
     *
     * Default: False
     */
    bool		autoShowIncoming;

    /**
     * Specify whether outgoing video should be activated by default when
     * making outgoing calls and/or when incoming video is detected. This
     * applies to incoming and outgoing calls, incoming re-INVITE, and
     * incoming UPDATE. If the setting is non-zero, outgoing video
     * transmission will be started as soon as response to these requests
     * is sent (or received).
     *
     * Regardless of the value of this setting, application can start and
     * stop outgoing video transmission with #pjsua_call_set_vid_strm().
     *
     * Default: False
     */
    bool		autoTransmitOutgoing;

    /**
     * Specify video window's flags. The value is a bitmask combination of
     * #pjmedia_vid_dev_wnd_flag.
     *
     * Default: 0
     */
    unsigned		windowFlags;

    /**
     * Specify the default capture device to be used by this account. If
     * vidOutAutoTransmit is enabled, this device will be used for
     * capturing video.
     *
     * Default: PJMEDIA_VID_DEFAULT_CAPTURE_DEV
     */
    pjmedia_vid_dev_index defaultCaptureDevice;

    /**
     * Specify the default rendering device to be used by this account.
     *
     * Default: PJMEDIA_VID_DEFAULT_RENDER_DEV
     */
    pjmedia_vid_dev_index defaultRenderDevice;

    /**
     * Specify the send rate control for video stream.
     */
    VidRateControlConfig  rateControlConfig;
};

/**
 * Account configuration.
 */
struct AccountConfig
{
    /**
     * Account priority, which is used to control the order of matching
     * incoming/outgoing requests. The higher the number means the higher
     * the priority is, and the account will be matched first.
     */
    int			priority;

    /**
     * The Address of Record or AOR, that is full SIP URL that identifies the
     * account. The value can take name address or URL format, and will look
     * something like "sip:account@serviceprovider".
     *
     * This field is mandatory.
     */
    String		uri;

    /**
     * Registration settings.
     */
    AccountRegConfig	regConfig;

    /**
     * SIP settings.
     */
    AccountSipConfig	sipConfig;

    /**
     * Call settings.
     */
    AccountCallConfig	callConfig;

    /**
     * Presence settings.
     */
    AccountPresConfig	presConfig;

    /**
     * MWI (Message Waiting Indication) settings.
     */
    AccountMwiConfig	mwiConfig;

    /**
     * NAT settings.
     */
    AccountNatConfig	natConfig;

    /**
     * Media settings (applicable for both audio and video).
     */
    AccountMediaConfig	mediaConfig;

    /**
     * Video settings.
     */
    AccountVideoConfig	videoConfig;

    /**
     * Default constructor will initialize with default values.
     */
    AccountConfig();

    /**
     * Convert to pjsip.
     */
    pjsua_acc_config toPj() const;

    /**
     * Initialize from pjsip.
     */
    void fromPj(const pjsua_acc_config &prm);
};

/**
 * Account information. Application can query the account information
 * by calling Account::getInfo().
 */
struct AccountInfo
{
    /**
     * The account ID.
     */
    pjsua_acc_id	id;

    /**
     * Flag to indicate whether this is the default account.
     */
    bool		isDefault;

    /**
     * Account URI
     */
    string		uri;

    /**
     * Flag to tell whether this account has registration setting
     * (reg_uri is not empty).
     */
    bool		hasRegistration;

    /**
     * Flag to tell whether this account is currently registered
     * (has active registration session).
     */
    bool		isRegistered;

    /**
     * An up to date expiration interval for account registration session.
     */
    int			expiresSec;

    /**
     * Last registration status code. If status code is zero, the account
     * is currently not registered. Any other value indicates the SIP
     * status code of the registration.
     */
    pjsip_status_code	status;

    /**
     * String describing the registration status.
     */
    string		statusText;

    /**
     * Last registration error code. When the status field contains a SIP
     * status code that indicates a registration failure, last registration
     * error code contains the error code that causes the failure. In any
     * other case, its value is zero.
     */
    pj_status_t		regLastErr;

    /**
     * Presence online status for this account.
     */
    bool		onlineStatus;

    /**
     * Presence online status text.
     */
    string		onlineStatusText;

};

/**
 * This structure contains parameters for onIncomingCall() callback.
 */
struct IncomingCallParam
{
    // rdata
};

/**
 * This structure contains parameters for onRegState() callback.
 */
struct RegStateParam
{
    // reginfo
};

/**
 * This structure contains parameters for onIncomingSubscribe() callback.
 */
struct IncomingSubscribeParam
{
    /*
    * @param srv_pres	    Server presence subscription instance. If
    *			    application delays the acceptance of the request,
    *			    it will need to specify this object when calling
    *			    #pjsua_pres_notify().
    * @param acc_id	    Account ID most appropriate for this request.
    * @param buddy_id	    ID of the buddy matching the sender of the
    *			    request, if any, or PJSUA_INVALID_ID if no
    *			    matching buddy is found.
    * @param from	    The From URI of the request.
    * @param rdata	    The incoming request.
    * @param code	    The status code to respond to the request. The
    *			    default value is 200. Application may set this
    *			    to other final status code to accept or reject
    *			    the request.
    * @param reason	    The reason phrase to respond to the request.
    * @param msg_data	    If the application wants to send additional
    *			    headers in the response, it can put it in this
    *			    parameter.
    */

    //pjsua_srv_pres *srv_pres,
    // pjsua_buddy_id buddy_id,
    // const pj_str_t *from,
    // pjsip_rx_data *rdata,
    // pjsua_msg_data *msg_data

    /**
     * The status code to respond to the request. The default value is 200.
     * Application may set this to other final status code to accept or
     * reject the request.
     */
    pjsip_status_code code;

    /**
     * The reason phrase to respond to the request.
     */
    string reason;
};

/**
 * Account callback
 */
class AccountCallback
{
public:
    /** Virtual destructor */
    virtual ~AccountCallback() {}

    /**
     * Notify application on incoming call.
     *
     * @param prm	Callback parameter.
     */
    virtual void onIncomingCall(IncomingCallParam &prm)
    {}

    /**
     * Notify application when registration status has changed.
     * Application may then query the account info to get the
     * registration details.
     *
     * @param acc_id	    The account ID.
     */
    virtual void onRegState(RegStateParam &prm)
    {}

    /**
     * Notification when incoming SUBSCRIBE request is received. Application
     * may use this callback to authorize the incoming subscribe request
     * (e.g. ask user permission if the request should be granted).
     *
     * If this callback is not implemented, all incoming presence subscription
     * requests will be accepted.
     *
     * If this callback is implemented, application has several choices on
     * what to do with the incoming request:
     *	- it may reject the request immediately by specifying non-200 class
     *    final response in the IncomingSubscribeParam.code parameter.
     *	- it may immediately accept the request by specifying 200 as the
     *	  IncomingSubscribeParam.code parameter. This is the default value if
     *	  application doesn't set any value to the IncomingSubscribeParam.code
     *	  parameter. In this case, the library will automatically send NOTIFY
     *	  request upon returning from this callback.
     *  - it may delay the processing of the request, for example to request
     *    user permission whether to accept or reject the request. In this
     *	  case, the application MUST set the IncomingSubscribeParam.code
     *	  argument to 202, then IMMEDIATELY calls #pjsua_pres_notify() with
     *	  state PJSIP_EVSUB_STATE_PENDING and later calls #pjsua_pres_notify()
     *    again to accept or reject the subscription request.
     *
     * Any IncomingSubscribeParam.code other than 200 and 202 will be treated
     * as 200.
     *
     * Application MUST return from this callback immediately (e.g. it must
     * not block in this callback while waiting for user confirmation).
     */
    virtual void onIncomingSubscribe(IncomingSubscribeParam &prm)
    {}

};


/**
 * @}  // PJSUA2_Acc_Data_Structure
 */

/**
 * @addtogroup PJSUA2_ACC
 * @{
 */


/**
 * Account.
 */
class Account
{
public:
    void setTransport();

protected:
    friend class Endpoint;

    Account();
};

/**
 * @}  // PJSUA2_ACC
 */

#endif	/* __PJSUA2_ACCOUNT_HPP__ */

