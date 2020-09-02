/* $Id$ */
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
#ifndef __PJSUA2_UA_HPP__
#define __PJSUA2_UA_HPP__

/**
 * @file pjsua2/endpoint.hpp
 * @brief PJSUA2 Base Agent Operation
 */
#include <pjsua2/persistent.hpp>
#include <pjsua2/media.hpp>
#include <pjsua2/siptypes.hpp>
#include <list>
#include <map>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_UA Endpoint
 * @ingroup PJSUA2_Ref
 * @{
 */

using std::string;
using std::vector;


//////////////////////////////////////////////////////////////////////////////

/**
 * Argument to Endpoint::onNatDetectionComplete() callback.
 */
struct OnNatDetectionCompleteParam
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
 * Argument to Endpoint::onNatCheckStunServersComplete() callback.
 */
struct OnNatCheckStunServersCompleteParam
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
 * Parameter of Endpoint::onTimer() callback.
 */
struct OnTimerParam
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
 * SSL certificate type and name structure.
 */
struct SslCertName
{
    pj_ssl_cert_name_type  type;    	    /**< Name type		*/
    string		   name;    	    /**< The name		*/

public:
    /**
     * Default constructor
     */
    SslCertName() : type(PJ_SSL_CERT_NAME_UNKNOWN)
    {}
};

/** Array of SSL certificate type and name. */
typedef std::vector<SslCertName> SslCertNameVector;

/**
 * SSL certificate information.
 */
struct SslCertInfo
{
    unsigned		version;	    /**< Certificate version	*/
    unsigned char	serialNo[20];	    /**< Serial number, array
				         	 of octets, first index
					 	 is MSB			*/
    string		subjectCn;	    /**< Subject common name	*/
    string		subjectInfo;	    /**< One line subject, fields
					 	 are separated by slash, e.g:
					 	 "CN=sample.org/OU=HRD" */

    string		issuerCn;	    /**< Issuer common name	*/
    string		issuerInfo;	    /**< One line subject, fields
					 	 are separated by slash */

    TimeVal		validityStart;	    /**< Validity start		*/
    TimeVal		validityEnd;	    /**< Validity end		*/
    bool		validityGmt;	    /**< Flag if validity 
					 	 date/time use GMT	*/

    SslCertNameVector	subjectAltName;     /**< Subject alternative
					 	 name extension		*/

    string 		raw;		    /**< Raw certificate in PEM
    						 format, only available
					 	 for remote certificate */

public:
    /**
     * Constructor.
     */
    SslCertInfo();

    /**
     * Check if the info is set with empty values.
     *
     * @return      	True if the info is empty.
     */
    bool isEmpty() const;

    /**
     * Convert from pjsip
     */
    void fromPj(const pj_ssl_cert_info &info);
    
private:
    bool empty;
};

/**
 * TLS transport information.
 */
struct TlsInfo
{
    /**
     * Describes whether secure socket connection is established, i.e: TLS/SSL 
     * handshaking has been done successfully.
     */
    bool 		established;

    /**
     * Describes secure socket protocol being used, see #pj_ssl_sock_proto. 
     * Use bitwise OR operation to combine the protocol type.
     */
    unsigned 		protocol;

    /**
     * Describes cipher suite being used, this will only be set when connection
     * is established.
     */
    pj_ssl_cipher	cipher;

    /**
     * Describes cipher name being used, this will only be set when connection
     * is established.
     */
    string		cipherName;

    /**
     * Describes local address.
     */
    SocketAddress 	localAddr;

    /**
     * Describes remote address.
     */
    SocketAddress 	remoteAddr;
   
    /**
     * Describes active local certificate info. Use SslCertInfo.isEmpty()
     * to check if the local cert info is available.
     */
    SslCertInfo 	localCertInfo;
   
    /**
     * Describes active remote certificate info. Use SslCertInfo.isEmpty()
     * to check if the remote cert info is available.
     */
    SslCertInfo 	remoteCertInfo;

    /**
     * Status of peer certificate verification.
     */
    unsigned		verifyStatus;

    /**
     * Error messages (if any) of peer certificate verification, based on
     * the field verifyStatus above.
     */
    StringVector	verifyMsgs;

public:
    /**
     * Constructor.
     */
    TlsInfo();

    /**
     * Check if the info is set with empty values.
     *
     * @return      	True if the info is empty.
     */
    bool isEmpty() const;

    /**
     * Convert from pjsip
     */
    void fromPj(const pjsip_tls_state_info &info);

private:
    bool empty;
};

/**
 * Parameter of Endpoint::onTransportState() callback.
 */
struct OnTransportStateParam
{
    /**
     * The transport handle.
     */
    TransportHandle	hnd;
    
    /**
     * The transport type.
     */
    string		type;

    /**
     * Transport current state.
     */
    pjsip_transport_state state;

    /**
     * The last error code related to the transport state.
     */
    pj_status_t		lastError;
    
    /**
     * TLS transport info, only used if transport type is TLS. Use 
     * TlsInfo.isEmpty() to check if this info is available.
     */
    TlsInfo		tlsInfo;
};

/**
 * Parameter of Endpoint::onSelectAccount() callback.
 */
struct OnSelectAccountParam
{
    /**
     * The incoming request.
     */
    SipRxData		rdata;

    /**
     * The account index to be used to handle the request.
     * Upon entry, this will be filled by the account index
     * chosen by the library. Application may change it to
     * another value to use another account.
     */
    int			accountIndex;
};

/**
 * Parameter of Endpoint::handleIpChange().
 */
struct IpChangeParam {
    /**
     * If set to PJ_TRUE, this will restart the transport listener.
     * 
     * Default : PJ_TRUE
     */
    bool	    restartListener;

    /** 
     * If \a restartListener is set to PJ_TRUE, some delay might be needed 
     * for the listener to be restarted. Use this to set the delay.
     * 
     * Default : PJSUA_TRANSPORT_RESTART_DELAY_TIME
     */
    unsigned	    restartLisDelay;

public:
    /**
     * Constructor.
     */
    IpChangeParam();

    /**
     * Export to pjsua_ip_change_param.
     */
    pjsua_ip_change_param toPj() const;

    /**
     * Convert from pjsip
     */
    void fromPj(const pjsua_ip_change_param &param);
};

/**
 * Information of Update contact on IP change progress.
 */
struct RegProgressParam
{
    /**
     * Indicate if this is a Register or Un-Register message.
     */
    bool    isRegister;

    /**
     * SIP status code received.
     */
    int	    code;
};

/**
 * Parameter of Endpoint::onIpChangeProgress().
 */
struct OnIpChangeProgressParam
{
    /**
     * The IP change progress operation.
     */
    pjsua_ip_change_op	op;

    /**
     * The operation progress status.
     */
    pj_status_t		status;

    /**
     * Information of the transport id. This is only available when the 
     * operation is PJSUA_IP_CHANGE_OP_RESTART_LIS.
     */
    TransportId		transportId;

    /**
     * Information of the account id. This is only available when the 
     * operation is:
     * - PJSUA_IP_CHANGE_OP_ACC_SHUTDOWN_TP 
     * - PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT 
     * - PJSUA_IP_CHANGE_OP_ACC_HANGUP_CALLS
     * - PJSUA_IP_CHANGE_OP_ACC_REINVITE_CALLS
     */
    int			accId;

    /**
     * Information of the call id. This is only available when the operation is
     * PJSUA_IP_CHANGE_OP_ACC_HANGUP_CALLS or 
     * PJSUA_IP_CHANGE_OP_ACC_REINVITE_CALLS
     */
    int			callId;

    /**
     * Registration information. This is only available when the operation is
     * PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT
     */
    RegProgressParam	regInfo;
};

/**
 * Parameter of Endpoint::onCallMediaEvent() callback.
 */
struct OnMediaEventParam
{
    /**
     * The media event.
     */
    MediaEvent      ev;
};

//////////////////////////////////////////////////////////////////////////////
/**
 * SIP User Agent related settings.
 */
struct UaConfig : public PersistentObject
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
     * When this flag is non-zero, all callbacks that come from thread
     * other than main thread will be posted to the main thread and
     * to be executed by Endpoint::libHandleEvents() function. This
     * includes the logging callback. Note that this will only work if
     * threadCnt is set to zero and Endpoint::libHandleEvents() is
     * performed by main thread. By default, the main thread is set
     * from the thread that invoke Endpoint::libCreate()
     *
     * Default: false
     */
    bool		mainThreadOnly;

    /**
     * Array of nameservers to be used by the SIP resolver subsystem.
     * The order of the name server specifies the priority (first name
     * server will be used first, unless it is not reachable).
     */
    StringVector	nameserver;

    /**
     * Specify the URL of outbound proxies to visit for all outgoing requests.
     * The outbound proxies will be used for all accounts, and it will
     * be used to build the route set for outgoing requests. The final
     * route set for outgoing requests will consists of the outbound proxies
     * and the proxy configured in the account.
     */
    StringVector	outboundProxies;

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
     * pj_gethostbyname() if it's not an IP address. Port number may be
     * specified if the server is not listening in standard STUN port.
     */
    StringVector	stunServer;

    /**
     * This specifies if the library should try to do an IPv6 resolution of
     * the STUN servers if the IPv4 resolution fails. It can be useful
     * in an IPv6-only environment, including on NAT64.
     *
     * Default: FALSE
     */
    bool	    	stunTryIpv6;

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
     * request and forward the request to Endpoint::onMwiInfo() callback.
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

    /**
     * Read this object from a container.
     *
     * @param node		Container to write values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);

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
struct LogConfig : public PersistentObject
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
     * Additional flags to be given to pj_file_open() when opening
     * the log file. By default, the flag is PJ_O_WRONLY. Application
     * may set PJ_O_APPEND here so that logs are appended to existing
     * file instead of overwriting it.
     *
     * Default is 0.
     */
    unsigned		fileFlags;

    /**
     * Custom log writer, if required. This instance will be destroyed
     * by the endpoint when the endpoint is destroyed.
     */
    LogWriter		*writer;

public:
    /** Default constructor initialises with default values */
    LogConfig();

    /** Construct from pjsua_logging_config */
    void fromPj(const pjsua_logging_config &lc);

    /** Generate pjsua_logging_config. */
    pjsua_logging_config toPj() const;

    /**
     * Read this object from a container.
     *
     * @param node		Container to write values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};


/**
 * This structure describes media configuration, which will be specified
 * when calling Lib::init().
 */
struct MediaConfig : public PersistentObject
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
     * Echo canceller options (see pjmedia_echo_create()).
     * Specify PJMEDIA_ECHO_USE_SW_ECHO here if application wishes
     * to use software echo canceller instead of device EC.
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
     * Set the algorithm the jitter buffer uses to discard frames in order to
     * adjust the latency.
     *
     * Default: PJMEDIA_JB_DISCARD_PROGRESSIVE
     */
    pjmedia_jb_discard_algo jbDiscardAlgo;

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

    /**
     * Read this object from a container.
     *
     * @param node		Container to write values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};


/**
 * Endpoint configuration
 */
struct EpConfig : public PersistentObject
{
    /** UA config */
    UaConfig		uaConfig;

    /** Logging config */
    LogConfig		logConfig;

    /** Media config */
    MediaConfig		medConfig;

    /**
     * Read this object from a container.
     *
     * @param node		Container to write values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);

};

/* This represents posted job */
struct PendingJob
{
    /** Perform the job */
    virtual void execute(bool is_pending) = 0;

    /** Virtual destructor */
    virtual ~PendingJob() {}
};

//////////////////////////////////////////////////////////////////////////////

/**
 * Endpoint represents an instance of pjsua library. There can only be
 * one instance of pjsua library in an application, hence this class
 * is a singleton.
 */
class Endpoint
{
public:
    /** Retrieve the singleton instance of the endpoint */
    static Endpoint &instance() PJSUA2_THROW(Error);

    /** Default constructor */
    Endpoint();

    /** Virtual destructor */
    virtual ~Endpoint();


    /*************************************************************************
     * Base library operations
     */

    /**
     * Get library version.
     */
    Version libVersion() const;

    /**
     * Instantiate pjsua application. Application must call this function before
     * calling any other functions, to make sure that the underlying libraries
     * are properly initialized. Once this function has returned success,
     * application must call libDestroy() before quitting.
     */
    void libCreate() PJSUA2_THROW(Error);

    /**
     * Get library state.
     *
     * @return			library state.
     */
    pjsua_state libGetState() const;

    /**
     * Initialize pjsua with the specified settings. All the settings are
     * optional, and the default values will be used when the config is not
     * specified.
     *
     * Note that create() MUST be called before calling this function.
     *
     * @param prmEpConfig	Endpoint configurations
     */
    void libInit( const EpConfig &prmEpConfig) PJSUA2_THROW(Error);

    /**
     * Call this function after all initialization is done, so that the
     * library can do additional checking set up. Application may call this
     * function any time after init().
     */
    void libStart() PJSUA2_THROW(Error);

    /**
     * Register a thread that was created by external or native API to the
     * library. Note that each time this function is called, it will allocate
     * some memory to store the thread description, which will only be freed
     * when the library is destroyed.
     *
     * @param name	The optional name to be assigned to the thread.
     */
    void libRegisterThread(const string &name) PJSUA2_THROW(Error);

    /**
     * Check if this thread has been registered to the library. Note that
     * this function is only applicable for library main & worker threads and
     * external/native threads registered using libRegisterThread().
     */
    bool libIsThreadRegistered();

    /**
     * Stop all worker threads.
     */
    void libStopWorkerThreads();

    /**
     * Poll pjsua for events, and if necessary block the caller thread for
     * the specified maximum interval (in miliseconds).
     *
     * Application doesn't normally need to call this function if it has
     * configured worker thread (\a thread_cnt field) in pjsua_config
     * structure, because polling then will be done by these worker threads
     * instead.
     *
     * If EpConfig::UaConfig::mainThreadOnly is enabled and this function
     * is called from the main thread (by default the main thread is thread
     * that calls libCreate()), this function will also scan and run any
     * pending jobs in the list.
     *
     * @param msec_timeout Maximum time to wait, in miliseconds.
     *
     * @return		The number of events that have been handled during the
     *	    		poll. Negative value indicates error, and application
     *	    		can retrieve the error as (status = -return_value).
     */
    int libHandleEvents(unsigned msec_timeout);

    /**
     * Destroy pjsua. Application is recommended to perform graceful shutdown
     * before calling this function (such as unregister the account from the
     * SIP server, terminate presense subscription, and hangup active calls),
     * however, this function will do all of these if it finds there are
     * active sessions that need to be terminated. This function will
     * block for few seconds to wait for replies from remote.
     *
     * Application.may safely call this function more than once if it doesn't
     * keep track of it's state.
     *
     * @param prmFlags	Combination of pjsua_destroy_flag enumeration.
     */
    void libDestroy(unsigned prmFlags=0) PJSUA2_THROW(Error);


    /*************************************************************************
     * Utilities
     */

    /**
     * Retrieve the error string for the specified status code.
     *
     * @param prmErr		The error code.
     */
    string utilStrError(pj_status_t prmErr);

    /**
     * Write a log message.
     *
     * @param prmLevel		Log verbosity level (1-5)
     * @param prmSender		The log sender.
     * @param prmMsg		The log message.
     */
    void utilLogWrite(int prmLevel,
                      const string &prmSender,
                      const string &prmMsg);

    /**
     * Write a log entry.
     * Application must implement its own custom LogWriter and
     * this function will then call the LogWriter::write() method.
     * Note that this function does not call PJSIP's internal
     * logging functionality. For that, you should use
     * utilLogWrite(prmLevel, prmSender, prmMsg) above.
     *
     * @param e			The log entry.
     */
    void utilLogWrite(LogEntry &e);

    /**
     * This is a utility function to verify that valid SIP url is given. If the
     * URL is a valid SIP/SIPS scheme, PJ_SUCCESS will be returned.
     *
     * @param prmUri		The URL string.
     *
     * @return			PJ_SUCCESS on success, or the appropriate error
     * 				code.
     *
     * @see utilVerifyUri()
     */
    pj_status_t utilVerifySipUri(const string &prmUri);

    /**
     * This is a utility function to verify that valid URI is given. Unlike
     * utilVerifySipUri(), this function will return PJ_SUCCESS if tel: URI
     * is given.
     *
     * @param prmUri		The URL string.
     *
     * @return			PJ_SUCCESS on success, or the appropriate error
     * 				code.
     *
     * @see pjsua_verify_sip_url()
     */
    pj_status_t utilVerifyUri(const string &prmUri);

    /**
     * Schedule a timer with the specified interval and user data. When the
     * interval elapsed, onTimer() callback will be
     * called. Note that the callback may be executed by different thread,
     * depending on whether worker thread is enabled or not.
     *
     * @param prmMsecDelay	The time interval in msec.
     * @param prmUserData	Arbitrary user data, to be given back to
     * 				application in the callback.
     *
     * @return			Token to identify the timer, which could be
     * 				given to utilTimerCancel().
     */
    Token utilTimerSchedule(unsigned prmMsecDelay,
                            Token prmUserData) PJSUA2_THROW(Error);

    /**
     * Cancel previously scheduled timer with the specified timer token.
     *
     * @param prmToken		The timer token, which was returned from
     * 				previous utilTimerSchedule() call.
     */
    void utilTimerCancel(Token prmToken);

    /**
     * Utility to register a pending job to be executed by main thread.
     * If EpConfig::UaConfig::mainThreadOnly is false, the job will be
     * executed immediately.
     *
     * @param job		The job class.
     */
    void utilAddPendingJob(PendingJob *job);

    /**
     * Get cipher list supported by SSL/TLS backend.
     */
    IntVector utilSslGetAvailableCiphers() PJSUA2_THROW(Error);

    /*************************************************************************
     * NAT operations
     */
    /**
     * This is a utility function to detect NAT type in front of this endpoint.
     * Once invoked successfully, this function will complete asynchronously
     * and report the result in onNatDetectionComplete().
     *
     * After NAT has been detected and the callback is called, application can
     * get the detected NAT type by calling natGetType(). Application
     * can also perform NAT detection by calling natDetectType()
     * again at later time.
     *
     * Note that STUN must be enabled to run this function successfully.
     */
    void natDetectType(void) PJSUA2_THROW(Error);

    /**
     * Get the NAT type as detected by natDetectType() function. This
     * function will only return useful NAT type after natDetectType()
     * has completed successfully and onNatDetectionComplete()
     * callback has been called.
     *
     * Exception: if this function is called while detection is in progress,
     * PJ_EPENDING exception will be raised.
     */
    pj_stun_nat_type natGetType() PJSUA2_THROW(Error);

    /**
     * Update the STUN servers list. The libInit() must have been called
     * before calling this function.
     *
     * @param prmServers	Array of STUN servers to try. The endpoint
     * 				will try to resolve and contact each of the
     * 				STUN server entry until it finds one that is
     * 				usable. Each entry may be a domain name, host
     * 				name, IP address, and it may contain an
     * 				optional port number. For example:
     *				- "pjsip.org" (domain name)
     *				- "sip.pjsip.org" (host name)
     *				- "pjsip.org:33478" (domain name and a non-
     *				   standard port number)
     *				- "10.0.0.1:3478" (IP address and port number)
     * @param prmWait		Specify if the function should block until
     *				it gets the result. In this case, the
     *				function will block while the resolution
     * 				is being done, and the callback
     * 				onNatCheckStunServersComplete() will be called
     * 				before this function returns.
     *
     */
    void natUpdateStunServers(const StringVector &prmServers,
                              bool prmWait) PJSUA2_THROW(Error);

    /**
     * Auxiliary function to resolve and contact each of the STUN server
     * entries (sequentially) to find which is usable. The libInit() must
     * have been called before calling this function.
     *
     * @param prmServers	Array of STUN servers to try. The endpoint
     * 				will try to resolve and contact each of the
     * 				STUN server entry until it finds one that is
     * 				usable. Each entry may be a domain name, host
     * 				name, IP address, and it may contain an
     * 				optional port number. For example:
     *				- "pjsip.org" (domain name)
     *				- "sip.pjsip.org" (host name)
     *				- "pjsip.org:33478" (domain name and a non-
     *				   standard port number)
     *				- "10.0.0.1:3478" (IP address and port number)
     * @param prmWait		Specify if the function should block until
     *				it gets the result. In this case, the function
     *				will block while the resolution is being done,
     *				and the callback will be called before this
     *				function returns.
     * @param prmUserData	Arbitrary user data to be passed back to
     * 				application in the callback.
     *
     * @see natCancelCheckStunServers()
     */
    void natCheckStunServers(const StringVector &prmServers,
                             bool prmWait,
                             Token prmUserData) PJSUA2_THROW(Error);

    /**
     * Cancel pending STUN resolution which match the specified token.
     *
     * @param token		The token to match. This token was given to
     *				natCheckStunServers()
     * @param notify_cb		Boolean to control whether the callback should
     *				be called for cancelled resolutions. When the
     *				callback is called, the status in the result
     *				will be set as PJ_ECANCELLED.
     *
     * Exception: PJ_ENOTFOUND if there is no matching one, or other error.
     */
    void natCancelCheckStunServers(Token token,
                                   bool notify_cb = false) PJSUA2_THROW(Error);

    /*************************************************************************
     * Transport operations
     */

    /**
     * Create and start a new SIP transport according to the specified
     * settings.
     *
     * @param type		Transport type.
     * @param cfg		Transport configuration.
     *
     * @return			The transport ID.
     */
    TransportId transportCreate(pjsip_transport_type_e type,
                                const TransportConfig &cfg) PJSUA2_THROW(Error);

    /**
     * Enumerate all transports currently created in the system. This
     * function will return all transport IDs, and application may then
     * call transportGetInfo() function to retrieve detailed information
     * about the transport.
     *
     * @return			Array of transport IDs.
     */
    IntVector transportEnum() PJSUA2_THROW(Error);

    /**
     * Get information about transport.
     *
     * @param id		Transport ID.
     *
     * @return			Transport info.
     */
    TransportInfo transportGetInfo(TransportId id) PJSUA2_THROW(Error);

    /**
     * Disable a transport or re-enable it. By default transport is always
     * enabled after it is created. Disabling a transport does not necessarily
     * close the socket, it will only discard incoming messages and prevent
     * the transport from being used to send outgoing messages.
     *
     * @param id		Transport ID.
     * @param enabled		Enable or disable the transport.
     *
     */
    void transportSetEnable(TransportId id, bool enabled) PJSUA2_THROW(Error);

    /**
     * Close the transport. The system will wait until all transactions are
     * closed while preventing new users from using the transport, and will
     * close the transport when its usage count reaches zero.
     *
     * @param id		Transport ID.
     */
    void transportClose(TransportId id) PJSUA2_THROW(Error);
    
    /**
     * Start graceful shutdown procedure for this transport handle. After
     * graceful shutdown has been initiated, no new reference can be
     * obtained for the transport. However, existing objects that currently
     * uses the transport may still use this transport to send and receive
     * packets. After all objects release their reference to this transport,
     * the transport will be destroyed immediately.
     *
     * Note: application normally uses this API after obtaining the handle
     * from onTransportState() callback.
     *
     * @param tp		The transport.
     */
    void transportShutdown(TransportHandle tp) PJSUA2_THROW(Error);

    /*************************************************************************
     * Call operations
     */
    
    /**
     * Terminate all calls. This will initiate call hangup for all
     * currently active calls.
     */
    void hangupAllCalls(void);
    
    /*************************************************************************
     * Media operations
     */

    /**
     * Add media to the media list.
     *
     * @param media	media to be added.
     */
    void mediaAdd(AudioMedia &media);

    /**
     * Remove media from the media list.
     *
     * @param media	media to be removed.
     */
    void mediaRemove(AudioMedia &media);

    /**
     * Check if media has been added to the media list.
     *
     * @param media	media to be check.
     *
     * @return 		True if media has been added, false otherwise.
     */
    bool mediaExists(const AudioMedia &media) const;

    /**
     * Get maximum number of media port.
     *
     * @return		Maximum number of media port in the conference bridge.
     */
    unsigned mediaMaxPorts() const;

    /**
     * Get current number of active media port in the bridge.
     *
     * @return		The number of active media port.
     */
    unsigned mediaActivePorts() const;

#if !DEPRECATED_FOR_TICKET_2232
    /**
     * Warning: deprecated, use mediaEnumPorts2() instead. This function is
     * not safe in multithreaded environment.
     *
     * Enumerate all media port.
     *
     * @return		The list of media port.
     */
    const AudioMediaVector &mediaEnumPorts() const PJSUA2_THROW(Error);
#endif

    /**
     * Enumerate all audio media port.
     *
     * @return		The list of audio media port.
     */
    AudioMediaVector2 mediaEnumPorts2() const PJSUA2_THROW(Error);

    /**
     * Enumerate all video media port.
     *
     * @return		The list of video media port.
     */
    VideoMediaVector mediaEnumVidPorts() const PJSUA2_THROW(Error);

    /**
     * Get the instance of Audio Device Manager.
     *
     * @return		The Audio Device Manager.
     */
    AudDevManager &audDevManager();

    /**
     * Get the instance of Video Device Manager.
     *
     * @return		The Video Device Manager.
     */
    VidDevManager &vidDevManager();

    /*************************************************************************
     * Codec management operations
     */

#if !DEPRECATED_FOR_TICKET_2232
    /**
     * Warning: deprecated, use codecEnum2() instead. This function is not
     * safe in multithreaded environment.
     *
     * Enum all supported codecs in the system.
     *
     * @return		Array of codec info.
     */
    const CodecInfoVector &codecEnum() PJSUA2_THROW(Error);
#endif

    /**
     * Enum all supported codecs in the system.
     *
     * @return		Array of codec info.
     */
    CodecInfoVector2 codecEnum2() const PJSUA2_THROW(Error);

    /**
     * Change codec priority.
     *
     * @param codec_id	Codec ID, which is a string that uniquely identify
     *			the codec (such as "speex/8000").
     * @param priority	Codec priority, 0-255, where zero means to disable
     *			the codec.
     *
     */
    void codecSetPriority(const string &codec_id,
			  pj_uint8_t priority) PJSUA2_THROW(Error);

    /**
     * Get codec parameters.
     *
     * @param codec_id	Codec ID.
     *
     * @return		Codec parameters. If codec is not found, Error
     * 			will be thrown.
     *
     */
    CodecParam codecGetParam(const string &codec_id) const PJSUA2_THROW(Error);

    /**
     * Set codec parameters.
     *
     * @param codec_id	Codec ID.
     * @param param	Codec parameter to set. Set to NULL to reset
     *			codec parameter to library default settings.
     *
     */
    void codecSetParam(const string &codec_id,
		       const CodecParam param) PJSUA2_THROW(Error);

#if !DEPRECATED_FOR_TICKET_2232
    /**
     * Warning: deprecated, use videoCodecEnum2() instead. This function is
     * not safe in multithreaded environment.
     *
     * Enum all supported video codecs in the system.
     *  
     * @return		Array of video codec info.
     */
    const CodecInfoVector &videoCodecEnum() PJSUA2_THROW(Error);
#endif

    /**
     * Enum all supported video codecs in the system.
     *  
     * @return		Array of video codec info.
     */
    CodecInfoVector2 videoCodecEnum2() const PJSUA2_THROW(Error);

    /**
     * Change video codec priority.
     *
     * @param codec_id	Codec ID, which is a string that uniquely identify
     *			the codec (such as "H263/90000"). Please see pjsua
     *			manual or pjmedia codec reference for details.
     * @param priority	Codec priority, 0-255, where zero means to disable
     *			the codec.
     *
     */
    void videoCodecSetPriority(const string &codec_id,
			       pj_uint8_t priority) PJSUA2_THROW(Error);

    /**
     * Get video codec parameters.
     *
     * @param codec_id	Codec ID.
     *
     * @return		Codec parameters. If codec is not found, Error 
     *			will be thrown.
     *
     */
    VidCodecParam getVideoCodecParam(const string &codec_id) const
				     PJSUA2_THROW(Error);

    /**
     * Set video codec parameters.
     *
     * @param codec_id	Codec ID.
     * @param param	Codec parameter to set.
     *
     */
    void setVideoCodecParam(const string &codec_id,
			    const VidCodecParam &param) PJSUA2_THROW(Error);
			    
    /**
     * Reset video codec parameters to library default settings.
     *
     * @param codec_id	Codec ID.
     *
     */
    void resetVideoCodecParam(const string &codec_id) PJSUA2_THROW(Error);

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC!=0)
    /**
     * Get codec Opus config.
     *
     */
     CodecOpusConfig getCodecOpusConfig() const PJSUA2_THROW(Error);

    /**
     * Set codec Opus config.
     *
     * @param opus_cfg	Codec Opus configuration.
     *
     */
    void setCodecOpusConfig(const CodecOpusConfig &opus_cfg)
			    PJSUA2_THROW(Error);
#endif

    /**
     * Enumerate all SRTP crypto-suite names.
     *
     * @return		The list of SRTP crypto-suite name.
     */
    StringVector srtpCryptoEnum() PJSUA2_THROW(Error);

    /*************************************************************************
     * IP Change
     */

    /**
     * Inform the stack that IP address change event was detected.
     * The stack will:
     * 1. Restart the listener (this step is configurable via
     *    \a IpChangeParam.restartListener).
     * 2. Shutdown the transport used by account registration (this step is
     *    configurable via \a AccountConfig.ipChangeConfig.shutdownTp).
     * 3. Update contact URI by sending re-Registration (this step is 
     *    configurable via a\ AccountConfig.natConfig.contactRewriteUse and
     *    a\ AccountConfig.natConfig.contactRewriteMethod)
     * 4. Hangup active calls (this step is configurable via
     *    a\ AccountConfig.ipChangeConfig.hangupCalls) or
     *    continue the call by sending re-INVITE
     *    (configurable via \a AccountConfig.ipChangeConfig.reinviteFlags).
     *
     * @param param	The IP change parameter, have a look at #IpChangeParam.
     *
     * @return		PJ_SUCCESS on success, other on error.
     */
    void handleIpChange(const IpChangeParam &param) PJSUA2_THROW(Error);

public:
    /*
     * Overrideables callbacks
     */

    /**
     * Callback when the Endpoint has finished performing NAT type
     * detection that is initiated with natDetectType().
     *
     * @param prm	Callback parameters containing the detection
     * 			result.
     */
    virtual void onNatDetectionComplete(
			const OnNatDetectionCompleteParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Callback when the Endpoint has finished performing STUN server
     * checking that is initiated when calling libInit(), or by
     * calling natCheckStunServers() or natUpdateStunServers().
     *
     * @param prm	Callback parameters.
     */
    virtual void onNatCheckStunServersComplete(
			const OnNatCheckStunServersCompleteParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * This callback is called when transport state has changed.
     *
     * @param prm	Callback parameters.
     */
    virtual void onTransportState(
			const OnTransportStateParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Callback when a timer has fired. The timer was scheduled by
     * utilTimerSchedule().
     *
     * @param prm	Callback parameters.
     */
    virtual void onTimer(const OnTimerParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * This callback can be used by application to override the account
     * to be used to handle an incoming message. Initially, the account to
     * be used will be calculated automatically by the library. This initial
     * account will be used if application does not implement this callback,
     * or application sets an invalid account upon returning from this
     * callback.
     *
     * Note that currently the incoming messages requiring account assignment
     * are INVITE, MESSAGE, SUBSCRIBE, and unsolicited NOTIFY. This callback
     * may be called before the callback of the SIP event itself, i.e:
     * incoming call, pager, subscription, or unsolicited-event.
     *
     * @param prm	Callback parameters.
     */
    virtual void onSelectAccount(OnSelectAccountParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Calling #handleIpChange() may involve different operation. This 
     * callback is called to report the progress of each enabled operation.
     *
     * @param prm	Callback parameters.
     * 
     */
    virtual void onIpChangeProgress(OnIpChangeProgressParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Notification about media events such as video notifications. This
     * callback will most likely be called from media threads, thus
     * application must not perform heavy processing in this callback.
     * If application needs to perform more complex tasks to handle the
     * event, it should post the task to another thread.
     *
     * @param prm	Callback parameter.
     */
    virtual void onMediaEvent(OnMediaEventParam &prm)
    { PJ_UNUSED_ARG(prm); }

private:
    static Endpoint		*instance_;	// static instance
    LogWriter			*writer;	// Custom writer, if any
    AudDevManager		 audioDevMgr;
    VidDevManager		 videoDevMgr;
#if !DEPRECATED_FOR_TICKET_2232
    CodecInfoVector		 codecInfoList;
    CodecInfoVector		 videoCodecInfoList;
#endif
    std::map<pj_thread_t*, pj_thread_desc*> threadDescMap;
    pj_mutex_t			*threadDescMutex;
#if !DEPRECATED_FOR_TICKET_2232
    AudioMediaVector 	 	 mediaList;
    pj_mutex_t			*mediaListMutex;
#endif

    /* Pending logging */
    bool			 mainThreadOnly;
    void			*mainThread;
    unsigned			 pendingJobSize;
    std::list<PendingJob*>	 pendingJobs;

    void performPendingJobs();

    /* Endpoint static callbacks */
    static void logFunc(int level, const char *data, int len);
    static void stun_resolve_cb(const pj_stun_resolve_result *result);
    static void on_timer(pj_timer_heap_t *timer_heap,
        		 struct pj_timer_entry *entry);
    static void on_nat_detect(const pj_stun_nat_detect_result *res);
    static void on_transport_state(pjsip_transport *tp,
    				   pjsip_transport_state state,
    				   const pjsip_transport_state_info *info);

private:
    /*
     * Account & Call lookups
     */
    static Account	*lookupAcc(int acc_id, const char *op);
    static Call		*lookupCall(int call_id, const char *op);

    /* static callbacks */
    static void on_incoming_call(pjsua_acc_id acc_id,
                                 pjsua_call_id call_id,
                                 pjsip_rx_data *rdata);
    static void on_reg_started(pjsua_acc_id acc_id,
                               pj_bool_t renew);
    static void on_reg_state2(pjsua_acc_id acc_id,
                              pjsua_reg_info *info);
    static void on_incoming_subscribe(pjsua_acc_id acc_id,
				      pjsua_srv_pres *srv_pres,
				      pjsua_buddy_id buddy_id,
				      const pj_str_t *from,
				      pjsip_rx_data *rdata,
				      pjsip_status_code *code,
				      pj_str_t *reason,
				      pjsua_msg_data *msg_data);
    static void on_pager2(pjsua_call_id call_id,
                          const pj_str_t *from,
                          const pj_str_t *to,
                          const pj_str_t *contact,
                          const pj_str_t *mime_type,
                          const pj_str_t *body,
                          pjsip_rx_data *rdata,
                          pjsua_acc_id acc_id);
    static void on_pager_status2(pjsua_call_id call_id,
				 const pj_str_t *to,
				 const pj_str_t *body,
				 void *user_data,
				 pjsip_status_code status,
				 const pj_str_t *reason,
				 pjsip_tx_data *tdata,
				 pjsip_rx_data *rdata,
				 pjsua_acc_id acc_id);
    static void on_typing2(pjsua_call_id call_id,
                           const pj_str_t *from,
                           const pj_str_t *to,
                           const pj_str_t *contact,
                           pj_bool_t is_typing,
                           pjsip_rx_data *rdata,
                           pjsua_acc_id acc_id);
    static void on_mwi_info(pjsua_acc_id acc_id,
                            pjsua_mwi_info *mwi_info);
    static void on_acc_find_for_incoming(const pjsip_rx_data *rdata,
				     	 pjsua_acc_id* acc_id);
    static void on_buddy_state(pjsua_buddy_id buddy_id);
    static void on_buddy_evsub_state(pjsua_buddy_id buddy_id,
				     pjsip_evsub *sub,
				     pjsip_event *event);
    // Call callbacks
    static void on_call_state(pjsua_call_id call_id, pjsip_event *e);
    static void on_call_tsx_state(pjsua_call_id call_id,
                                  pjsip_transaction *tsx,
                                  pjsip_event *e);
    static void on_call_media_state(pjsua_call_id call_id);
    static void on_call_sdp_created(pjsua_call_id call_id,
                                    pjmedia_sdp_session *sdp,
                                    pj_pool_t *pool,
                                    const pjmedia_sdp_session *rem_sdp);
    static void on_stream_created2(pjsua_call_id call_id,
				   pjsua_on_stream_created_param *param);
    static void on_stream_destroyed(pjsua_call_id call_id,
                                    pjmedia_stream *strm,
                                    unsigned stream_idx);
    static void on_dtmf_digit(pjsua_call_id call_id, int digit);
    static void on_dtmf_digit2(pjsua_call_id call_id, 
			       const pjsua_dtmf_info *info);
    static void on_dtmf_event(pjsua_call_id call_id,
                              const pjsua_dtmf_event *event);
    static void on_call_transfer_request(pjsua_call_id call_id,
                                         const pj_str_t *dst,
                                         pjsip_status_code *code);
    static void on_call_transfer_request2(pjsua_call_id call_id,
                                          const pj_str_t *dst,
                                          pjsip_status_code *code,
                                          pjsua_call_setting *opt);
    static void on_call_transfer_status(pjsua_call_id call_id,
                                        int st_code,
                                        const pj_str_t *st_text,
                                        pj_bool_t final,
                                        pj_bool_t *p_cont);
    static void on_call_replace_request(pjsua_call_id call_id,
                                        pjsip_rx_data *rdata,
                                        int *st_code,
                                        pj_str_t *st_text);
    static void on_call_replace_request2(pjsua_call_id call_id,
                                         pjsip_rx_data *rdata,
                                         int *st_code,
                                         pj_str_t *st_text,
                                         pjsua_call_setting *opt);
    static void on_call_replaced(pjsua_call_id old_call_id,
                                 pjsua_call_id new_call_id);
    static void on_call_rx_offer(pjsua_call_id call_id,
                                 const pjmedia_sdp_session *offer,
                                 void *reserved,
                                 pjsip_status_code *code,
                                 pjsua_call_setting *opt);
    static void on_call_rx_reinvite(pjsua_call_id call_id,
                                    const pjmedia_sdp_session *offer,
                                    pjsip_rx_data *rdata,
                                    void *reserved,
                                    pj_bool_t *async,
                                    pjsip_status_code *code,
                                    pjsua_call_setting *opt);
    static void on_call_tx_offer(pjsua_call_id call_id,
				 void *reserved,
				 pjsua_call_setting *opt);
    static pjsip_redirect_op on_call_redirected(pjsua_call_id call_id,
                                                const pjsip_uri *target,
                                                const pjsip_event *e);
    static pj_status_t
    on_call_media_transport_state(pjsua_call_id call_id,
                                  const pjsua_med_tp_state_info *info);
    static void on_media_event(pjmedia_event *event);
    static void on_call_media_event(pjsua_call_id call_id,
                                    unsigned med_idx,
                                    pjmedia_event *event);
    static pjmedia_transport*
    on_create_media_transport(pjsua_call_id call_id,
                              unsigned media_idx,
                              pjmedia_transport *base_tp,
                              unsigned flags);
    static void
    on_create_media_transport_srtp(pjsua_call_id call_id,
                                   unsigned media_idx,
                                   pjmedia_srtp_setting *srtp_opt);

    static void
    on_ip_change_progress(pjsua_ip_change_op op,
			  pj_status_t status,
			  const pjsua_ip_change_op_info *info);

private:
    void clearCodecInfoList(CodecInfoVector &codec_list);
    void updateCodecInfoList(pjsua_codec_info pj_codec[], unsigned count,
			     CodecInfoVector &codec_list);

};



/**
 * @}  PJSUA2_UA
 */

}
/* End pj namespace */


#endif	/* __PJSUA2_UA_HPP__ */

