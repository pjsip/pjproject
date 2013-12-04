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
 * @file pjsua2/ua.hpp
 * @brief PJSUA2 Base Agent Operation
 */
#include <pjsua2/persistent.hpp>
#include <pjsua2/media.hpp>
#include <pjsua2/siptypes.hpp>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_UA Base Endpoint Operations
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
 * Parameter of Endpoint::onTransportState() callback.
 */
struct OnTransportStateParam
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

//////////////////////////////////////////////////////////////////////////////

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
    virtual void readObject(const ContainerNode &node) throw(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const throw(Error);

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
     * Additional flags to be given to #pj_file_open() when opening
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
    virtual void readObject(const ContainerNode &node) throw(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const throw(Error);
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

    /**
     * Read this object from a container.
     *
     * @param node		Container to write values from.
     */
    virtual void readObject(const ContainerNode &node) throw(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const throw(Error);
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
    virtual void readObject(const ContainerNode &node) throw(Error);

    /**
     * Write this object to a container.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const throw(Error);

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
    static Endpoint &instance() throw(Error);

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
     * application must call destroy() before quitting.
     */
    void libCreate() throw(Error);

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
    void libInit( const EpConfig &prmEpConfig) throw(Error);

    /**
     * Call this function after all initialization is done, so that the
     * library can do additional checking set up. Application may call this
     * function any time after init().
     */
    void libStart() throw(Error);

    /**
     * Register a thread to poll for events. This function should be
     * called by an external worker thread, and it will block polling
     * for events until the library is destroyed.
     */
    void libRegisterWorkerThread(const string &name) throw(Error);

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
    void libDestroy(unsigned prmFlags=0) throw(Error);


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
     * interval elapsed, Endpoint::onTimer() callback will be
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
                            Token prmUserData) throw (Error);

    /**
     * Cancel previously scheduled timer with the specified timer token.
     *
     * @param prmToken		The timer token, which was returned from
     * 				previous utilTimerSchedule() call.
     */
    void utilTimerCancel(Token prmTimerToken);

    /**
     * Get cipher list supported by SSL/TLS backend.
     */
    IntVector utilSslGetAvailableCiphers() throw (Error);

    /*************************************************************************
     * NAT operations
     */
    /**
     * This is a utility function to detect NAT type in front of this endpoint.
     * Once invoked successfully, this function will complete asynchronously
     * and report the result in Endpoint::onNatDetectionComplete().
     *
     * After NAT has been detected and the callback is called, application can
     * get the detected NAT type by calling #natGetType(). Application
     * can also perform NAT detection by calling #natDetectType()
     * again at later time.
     *
     * Note that STUN must be enabled to run this function successfully.
     */
    void natDetectType(void) throw(Error);

    /**
     * Get the NAT type as detected by #natDetectType() function. This
     * function will only return useful NAT type after #natDetectType()
     * has completed successfully and Endpoint::onNatDetectionComplete()
     * callback has been called.
     *
     * Exception: if this function is called while detection is in progress,
     * PJ_EPENDING exception will be raised.
     */
    pj_stun_nat_type natGetType() throw(Error);

    /**
     * Auxiliary function to resolve and contact each of the STUN server
     * entries (sequentially) to find which is usable. The #pjsua_init() must
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
                             Token prmUserData) throw(Error);

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
                                   bool notify_cb = false) throw(Error);

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
                                const TransportConfig &cfg) throw(Error);

    /**
     * Enumerate all transports currently created in the system. This
     * function will return all transport IDs, and application may then
     * call transportGetInfo() function to retrieve detailed information
     * about the transport.
     *
     * @return			Array of transport IDs.
     */
    IntVector transportEnum() throw(Error);

    /**
     * Get information about transport.
     *
     * @param id		Transport ID.
     *
     * @return			Transport info.
     */
    TransportInfo transportGetInfo(TransportId id) throw(Error);

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
    void transportSetEnable(TransportId id, bool enabled) throw(Error);

    /**
     * Close the transport. The system will wait until all transactions are
     * closed while preventing new users from using the transport, and will
     * close the transport when its usage count reaches zero.
     *
     * @param id		Transport ID.
     */
    void transportClose(TransportId id) throw(Error);

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
    void addMedia(AudioMedia &media);

    /**
     * Remove media from the media list.
     *
     * @param media	media to be removed.
     */
    void removeMedia(AudioMedia &media);

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

    /**
     * Enumerate all media port.
     *
     * @return		The list of media port.
     */
    const AudioMediaVector &mediaEnumPorts() const throw(Error);

    AudDevManager &audDevManager();

    /*************************************************************************
     * Codec management operations
     */

    /**
     * Enum all supported codecs in the system.
     *
     * @return		Array of codec info.
     */
    const CodecInfoVector &codecEnum() throw(Error);

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
			  pj_uint8_t priority) throw(Error);

    /**
     * Get codec parameters.
     *
     * @param codec_id		Codec ID.
     *
     * @return			Codec parameters. If codec is not found, Error
     * 				will be thrown.
     *
     */
    CodecParam codecGetParam(const string &codec_id) const throw(Error);

    /**
     * Set codec parameters.
     *
     * @param codec_id	Codec ID.
     * @param param	Codec parameter to set. Set to NULL to reset
     *			codec parameter to library default settings.
     *
     */
    void codecSetParam(const string &codec_id,
		       const CodecParam param) throw(Error);


public:
    /*
     * Overrideables callbacks
     */

    /**
     * Callback when the Endpoint has finished performing NAT type
     * detection that is initiated with Endpoint::natDetectType().
     *
     * @param prm	Callback parameters containing the detection
     * 			result.
     */
    virtual void onNatDetectionComplete(
			const OnNatDetectionCompleteParam &prm)
    { PJ_UNUSED_ARG(prm); }

    /**
     * Callback when the Endpoint has finished performing STUN server
     * checking that is initiated with Endpoint::natCheckStunServers().
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
     * Endpoint::utilTimerSchedule().
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


private:
    static Endpoint		*instance_;	// static instance
    LogWriter			*writer;	// Custom writer, if any
    AudioMediaVector 	 	 mediaList;
    AudDevManager		 audioDevMgr;
    CodecInfoVector		 codecInfoList;

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

    static void on_buddy_state(pjsua_buddy_id buddy_id);
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
    static void on_stream_created(pjsua_call_id call_id,
                                  pjmedia_stream *strm,
                                  unsigned stream_idx,
                                  pjmedia_port **p_port);
    static void on_stream_destroyed(pjsua_call_id call_id,
                                    pjmedia_stream *strm,
                                    unsigned stream_idx);
    static void on_dtmf_digit(pjsua_call_id call_id, int digit);
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
    static pjsip_redirect_op on_call_redirected(pjsua_call_id call_id,
                                                const pjsip_uri *target,
                                                const pjsip_event *e);
    static pj_status_t
    on_call_media_transport_state(pjsua_call_id call_id,
                                  const pjsua_med_tp_state_info *info);
    static void on_call_media_event(pjsua_call_id call_id,
                                    unsigned med_idx,
                                    pjmedia_event *event);
    static pjmedia_transport*
    on_create_media_transport(pjsua_call_id call_id,
                              unsigned media_idx,
                              pjmedia_transport *base_tp,
                              unsigned flags);

private:
    void clearCodecInfoList();

};



/**
 * @}  PJSUA2_UA
 */

}
/* End pj namespace */


#endif	/* __PJSUA2_UA_HPP__ */

