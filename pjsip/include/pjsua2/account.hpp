/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#include <pjsua2/persistent.hpp>
#include <pjsua2/presence.hpp>
#include <pjsua2/siptypes.hpp>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_ACC Account
 * @ingroup PJSUA2_Ref
 * @{
 */

using std::string;

/**
 * Account registration config. This will be specified in AccountConfig.
 */
struct AccountRegConfig : public PersistentObject
{
    /**
     * This is the URL to be put in the request URI for the registration,
     * and will look something like "sip:serviceprovider".
     *
     * This field should be specified if registration is desired. If the
     * value is empty, no account registration will be performed.
     */
    string              registrarUri;

    /**
     * Specify whether the account should register as soon as it is
     * added to the UA. Application can set this to PJ_FALSE and control
     * the registration manually with pjsua_acc_set_registration().
     *
     * Default: True
     */
    bool                registerOnAdd;

    /**
     * Specify whether account modification with Account::modify() should
     * automatically update registration if necessary, for example if
     * account credentials change.
     *
     * Disable this when immediate registration is not desirable, such as
     * during IP address change.
     *
     * Default: false.
     */
    bool                disableRegOnModify;

    /**
     * The optional custom SIP headers to be put in the registration
     * request.
     */
    SipHeaderVector     headers;

    /**
     * Additional parameters that will be appended in the Contact header
     * of the registration requests. This will be appended after
     * \a AccountSipConfig.contactParams;
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *   ";my-param=X;another-param=Hi%20there"
     */
    string              contactParams;

    /**
     * Additional parameters that will be appended in the Contact URI
     * of the registration requests. This will be appended after
     * \a AccountSipConfig.contactUriParams;
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *   ";my-param=X;another-param=Hi%20there"
     */
    string              contactUriParams;

    /**
     * Optional interval for registration, in seconds. If the value is zero,
     * default interval will be used (PJSUA_REG_INTERVAL, 300 seconds).
     */
    unsigned            timeoutSec;

    /**
     * Specify interval of auto registration retry upon registration failure
     * (including caused by transport problem), in second. Set to 0 to
     * disable auto re-registration. Note that if the registration retry
     * occurs because of transport failure, the first retry will be done
     * after \a firstRetryIntervalSec seconds instead. Also note that
     * the interval will be randomized slightly by some seconds (specified
     * in \a reg_retry_random_interval) to avoid all clients re-registering
     * at the same time.
     *
     * See also \a firstRetryIntervalSec and \a randomRetryIntervalSec
     * settings.
     *
     * Default: PJSUA_REG_RETRY_INTERVAL
     */
    unsigned            retryIntervalSec;

    /**
     * This specifies the interval for the first registration retry. The
     * registration retry is explained in \a retryIntervalSec. Note that
     * the value here will also be randomized by some seconds (specified
     * in \a reg_retry_random_interval) to avoid all clients re-registering
     * at the same time.
     *
     * See also \a retryIntervalSec and \a randomRetryIntervalSec settings.
     *
     * Default: 0
     */
    unsigned            firstRetryIntervalSec;

    /**
     * This specifies maximum randomized value to be added/substracted
     * to/from the registration retry interval specified in \a
     * reg_retry_interval and \a reg_first_retry_interval, in second.
     * This is useful to avoid all clients re-registering at the same time.
     * For example, if the registration retry interval is set to 100 seconds
     * and this is set to 10 seconds, the actual registration retry interval
     * will be in the range of 90 to 110 seconds.
     *
     * See also \a retryIntervalSec and \a firstRetryIntervalSec settings.
     *
     * Default: 10
     */
    unsigned            randomRetryIntervalSec;

    /**
     * Specify the number of seconds to refresh the client registration
     * before the registration expires.
     *
     * Default: PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH, 5 seconds
     */
    unsigned            delayBeforeRefreshSec;

    /**
     * Specify whether calls of the configured account should be dropped
     * after registration failure and an attempt of re-registration has
     * also failed.
     *
     * Default: FALSE (disabled)
     */
    bool                dropCallsOnFail;

    /**
     * Specify the maximum time to wait for unregistration requests to
     * complete during library shutdown sequence.
     *
     * Default: PJSUA_UNREG_TIMEOUT
     */
    unsigned            unregWaitMsec;

    /**
     * Specify how the registration uses the outbound and account proxy
     * settings. This controls if and what Route headers will appear in
     * the REGISTER request of this account. The value is bitmask combination
     * of PJSUA_REG_USE_OUTBOUND_PROXY and PJSUA_REG_USE_ACC_PROXY bits.
     * If the value is set to 0, the REGISTER request will not use any proxy
     * (i.e. it will not have any Route headers).
     *
     * Default: 3 (PJSUA_REG_USE_OUTBOUND_PROXY | PJSUA_REG_USE_ACC_PROXY)
     */
    unsigned            proxyUse;

public:
    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);

};

/** Array of SIP credentials */
typedef std::vector<AuthCredInfo> AuthCredInfoVector;

/**
 * Various SIP settings for the account. This will be specified in
 * AccountConfig.
 */
struct AccountSipConfig : public PersistentObject
{
    /**
     * Array of credentials. If registration is desired, normally there should
     * be at least one credential specified, to successfully authenticate
     * against the service provider. More credentials can be specified, for
     * example when the requests are expected to be challenged by the
     * proxies in the route set.
     */
    AuthCredInfoVector  authCreds;

    /**
     * Array of proxy servers to visit for outgoing requests. Each of the
     * entry is translated into one Route URI.
     */
    StringVector        proxies;

    /**
     * Optional URI to be put as Contact for this account. It is recommended
     * that this field is left empty, so that the value will be calculated
     * automatically based on the transport address.
     */
    string              contactForced;

    /**
     * Additional parameters that will be appended in the Contact header
     * for this account. This will affect the Contact header in all SIP
     * messages sent on behalf of this account, including but not limited to
     * REGISTER, INVITE, and SUBCRIBE requests or responses.
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *   ";my-param=X;another-param=Hi%20there"
     */
    string              contactParams;

    /**
     * Additional URI parameters that will be appended in the Contact URI
     * for this account. This will affect the Contact URI in all SIP
     * messages sent on behalf of this account, including but not limited to
     * REGISTER, INVITE, and SUBCRIBE requests or responses.
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *   ";my-param=X;another-param=Hi%20there"
     */
    string              contactUriParams;


    /**
     * If this flag is set, the authentication client framework will
     * send an empty Authorization header in each initial request.
     * Default is no.
     */
    bool                authInitialEmpty;

    /**
     * Specify the algorithm to use when empty Authorization header
     * is to be sent for each initial request (see above)
     */
    string              authInitialAlgorithm;

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
    TransportId         transportId;

    /**
     * Specify whether IPv6 should be used for SIP signalling.
     *
     * Default: PJSUA_IPV6_ENABLED_NO_PREFERENCE
     * (IP version used will be based on the address resolution
     * returned by OS/resolver)
     */
    pjsua_ipv6_use      ipv6Use;

public:
    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * Account's call settings. This will be specified in AccountConfig.
 */
struct AccountCallConfig : public PersistentObject
{
    /**
     * Specify how to offer call hold to remote peer. Please see the
     * documentation on pjsua_call_hold_type for more info.
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
    pjsua_100rel_use    prackUse;

    /**
     * Specify the usage of Session Timers for all sessions. See the
     * pjsua_sip_timer_use for possible values.
     *
     * Default: PJSUA_SIP_TIMER_OPTIONAL
     */
    pjsua_sip_timer_use timerUse;

    /**
     * Specify minimum Session Timer expiration period, in seconds.
     * Must not be lower than 90. Default is 90.
     */
    unsigned            timerMinSESec;

    /**
     * Specify Session Timer expiration period, in seconds.
     * Must not be lower than timerMinSE. Default is 1800.
     */
    unsigned            timerSessExpiresSec;

public:
    /**
     * Default constructor
     */
    AccountCallConfig() : holdType(PJSUA_CALL_HOLD_TYPE_DEFAULT),
                          prackUse(PJSUA_100REL_NOT_USED),
                          timerUse(PJSUA_SIP_TIMER_OPTIONAL),
                          timerMinSESec(90),
                          timerSessExpiresSec(PJSIP_SESS_TIMER_DEF_SE)
    {}

    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * Account presence config. This will be specified in AccountConfig.
 */
struct AccountPresConfig : public PersistentObject
{
    /**
     * The optional custom SIP headers to be put in the presence
     * subscription request.
     */
    SipHeaderVector     headers;

    /**
     * If this flag is set, the presence information of this account will
     * be PUBLISH-ed to the server where the account belongs.
     *
     * Default: PJ_FALSE
     */
    bool                publishEnabled;

    /**
     * Specify whether the client publication session should queue the
     * PUBLISH request should there be another PUBLISH transaction still
     * pending. If this is set to false, the client will return error
     * on the PUBLISH request if there is another PUBLISH transaction still
     * in progress.
     *
     * Default: PJSIP_PUBLISHC_QUEUE_REQUEST (TRUE)
     */
    bool                publishQueue;

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
    unsigned            publishShutdownWaitMsec;

    /**
     * Optional PIDF tuple ID for outgoing PUBLISH and NOTIFY. If this value
     * is not specified, a random string will be used.
     */
    string              pidfTupleId;

public:
    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * Account MWI (Message Waiting Indication) settings. This will be specified
 * in AccountConfig.
 */
struct AccountMwiConfig : public PersistentObject
{
    /**
     * Subscribe to message waiting indication events (RFC 3842).
     *
     * See also UaConfig.mwiUnsolicitedEnabled setting.
     *
     * Default: FALSE
     */
    bool                enabled;

    /**
     * Specify the default expiration time (in seconds) for Message
     * Waiting Indication (RFC 3842) event subscription. This must not
     * be zero.
     *
     * Default: PJSIP_MWI_DEFAULT_EXPIRES (3600)
     */
    unsigned            expirationSec;

public:
    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * Account's NAT (Network Address Translation) settings. This will be
 * specified in AccountConfig.
 */
struct AccountNatConfig : public PersistentObject
{
    /**
     * Control the use of STUN for the SIP signaling.
     *
     * Default: PJSUA_STUN_USE_DEFAULT
     */
    pjsua_stun_use      sipStunUse;

    /**
     * Control the use of STUN for the media transports.
     *
     * Default: PJSUA_STUN_USE_DEFAULT
     */
    pjsua_stun_use      mediaStunUse;

    /**
     * Control the use of UPnP for the SIP signaling.
     *
     * Default: PJSUA_UPNP_USE_DEFAULT
     */
    pjsua_upnp_use      sipUpnpUse;

    /**
     * Control the use of UPnP for the media transports.
     *
     * Default: PJSUA_UPNP_USE_DEFAULT
     */
    pjsua_upnp_use      mediaUpnpUse;

    /**
     * Specify NAT64 options.
     *
     * Default: PJSUA_NAT64_DISABLED
     */
    pjsua_nat64_opt     nat64Opt;

    /**
     * Enable ICE for the media transport.
     *
     * Default: False
     */
    bool                iceEnabled;

    /**
     * Set trickle ICE mode for ICE media transport.
     *
     * Default: PJ_ICE_SESS_TRICKLE_DISABLED
     */
    pj_ice_sess_trickle iceTrickle;

    /**
     * Set the maximum number of ICE host candidates.
     *
     * Default: -1 (maximum not set)
     */
    int                 iceMaxHostCands;

    /**
     * Specify whether to use aggressive nomination.
     *
     * Default: True
     */
    bool                iceAggressiveNomination;

    /**
     * For controlling agent if it uses regular nomination, specify the delay
     * to perform nominated check (connectivity check with USE-CANDIDATE
     * attribute) after all components have a valid pair.
     *
     * Default value is PJ_ICE_NOMINATED_CHECK_DELAY.
     */
    unsigned            iceNominatedCheckDelayMsec;

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
    int                 iceWaitNominationTimeoutMsec;

    /**
     * Disable RTCP component.
     *
     * Default: False
     */
    bool                iceNoRtcp;

    /**
     * Always send re-INVITE/UPDATE after ICE negotiation regardless of whether
     * the default ICE transport address is changed or not. When this is set
     * to False, re-INVITE/UPDATE will be sent only when the default ICE
     * transport address is changed.
     *
     * Default: yes
     */
    bool                iceAlwaysUpdate;

    /**
     * Enable TURN candidate in ICE.
     */
    bool                turnEnabled;

    /**
     * Specify TURN domain name or host name, in in "DOMAIN:PORT" or
     * "HOST:PORT" format.
     */
    string              turnServer;

    /**
     * Specify the connection type to be used to the TURN server. Valid
     * values are PJ_TURN_TP_UDP or PJ_TURN_TP_TCP.
     *
     * Default: PJ_TURN_TP_UDP
     */
    pj_turn_tp_type     turnConnType;

    /**
     * Specify the username to authenticate with the TURN server.
     */
    string              turnUserName;

    /**
     * Specify the type of password. Currently this must be zero to
     * indicate plain-text password will be used in the password.
     */
    int                 turnPasswordType;

    /**
     * Specify the password to authenticate with the TURN server.
     */
    string              turnPassword;

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
     * Possible values:
     * * 0 (disabled).
     * * 1 (enabled). Update except if both Contact and server's IP address
     * are public but response contains private IP.
     * * 2 (enabled). Update without exception.
     *
     * See also contactRewriteMethod field.
     *
     * Default: 1
     */
    int                 contactRewriteUse;

    /**
     * Specify how Contact update will be done with the registration, if
     * \a contactRewriteEnabled is enabled. The value is bitmask combination of
     * \a pjsua_contact_rewrite_method. See also pjsua_contact_rewrite_method.
     *
     * Value PJSUA_CONTACT_REWRITE_UNREGISTER(1) is the legacy behavior.
     *
     * Default value: PJSUA_CONTACT_REWRITE_METHOD
     *   (PJSUA_CONTACT_REWRITE_NO_UNREG | PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE)
     */
    int                 contactRewriteMethod;

    /**
     * Specify if source TCP port should be used as the initial Contact
     * address if TCP/TLS transport is used. Note that this feature will
     * be automatically turned off when nameserver is configured because
     * it may yield different destination address due to DNS SRV resolution.
     * Also some platforms are unable to report the local address of the
     * TCP socket when it is still connecting. In these cases, this
     * feature will also be turned off.
     *
     * Default: 1 (PJ_TRUE / yes).
     */
    int                 contactUseSrcPort;

    /**
     * This option is used to overwrite the "sent-by" field of the Via header
     * for outgoing messages with the same interface address as the one in
     * the REGISTER request, as long as the request uses the same transport
     * instance as the previous REGISTER request.
     *
     * Default: 1 (PJ_TRUE / yes)
     */
    int                 viaRewriteUse;

    /**
     * This option controls whether the IP address in SDP should be replaced
     * with the IP address found in Via header of the REGISTER response, ONLY
     * when STUN and ICE are not used. If the value is FALSE (the original
     * behavior), then the local IP address will be used. If TRUE, and when
     * STUN and ICE are disabled, then the IP address found in registration
     * response will be used.
     *
     * Default: PJ_FALSE (no)
     */
    int                 sdpNatRewriteUse;

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
     * Default: 1 (PJ_TRUE / yes)
     */
    int                 sipOutboundUse;

    /**
     * Specify SIP outbound (RFC 5626) instance ID to be used by this
     * account. If empty, an instance ID will be generated based on
     * the hostname of this agent. If application specifies this parameter, the
     * value will look like "<urn:uuid:00000000-0000-1000-8000-AABBCCDDEEFF>"
     * without the double-quotes.
     *
     * Default: empty
     */
    string              sipOutboundInstanceId;

    /**
     * Specify SIP outbound (RFC 5626) registration ID. The default value
     * is empty, which would cause the library to automatically generate
     * a suitable value.
     *
     * Default: empty
     */
    string              sipOutboundRegId;

    /**
     * Set the interval for periodic keep-alive transmission for this account.
     * If this value is zero, keep-alive will be disabled for this account.
     * The keep-alive transmission will be sent to the registrar's address,
     * after successful registration.
     *
     * Default: 15 (seconds)
     */
    unsigned            udpKaIntervalSec;

    /**
     * Specify the data to be transmitted as keep-alive packets.
     *
     * Default: CR-LF
     */
    string              udpKaData;

public:
    /**
     * Default constructor
     */
    AccountNatConfig() : sipStunUse(PJSUA_STUN_USE_DEFAULT),
      mediaStunUse(PJSUA_STUN_USE_DEFAULT),
      sipUpnpUse(PJSUA_UPNP_USE_DEFAULT),
      mediaUpnpUse(PJSUA_UPNP_USE_DEFAULT),
      nat64Opt(PJSUA_NAT64_DISABLED),
      iceEnabled(false),
      iceTrickle(PJ_ICE_SESS_TRICKLE_DISABLED),
      iceMaxHostCands(-1),
      iceAggressiveNomination(true),
      iceNominatedCheckDelayMsec(PJ_ICE_NOMINATED_CHECK_DELAY),
      iceWaitNominationTimeoutMsec(ICE_CONTROLLED_AGENT_WAIT_NOMINATION_TIMEOUT),
      iceNoRtcp(false),
      iceAlwaysUpdate(true),
      turnEnabled(false),
      turnConnType(PJ_TURN_TP_UDP),
      turnPasswordType(0),
      contactRewriteUse(PJ_TRUE),
      contactRewriteMethod(PJSUA_CONTACT_REWRITE_METHOD),
      contactUseSrcPort(PJ_TRUE),
      viaRewriteUse(PJ_TRUE),
      sdpNatRewriteUse(PJ_FALSE),
      sipOutboundUse(PJ_TRUE),
      udpKaIntervalSec(15),
      udpKaData("\r\n")
    {}

    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * This structure contains parameters for Account::sendRequest()
 */
struct SendRequestParam
{
    /**
     * Token or arbitrary user data ownd by the application,
     * which will be passed back in callback Account::onSendRequest().
     */
    Token        userData;

    /**
     * SIP method of the request.
     */
    string       method;

    /**
     * Message body and/or list of headers etc. to be included in
     * the outgoing request.
     */
    SipTxOption  txOption;

public:
    /**
     * Default constructor initializes with zero/empty values.
     */
    SendRequestParam();
};



/**
 * SRTP crypto.
 */
struct SrtpCrypto
{
    /**
     * Optional key. If empty, a random key will be autogenerated.
     */
    string      key;

    /**
     * Crypto name.
     */
    string      name;

    /**
     * Flags, bitmask from #pjmedia_srtp_crypto_option
     */
    unsigned    flags;

public:
    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_srtp_crypto &prm);

    /**
     * Convert to pjsip
     */
    pjmedia_srtp_crypto toPj() const;
};

/** Array of SRTP cryptos. */
typedef std::vector<SrtpCrypto> SrtpCryptoVector;

/**
 * SRTP settings.
 */
struct SrtpOpt : public PersistentObject
{
    /**
     * Specify SRTP cryptos. If empty, all crypto will be enabled.
     * Available crypto can be enumerated using Endpoint::srtpCryptoEnum().
     *
     * Default: empty.
     */
    SrtpCryptoVector            cryptos;

    /**
     * Specify SRTP keying methods, valid keying method is defined in
     * pjmedia_srtp_keying_method. If empty, all keying methods will be
     * enabled with priority order: SDES, DTLS-SRTP.
     *
     * Default: empty.
     */
    IntVector                   keyings;

public:
    /**
     * Default constructor initializes with default values.
     */
    SrtpOpt();

    /**
     * Convert from pjsip
     */
    void fromPj(const pjsua_srtp_opt &prm);

    /**
     * Convert to pjsip
     */
    pjsua_srtp_opt toPj() const;

public:
    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * RTCP Feedback capability.
 */
struct RtcpFbCap
{
    /**
     * Specify the codecs to which the capability is applicable. Codec ID is
     * using the same format as in pjmedia_codec_mgr_find_codecs_by_id() and
     * pjmedia_vid_codec_mgr_find_codecs_by_id(), e.g: "L16/8000/1", "PCMU",
     * "H264". This can also be an asterisk ("*") to represent all codecs.
     */
    string                  codecId;

    /**
     * Specify the RTCP Feedback type.
     */
    pjmedia_rtcp_fb_type    type;

    /**
     * Specify the type name if RTCP Feedback type is PJMEDIA_RTCP_FB_OTHER.
     */
    string                  typeName;

    /**
     * Specify the RTCP Feedback parameters.
     */
    string                  param;

public:
    /**
     * Constructor.
     */
    RtcpFbCap() : type(PJMEDIA_RTCP_FB_OTHER)
    {}

    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_rtcp_fb_cap &prm);

    /**
     * Convert to pjsip
     */
    pjmedia_rtcp_fb_cap toPj() const;
};

/** Array of RTCP Feedback capabilities. */
typedef std::vector<RtcpFbCap> RtcpFbCapVector;


/**
 * RTCP Feedback settings.
 */
struct RtcpFbConfig : public PersistentObject
{
    /**
     * Specify whether transport protocol in SDP media description uses
     * RTP/AVP instead of RTP/AVPF. Note that the standard mandates to signal
     * AVPF profile, but it may cause SDP negotiation failure when negotiating
     * with endpoints that does not support RTCP Feedback (including older
     * version of PJSIP).
     *
     * Default: false.
     */
    bool                    dontUseAvpf;

    /**
     * RTCP Feedback capabilities.
     */
    RtcpFbCapVector         caps;

public:
    /**
     * Constructor.
     */
    RtcpFbConfig();

    /**
     * Convert from pjsip
     */
    void fromPj(const pjmedia_rtcp_fb_setting &prm);

    /**
     * Convert to pjsip
     */
    pjmedia_rtcp_fb_setting toPj() const;

public:
    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * Account media config (applicable for both audio and video). This will be
 * specified in AccountConfig.
 */
struct AccountMediaConfig : public PersistentObject
{
    /**
     * Media transport (RTP) configuration.
     * 
     * For \a port and \a portRange settings, RTCP port is selected as 
     * RTP port+1.
     * Example: \a port=5000, \a portRange=4
     * - Available ports: 5000, 5002, 5004 (Media/RTP transport)
     *                    5001, 5003, 5005 (Media/RTCP transport)
     */
    TransportConfig     transportConfig;

    /**
     * If remote sends SDP answer containing more than one format or codec in
     * the media line, send re-INVITE or UPDATE with just one codec to lock
     * which codec to use.
     *
     * Default: True (Yes).
     */
    bool                lockCodecEnabled;

    /**
     * Specify whether stream keep-alive and NAT hole punching with
     * non-codec-VAD mechanism (see PJMEDIA_STREAM_ENABLE_KA) is enabled
     * for this account.
     *
     * Default: False
     */
    bool                streamKaEnabled;

    /**
     * Specify whether secure media transport should be used for this account.
     * Valid values are PJMEDIA_SRTP_DISABLED, PJMEDIA_SRTP_OPTIONAL, and
     * PJMEDIA_SRTP_MANDATORY.
     *
     * Default: PJSUA_DEFAULT_USE_SRTP
     */
    pjmedia_srtp_use    srtpUse;

    /**
     * Specify whether SRTP requires secure signaling to be used. This option
     * is only used when \a use_srtp option above is non-zero.
     *
     * Valid values are:
     *  0: SRTP does not require secure signaling
     *  1: SRTP requires secure transport such as TLS
     *  2: SRTP requires secure end-to-end transport (SIPS)
     *
     * Default: PJSUA_DEFAULT_SRTP_SECURE_SIGNALING
     */
    int                 srtpSecureSignaling;

    /**
     * Specify SRTP settings, like cryptos and keying methods.
     */
    SrtpOpt             srtpOpt;

    /**
     * Specify whether IPv6 should be used on media.
     *
     * Default: PJSUA_IPV6_ENABLED_PREFER_IPV4
     * (Dual stack media, capable to use IPv4/IPv6.
     * Outgoing offer will prefer to use IPv4)
     */
    pjsua_ipv6_use      ipv6Use;

    /**
     * Enable RTP and RTCP multiplexing.
     * Default: false
     */
    bool                rtcpMuxEnabled;

    /**
     * RTCP Feedback settings.
     */
    RtcpFbConfig        rtcpFbConfig;

    /**
     * Enable RTCP Extended Report (RTCP XR).
     *
     * Default: PJMEDIA_STREAM_ENABLE_XR
     */
    bool                rtcpXrEnabled;

    /**
     * Use loopback media transport. This may be useful if application
     * doesn't want PJSUA2 to create real media transports/sockets, such as
     * when using third party media.
     *
     * Default: false
     */
    bool                useLoopMedTp;

    /**
     * Enable local loopback when useLoopMedTp is set to TRUE.
     * If enabled, packets sent to the transport will be sent back to
     * the streams attached to the transport.
     *
     * Default: false
     */
    bool                enableLoopback;

public:
    /**
     * Default constructor
     */
    AccountMediaConfig() 
    : lockCodecEnabled(true),
      streamKaEnabled(false),
      srtpUse(PJSUA_DEFAULT_USE_SRTP),
      srtpSecureSignaling(PJSUA_DEFAULT_SRTP_SECURE_SIGNALING),
      ipv6Use(PJSUA_IPV6_ENABLED_PREFER_IPV4),
      rtcpMuxEnabled(false),
      rtcpXrEnabled(PJMEDIA_STREAM_ENABLE_XR),
      useLoopMedTp(false),
      enableLoopback(false)
    {}

    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * Account video config. This will be specified in AccountConfig.
 */
struct AccountVideoConfig : public PersistentObject
{
    /**
     * Specify whether incoming video should be shown to screen by default.
     * This applies to incoming call (INVITE), incoming re-INVITE, and
     * incoming UPDATE requests.
     *
     * Regardless of this setting, application can detect incoming video
     * by implementing \a on_call_media_state() callback and enumerating
     * the media stream(s) with pjsua_call_get_info(). Once incoming
     * video is recognised, application may retrieve the window associated
     * with the incoming video and show or hide it with
     * pjsua_vid_win_set_show().
     *
     * Default: False
     */
    bool                        autoShowIncoming;

    /**
     * Specify whether outgoing video should be activated by default when
     * making outgoing calls and/or when incoming video is detected. This
     * applies to incoming and outgoing calls, incoming re-INVITE, and
     * incoming UPDATE. If the setting is non-zero, outgoing video
     * transmission will be started as soon as response to these requests
     * is sent (or received).
     *
     * Regardless of the value of this setting, application can start and
     * stop outgoing video transmission with pjsua_call_set_vid_strm().
     *
     * Default: False
     */
    bool                        autoTransmitOutgoing;

    /**
     * Specify video window's flags. The value is a bitmask combination of
     * pjmedia_vid_dev_wnd_flag.
     *
     * Default: 0
     */
    unsigned                    windowFlags;

    /**
     * Specify the default capture device to be used by this account. If
     * vidOutAutoTransmit is enabled, this device will be used for
     * capturing video.
     *
     * Default: PJMEDIA_VID_DEFAULT_CAPTURE_DEV
     */
    pjmedia_vid_dev_index       defaultCaptureDevice;

    /**
     * Specify the default rendering device to be used by this account.
     *
     * Default: PJMEDIA_VID_DEFAULT_RENDER_DEV
     */
    pjmedia_vid_dev_index       defaultRenderDevice;

    /**
     * Rate control method.
     *
     * Default: PJMEDIA_VID_STREAM_RC_SIMPLE_BLOCKING.
     */
    pjmedia_vid_stream_rc_method rateControlMethod;

    /**
     * Upstream/outgoing bandwidth. If this is set to zero, the video stream
     * will use codec maximum bitrate setting.
     *
     * Default: 0 (follow codec maximum bitrate).
     */
    unsigned                    rateControlBandwidth;

    /**
     * The number of keyframe to be sent after the stream is created.
     *
     * Default: PJMEDIA_VID_STREAM_START_KEYFRAME_CNT
     */
    unsigned                        startKeyframeCount;

    /**
     * The keyframe sending interval after the stream is created.
     *
     * Default: PJMEDIA_VID_STREAM_START_KEYFRAME_INTERVAL_MSEC
     */
    unsigned                        startKeyframeInterval;


public:
    /**
     * Default constructor
     */
    AccountVideoConfig() 
    : autoShowIncoming(false),
      autoTransmitOutgoing(false),
      windowFlags(0),
      defaultCaptureDevice(PJMEDIA_VID_DEFAULT_CAPTURE_DEV),
      defaultRenderDevice(PJMEDIA_VID_DEFAULT_RENDER_DEV),
      rateControlMethod(PJMEDIA_VID_STREAM_RC_SIMPLE_BLOCKING),
      rateControlBandwidth(0),
      startKeyframeCount(PJMEDIA_VID_STREAM_START_KEYFRAME_CNT),
      startKeyframeInterval(PJMEDIA_VID_STREAM_START_KEYFRAME_INTERVAL_MSEC)
    {}

    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * Account config specific to IP address change.
 */
typedef struct AccountIpChangeConfig
{    
    /**
     * Shutdown the transport used for account registration. If this is set to
     * PJ_TRUE, the transport will be shutdown altough it's used by multiple
     * account. Shutdown transport will be followed by re-Registration if
     * AccountConfig.natConfig.contactRewriteUse is enabled.
     *
     * Default: true
     */
    bool                shutdownTp;

    /**
     * Hangup active calls associated with the acount. If this is set to true, 
     * then the calls will be hang up.
     *
     * Default: false
     */
    bool                hangupCalls;

    /**
     * Specify the call flags used in the re-INVITE when \a hangupCalls is set 
     * to false. If this is set to 0, no re-INVITE will be sent. The 
     * re-INVITE will be sent after re-Registration is finished.
     *
     * Default: PJSUA_CALL_REINIT_MEDIA | PJSUA_CALL_UPDATE_CONTACT |
     *          PJSUA_CALL_UPDATE_VIA
     */
    unsigned            reinviteFlags;

    /**
     * For refreshing the call, use SIP UPDATE, instead of re-INVITE, if
     * remote supports it (by publishing it in Allow header). If remote
     * does not support UPDATE method or somehow the UPDATE attempt fails,
     * it will fallback to using re-INVITE. The \a reinviteFlags will be
     * used regardless whether it is re-INVITE or UPDATE that is sent.
     *
     * Default: PJ_FALSE (using re-INVITE).
     */
    unsigned            reinvUseUpdate;

public:
    /**
     * Virtual destructor
     */
    virtual ~AccountIpChangeConfig()
    {}

    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
    
} AccountIpChangeConfig;

/**
 * Account configuration.
 */
struct AccountConfig : public PersistentObject
{
    /**
     * Account priority, which is used to control the order of matching
     * incoming/outgoing requests. The higher the number means the higher
     * the priority is, and the account will be matched first.
     */
    int                 priority;

    /**
     * The Address of Record or AOR, that is full SIP URL that identifies the
     * account. The value can take name address or URL format, and will look
     * something like "sip:account@serviceprovider".
     *
     * This field is mandatory.
     */
    string              idUri;

    /**
     * Registration settings.
     */
    AccountRegConfig    regConfig;

    /**
     * SIP settings.
     */
    AccountSipConfig    sipConfig;

    /**
     * Call settings.
     */
    AccountCallConfig   callConfig;

    /**
     * Presence settings.
     */
    AccountPresConfig   presConfig;

    /**
     * MWI (Message Waiting Indication) settings.
     */
    AccountMwiConfig    mwiConfig;

    /**
     * NAT settings.
     */
    AccountNatConfig    natConfig;

    /**
     * Media settings (applicable for both audio and video).
     */
    AccountMediaConfig  mediaConfig;

    /**
     * Video settings.
     */
    AccountVideoConfig  videoConfig;

    /**
     * IP Change settings.
     */
    AccountIpChangeConfig ipChangeConfig;

public:
    /**
     * Default constructor will initialize with default values.
     */
    AccountConfig();

    /**
     * This will return a temporary pjsua_acc_config instance, which contents
     * are only valid as long as this AccountConfig structure remains valid
     * AND no modifications are done to it AND no further toPj() function call
     * is made. Any call to toPj() function will invalidate the content of
     * temporary pjsua_acc_config that was returned by the previous call.
     */
    void toPj(pjsua_acc_config &cfg) const;

    /**
     * Initialize from pjsip.
     */
    void fromPj(const pjsua_acc_config &prm, const pjsua_media_config *mcfg);

    /**
     * Read this object from a container node.
     *
     * @param node              Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node              Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
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
    pjsua_acc_id        id;

    /**
     * Flag to indicate whether this is the default account.
     */
    bool                isDefault;

    /**
     * Account URI
     */
    string              uri;

    /**
     * Flag to tell whether this account has registration setting
     * (reg_uri is not empty).
     */
    bool                regIsConfigured;

    /**
     * Flag to tell whether this account is currently registered
     * (has active registration session).
     */
    bool                regIsActive;

    /**
     * An up to date expiration interval for account registration session.
     */
    unsigned            regExpiresSec;

    /**
     * Last registration status code. If status code is zero, the account
     * is currently not registered. Any other value indicates the SIP
     * status code of the registration.
     */
    pjsip_status_code   regStatus;

    /**
     * String describing the registration status.
     */
    string              regStatusText;

    /**
     * Last registration error code. When the status field contains a SIP
     * status code that indicates a registration failure, last registration
     * error code contains the error code that causes the failure. In any
     * other case, its value is zero.
     */
    pj_status_t         regLastErr;

    /**
     * Presence online status for this account.
     */
    bool                onlineStatus;

    /**
     * Presence online status text.
     */
    string              onlineStatusText;

public:
    /**
     * Default constructor
     */
    AccountInfo() : id(PJSUA_INVALID_ID), 
                    isDefault(false),
                    regIsConfigured(false),
                    regIsActive(false),
                    regExpiresSec(0),
                    regStatus(PJSIP_SC_NULL),
                    regLastErr(-1),
                    onlineStatus(false)
    {}

    /** Import from pjsip data */
    void fromPj(const pjsua_acc_info &pai);
};

/**
 * This structure contains parameters for onIncomingCall() account callback.
 */
struct OnIncomingCallParam
{
    /**
     * The library call ID allocated for the new call.
     */
    int                 callId;

    /**
     * The incoming INVITE request.
     */
    SipRxData           rdata;
};

/**
 * This structure contains parameters for onRegStarted() account callback.
 */
struct OnRegStartedParam
{
    /**
     * True for registration and False for unregistration.
     */
    bool renew;
};

/**
 * This structure contains parameters for onRegState() account callback.
 */
struct OnRegStateParam
{
    /**
     * Registration operation status.
     */
    pj_status_t         status;

    /**
     * SIP status code received.
     */
    pjsip_status_code   code;

    /**
     * SIP reason phrase received.
     */
    string              reason;

    /**
     * The incoming message.
     */
    SipRxData           rdata;

    /**
     * Next expiration interval.
     */
    unsigned            expiration;
};

/**
 * This structure contains parameters for onIncomingSubscribe() callback.
 */
struct OnIncomingSubscribeParam
{
    /**
     * Server presence subscription instance. If application delays
     * the acceptance of the request, it will need to specify this object
     * when calling Account::presNotify().
     */
    void               *srvPres;

    /**
     *  Sender URI.
     */
    string              fromUri;

    /**
     * The incoming message.
     */
    SipRxData           rdata;

    /**
     * The status code to respond to the request. The default value is 200.
     * Application may set this to other final status code to accept or
     * reject the request.
     */
    pjsip_status_code   code;

    /**
     * The reason phrase to respond to the request.
     */
    string              reason;

    /**
     * Additional data to be sent with the response, if any.
     */
    SipTxOption         txOption;
};

/**
 * Parameters for onInstantMessage() account callback.
 */
struct OnInstantMessageParam
{
    /**
     * Sender From URI.
     */
    string              fromUri;

    /**
     * To URI of the request.
     */
    string              toUri;

    /**
     * Contact URI of the sender.
     */
    string              contactUri;

    /**
     * MIME type of the message body.
     */
    string              contentType;

    /**
     * The message body.
     */
    string              msgBody;

    /**
     * The whole message.
     */
    SipRxData           rdata;
};

/**
 * Parameters for onInstantMessageStatus() account callback.
 */
struct OnInstantMessageStatusParam
{
    /**
     * Token or a user data that was associated with the pager
     * transmission.
     */
    Token               userData;

    /**
     * Destination URI.
     */
    string              toUri;

    /**
     * The message body.
     */
    string              msgBody;

    /**
     * The SIP status code of the transaction.
     */
    pjsip_status_code   code;

    /**
     * The reason phrase of the transaction.
     */
    string              reason;

    /**
     * The incoming response that causes this callback to be called.
     * If the transaction fails because of time out or transport error,
     * the content will be empty.
     */
    SipRxData           rdata;
};

/**
 * Parameters for onTypingIndication() account callback.
 */
struct OnTypingIndicationParam
{
    /**
     * Sender/From URI.
     */
    string              fromUri;

    /**
     * To URI.
     */
    string              toUri;

    /**
     * The Contact URI.
     */
    string              contactUri;

    /**
     * Boolean to indicate if sender is typing.
     */
    bool                isTyping;

    /**
     * The whole message buffer.
     */
    SipRxData           rdata;
};

/**
 * Parameters for onMwiInfo() account callback.
 */
struct OnMwiInfoParam
{
    /**
     * MWI subscription state.
     */
    pjsip_evsub_state   state;

    /**
     * The whole message buffer.
     */
    SipRxData           rdata;
};

/**
 * This structure contains parameters for Account::onSendRequest() callback.
 */
struct OnSendRequestParam
{
    /**
     * Token or arbitrary user data owned by the application,
     * which was passed to Endpoint::sendRquest() function.
     */
    Token               userData;

    /**
     * Transaction event that caused the state change.
     */
    SipEvent    e;
};


/**
 * Parameters for presNotify() account method.
 */
struct PresNotifyParam
{
    /**
     * Server presence subscription instance.
     */
    void               *srvPres;

    /**
     * Server presence subscription state to set.
     */
    pjsip_evsub_state   state;
    
    /**
     * Optionally specify the state string name, if state is not "active",
     * "pending", or "terminated".
     */
    string              stateStr;

    /**
     * If the new state is PJSIP_EVSUB_STATE_TERMINATED, optionally specify
     * the termination reason.
     */
    string              reason;

    /**
     * If the new state is PJSIP_EVSUB_STATE_TERMINATED, this specifies
     * whether the NOTIFY request should contain message body containing
     * account's presence information.
     */
    bool                withBody;

    /**
     * Optional list of headers to be sent with the NOTIFY request.
     */
    SipTxOption         txOption;
};


/**
 * Wrapper class for Buddy matching algo.
 *
 * Default algo is a simple substring lookup of search-token in the
 * Buddy URIs, with case sensitive. Application can implement its own
 * matching algo by overriding this class and specifying its instance
 * in Account::findBuddy().
 */
class FindBuddyMatch
{
public:
    /**
     * Default algo implementation.
     */
    virtual bool match(const string &token, const Buddy &buddy)
    {
        BuddyInfo bi = buddy.getInfo();
        return bi.uri.find(token) != string::npos;
    }

    /**
     * Destructor.
     */
    virtual ~FindBuddyMatch() {}
};


/**
 * Account.
 */
class Account
{
public:
    /**
     * Constructor.
     */
    Account();

    /**
     * Destructor. Note that if the account is deleted, it will also delete
     * the corresponding account in the PJSUA-LIB.
     *
     * If application implements a derived class, the derived class should
     * call shutdown() in the beginning stage in its destructor, or
     * alternatively application should call shutdown() before deleting
     * the derived class instance. This is to avoid race condition between
     * the derived class destructor and Account callbacks.
     */
    virtual ~Account();

    /**
     * Create the account.
     *
     * If application implements a derived class, the derived class should
     * call shutdown() in the beginning stage in its destructor, or
     * alternatively application should call shutdown() before deleting
     * the derived class instance. This is to avoid race condition between
     * the derived class destructor and Account callbacks.
     *
     * @param cfg               The account config.
     * @param make_default      Make this the default account.
     */
    void create(const AccountConfig &cfg,
                bool make_default=false) PJSUA2_THROW(Error);

    /**
     * Shutdown the account. This will initiate unregistration if needed,
     * and delete the corresponding account in the PJSUA-LIB.
     *
     * Note that application must delete all Buddy instances belong to this
     * account before shutting down the account.
     *
     * If application implements a derived class, the derived class should
     * call this method in the beginning stage in its destructor, or
     * alternatively application should call this method before deleting
     * the derived class instance. This is to avoid race condition between
     * the derived class destructor and Account callbacks.
     */
    void shutdown();

    /**
     * Modify the account to use the specified account configuration.
     * Depending on the changes, this may cause unregistration or
     * reregistration on the account.
     *
     * @param cfg               New account config to be applied to the
     *                          account.
     */
    void modify(const AccountConfig &cfg) PJSUA2_THROW(Error);

    /**
     * Check if this account is still valid.
     *
     * @return                  True if it is.
     */
    bool isValid() const;

    /**
     * Set this as default account to be used when incoming and outgoing
     * requests don't match any accounts.
     */
    void setDefault() PJSUA2_THROW(Error);

    /**
     * Check if this account is the default account. Default account will be
     * used for incoming and outgoing requests that don't match any other
     * accounts.
     *
     * @return                  True if this is the default account.
     */
    bool isDefault() const;

    /**
     * Get PJSUA-LIB account ID or index associated with this account.
     *
     * @return                  Integer greater than or equal to zero.
     */
    int getId() const;

    /**
     * Get the Account class for the specified account Id.
     *
     * @param acc_id            The account ID to lookup
     *
     * @return                  The Account instance or NULL if not found.
     */
    static Account *lookup(int acc_id);

    /**
     * Get account info.
     *
     * @return                  Account info.
     */
    AccountInfo getInfo() const PJSUA2_THROW(Error);

    /**
     * Send arbitrary requests using the account. Application should only use
     * this function to create auxiliary requests outside dialog, such as
     * OPTIONS, and use the call or presence API to create dialog related
     * requests.
     *
     * @param prm.method    SIP method of the request.
     * @param prm.txOption  Optional message body and/or list of headers to be
     *                      included in outgoing request.
     */
    void sendRequest(const pj::SendRequestParam& prm) PJSUA2_THROW(Error);

    /**
     * Update registration or perform unregistration. Application normally
     * only needs to call this function if it wants to manually update the
     * registration or to unregister from the server.
     *
     * @param renew             If False, this will start unregistration
     *                          process.
     */
    void setRegistration(bool renew) PJSUA2_THROW(Error);

    /**
     * Set or modify account's presence online status to be advertised to
     * remote/presence subscribers. This would trigger the sending of
     * outgoing NOTIFY request if there are server side presence subscription
     * for this account, and/or outgoing PUBLISH if presence publication is
     * enabled for this account.
     *
     * @param pres_st           Presence online status.
     */
    void setOnlineStatus(const PresenceStatus &pres_st) PJSUA2_THROW(Error);

    /**
     * Lock/bind this account to a specific transport/listener. Normally
     * application shouldn't need to do this, as transports will be selected
     * automatically by the library according to the destination.
     *
     * When account is locked/bound to a specific transport, all outgoing
     * requests from this account will use the specified transport (this
     * includes SIP registration, dialog (call and event subscription), and
     * out-of-dialog requests such as MESSAGE).
     *
     * Note that transport id may be specified in AccountConfig too.
     *
     * @param tp_id             The transport ID.
     */
    void setTransport(TransportId tp_id) PJSUA2_THROW(Error);

    /**
     * Send NOTIFY to inform account presence status or to terminate server
     * side presence subscription. If application wants to reject the incoming
     * request, it should set the param \a PresNotifyParam.state to
     * PJSIP_EVSUB_STATE_TERMINATED.
     *
     * @param prm               The sending NOTIFY parameter.
     */
    void presNotify(const PresNotifyParam &prm) PJSUA2_THROW(Error);
    
#if !DEPRECATED_FOR_TICKET_2232
    /**
     * Warning: deprecated, use enumBuddies2() instead. This function is not
     * safe in multithreaded environment.
     *
     * Enumerate all buddies of the account.
     *
     * @return                  The buddy list.
     */
    const BuddyVector& enumBuddies() const PJSUA2_THROW(Error);
#endif

    /**
     * Enumerate all buddies of the account.
     *
     * @return                  The buddy list.
     */
    BuddyVector2 enumBuddies2() const PJSUA2_THROW(Error);

#if !DEPRECATED_FOR_TICKET_2232
    /**
     * Warning: deprecated, use findBuddy2 instead. This function is not
     * safe in multithreaded environment.
     *
     * Find a buddy in the buddy list with the specified URI. 
     *
     * Exception: if buddy is not found, PJ_ENOTFOUND will be thrown.
     *
     * @param uri               The buddy URI.
     * @param buddy_match       The buddy match algo.
     *
     * @return                  The pointer to buddy.
     */
    Buddy* findBuddy(string uri, FindBuddyMatch *buddy_match = NULL) const
                    PJSUA2_THROW(Error);
#endif

    /**
     * Find a buddy in the buddy list with the specified URI. 
     *
     * Exception: if buddy is not found, PJ_ENOTFOUND will be thrown.
     *
     * @param uri               The buddy URI.
     *
     * @return                  The pointer to buddy.
     */
    Buddy findBuddy2(string uri) const PJSUA2_THROW(Error);

public:
    /*
     * Callbacks
     */
    /**
     * Notify application on incoming call.
     *
     * @param prm       Callback parameter.
     */
    virtual void onIncomingCall(OnIncomingCallParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application when registration or unregistration has been
     * initiated. Note that this only notifies the initial registration
     * and unregistration. Once registration session is active, subsequent
     * refresh will not cause this callback to be called.
     *
     * @param prm           Callback parameter.
     */
    virtual void onRegStarted(OnRegStartedParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application when registration status has changed.
     * Application may then query the account info to get the
     * registration details.
     *
     * @param prm           Callback parameter.
     */
    virtual void onRegState(OnRegStateParam &prm)
    { PJ_UNUSED_ARG(prm); }

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
     *  - it may reject the request immediately by specifying non-200 class
     *    final response in the IncomingSubscribeParam.code parameter.
     *  - it may immediately accept the request by specifying 200 as the
     *    IncomingSubscribeParam.code parameter. This is the default value if
     *    application doesn't set any value to the IncomingSubscribeParam.code
     *    parameter. In this case, the library will automatically send NOTIFY
     *    request upon returning from this callback.
     *  - it may delay the processing of the request, for example to request
     *    user permission whether to accept or reject the request. In this
     *    case, the application MUST set the IncomingSubscribeParam.code
     *    argument to 202, then IMMEDIATELY calls presNotify() with
     *    state PJSIP_EVSUB_STATE_PENDING and later calls presNotify()
     *    again to accept or reject the subscription request.
     *
     * Any IncomingSubscribeParam.code other than 200 and 202 will be treated
     * as 200.
     *
     * Application MUST return from this callback immediately (e.g. it must
     * not block in this callback while waiting for user confirmation).
     *
     * @param prm           Callback parameter.
     */
    virtual void onIncomingSubscribe(OnIncomingSubscribeParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application on incoming instant message or pager (i.e. MESSAGE
     * request) that was received outside call context.
     *
     * @param prm           Callback parameter.
     */
    virtual void onInstantMessage(OnInstantMessageParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application about the delivery status of outgoing pager/instant
     * message (i.e. MESSAGE) request.
     *
     * @param prm           Callback parameter.
     */
    virtual void onInstantMessageStatus(OnInstantMessageStatusParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application when a transaction started by Account::sendRequest()
     * has been completed,i.e. when a response has been received.
     *
     * @param prm       Callback parameter.
     */
    virtual void onSendRequest(OnSendRequestParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notify application about typing indication.
     *
     * @param prm           Callback parameter.
     */
    virtual void onTypingIndication(OnTypingIndicationParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notification about MWI (Message Waiting Indication) status change.
     * This callback can be called upon the status change of the
     * SUBSCRIBE request (for example, 202/Accepted to SUBSCRIBE is received)
     * or when a NOTIFY reqeust is received.
     *
     * @param prm           Callback parameter.
     */
    virtual void onMwiInfo(OnMwiInfoParam &prm)
    { PJ_UNUSED_ARG(prm); }

private:
    friend class Endpoint;
    friend class Buddy;

    /**
     * An internal function to add a Buddy to Account buddy list.
     * This method is used by Buddy::create().
     */
    void addBuddy(Buddy *buddy);

    /**
     * An internal function to remove a Buddy from Account buddy list.
     * This method is used by Buddy::~Buddy().
     */
    void removeBuddy(Buddy *buddy);

private:
    pjsua_acc_id         id;
    string               tmpReason;     // for saving response's reason
#if !DEPRECATED_FOR_TICKET_2232
    BuddyVector          buddyList;
#endif
};

/**
 * @}  // PJSUA2_ACC
 */

} // namespace pj

#endif  /* __PJSUA2_ACCOUNT_HPP__ */

