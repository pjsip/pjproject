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
#ifndef __PJSUA2_UA_HPP__
#define __PJSUA2_UA_HPP__

/**
 * @file pjsua2/ua.hpp
 * @brief PJSUA2 Base Agent Operation
 */
#include <pjsua2/types.hpp>

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


/**
 * Interface for receiving callbacks from the library. Application inherits
 * this class and specify the instance when calling Endpoint::libInit().
 */
class EpCallback
{
public:
    /** Virtual destructor */
    virtual ~EpCallback() {}

    /**
     * Callback when the Endpoint has finished performing NAT type
     * detection that is initiated with Endpoint::natDetectType().
     *
     * @param prm	Callback parameters containing the detection
     * 			result.
     */
    virtual void onNatDetectionComplete(
			const NatDetectionCompleteParam &prm)
    {}

    /**
     * Callback when the Endpoint has finished performing STUN server
     * checking that is initiated with Endpoint::natCheckStunServers().
     *
     * @param prm	Callback parameters.
     */
    virtual void onNatCheckStunServersComplete(
			const NatCheckStunServersCompleteParam &prm)
    {}

    /**
     * This callback is called when transport state has changed.
     *
     * @param prm	Callback parameters.
     */
    virtual void onTransportStateChanged(
			const TransportStateChangedParam &prm)
    {}

    /**
     * Callback when a timer has fired. The timer was scheduled by
     * Endpoint::utilTimerSchedule().
     *
     * @param prm	Callback parameters.
     */
    virtual void OnTimerComplete(const TimerCompleteParam &prm)
    {}
};

/**
 * Endpoint represents an instance of pjsua library. There can only be
 * one instance of pjsua library in an application, hence this class
 * is a singleton.
 */
class Endpoint
{
public:
    /** Retrieve the singleton instance of the endpoint */
    static Endpoint &instance();

    /* For testing */
    void testException() throw(Error);


    /*************************************************************************
     * Base library operations
     */

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
     * @param prmCb		Optional callback to receive events from the
     * 				library. If specified, this instance must be
     * 				kept alive throughout the lifetime of the
     * 				library.
     */
    void libInit( const EpConfig &prmEpConfig,
                  EpCallback *prmCb = NULL) throw(Error);

    /**
     * Call this function after all initialization is done, so that the
     * library can do additional checking set up. Application may call this
     * function any time after init().
     */
    void libStart() throw(Error);

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
     * @param prmFlags		Combination of pjsua_destroy_flag enumeration.
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
     * interval elapsed, EpCallback::OnTimerComplete() callback will be
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
     * and report the result in EpCallback::onNatDetectionComplete().
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
     * has completed successfully and EpCallback::onNatDetectionComplete()
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


private:
    /* Anybody else can't instantiate Endpoint */
    Endpoint();

private:
    /* Custom writer, if any */
    LogWriter	*writer;
    EpCallback	*epCallback;

    /*
     * Callbacks (static)
     */
    static void logFunc(int level, const char *data, int len);
    static void stun_resolve_cb(const pj_stun_resolve_result *result);
    static void on_timer(pj_timer_heap_t *timer_heap,
        		 struct pj_timer_entry *entry);
    static void on_nat_detect(const pj_stun_nat_detect_result *res);
    static void on_transport_state(pjsip_transport *tp,
    				   pjsip_transport_state state,
    				   const pjsip_transport_state_info *info);
};



/**
 * @}  PJSUA2_UA
 */

}
/* End pj namespace */


#endif	/* __PJSUA2_UA_HPP__ */

