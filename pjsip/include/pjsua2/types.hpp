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
#ifndef __PJSUA2_TYPES_HPP__
#define __PJSUA2_TYPES_HPP__

/**
 * @file pjsua2/ua.hpp
 * @brief PJSUA2 Base Agent Operation
 */
#include <pjsua2/config.hpp>

#include <string>
#include <vector>

/**
 * @defgroup PJSUA2_TYPES Data structure
 * @ingroup PJSUA2_Ref
 * @{
 */

/** PJSUA2 API is inside pj namespace */
namespace pj
{
using std::string;
using std::vector;

/** Array of strings */
typedef std::vector<std::string> StringVector;

/** Array of integers */
typedef std::vector<int> IntVector;

/**
 * Type of token, i.e. arbitrary application user data
 */
typedef void *Token;

/**
 * Socket address, encoded as string. The socket address contains host
 * and port number in "host[:port]" format. The host part may contain
 * hostname, domain name, IPv4 or IPv6 address. For IPv6 address, the
 * address will be enclosed with square brackets, e.g. "[::1]:5060".
 */
typedef string SocketAddress;

/**
 * Transport ID is an integer.
 */
typedef int TransportId;

/**
 * Transport handle, corresponds to pjsip_transport instance.
 */
typedef void *TransportHandle;


/**
 * Constants
 */
enum
{
    /** Invalid ID, equal to PJSUA_INVALID_ID */
    INVALID_ID	= -1,

    /** Success, equal to PJ_SUCCESS */
    SUCCESS = 0
};

//////////////////////////////////////////////////////////////////////////////

/**
 * This structure contains information about an error that is thrown
 * as an exception.
 */
struct Error
{
    /** The error code. */
    pj_status_t	status;

    /** The error message */
    string	reason;

    /** The PJSUA API operation that throws the error. */
    string	title;

    /** The PJSUA source file that throws the error */
    string	srcFile;

    /** The line number of PJSUA source file that throws the error */
    int		srcLine;

    /** Default constructor */
    Error();

    /**
     * Construct an Error instance from the specified parameters. If
     * \a prm_reason is empty, it will be filled with the error description
     *  for the status code.
     */
    Error(pj_status_t prm_status,
          const string &prm_reason,
          const string &prm_title,
          const string &prm_src_file,
          int prm_src_line);
};

/**
 * This is a utility macro to check the status code and raise an Error
 * exception on error.
 */
#if PJSUA2_ERROR_HAS_EXTRA_INFO
#   define PJSUA2_CHECK_RAISE_ERROR(op, status)	\
	    do { \
		if (status != PJ_SUCCESS) \
		    throw Error(status, string(), op, __FILE__, __LINE__); \
	    } while (0)
#else
#   define PJSUA2_CHECK_RAISE_ERROR(op, status)	\
	    do { \
		if (status != PJ_SUCCESS) \
		    throw Error(status, string(), string(), string(), 0); \
	    } while (0)
#endif

//////////////////////////////////////////////////////////////////////////////

/**
 * Credential information. Credential contains information to authenticate
 * against a service.
 */
struct AuthCredInfo
{
    /**
     * Realm on which this credential is to be used. Use "*" to make
     * a credential that can be used to authenticate against any challenges.
     */
    string	realm;

    /**
     * The authentication scheme (e.g. "digest").
     */
    string	scheme;

    /**
     * Authentication user name.
     */
    string	username;

    /**
     * Type of data that is contained in the "data" field. Use 0 if the data
     * contains plain text password.
     */
    int		dataType;

    /**
     * The data, which can be a plain text password or a hashed digest.
     */
    string	data;

    /*
     * Digest AKA credential information. Note that when AKA credential
     * is being used, the \a data field of this #pjsip_cred_info is
     * not used, but it still must be initialized to an empty string.
     * Please see \ref PJSIP_AUTH_AKA_API for more information.
     */

    /** Permanent subscriber key. */
    string	aka_k;

    /** Operator variant key. */
    string	aka_op;

    /** Authentication Management Field	*/
    string	aka_amf;

    /**
     * Default constructor.
     */
    AuthCredInfo();
};

/** Array of SIP credentials */
typedef std::vector<AuthCredInfo> AuthCredInfoVector;

//////////////////////////////////////////////////////////////////////////////

struct UaConfig
{
    /**
     * Maximum calls to support (default: 4). The value specified here
     * must be smaller than the compile time maximum settings
     * PJSUA_MAX_CALLS, which by default is 32. To increase this
     * limit, the library must be recompiled with new PJSUA_MAX_CALLS
     * value.
     */
    unsigned		maxCalls;

    /**
     * Number of worker threads. Normally application will want to have at
     * least one worker thread, unless when it wants to poll the library
     * periodically, which in this case the worker thread can be set to
     * zero.
     */
    unsigned		threadCnt;

    /**
     * Array of nameservers to be used by the SIP resolver subsystem.
     * The order of the name server specifies the priority (first name
     * server will be used first, unless it is not reachable).
     */
    StringVector	nameserver;

    /**
     * Optional user agent string (default empty). If it's empty, no
     * User-Agent header will be sent with outgoing requests.
     */
    string		userAgent;

    /**
     * Array of STUN servers to try. The library will try to resolve and
     * contact each of the STUN server entry until it finds one that is
     * usable. Each entry may be a domain name, host name, IP address, and
     * it may contain an optional port number. For example:
     *	- "pjsip.org" (domain name)
     *	- "sip.pjsip.org" (host name)
     *	- "pjsip.org:33478" (domain name and a non-standard port number)
     *	- "10.0.0.1:3478" (IP address and port number)
     *
     * When nameserver is configured in the \a pjsua_config.nameserver field,
     * if entry is not an IP address, it will be resolved with DNS SRV
     * resolution first, and it will fallback to use DNS A resolution if this
     * fails. Port number may be specified even if the entry is a domain name,
     * in case the DNS SRV resolution should fallback to a non-standard port.
     *
     * When nameserver is not configured, entries will be resolved with
     * #pj_gethostbyname() if it's not an IP address. Port number may be
     * specified if the server is not listening in standard STUN port.
     */
    StringVector	stunServer;

    /**
     * This specifies if the library startup should ignore failure with the
     * STUN servers. If this is set to PJ_FALSE, the library will refuse to
     * start if it fails to resolve or contact any of the STUN servers.
     *
     * Default: TRUE
     */
    bool		stunIgnoreFailure;

    /**
     * Support for adding and parsing NAT type in the SDP to assist
     * troubleshooting. The valid values are:
     *	- 0: no information will be added in SDP, and parsing is disabled.
     *	- 1: only the NAT type number is added.
     *	- 2: add both NAT type number and name.
     *
     * Default: 1
     */
    int			natTypeInSdp;

    /**
     * Handle unsolicited NOTIFY requests containing message waiting
     * indication (MWI) info. Unsolicited MWI is incoming NOTIFY requests
     * which are not requested by client with SUBSCRIBE request.
     *
     * If this is enabled, the library will respond 200/OK to the NOTIFY
     * request and forward the request to EpCallback.onMwiInfo() callback.
     *
     * See also AccountMwiConfig.enabled.
     *
     * Default: PJ_TRUE
     */
    bool	    	mwiUnsolicitedEnabled;

public:
    /**
     * Default constructor to initialize with default values.
     */
    UaConfig();

    /**
     * Construct from pjsua_config.
     */
    void fromPj(const pjsua_config &ua_cfg);

    /**
     * Export to pjsua_config
     */
    pjsua_config toPj() const;
};


/**
 * Data containing log entry to be written by the LogWriter.
 */
struct LogEntry
{
    /** Log verbosity level of this message */
    int		level;

    /** The log message */
    string	msg;

    /** ID of current thread */
    long	threadId;

    /** The name of the thread that writes this log */
    string	threadName;
};


/**
 * Interface for writing log messages. Applications can inherit this class
 * and supply it in the LogConfig structure to implement custom log
 * writing facility.
 */
class LogWriter
{
public:
    /** Destructor */
    virtual ~LogWriter() {}

    /** Write a log entry. */
    virtual void write(const LogEntry &entry) = 0;
};


/**
 * Logging configuration, which can be (optionally) specified when calling
 * Lib::init().
 */
struct LogConfig
{
    /** Log incoming and outgoing SIP message? Yes!  */
    unsigned		msgLogging;

    /** Input verbosity level. Value 5 is reasonable. */
    unsigned		level;

    /** Verbosity level for console. Value 4 is reasonable. */
    unsigned		consoleLevel;

    /** Log decoration. */
    unsigned		decor;

    /** Optional log filename if app wishes the library to write to log file.
     */
    string		filename;

    /**
     * Additional flags to be given to #pj_file_open() when opening
     * the log file. By default, the flag is PJ_O_WRONLY. Application
     * may set PJ_O_APPEND here so that logs are appended to existing
     * file instead of overwriting it.
     *
     * Default is 0.
     */
    unsigned		fileFlags;

    /**
     * Custom log writer, if required. If specified, the instance of LogWriter
     * must be kept alive througout the duration of the application.
     */
    LogWriter		*writer;

public:
    /** Default constructor initialises with default values */
    LogConfig();

    /** Construct from pjsua_logging_config */
    void fromPj(const pjsua_logging_config &lc);

    /** Generate pjsua_logging_config. */
    pjsua_logging_config toPj() const;
};


/**
 * This structure describes media configuration, which will be specified
 * when calling Lib::init().
 */
struct MediaConfig
{
public:
    /**
     * Clock rate to be applied to the conference bridge.
     * If value is zero, default clock rate will be used
     * (PJSUA_DEFAULT_CLOCK_RATE, which by default is 16KHz).
     */
    unsigned		clockRate;

    /**
     * Clock rate to be applied when opening the sound device.
     * If value is zero, conference bridge clock rate will be used.
     */
    unsigned		sndClockRate;

    /**
     * Channel count be applied when opening the sound device and
     * conference bridge.
     */
    unsigned		channelCount;

    /**
     * Specify audio frame ptime. The value here will affect the
     * samples per frame of both the sound device and the conference
     * bridge. Specifying lower ptime will normally reduce the
     * latency.
     *
     * Default value: PJSUA_DEFAULT_AUDIO_FRAME_PTIME
     */
    unsigned		audioFramePtime;

    /**
     * Specify maximum number of media ports to be created in the
     * conference bridge. Since all media terminate in the bridge
     * (calls, file player, file recorder, etc), the value must be
     * large enough to support all of them. However, the larger
     * the value, the more computations are performed.
     *
     * Default value: PJSUA_MAX_CONF_PORTS
     */
    unsigned		maxMediaPorts;

    /**
     * Specify whether the media manager should manage its own
     * ioqueue for the RTP/RTCP sockets. If yes, ioqueue will be created
     * and at least one worker thread will be created too. If no,
     * the RTP/RTCP sockets will share the same ioqueue as SIP sockets,
     * and no worker thread is needed.
     *
     * Normally application would say yes here, unless it wants to
     * run everything from a single thread.
     */
    bool		hasIoqueue;

    /**
     * Specify the number of worker threads to handle incoming RTP
     * packets. A value of one is recommended for most applications.
     */
    unsigned		threadCnt;

    /**
     * Media quality, 0-10, according to this table:
     *   5-10: resampling use large filter,
     *   3-4:  resampling use small filter,
     *   1-2:  resampling use linear.
     * The media quality also sets speex codec quality/complexity to the
     * number.
     *
     * Default: 5 (PJSUA_DEFAULT_CODEC_QUALITY).
     */
    unsigned		quality;

    /**
     * Specify default codec ptime.
     *
     * Default: 0 (codec specific)
     */
    unsigned		ptime;

    /**
     * Disable VAD?
     *
     * Default: 0 (no (meaning VAD is enabled))
     */
    bool		noVad;

    /**
     * iLBC mode (20 or 30).
     *
     * Default: 30 (PJSUA_DEFAULT_ILBC_MODE)
     */
    unsigned		ilbcMode;

    /**
     * Percentage of RTP packet to drop in TX direction
     * (to simulate packet lost).
     *
     * Default: 0
     */
    unsigned		txDropPct;

    /**
     * Percentage of RTP packet to drop in RX direction
     * (to simulate packet lost).
     *
     * Default: 0
     */
    unsigned		rxDropPct;

    /**
     * Echo canceller options (see #pjmedia_echo_create())
     *
     * Default: 0.
     */
    unsigned		ecOptions;

    /**
     * Echo canceller tail length, in miliseconds. Setting this to zero
     * will disable echo cancellation.
     *
     * Default: PJSUA_DEFAULT_EC_TAIL_LEN
     */
    unsigned		ecTailLen;

    /**
     * Audio capture buffer length, in milliseconds.
     *
     * Default: PJMEDIA_SND_DEFAULT_REC_LATENCY
     */
    unsigned		sndRecLatency;

    /**
     * Audio playback buffer length, in milliseconds.
     *
     * Default: PJMEDIA_SND_DEFAULT_PLAY_LATENCY
     */
    unsigned		sndPlayLatency;

    /**
     * Jitter buffer initial prefetch delay in msec. The value must be
     * between jb_min_pre and jb_max_pre below.
     *
     * Default: -1 (to use default stream settings, currently 150 msec)
     */
    int			jbInit;

    /**
     * Jitter buffer minimum prefetch delay in msec.
     *
     * Default: -1 (to use default stream settings, currently 60 msec)
     */
    int			jbMinPre;

    /**
     * Jitter buffer maximum prefetch delay in msec.
     *
     * Default: -1 (to use default stream settings, currently 240 msec)
     */
    int			jbMaxPre;

    /**
     * Set maximum delay that can be accomodated by the jitter buffer msec.
     *
     * Default: -1 (to use default stream settings, currently 360 msec)
     */
    int			jbMax;

    /**
     * Specify idle time of sound device before it is automatically closed,
     * in seconds. Use value -1 to disable the auto-close feature of sound
     * device
     *
     * Default : 1
     */
    int			sndAutoCloseTime;

    /**
     * Specify whether built-in/native preview should be used if available.
     * In some systems, video input devices have built-in capability to show
     * preview window of the device. Using this built-in preview is preferable
     * as it consumes less CPU power. If built-in preview is not available,
     * the library will perform software rendering of the input. If this
     * field is set to PJ_FALSE, software preview will always be used.
     *
     * Default: PJ_TRUE
     */
    bool		vidPreviewEnableNative;

public:
    /** Default constructor initialises with default values */
    MediaConfig();

    /** Construct from pjsua_media_config. */
    void fromPj(const pjsua_media_config &mc);

    /** Export */
    pjsua_media_config toPj() const;
};


/**
 * Endpoint configuration
 */
struct EpConfig
{
    /** UA config */
    UaConfig		uaConfig;

    /** Logging config */
    LogConfig		logConfig;

    /** Media config */
    MediaConfig		medConfig;
};


//////////////////////////////////////////////////////////////////////////////

/**
 * Argument to EpCallback::onNatDetectionComplete() callback.
 */
struct NatDetectionCompleteParam
{
    /**
     * Status of the detection process. If this value is not PJ_SUCCESS,
     * the detection has failed and \a nat_type field will contain
     * PJ_STUN_NAT_TYPE_UNKNOWN.
     */
    pj_status_t		status;

    /**
     * The text describing the status, if the status is not PJ_SUCCESS.
     */
    string		reason;

    /**
     * This contains the NAT type as detected by the detection procedure.
     * This value is only valid when the \a status is PJ_SUCCESS.
     */
    pj_stun_nat_type	natType;

    /**
     * Text describing that NAT type.
     */
    string		natTypeName;

};

/**
 * Argument to EpCallback::onNatCheckStunServersComplete() callback.
 */
struct NatCheckStunServersCompleteParam
{
    /**
     * Arbitrary user data that was passed to Endpoint::natCheckStunServers()
     * function.
     */
    Token		userData;

    /**
     * This will contain PJ_SUCCESS if at least one usable STUN server
     * is found, otherwise it will contain the last error code during
     * the operation.
     */
    pj_status_t		status;

    /**
     * The server name that yields successful result. This will only
     * contain value if status is successful.
     */
    string		name;

    /**
     * The server IP address and port in "IP:port" format. This will only
     * contain value if status is successful.
     */
    SocketAddress	addr;
};

/**
 * Parameter of EpCallback::OnTimerComplete() callback.
 */
struct TimerCompleteParam
{
    /**
     * Arbitrary user data that was passed to Endpoint::utilTimerSchedule()
     * function.
     */
    Token		userData;

    /**
     * The interval of this timer, in miliseconds.
     */
    unsigned		msecDelay;
};

/**
 * Parameter of EpCallback::onTransportStateChanged() callback.
 */
struct TransportStateChangedParam
{
    /**
     * The transport handle.
     */
    TransportHandle	hnd;

    /**
     * Transport current state.
     */
    pjsip_transport_state state;

    /**
     * The last error code related to the transport state.
     */
    pj_status_t		lastError;
};

//////////////////////////////////////////////////////////////////////////////

/**
 * TLS transport settings, to be specified in TransportConfig.
 */
struct TlsConfig
{
    /**
     * Certificate of Authority (CA) list file.
     */
    string		CaListFile;

    /**
     * Public endpoint certificate file, which will be used as client-
     * side  certificate for outgoing TLS connection, and server-side
     * certificate for incoming TLS connection.
     */
    string		certFile;

    /**
     * Optional private key of the endpoint certificate to be used.
     */
    string		privKeyFile;

    /**
     * Password to open private key.
     */
    string		password;

    /**
     * TLS protocol method from #pjsip_ssl_method.
     *
     * Default is PJSIP_SSL_UNSPECIFIED_METHOD (0), which in turn will
     * use PJSIP_SSL_DEFAULT_METHOD, which default value is
     * PJSIP_TLSV1_METHOD.
     */
    pjsip_ssl_method	method;

    /**
     * Ciphers and order preference. The Endpoint::utilSslGetAvailableCiphers()
     * can be used to check the available ciphers supported by backend.
     * If the array is empty, then default cipher list of the backend
     * will be used.
     */
    IntVector		ciphers;

    /**
     * Specifies TLS transport behavior on the server TLS certificate
     * verification result:
     * - If \a verifyServer is disabled, TLS transport will just notify
     *   the application via #pjsip_tp_state_callback with state
     *   PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verifyServer is enabled, TLS transport will be shutdown
     *   and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect #pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is false.
     */
    bool		verifyServer;

    /**
     * Specifies TLS transport behavior on the client TLS certificate
     * verification result:
     * - If \a verifyClient is disabled, TLS transport will just notify
     *   the application via #pjsip_tp_state_callback with state
     *   PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verifyClient is enabled, TLS transport will be shutdown
     *   and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect #pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is PJ_FALSE.
     */
    bool		verifyClient;

    /**
     * When acting as server (incoming TLS connections), reject incoming
     * connection if client doesn't supply a TLS certificate.
     *
     * This setting corresponds to SSL_VERIFY_FAIL_IF_NO_PEER_CERT flag.
     * Default value is PJ_FALSE.
     */
    bool		requireClientCert;

    /**
     * TLS negotiation timeout to be applied for both outgoing and incoming
     * connection, in milliseconds. If zero, the SSL negotiation doesn't
     * have a timeout.
     *
     * Default: zero
     */
    unsigned		msecTimeout;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qosParam fields since this is more portable.
     *
     * Default value is PJ_QOS_TYPE_BEST_EFFORT.
     */
    pj_qos_type 	qosType;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qosType field and may not be
     * supported on all platforms.
     *
     * By default all settings in this structure are disabled.
     */
    pj_qos_params 	qosParams;

    /**
     * Specify if the transport should ignore any errors when setting the QoS
     * traffic type/parameters.
     *
     * Default: PJ_TRUE
     */
    bool		qosIgnoreError;

public:
    /** Default constructor initialises with default values */
    TlsConfig();

    /** Convert to pjsip */
    pjsip_tls_setting toPj() const;

    /** Convert from pjsip */
    void fromPj(const pjsip_tls_setting &prm);
};


/**
 * Parameters to create a transport instance.
 */
struct TransportConfig
{
    /**
     * UDP port number to bind locally. This setting MUST be specified
     * even when default port is desired. If the value is zero, the
     * transport will be bound to any available port, and application
     * can query the port by querying the transport info.
     */
    unsigned		port;

    /**
     * Optional address to advertise as the address of this transport.
     * Application can specify any address or hostname for this field,
     * for example it can point to one of the interface address in the
     * system, or it can point to the public address of a NAT router
     * where port mappings have been configured for the application.
     *
     * Note: this option can be used for both UDP and TCP as well!
     */
    string		publicAddress;

    /**
     * Optional address where the socket should be bound to. This option
     * SHOULD only be used to selectively bind the socket to particular
     * interface (instead of 0.0.0.0), and SHOULD NOT be used to set the
     * published address of a transport (the public_addr field should be
     * used for that purpose).
     *
     * Note that unlike public_addr field, the address (or hostname) here
     * MUST correspond to the actual interface address in the host, since
     * this address will be specified as bind() argument.
     */
    string		boundAddress;

    /**
     * This specifies TLS settings for TLS transport. It is only be used
     * when this transport config is being used to create a SIP TLS
     * transport.
     */
    TlsConfig		tlsConfig;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qosParam fields since this is more portable.
     *
     * Default is QoS not set.
     */
    pj_qos_type		qosType;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qosType field and may not be
     * supported on all platforms.
     *
     * Default is QoS not set.
     */
    pj_qos_params	qosParams;

public:
    /** Default constructor initialises with default values */
    TransportConfig();

    /** Convert from pjsip */
    void fromPj(const pjsua_transport_config &prm);

    /** Convert to pjsip */
    pjsua_transport_config toPj() const;
};

/**
 * This structure describes transport information returned by
 * Endpoint::transportGetInfo() function.
 */
struct TransportInfo
{
    /** PJSUA transport identification. */
    TransportId	    	    id;

    /** Transport type. */
    pjsip_transport_type_e  type;

    /** Transport type name. */
    string		    typeName;

    /** Transport string info/description. */
    string		    info;

    /** Transport flags (see #pjsip_transport_flags_e). */
    unsigned		    flags;

    /** Local/bound address. */
    SocketAddress	    localAddress;

    /** Published address (or transport address name). */
    SocketAddress	    localName;

    /** Current number of objects currently referencing this transport. */
    unsigned		    usageCount;

    /** Construct from pjsip */
    TransportInfo(const pjsua_transport_info &info);

};


} // namespace pj

/**
 * @}  PJSUA2
 */



#endif	/* __PJSUA2_TYPES_HPP__ */
