#import <Foundation/Foundation.h>
#import <PortSIPVoIPSDK/PortSIPErrors.hxx>
#import <PortSIPVoIPSDK/PortSIPTypes.hxx>
#import <PortSIPVoIPSDK/PortSIPVideoRenderView.h>
#import <PortSIPVoIPSDK/PortSIPEventDelegate.h>

/*!
* @author Copyright (c) 2006-2021 PortSIP Solutions,Inc. All rights reserved.
* @version 19
* @see http://www.PortSIP.com
* @class PortSIPSDK
* @brief PortSIP VoIP SDK functions class.
 
PortSIP SDK functions class description.
*/
@interface PortSIPSDK : NSObject

@property (nonatomic, weak) id<PortSIPEventDelegate> delegate;

/** @defgroup groupSDK SDK functions
 * SDK functions
 * @{
 */
/** @defgroup group1 Initialize and register functions
 * Initialize and register functions
 * @{
 */

/*!
 * @brief Initialize the SDK.
 *
 * @param transport Transport for SIP signaling. TRANSPORT_PERS_UDP/TRANSPORT_PERS_TCP is the PortSIP private transport for anti SIP blocking. It must be used with PERS Server.
 *  @param localIP            The local computer IP address to be bounded (for example: 192.168.1.108). It will be used for sending and receiving SIP messages and RTP packets. If this IP is transferred in IPv6 format, the SDK will use IPv6.<br>
 *                            If you want the SDK to choose correct network interface (IP) automatically, please pass the "0.0.0.0"(for IPv4) or "::" (for IPv6).
 *  @param localSIPPort       The SIP message transport listener port (for example: 5060).
 * @param logLevel Set the application log level. The SDK will generate "PortSIP_Log_datatime.log" file if the log enabled.
 * @param logFilePath   The log file path. The path (folder) MUST be existent.
 * @param maxCallLines  Theoretically, unlimited lines are supported depending on the device capability. For SIP client recommended value ranges 1 - 100.
 * @param sipAgent     The User-Agent header to be inserted in SIP messages.
 * @param audioDeviceLayer
 *              0 = Use OS default device
 *              1 = Set to 1 to use the virtual audio device if the no sound device installed.
 * @param videoDeviceLayer
 *              0 = Use OS default device
 *              1 = Set to 1 to use the virtual video device if no camera installed.
 * @param TLSCertificatesRootPath  Specify the TLS certificate path, from which the SDK will load the certificates automatically. Note: On Windows, this path will be ignored, and SDK will read the certificates from Windows certificates stored area instead.
 * @param TLSCipherList  Specify the TLS cipher list. This parameter is usually passed as empty so that the SDK will offer all available ciphers.
 * @param verifyTLSCertificate  Indicate if SDK will verify the TLS certificate. By setting to false, the SDK will not verify the validity of TLS certificate.
 * @param dnsServers Additional Nameservers DNS servers. Value null indicates system DNS Server. Multiple servers will be split by ";", e.g "8.8.8.8;8.8.4.4"
 * @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int) initialize:(TRANSPORT_TYPE)transport
           localIP:(NSString*)localIP
      localSIPPort:(int)localSIPPort
          loglevel:(PORTSIP_LOG_LEVEL)logLevel
           logPath:(NSString*)logFilePath
           maxLine:(int)maxCallLines
             agent:(NSString*)sipAgent
  audioDeviceLayer:(int)audioDeviceLayer
  videoDeviceLayer:(int)videoDeviceLayer
TLSCertificatesRootPath:(NSString*)TLSCertificatesRootPath
     TLSCipherList:(NSString*)TLSCipherList
verifyTLSCertificate:(BOOL)verifyTLSCertificate
        dnsServers:(NSString*)dnsServers;

/*!
 * @brief Set the instance Id, the outbound instanceId((RFC5626) ) used in contact headers.
 *
 * @param instanceId
 *						The SIP instance ID. If this function is not called, the SDK will generate an instance ID automatically.
 *            The instance ID MUST be unique on the same device (device ID or IMEI ID is recommended).
 *            Recommend to call this function to set the ID on Android devices.
 * @return If the function succeeds, it will return value 0. If the function
 *         fails, it will return a specific error code.
 */
- (int)setInstanceId:(NSString*)instanceId;

/*!
 *  @brief Un-initialize the SDK and release resources.
 */
- (void) unInitialize;

/*!
 *  @brief Set user account info.
 *
 *  @param userName           Account (username) of the SIP. It's usually provided by an IP-Telephony service provider.
 *  @param displayName        The display name of user. You can set it as your like, such as "James Kend". It's optional.
 *  @param authName           Authorization user name (usually equal to the username).
 *  @param password           The password of user. It's optional.
 *  @param userDomain         User domain. This parameter is optional. It allows to pass an empty string if you are not using domain.
 *  @param sipServer          SIP proxy server IP or domain. For example: xx.xxx.xx.x or sip.xxx.com.
 *  @param sipServerPort      Port of the SIP proxy server. For example: 5060.
 *  @param stunServer         Stun server, used for NAT traversal. It's optional and can pass an empty string to disable STUN.
 *  @param stunServerPort     STUN server port. It will be ignored if the outboundServer is empty.
 *  @param outboundServer     Outbound proxy server. For example: sip.domain.com. It's optional and allows to pass an empty string if not using outbound server.
 *  @param outboundServerPort Outbound proxy server port. It will be ignored if the outboundServer is empty.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int) setUser:(NSString*)userName
    displayName:(NSString*)displayName
       authName:(NSString*)authName
       password:(NSString*)password
     userDomain:(NSString*)userDomain
      SIPServer:(NSString*)sipServer
  SIPServerPort:(int)sipServerPort
     STUNServer:(NSString*)stunServer
 STUNServerPort:(int)stunServerPort
 outboundServer:(NSString*)outboundServer
outboundServerPort:(int)outboundServerPort;

/*!
 *  @brief Remove user account info.
 *
 */
- (void)removeUser;

/*!
*  @brief Set the display name of user.
*
*  @param displayName that will appear in the From/To Header.
*
*  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
*/
- (int)setDisplayName:(NSString*)displayName;

/*!
 *  @brief Register to SIP proxy server (login to server)
 *
 *  @param expires Registration refreshment interval in seconds. Maximum of 3600 allowed. It will be inserted into SIP REGISTER message headers.
 *  @param retryTimes The retry times if failed to refresh the registration. Once set to <= 0, the retry will be disabled and onRegisterFailure callback triggered for retry failure.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  If registration to server succeeds, onRegisterSuccess will be triggered, otherwise onRegisterFailure triggered.
 */
- (int)registerServer:(int)expires
           retryTimes:(int)retryTimes;


/*!
 *  @brief Refresh the registration manually after successfully registered.
 *
 *  @param expires Registration refreshment interval in seconds. Maximum of 3600 supported. It will be inserted into SIP REGISTER message header. If it's set to 0, default value will be used.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  If registration to server succeeds, onRegisterSuccess will be triggered, otherwise onRegisterFailure triggered.
 */
- (int)refreshRegistration:(int)expires;


/*!
 *  @brief Un-register from the SIP proxy server.
 *
 *  @param waitMS Wait for the server to reply that the un-registration is successful, waitMS is the longest waiting milliseconds, 0 means not waiting.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)unRegisterServer:(int)waitMS;

/*!
 *  @brief Set the license key. It must be called before setUser function.
 *
 *  @param key The SDK license key. Please purchase from PortSIP.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setLicenseKey:(NSString*)key;

/** @} */ // end of group1

/** @defgroup group2 NIC and local IP functions
 * @{
 */

/*!
 *  @brief Get the Network Interface Card numbers.
 *
 *  @return If the function succeeds, it will return NIC numbers, which are greater than or equal to 0. If the function fails, it will return a specific error code.
 */
- (int)getNICNums;

/*!
 *  @brief Get the local IP address by Network Interface Card index.
 *
 *  @param index The IP address index. For example, suppose the PC has two NICs. If we want to obtain the second NIC IP, please set this parameter as 1, and the first NIC IP index 0.
 *
 *  @return The buffer for receiving the IP.
 */
- (NSString*)getLocalIpAddress:(int)index;

/** @} */ // end of group2

/** @defgroup group3 Audio and video codecs functions
 * @{
 */

/*!
 *  @brief Enable an audio codec. It will appear in SDP.
 *
 *  @param codecType Audio codec type.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)addAudioCodec:(AUDIOCODEC_TYPE)codecType;

/*!
 *  @brief Enable a video codec. It will appear in SDP.
 *
 *  @param codecType Video codec type.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)addVideoCodec:(VIDEOCODEC_TYPE)codecType;

/*!
 *  @brief Detect if the enabled audio codecs is empty.
 *
 *  @return If no audio codec is enabled, it will return value true, otherwise false.
 */
- (BOOL)isAudioCodecEmpty;

/*!
 *  @brief Detect if enabled video codecs is empty or not.
 *
 *  @return If no video codec is enabled, it will return value true, otherwise false.
 */
- (BOOL)isVideoCodecEmpty;

/*!
 *  @brief Set the RTP payload type for dynamic audio codec.
 *
 *  @param codecType   Audio codec type defined in the PortSIPTypes file.
 *  @param payloadType The new RTP payload type that you want to set.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setAudioCodecPayloadType:(AUDIOCODEC_TYPE)codecType
                    payloadType:(int) payloadType;

/*!
 *  @brief Set the RTP payload type for dynamic Video codec.
 *
 *  @param codecType   Video codec type defined in the PortSIPTypes file.
 *  @param payloadType The new RTP payload type that you want to set.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoCodecPayloadType:(VIDEOCODEC_TYPE)codecType
                    payloadType:(int) payloadType;

/*!
 *  @brief Remove all enabled audio codecs.
 */
- (void)clearAudioCodec;

/*!
 *  @brief Remove all enabled video codecs.
 */
- (void)clearVideoCodec;

/*!
 *  @brief Set the codec parameter for audio codec.
 *
 *  @param codecType Audio codec type defined in the PortSIPTypes file.
 *  @param parameter The parameter in string format.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 @remark Example:
 @code [myVoIPsdk setAudioCodecParameter:AUDIOCODEC_AMR parameter:"mode-set=0; octet-align=1; robust-sorting=0"]; @endcode
 */
- (int)setAudioCodecParameter:(AUDIOCODEC_TYPE)codecType
                     parameter:(NSString*)parameter;

/*!
 *  @brief Set the codec parameter for video codec.
 *
 *  @param codecType Video codec type defined in the PortSIPTypes file.
 *  @param parameter The parameter in string format.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return value a specific error code.
 @remark Example:
 @code [myVoIPsdk setVideoCodecParameter:VIDEOCODEC_H264 parameter:"profile-level-id=420033; packetization-mode=0"]; @endcode
 */
- (int)setVideoCodecParameter:(VIDEOCODEC_TYPE) codecType
                     parameter:(NSString*)parameter;

/** @} */ // end of group3

/** @defgroup group4 Additional settings functions
 * @{
 */

/*!
 *  @brief Get the current version number of the SDK.
 *
 *  @return Return a current version number MAJOR.MINOR.PATCH of the SDK.
 */
- (NSString*)getVersion;

/*!
 *  @brief Enable/disable rport(RFC3581).
 *
 *  @param enable Set to true to enable the SDK to support rport. By default it is enabled.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enableRport:(BOOL)enable;

/*!
 *  @brief Enable/disable Early Media.
 *
 *  @param enable Set to true to enable the SDK to support Early Media. By default the Early Media is disabled.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enableEarlyMedia:(BOOL)enable;

/*!
 *  @brief Enable/disable PRACK.
 *
 *  @param mode Modes work as follows:
 *    0 - Never,                        Disable PRACK,By default the PRACK is disabled.
 *    1 - SupportedEssential,  Only send reliable provisionals if sending a body and far end supports.
 *    2 - Supported,                 Always send reliable provisionals if far end supports.
 *    3 - Required                    Always send reliable provisionals.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
-(int)setReliableProvisional:(int)mode;

/*!
 *  @brief Enable/disable the 3Gpp tags, including "ims.icsi.mmtel" and "g.3gpp.smsip".
 *
 *  @param enable Set to true to enable the SDK to support 3Gpp tags.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enable3GppTags:(BOOL)enable;

/*!
 *  @brief Enable/disable to callback the SIP messages.
 *
 *  @param enableSending Set as true to enable to callback the sent SIP messages, or false to disable. Once enabled, the "onSendingSignaling" event will be triggered when the SDK sends a SIP message.
 *  @param enableReceived Set as true to enable to callback the received SIP messages, or false to disable. Once enabled, the "onReceivedSignaling" event will be triggered when the SDK receives a SIP message.
 */
- (void)enableCallbackSignaling:(BOOL)enableSending
                 enableReceived:(BOOL)enableReceived;

/*!
 *  @brief Set the SRTP policy.
 *
 *  @param srtpPolicy The SRTP policy.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setSrtpPolicy:(SRTP_POLICY)srtpPolicy;

/*!
 *  @brief Set the RTP ports range for audio and video streaming.
 *
 *  @param minimumRtpPort The minimum RTP port.
 *  @param maximumRtpPort The maximum RTP port.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *  The port range ((max - min) / maxCallLines) should be greater than 4.
 */
- (int)setRtpPortRange:(int) minimumRtpPort
        maximumRtpPort:(int) maximumRtpPort;

/*!
 *  @brief Enable call forwarding.
 *
 *  @param forBusyOnly If this parameter is set as true, the SDK will forward all incoming calls when currently it's busy. If it's set as false, the SDK forward all incoming calls anyway.
 *  @param forwardTo   The target of call forwarding. It must in the format of sip:xxxx@sip.portsip.com.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enableCallForward:(BOOL)forBusyOnly forwardTo:(NSString*) forwardTo;

/*!
 *  @brief Disable the call forwarding. The SDK is not forwarding any incoming calls once this function is called.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)disableCallForward;

/*!
 *  @brief Allows to periodically refresh Session Initiation Protocol (SIP) sessions by sending INVITE requests repeatedly.
 *
 *  @param timerSeconds The value of the refreshment interval in seconds. Minimum of 90 seconds required.
 *  @param refreshMode  Allow to set the session refreshment by UAC or UAS: SESSION_REFERESH_UAC or SESSION_REFERESH_UAS;
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark The INVITE requests, or re-INVITEs, are sent repeatedly during an active call log to allow user agents (UA) or proxies to determine the status of a SIP session.
 *  Without this keep-alive mechanism, proxies for remembering incoming and outgoing requests (stateful proxies) may continue to retain call state needlessly.
 *  If a UA fails to send a BYE message at the end of a session or if the BYE message is lost because of network problems, a stateful proxy does not know that the session has ended.
 *  The re-INVITEs ensure that active sessions stay active and completed sessions are terminated.
 */
- (int)enableSessionTimer:(int) timerSeconds refreshMode:(SESSION_REFRESH_MODE)refreshMode;

/*!
 *  @brief Disable the session timer.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)disableSessionTimer;

/*!
 *  @brief Enable the "Do not disturb" to enable/disable.
 *
 *  @param state If it is set to true, the SDK will reject all incoming calls anyway.
 */
- (void)setDoNotDisturb:(BOOL)state;

/*!
 *  @brief Enable the CheckMwi to enable/disable.
 *
 *  @param state If it is set to true, the SDK will check Mwi automatically.
 */
- (void)enableAutoCheckMwi:(BOOL)state;


/*!
 *  @brief Enable or disable to send RTP keep-alive packet when the call is established.
 *
 *  @param state                Set as true to allow to send the keep-alive packet during the conversation.
 *  @param keepAlivePayloadType The payload type of the keep-alive RTP packet. It's usually set to 126.
 *  @param deltaTransmitTimeMS  The keep-alive RTP packet sending interval, in milliseconds. Recommended value ranges 15000 - 300000.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setRtpKeepAlive:(BOOL)state
  keepAlivePayloadType:(int)keepAlivePayloadType
   deltaTransmitTimeMS:(int)deltaTransmitTimeMS;

/*!
 *  @brief Enable or disable to send SIP keep-alive packet.
 *
 *  @param keepAliveTime This is the SIP keep-alive time interval in seconds. By setting to 0, the SIP keep-alive will be disabled. Recommended value is 30 or 50.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setKeepAliveTime:(int) keepAliveTime;
/*!
 *  @brief Set the audio capturing sample.
 *
 *  @param ptime    It should be a multiple of 10 between 10 - 60 (with 10 and 60 inclusive).
 *  @param maxPtime For the "maxptime" attribute, it should be a multiple of 10 between 10 - 60 (with 10 and 60 inclusive). It cannot be less than "ptime".
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark It will appear in the SDP of INVITE and 200 OK message as "ptime and "maxptime" attribute.
 */
- (int)setAudioSamples:(int) ptime
              maxPtime:(int) maxPtime;

/*!
 *  @brief Set the SDK to receive the SIP message that includes special mime type.
 *
 *  @param methodName  Method name of the SIP message, such as INVITE, OPTION, INFO, MESSAGE, UPDATE, ACK etc. For more details please refer to the RFC3261.
 *  @param mimeType    The mime type of SIP message.
 *  @param subMimeType The sub mime type of SIP message.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *@remarks
 * By default, PortSIP VoIP SDK supports the media types (mime types) listed in the below incoming SIP messages:
 * @code
 "message/sipfrag" in NOTIFY message.
 "application/simple-message-summary" in NOTIFY message.
 "text/plain" in MESSAGE message.
 "application/dtmf-relay" in INFO message.
 "application/media_control+xml" in INFO message.
 * @endcode
 * The SDK allows to receive SIP messages that include above mime types. Now if remote side sends an INFO
 * SIP message with its "Content-Type" header value "text/plain", SDK will reject this INFO message,
 * for "text/plain" of INFO message is not included in the default supported list.
 * How should we enable the SDK to receive the SIP INFO messages that include "text/plain" mime type? The answer is to use
 * addSupportedMimyType:
 * @code
[myVoIPSdk addSupportedMimeType:@"INFO" mimeType:@"text" subMimeType:@"plain"];
 * @endcode
 * To receive the NOTIFY message with "application/media_control+xml":
 *@code
 [myVoIPSdk addSupportedMimeType:@"NOTIFY" mimeType:@"application" subMimeType:@"media_control+xml"];
 * @endcode
 * For more details about the mime type, please visit: http://www.iana.org/assignments/media-types/
 */
- (int)addSupportedMimeType:(NSString*) methodName
                   mimeType:(NSString*) mimeType
                subMimeType:(NSString*) subMimeType;

/** @} */ // end of group4

/** @defgroup group5 Access SIP message header functions
 * @{
 */

/*!
 *  @brief Access the SIP header of SIP message.
 *
 *  @param sipMessage        The SIP message.
 *  @param headerName        The header with which to access the SIP message.
 *
 *  @return If the function succeeds, it will return value headerValue. If the function fails, it will return value null.
 * @remark
 * When receiving an SIP message in the onReceivedSignaling callback event, and user wishes to get SIP message header value, use getExtensionHeaderValue:
 * @code
 NSString* headerValue = [myVoIPSdk getSipMessageHeaderValue:message headerName:name];
 * @endcode
 */
-(NSString*)getSipMessageHeaderValue:(NSString*)sipMessage
                    headerName:(NSString*) headerName;

/*!
 *  @brief Add the SIP Message header into the specified outgoing SIP message.
 *
 *  @param sessionId Add the header to the SIP message with the specified session Id only.
 *                    By setting to -1, it will be added to all messages.
 *  @param methodName Just add the header to the SIP message with specified method name.
 *                    For example: "INVITE", "REGISTER", "INFO" etc. If "ALL" specified, it will add all SIP messages.
 *  @param msgType 1 refers to apply to the request message,
 *                 2 refers to apply to the response message,
 *                 3 refers to apply to both request and response.
 *  @param headerName  The header name that will appear in SIP message.
 *  @param headerValue The custom header value.
 *
 *  @return If the function succeeds, it will return addedSipMessageId, which is greater than 0. If the function fails, it will return a specific error code.
 */
- (long)addSipMessageHeader:(long) sessionId
                methodName:(NSString*) methodName
                   msgType:(int) msgType
                headerName:(NSString*) headerName
               headerValue:(NSString*) headerValue;

/*!
 *  @brief Remove the headers (custom header) added by addSipMessageHeader.
 *
 *  @param addedSipMessageId The addedSipMessageId return by addSipMessageHeader.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)removeAddedSipMessageHeader:(long) addedSipMessageId;


/*!
 *  @brief Clear the added extension headers (custom headers)
 *
 @remark For example, we have added two custom headers into every outgoing SIP message and wish to remove them.
 @code
 [myVoIPSdk addedSipMessageId:-1  methodName:@"ALL"  msgType:3 headerName:@"Blling" headerValue:@"usd100.00"];
 [myVoIPSdk addedSipMessageId:-1  methodName:@"ALL"  msgType:3 headerName:@"ServiceId" headerValue:@"8873456"];
 [myVoIPSdk clearAddedSipMessageHeaders];
 @endcode
 */
- (void)clearAddedSipMessageHeaders;

/*!
 *  @brief Modify the special SIP header value for every outgoing SIP message.
 *
 *  @param sessionId The header to the SIP message with the specified session Id.
 *                    By setting to -1, it will be added to all messages.
 *  @param methodName Modify the header to the SIP message with specified method name only.
 *                    For example: "INVITE", "REGISTER", "INFO" etc. If "ALL" specified, it will add all SIP messages.
 *  @param msgType 1 refers to apply to the request message,
 *                 2 refers to apply to the response message,
 *                 3 refers to apply to both request and response.
 *  @param headerName  The SIP header name of which the value will be modified.
 *  @param headerValue The heaver value to be modified.
 *
 *  @return If the function succeeds, it will return modifiedSipMessageId, which is greater than 0. If the function fails, it will return a specific error code.
 */
- (long)modifySipMessageHeader:(long) sessionId
                methodName:(NSString*) methodName
                   msgType:(int) msgType
                headerName:(NSString*) headerName
               headerValue:(NSString*) headerValue;

/*!
 *  @brief Remove the extension header (custom header) from every outgoing SIP message.
 *
 *  @param modifiedSipMessageId The modifiedSipMessageId is returned by modifySipMessageHeader.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)removeModifiedSipMessageHeader:(long) modifiedSipMessageId;


/*!
 *  @brief Clear the modified headers value, and do not modify every outgoing SIP message header values any longer.
 *
 @remark  For example, to modify two headers' value for every outging SIP message and wish to clear it:
 @code
 [myVoIPSdk removeModifiedSipMessageHeader:-1  methodName:@"ALL"  msgType:3 headerName:@"Expires" headerValue:@"1000"];
 [myVoIPSdk removeModifiedSipMessageHeader:-1  methodName:@"ALL"  msgType:3 headerName:@"User-Agent" headerValue:@"MyTest Softphone 1.0"];
 [myVoIPSdk clearModifiedSipMessageHeaders];
 @endcode
 */
- (void)clearModifiedSipMessageHeaders;

/** @} */ // end of group5

/** @defgroup group6 Audio and video functions
 * @{
 */

/*!
 *  @brief Set the video device that will be used for video call.
 *
 *  @param deviceId Device ID (index) for video device (camera).
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoDeviceId:(int) deviceId;

/*!
 *  @brief Set the video Device Orientation.
 *
 *  @param rotation Device Orientation for video device (camera), e.g 0,90,180,270.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoOrientation:(int) rotation;

/*!
 *  @brief Set the video capturing resolution.
 *
 *  @param width Video width.
 *  @param height Video height.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoResolution:(int)width
                   height:(int)height;

/*!
 *  @brief Set the audio bit rate.
 *
 *  @param sessionId The session ID of the call.
 *  @param codecType Audio codec type.
 *  @param bitrateKbps The Audio bit rate in KBPS.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setAudioBitrate:(long) sessionId
             codecType:(AUDIOCODEC_TYPE)codecType
           bitrateKbps:(int)bitrateKbps;

/*!
 *  @brief Set the video bitrate.
 *
 *  @param sessionId The session ID of the call. Set it to -1 for all calls.
 *  @param bitrateKbps The video bit rate in KBPS.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoBitrate:(long) sessionId bitrateKbps:(int) bitrateKbps;

/*!
 *  @brief Set the video frame rate.
 *
 *  @param sessionId The session ID of the call. Set it to -1 for all calls.
 *  @param frameRate The frame rate value, with its minimum value 5, and maximum value 30. Greater value renders better video quality but requires more bandwidth.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark Usually you do not need to call this function to set the frame rate, as the SDK uses default frame rate.
 */
- (int)setVideoFrameRate:(long) sessionId frameRate:(int) frameRate;
/*!
 *  @brief Send the video to remote side.
 *
 *  @param sessionId The session ID of the call.
 *  @param sendState Set to true to send the video, or false to stop sending.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)sendVideo:(long)sessionId
       sendState:(BOOL)sendState;

/*!
 *  @brief Set the window for a session to display the received remote video image.
 *
 *  @param sessionId         The session ID of the call.
 *  @param remoteVideoWindow The PortSIPVideoRenderView for displaying received remote video image.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setRemoteVideoWindow:(long) sessionId
          remoteVideoWindow:(PortSIPVideoRenderView*) remoteVideoWindow;

/*!
 *  @brief Set the window for a session to display the received remote screen image.
 *
 *  @param sessionId         The session ID of the call.
 *  @param remoteScreenWindow The PortSIPVideoRenderView for displaying received remote screen image.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setRemoteScreenWindow:(long) sessionId
          remoteScreenWindow:(PortSIPVideoRenderView*) remoteScreenWindow;

/*!
 *  @brief Start/stop displaying the local video image.
 *
 *  @param state Set to true to display local video image.
 *  @param mirror Set to true to display the mirror image of local video.
 *  @param localVideoWindow The PortSIPVideoRenderView for displaying local video image from camera.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)displayLocalVideo:(BOOL) state
                  mirror:(BOOL)mirror
        localVideoWindow:(PortSIPVideoRenderView*)localVideoWindow;

/*!
 *  @brief Enable/disable the NACK feature (RFC4585) to help to improve the video quality.
 *
 *  @param state Set to true to enable.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoNackStatus:(BOOL) state;

/*!
 *  @brief Mute the device microphone. It's unavailable for Android and iOS.
 *
 *  @param mute If the value is set to true, the microphone is muted, or set to false to be un-muted.
 */
- (void)muteMicrophone:(BOOL) mute;

/*!
 *  @brief Mute the device speaker. It's unavailable for Android and iOS.
 *
 *  @param mute If the value is set to true, the speaker is muted, or set to false to be un-muted.
 */
- (void)muteSpeaker:(BOOL) mute;

/*!
 *  @brief Set the audio device that will be used for audio call. 
 *
 *  @param enable By setting to true the SDK uses loudspeaker for audio call. This is available for mobile platform only.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark Allow to switch between earphone and loudspeaker only.
 */
- (int)setLoudspeakerStatus:(BOOL)enable;

/**
 * Set a volume |scaling| to be applied to the outgoing signal of a specific
 * audio channel. 
 * 
 * @param sessionId
 *            The session ID of the call.
 * @param scaling
 *            Valid scale ranges [0, 1000]. Default is 100.
 * @return If the function succeeds, it will return value 0. If the function
 *         fails, it will return a specific error code.
 */
- (int)setChannelOutputVolumeScaling:(long) sessionId
                              scaling:(int) scaling;

/**
 * Set a volume |scaling| to be applied to the microphone signal of a specific
 * audio channel.
 *
 * @param sessionId
 *            The session ID of the call.
 * @param scaling
 *            Valid scale ranges [0, 1000]. Default is 100.
 * @return If the function succeeds, it will return value 0. If the function
 *         fails, it will return a specific error code.
 */
- (int)setChannelInputVolumeScaling:(long) sessionId
                             scaling:(int) scaling;
                              	
/** @} */ // end of group6

/** @defgroup group7 Call functions
 * @{
 */

/*!
 *  @brief Make a call
 *
 *  @param callee    The callee. It can be a name only or full SIP URI. For example, user001, sip:user001@sip.iptel.org or sip:user002@sip.yourdomain.com:5068.
 *  @param sendSdp   If set to false, the outgoing call will not include the SDP in INVITE message.
 *  @param videoCall If set to true and at least one video codec was added, the outgoing call will include the video codec into SDP.
 *
 *  @return If the function succeeds, it will return the session ID of the call, which is greater than 0. If the function fails, it will return a specific error code. Note: the function success just means the outgoing call is being processed, and you need to detect the final state of calling in onInviteTrying, onInviteRinging, onInviteFailure callback events.
 */
- (long)call:(NSString*) callee
     sendSdp:(BOOL)sendSdp
   videoCall:(BOOL)videoCall;

/*!
 *  @brief rejectCall Reject the incoming call.
 *
 *  @param sessionId The session ID of the call.
 *  @param code      Reject code. For example, 486 and 480.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)rejectCall:(long)sessionId code:(int)code;

/*!
 *  @brief hangUp Hang up the call.
 *
 *  @param sessionId Session ID of the call.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)hangUp:(long)sessionId;

/*!
 *  @brief answerCall Answer the incoming call.
 *
 *  @param sessionId The session ID of the call.
 *  @param videoCall If the incoming call is a video call and the video codec is matched, set it to true to answer the video call.<br>If set to false, the answer call will not include video codec answer anyway.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)answerCall:(long)sessionId videoCall:(BOOL)videoCall;

/*!
 *  @brief Use the re-INVITE to update the established call.
 *  @param sessionId   The session ID of call.
 *  @param enableAudio Set to true to allow the audio in updated call, or false to disable audio in updated call.
 *  @param enableVideo Set to true to allow the video in updated call, or false to disable video in updated call.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 @remark
 Example usage:<br>

 Example 1: A called B with the audio only, and B answered A, then there would be an audio conversation between A and B. Now if A wants to see B visually,
 A could use these functions to fulfill it.
 @code
 [myVoIPSdk clearVideoCodec];
 [myVoIPSdk addVideoCodec:VIDEOCODEC_H264];
 [myVoIPSdk updateCall:sessionId enableAudio:true enableVideo:true];
 @endcode
 Example 2: Remove video stream from current conversation.
 @code
 [myVoIPSdk updateCall:sessionId enableAudio:true enableVideo:false];
 @endcode
 */
- (int)updateCall:(long)sessionId
      enableAudio:(BOOL)enableAudio
      enableVideo:(BOOL)enableVideo;

/*!
 *  @brief Place a call on hold.
 *
 *  @param sessionId The session ID of the call.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)hold:(long)sessionId;

/*!
 *  @brief Take off hold.
 *
 *  @param sessionId The session ID of call.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)unHold:(long)sessionId;

/*!
 *  @brief Mute the specified session audio or video.
 *
 *  @param sessionId         The session ID of the call.
 *  @param muteIncomingAudio Set it true to mute incoming audio stream, and user cannot hear from remote side audio.
 *  @param muteOutgoingAudio Set it true to mute outgoing audio stream, and the remote side cannot hear the audio.
 *  @param muteIncomingVideo Set it true to mute incoming video stream, and user cannot see remote side video.
 *  @param muteOutgoingVideo Set it true to mute outgoing video stream, and the remote side cannot see video.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)muteSession:(long)sessionId
 muteIncomingAudio:(BOOL)muteIncomingAudio
 muteOutgoingAudio:(BOOL)muteOutgoingAudio
 muteIncomingVideo:(BOOL)muteIncomingVideo
 muteOutgoingVideo:(BOOL)muteOutgoingVideo;

/*!
 *  @brief Forward the call to another user once received an incoming call.
 *
 *  @param sessionId The session ID of the call.
 *  @param forwardTo Target of the call forwarding. It can be "sip:number@sipserver.com" or "number" only.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)forwardCall:(long)sessionId forwardTo:(NSString*) forwardTo;

/*!
 *  @brief This function will be used for picking up a call based on the BLF (Busy Lamp Field) status.
 *
 *  @param replaceDialogId The ID of the call to be picked up. It comes with onDialogStateUpdated callback.
 *  @param videoCall Indicates if it is video call or audio call to be picked up.
 *
 *  @return If the function succeeds, it will return a session ID that is greater than 0 to the new call, otherwise returns a specific error code that is less than 0.
 *  @remark
 * The scenario is:<br>
 *   1. User 101 subscribed the user 100's call status: sendSubscription("100", "dialog");
 *   2. When 100 hold a call or 100 is ringing, onDialogStateUpdated callback will be triggered,
 *   and 101 will receive this callback. Now 101 can use pickupBLFCall function to pick the call rather than
 *   100 to talk with caller.
 */
- (long)pickupBLFCall:(const char * )replaceDialogId videoCall:(BOOL)videoCall;
/*!
 *  @brief Send DTMF tone.
 *
 *  @param sessionId    The session ID of the call.
 *  @param dtmfMethod   Support sending DTMF tone with two methods: DTMF_RFC2833 and DTMF_INFO. The DTMF_RFC2833 is recommended.
 *  @param code         The DTMF tone (0-16).
 * <p><table>
 * <tr><th>code</th><th>Description</th></tr>
 * <tr><td>0</td><td>The DTMF tone 0.</td></tr><tr><td>1</td><td>The DTMF tone 1.</td></tr><tr><td>2</td><td>The DTMF tone 2.</td></tr>
 * <tr><td>3</td><td>The DTMF tone 3.</td></tr><tr><td>4</td><td>The DTMF tone 4.</td></tr><tr><td>5</td><td>The DTMF tone 5.</td></tr>
 * <tr><td>6</td><td>The DTMF tone 6.</td></tr><tr><td>7</td><td>The DTMF tone 7.</td></tr><tr><td>8</td><td>The DTMF tone 8.</td></tr>
 * <tr><td>9</td><td>The DTMF tone 9.</td></tr><tr><td>10</td><td>The DTMF tone *.</td></tr><tr><td>11</td><td>The DTMF tone #.</td></tr>
 * <tr><td>12</td><td>The DTMF tone A.</td></tr><tr><td>13</td><td>The DTMF tone B.</td></tr><tr><td>14</td><td>The DTMF tone C.</td></tr>
 * <tr><td>15</td><td>The DTMF tone D.</td></tr><tr><td>16</td><td>The DTMF tone FLASH.</td></tr>
 * </table></p>
 *  @param dtmfDuration The DTMF tone samples. Recommended value 160.
 *  @param playDtmfTone By setting to true, the SDK plays local DTMF tone sound when sending DTMF.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)sendDtmf:(long) sessionId
     dtmfMethod:(DTMF_METHOD) dtmfMethod
           code:(int) code
    dtmfDration:(int) dtmfDuration
   playDtmfTone:(BOOL) playDtmfTone;

/** @} */ // end of group7

/** @defgroup group8 Refer functions
 * @{
 */

/*!
 *  @brief Refer the current call to another one.<br>
 *  @param sessionId The session ID of the call.
 *  @param referTo   Target of the refer. It could be either "sip:number@sipserver.com" or "number".
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 * @code
  	[myVoIPSdk refer:sessionId  referTo:@"sip:testuser12@sip.portsip.com"];
 * @endcode
 * You can watch the video on YouTube at https://www.youtube.com/watch?v=_2w9EGgr3FY. It will demonstrate the transfer.
 */
- (int)refer:(long)sessionId referTo:(NSString*)referTo;

/*!
 *  @brief  Make an attended refer.
 *
 *  @param sessionId        The session ID of the call.
 *  @param replaceSessionId Session ID of the replaced call.
 *  @param referTo          Target of the refer. It can be either "sip:number@sipserver.com" or "number".
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *  Please read the sample project source code for more details, or you can watch the video on YouTube at https://www.youtube.com/watch?v=NezhIZW4lV4,
 *  which will demonstrate the transfer.
 */
- (int)attendedRefer:(long)sessionId
    replaceSessionId:(long)replaceSessionId
             referTo:(NSString*)  referTo;

/*!
 *  @brief  Make an attended refer with specified request line and specified method embedded into the "Refer-To" header.
 *
 *  @param sessionId        Session ID of the call.
 *  @param replaceSessionId Session ID of the replaced call.
 *  @param replaceMethod    The SIP method name which will be embedded in the "Refer-To" header, usually INVITE or BYE.
 *  @param target           The target to which the REFER message will be sent. It appears in the "Request Line" of REFER message.
 *  @param referTo          Target of the refer that appears in the "Refer-To" header. It can be either "sip:number@sipserver.com" or "number".
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *  Please read the sample project source code for more details, or you can watch the video on YouTube at https://www.youtube.com/watch?v=NezhIZW4lV4,
 *  which will demonstrate the transfer.
 */
- (int)attendedRefer2:(long)sessionId
     replaceSessionId:(long)replaceSessionId
        replaceMethod:(NSString*)replaceMethod
              target:(NSString*)target
        referTo:(NSString*)referTo;


/*!
 *  @brief  Send an out of dialog REFER to replace the specified call.
 *
 *  @param replaceSessionId The session ID of the session which will be replaced.
 *  @param replaceMethod    The SIP method name which will be added in the "Refer-To" header, usually INVITE or BYE.
 *  @param target           The target to which the REFER message will be sent.
 *  @param referTo          The URI to be added into the "Refer-To" header.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)outOfDialogRefer:(long)replaceSessionId
          replaceMethod:(NSString*)replaceMethod
                 target:(NSString*)target
                referTo:(NSString*)referTo;
/*!
 *  @brief Once the REFER request accepted, a new call will be made if called this function. The function is usually called after onReceivedRefer callback event.
 *
 *  @param referId        The ID of REFER request that comes from onReceivedRefer callback event.
 *  @param referSignaling The SIP message of REFER request that comes from onReceivedRefer callback event.
 *
 *  @return If the function succeeds, it will return a session ID that is greater than 0 to the new call for REFER, otherwise returns a specific error code that is less than 0.
 */
- (long)acceptRefer:(long)referId referSignaling:(NSString*) referSignaling;

/*!
 *  @brief Reject the REFER request.
 *
 *  @param referId The ID of REFER request that comes from onReceivedRefer callback event.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)rejectRefer:(long)referId;

/** @} */ // end of group8

/** @defgroup group9 Send audio and video stream functions
 * @{
 */

/*!
 *  @brief Enable the SDK to send PCM stream data to remote side from another source instead of microphone.
 *
 *  @param sessionId           The session ID of call.
 *  @param state               Set to true to enable the sending stream, or false to disable.
 *  @param streamSamplesPerSec The PCM stream data sample in seconds. For example: 8000 or 16000.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark To send the PCM stream data to another side, this function MUST be called first.
 */
- (int)enableSendPcmStreamToRemote:(long)sessionId state:(BOOL)state streamSamplesPerSec:(int)streamSamplesPerSec;

/*!
 *  @brief Send the audio stream in PCM format from another source instead of audio device capturing (microphone).
 *
 *  @param sessionId Session ID of the call conversation.
 *  @param data      The PCM audio stream data. It must be in 16bit, mono.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark Usually we should use it like below:
 *  @code
 [myVoIPSdk enableSendPcmStreamToRemote:sessionId state:YES streamSamplesPerSec:16000];
 [myVoIPSdk sendPcmStreamToRemote:sessionId data:data];
 *  @endcode
 *  You can't have too much audio data at one time as we have 100ms audio buffer only. Once you put too much, data will be lost.
 *  It is recommended to send 20ms audio data every 20ms.
 */
- (int)sendPcmStreamToRemote:(long)sessionId data:(NSData*) data;

/*!
 *  @brief Enable the SDK to send video stream data to remote side from another source instead of camera.
 *
 *  @param sessionId The session ID of call.
 *  @param state     Set to true to enable the sending stream, or false to disable.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enableSendVideoStreamToRemote:(long)sessionId state:(BOOL)state;

/*!
 *  @brief Send the video stream to remote side
 *
 *  @param sessionId Session ID of the call conversation.
 *  @param data      The video stream data. It must be in i420 format.
 *  @param width     The video image width.
 *  @param height    The video image height.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark  Send the video stream in i420 from another source instead of video device capturing (camera).<br>
 Before calling this function, you MUST call the enableSendVideoStreamToRemote function.<br>
 Usually we should use it like below:
 @code
 [myVoIPSdk enableSendVideoStreamToRemote:sessionId state:YES];
 [myVoIPSdk sendVideoStreamToRemote:sessionId data:data width:352 height:288];
 @endcode
 */
- (int)sendVideoStreamToRemote:(long)sessionId data:(NSData*) data width:(int)width height:(int)height;

/** @} */ // end of group9

/** @defgroup group10 RTP packets, audio stream and video stream callback functions
 * @{
 */

/*!
 *  @brief Set the RTP callbacks to allow to access the sent and received RTP packets.
 *
 *  @param sessionId    The session ID of call.
 *  @param mediaType     0 -audo 1-video 2-screen.
 *  @param mode  The RTP stream callback mode.
 * <p><table>
 * <tr><th>Type</th><th>Description</th></tr>
 * <tr><td>DIRECTION_SEND</td><td>Callback the send RTP stream for one channel based on the given sessionId. </td></tr>
 * <tr><td>DIRECTION_RECV</td><td>Callback the received  RTP stream for one channel based on the given sessionId.</td></tr>
 * <tr><td>DIRECTION_SEND_RECV</td><td>Callback both local and remote RTP stream on the given sessionId. </td></tr>
 * </table></p>
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enableRtpCallback:(long) sessionId
               mediaType:(int)mediaType
                    mode:(DIRECTION_MODE)mode;


/*!
 *  @brief Enable/disable the audio stream callback
 *
 *  @param sessionId    The session ID of call.
 *  @param enable       Set to true to enable audio stream callback, or false to stop the callback.
 *  @param callbackMode The audio stream callback mode.
 * <p><table>
 * <tr><th>Type</th><th>Description</th></tr>
 * <tr><td>DIRECTION_SEND</td><td>Callback the audio stream from microphone for one channel based on the given sessionId. </td></tr>
 * <tr><td>DIRECTION_RECV</td><td>Callback the received audio stream for one channel based on the given sessionId.</td></tr>
 * <tr><td>DIRECTION_SEND_RECV</td><td>Callback both local and remote audio stream on the given sessionId. </td></tr>
 * </table></p>
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark onAudioRawCallback event will be triggered if the callback is enabled.
 */
- (int)enableAudioStreamCallback:(long) sessionId  enable:(BOOL)enable callbackMode:(DIRECTION_MODE)callbackMode;

/*!
 *  @brief Enable/disable the video stream callback.
 *
 *  @param sessionId    The session ID of call.
 *  @param callbackMode The video stream callback mode.
 * <p><table>
 * <tr><th>Mode</th><th>Description</th></tr>
 * <tr><td>DIRECTION_INACTIVE</td><td>Disable video stream callback. </td></tr>
 * <tr><td>DIRECTION_SEND</td><td>Local video stream callback. </td></tr>
 * <tr><td>DIRECTION_RECV</td><td>Remote video stream callback. </td></tr>
 * <tr><td>DIRECTION_SEND_RECV</td><td>Both of local and remote video stream callback. </td></tr>
 * </table></p>
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark The onVideoRawCallback event will be triggered if the callback is enabled.
 */
- (int)enableVideoStreamCallback:(long) sessionId callbackMode:(DIRECTION_MODE) callbackMode;

/** @} */ // end of group10

/** @defgroup group11 Record functions
 * @{
 */

/*!
 *  @brief Start recording the call.
 *
 *  @param sessionId        The session ID of call conversation.
 *  @param recordFilePath   The filepath to which the recording will be saved. It must be existent.
 *  @param recordFileName   The filename of the recording. For example audiorecord.wav or videorecord.avi.
 *  @param appendTimeStamp  Set to true to append the timestamp to the filename of the recording.
 *  @param channels  Set to record file audio channels,  1 - mono 2 - stereo.
 *  @param recordFileFormat  The file format for the recording.
 *  @param audioRecordMode  Allow to set audio recording mode. Support to record received and/or sent video.
 *  @param videoRecordMode  Allow to set video recording mode. Support to record received and/or sent video.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)startRecord:(long)sessionId
    recordFilePath:(NSString*) recordFilePath
    recordFileName:(NSString*) recordFileName
   appendTimeStamp:(BOOL)appendTimeStamp
          channels:(int)channels
        fileFormat:(FILE_FORMAT) recordFileFormat
   audioRecordMode:(RECORD_MODE) audioRecordMode
   videoRecordMode:(RECORD_MODE) videoRecordMode;

/*!
 *  @brief Stop recording.
 *
 *  @param sessionId The session ID of call conversation.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)stopRecord:(long)sessionId;

/** @} */ // end of group11

/** @defgroup group12 Play audio and video files to remote party
 * @{
 */


/*!
 *  @brief Play a file to remote party.
 *
 *  @param sessionId Session ID of the call.
 *  @param fileUrl   url or file name, such as "/test.mp4","/test.wav",.
 *  @param loop      Set to false to stop playing video file when it is ended, or true to play it repeatedly.
 *  @param playAudio If it is set to 0 - Not play file audio. 1 - Play file audio,  2 - Play file audio mix with Mic
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int) startPlayingFileToRemote:(long)sessionId fileUrl:(NSString*) fileUrl loop:(BOOL)loop playAudio:(int)playAudio;

/*!
 *  @brief Stop playing file to remote party.
 *
 *  @param sessionId Session ID of the call.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)stopPlayingFileToRemote:(long)sessionId;


/*!
 *  @brief Play a file to locally
 *
 *  @param fileUrl   url or file name, such as "/test.mp4","/test.wav",.
 *  @param loop      Set to false to stop playing video file when it is ended, or true to play it repeatedly.
 *  @param playVideoWindow  The PortSIPVideoRenderView used for displaying the video.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int) startPlayingFileLocally:(NSString*) fileUrl
                           loop:(BOOL)loop
                playVideoWindow:(PortSIPVideoRenderView*) playVideoWindow;

/*!
 *  @brief Stop playing file to locally.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)stopPlayingFileLocally;


/*!
 *  @brief Used for the loop back testing against audio device.
 *
 *  @param enable Set to true to start audio look back test, or false to stop.
 */
- (void)audioPlayLoopbackTest:(BOOL)enable;

/** @} */ // end of group12

/** @defgroup group13 Conference functions
 * @{
 */

/*!
 *  @brief Create an audio conference.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)createAudioConference;

/*!
 *  @brief Create a video conference.
 *
 *  @param conferenceVideoWindow         The PortSIPVideoRenderView used for displaying the conference video.
 *  @param videoWidth                    The conference video width.
 *  @param videoHeight                   The conference video height.
 *  @param layout Conference Video layout, default is 0 - Adaptive.
 *              0 - Adaptive(1,3,5,6)
 *              1 - Only Local Video
 *              2 - 2 video,PIP
 *              3 - 2 video, Left and right
 *              4 - 2 video, Up and Down
 *              5 - 3 video
 *              6 - 4 split video
 *              7 - 5 video
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)createVideoConference:(PortSIPVideoRenderView*) conferenceVideoWindow
                  videoWidth:(int) videoWidth
                 videoHeight:(int) videoHeight
                      layout:(int) layout;
/*!
 *  @brief Destroy the existent conference.
 */
- (void)destroyConference;

/*!
 *  @brief Set the window for a conference that is used to display the received remote video image.
 *
 *  @param conferenceVideoWindow The PortSIPVideoRenderView used to display the conference video.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setConferenceVideoWindow:(PortSIPVideoRenderView*) conferenceVideoWindow;

/*!
 *  @brief Join a session into existent conference. If the call is in hold, please un-hold first.
 *
 *  @param sessionId Session ID of the call.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)joinToConference:(long)sessionId;

/*!
 *  @brief Remove a session from an existent conference.
 *
 *  @param sessionId Session ID of the call.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)removeFromConference:(long)sessionId;

/** @} */ // end of group13

/** @defgroup group14 RTP and RTCP QOS functions
 * @{
 */

/*!
 *  @brief Set the audio RTCP bandwidth parameters as the RFC3556.
 *
 *  @param sessionId The session ID of call conversation.
 *  @param BitsRR    The bits for the RR parameter.
 *  @param BitsRS    The bits for the RS parameter.
 *  @param KBitsAS   The Kbits for the AS parameter.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setAudioRtcpBandwidth:(long)sessionId
                      BitsRR:(int)BitsRR
                      BitsRS:(int)BitsRS
                     KBitsAS:(int)KBitsAS;

/*!
 *  @brief Set the video RTCP bandwidth parameters as the RFC3556.
 *
 *  @param sessionId The session ID of call conversation.
 *  @param BitsRR    The bits for the RR parameter.
 *  @param BitsRS    The bits for the RS parameter.
 *  @param KBitsAS   The Kbits for the AS parameter.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoRtcpBandwidth:(long)sessionId
                      BitsRR:(int)BitsRR
                      BitsRS:(int)BitsRS
                     KBitsAS:(int)KBitsAS;

/*!
 *  @brief Set the DSCP (differentiated services code point) value of QoS (Quality of Service) for audio channel.
 *
 *  @param state    Set to YES to enable audio QoS, and DSCP value will be 46; or NO to disable audio QoS, and DSCP value will be 0.
*
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enableAudioQos:(BOOL)state;

/*!
 *  @brief Set the DSCP (differentiated services code point) value of QoS (Quality of Service) for video channel.
 *
 *  @param state    Set to YES to enable video QoS and DSCP value will be 34; or NO to disable video QoS, and DSCP value will be 0.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)enableVideoQos:(BOOL)state;

/*!
 *  @brief Set the MTU size for video RTP packet.
 *
 *  @param mtu    Set MTU value. Allowed value ranges 512-65507. Other values will be automatically changed to the default 1400.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setVideoMTU:(int)mtu;

/** @} */ // end of group14

/** @defgroup group15 Media statistics functions
 * @{
 */

/*!
 *  @brief Obtain the statistics of channel. the event onStatistics will be triggered.
 *
 *  @param sessionId          The session ID of call conversation.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)getStatistics:(long)sessionId;


/** @} */ // end of group15

/** @defgroup group16 Audio effect functions
 * @{
 */

/*!
 *  @brief Enable/disable Voice Activity Detection (VAD).
 *
 *  @param state Set to true to enable VAD, or false to disable it.
 */
- (void)enableVAD:(BOOL)state;

/*!
 *  @brief Enable/disable Comfort Noise Generator (CNG).
 *
 *  @param state Set to true to enable CNG, or false to disable.
 */
- (void)enableCNG:(BOOL)state;

/** @} */ // end of group16

/** @defgroup group17 Send OPTIONS/INFO/MESSAGE functions
 * @{
 */

/*!
 *  @brief Send OPTIONS message.
 *
 *  @param to  The recipient of OPTIONS message.
 *  @param sdp The SDP of OPTIONS message. It's optional if user does not wish to send the SDP with OPTIONS message.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)sendOptions:(NSString*)to sdp:(NSString*) sdp;

/*!
 *  @brief Send an INFO message to remote side in dialog.
 *
 *  @param sessionId    The session ID of call.
 *  @param mimeType     The mime type of INFO message.
 *  @param subMimeType  The sub mime type of INFO message.
 *  @param infoContents The contents to be sent with INFO message.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)sendInfo:(long)sessionId
       mimeType:(NSString*) mimeType
    subMimeType:(NSString*) subMimeType
   infoContents:(NSString*) infoContents;

/*!
 *  @brief Send a MESSAGE message to remote side in dialog.
 *
 *  @param sessionId     The session ID of the call.
 *  @param mimeType      The mime type of MESSAGE message.
 *  @param subMimeType   The sub mime type of MESSAGE message.
 *  @param message       The contents to be sent with MESSAGE message. Binary data allowed.
 *  @param messageLength The message size.
 *
 *  @return If the function succeeds, it will return a message ID that allows to track the message sending state in onSendMessageSuccess and onSendMessageFailure. If the function fails, it will return a specific error code less than 0.
 *  @remark
 *  Example 1: Send a plain text message. Note: to send other languages text, please use the UTF-8 to encode the message before sending.
 *  @code
	[myVoIPsdk sendMessage:sessionId mimeType:@"text" subMimeType:@"plain" message:data messageLength:dataLen];
 *  @endcode
 *  Example 2: Send a binary message.
 *  @code
	[myVoIPsdk sendMessage:sessionId mimeType:@"application" subMimeType:@"vnd.3gpp.sms" message:data messageLength:dataLen];
 *  @endcode
 */
- (long)sendMessage:(long)sessionId
           mimeType:(NSString*) mimeType
        subMimeType:(NSString*)subMimeType
            message:(NSData*) message
      messageLength:(int)messageLength;

/*!
 *  @brief Send an out of dialog MESSAGE message to remote side.
 *
 *  @param to            The message recipient, such as sip:receiver@portsip.com.
 *  @param mimeType      The mime type of MESSAGE message.
 *  @param subMimeType   The sub mime type of MESSAGE message.
 *  @isSMS isSMS         Set to YES to specify "messagetype=SMS" in the To line, or NO to disable.
 *  @param message       The contents sent with MESSAGE message. Binary data allowed.
 *  @param messageLength The message size.
 *
 *  @return If the function succeeds, it will return a message ID that allows to track the message sending state in onSendOutOfMessageSuccess and onSendOutOfMessageFailure. If the function fails, it will return a specific error code less than 0.
 * @remark
 * Example 1: Send a plain text message. Note: to send text in other languages, please use UTF-8 to encode the message before sending.
 * @code
		[myVoIPsdk sendOutOfDialogMessage:@"sip:user1@sip.portsip.com" mimeType:@"text" subMimeType:@"plain" message:data messageLength:dataLen];
 * @endcode
 * Example 2: Send a binary message.
 * @code
 [myVoIPsdk sendOutOfDialogMessage:@"sip:user1@sip.portsip.com" mimeType:@"application" subMimeType:@"vnd.3gpp.sms" isSMS:NO message:data messageLength:dataLen];
 * @endcode
 */
- (long)sendOutOfDialogMessage:(NSString*) to
                      mimeType:(NSString*) mimeType
                   subMimeType:(NSString*)subMimeType
                         isSMS:(BOOL)isSMS
                       message:(NSData*) message
                 messageLength:(int)messageLength;

/** @} */ // end of group17

/** @defgroup group18 Presence functions
 * @{
 */

/*!
 *  @brief Indicate that SDK uses the P2P mode for presence or presence agent mode.
 *
 *  @param mode    0 - P2P mode; 1 - Presence Agent mode, default is P2P mode.
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *  Since presence agent mode requires the PBX/Server support the PUBLISH,
 *  please ensure you have your server and PortSIP PBX support this
 *  feature. For more details please visit: https://www.portsip.com/portsip-pbx
 */
- (int)setPresenceMode:(PRESENCE_MODES) mode;

/*!
 *  @brief Set the default expiration time to be used when creating a subscription.
 *
 *  @param secs    The default expiration time of subscription.
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setDefaultSubscriptionTime:(int) secs;

/*!
 *  @brief Set the default expiration time to be used when creating a publication.
 *
 *  @param secs    The default expiration time of publication.
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)setDefaultPublicationTime:(int) secs;


/*!
 *  @brief Send a SUBSCRIBE message for subscribing the contact's presence status.
 *
 *  @param contact The target contact. It must be like sip:contact001@sip.portsip.com.
 *  @param subject This subject text will be inserted into the SUBSCRIBE message. For example: "Hello, I'm Jason".<br>
 The subject maybe in UTF-8 format. You should use UTF-8 to decode it.
 *
 *  @return If the function succeeds, it will return subscribeId. If the function fails, it will return a specific error code.
 */
- (long)presenceSubscribe:(NSString*) contact
                  subject:(NSString*) subject;


/*!
 *  @brief Terminate the given presence subscription.
 *
 *  @param subscribeId    The ID of the subscription.
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int)presenceTerminateSubscribe:(long) subscribeId;

/*!
 *  @brief Accept the presence SUBSCRIBE request which is received from contact.
 *
 *  @param subscribeId Subscription ID. When receiving a SUBSCRIBE request from contact, the event onPresenceRecvSubscribe will be triggered. The event will include the subscription ID.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *  If the P2P presence mode is enabled, when someone subscribes your presence status,
 *  you will receive the subscription request in the callback, and you can use this function to reject it.
 */
- (int)presenceAcceptSubscribe:(long)subscribeId;

/*!
 *  @brief Reject a presence SUBSCRIBE request which is received from contact.
 *
 *  @param subscribeId Subscription ID. When receiving a SUBSCRIBE request from contact, the event onPresenceRecvSubscribe will be triggered. The event includes the subscription ID.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *  If the P2P presence mode is enabled, when someone subscribe your presence status,
 *  you will receive the subscribe request in the callback, and you can use this function to accept it.
 */
- (int)presenceRejectSubscribe:(long)subscribeId;

/*!
 *  @brief Send a NOTIFY message to contact to notify that presence status is online/offline/changed.
 *
 *  @param subscribeId Subscription ID. When receiving a SUBSCRIBE request from contact, the event onPresenceRecvSubscribe that includes the Subscription ID will be triggered.
 *  @param statusText  The state text of presence status. For example: "I'm here", offline must use "offline"
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return value a specific error code.
 */
- (int)setPresenceStatus:(long)subscribeId
              statusText:(NSString*) statusText;


/*!
 *  @brief Send a SUBSCRIBE message to remote side.
 *
 *  @param to    The subscribe user.
 *  @param eventName    The event name to be subscribed.
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *   Example 1, to subscribe the MWI (Message Waiting notifications),
 *   You can use this code: long mwiSubId = sendSubscription("sip:101@test.com", "message-summary");
 *
 *   Example 2, to monitor a user/extension call status,
 *   You can use code: sendSubscription("100", "dialog");
 *   Extension 100 is the one to be monitored. Once being monitored, when extension 100 hold a call or is ringing, the
 *    onDialogStateUpdated callback will be triggered.
 */
- (long)sendSubscription:(NSString*) to
               eventName:(NSString*) eventName;

/*!
 *  @brief Terminate the given subscription.
 *
 *  @param subscribeId    The ID of the subscription.
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 *  @remark
 *  For example, if you want stop check the MWI, use below code:
 @code
 terminateSubscription(mwiSubId);
 @endcode
 */
- (int)terminateSubscription:(long)subscribeId;
/** @} */ // end of group18

/** @defgroup group19 Keep awake functions
 * @{
 */


/*!
 * @brief Keep VoIP awake in the background.
   @discussion If you want your application to be able to receive the incoming call while it's running in background, you should call this function in applicationDidEnterBackground.
 *
 *  @return If the function succeeds, it will return value true. If the function fails, it will return value false. 
 */
- (BOOL) startKeepAwake;//call in applicationDidEnterBackground

/*!
 *  @brief Keep VoIP awake in the background.
    @discussion Call this function in applicationWillEnterForeground once your application comes back to foreground.
 *
 *  @return If the function succeeds, it will return value true. If the function fails, it will return value false.
 */
- (BOOL) stopKeepAwake;//call in applicatEionWillEnterForeground

/** @} */ // end of group19

/** @defgroup group20 Audio Controller
 * @{
 */


/*!
 * @brief Start Audio Device.
 @discussion Call it as AVAudioSessionInterruptionTypeEnded.
 *
 *  @return If the function succeeds, it will return value true. If the function fails, it will return value false.
 */
- (BOOL) startAudio;

/*!
 *  @brief Stop Audio Device.
 @discussion Call it as AVAudioSessionInterruptionTypeBegan.
 *
 *  @return If the function succeeds, it will return value true. If the function fails, it will return value false.
 */
- (BOOL) stopAudio;

/*!
 *  @brief Enable/disable CallKit(Native Integration).
 *  @param state    Set to true to enable CallKit, or false to disable..
 *
 *  @return If the function succeeds, it will return value 0. If the function fails, it will return a specific error code.
 */
- (int) enableCallKit:(BOOL)state;

/** @} */ // end of group20
/** @} */ // end of groupSDK

@end
