/*!
 * @author Copyright (c) 2006-2022 PortSIP Solutions,Inc. All rights reserved.
 * @version 19
 * @see http://www.PortSIP.com
 * @brief PortSIP SDK Callback events Delegate.
 
 PortSIP SDK Callback events Delegate description.
 */
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#endif



@protocol PortSIPEventDelegate <NSObject>
@required
/** @defgroup groupDelegate SDK Callback events
 * SDK Callback events
 * @{
 */
/** @defgroup group21 Register events
 * Register events
 * @{
 */

/*!
 *  When successfully registered to server, this event will be triggered.
 *
 *  @param statusText The status text.
 *  @param statusCode The status code.
 *  @param sipMessage The SIP message received.
 */
- (void)onRegisterSuccess:(NSString*) statusText
               statusCode:(int)statusCode
               sipMessage:(NSString*)sipMessage;

/*!
 *  If registration to SIP server fails, this event will be triggered.
 *
 *  @param statusText The status text.
 *  @param statusCode The status code.
 *  @param sipMessage The SIP message received.
 */
- (void)onRegisterFailure:(NSString*) statusText
               statusCode:(int)statusCode
               sipMessage:(NSString*)sipMessage;

/** @} */ // end of group21

/** @defgroup group22 Call events
 * @{
 */

/*!
 *  When the call is coming, this event will be triggered.
 *
 *  @param sessionId         The session ID of the call.
 *  @param callerDisplayName The display name of caller
 *  @param caller            The caller.
 *  @param calleeDisplayName The display name of callee.
 *  @param callee            The callee.
 *  @param audioCodecs       The matched audio codecs. It's separated by "#" if there are more than one codecs.
 *  @param videoCodecs       The matched video codecs. It's separated by "#" if there are more than one codecs.
 *  @param existsAudio       By setting to true, it indicates that this call includes the audio.
 *  @param existsVideo       By setting to true, it indicates that this call includes the video.
 *  @param sipMessage        The SIP message received.
 */
- (void)onInviteIncoming:(long)sessionId
       callerDisplayName:(NSString*)callerDisplayName
                  caller:(NSString*)caller
       calleeDisplayName:(NSString*)calleeDisplayName
                  callee:(NSString*)callee
             audioCodecs:(NSString*)audioCodecs
             videoCodecs:(NSString*)videoCodecs
             existsAudio:(BOOL)existsAudio
             existsVideo:(BOOL)existsVideo
              sipMessage:(NSString*)sipMessage;

/*!
 *  If the outgoing call is being processed, this event will be triggered.
 *
 *  @param sessionId The session ID of the call.
 */
- (void)onInviteTrying:(long)sessionId;

/*!
 *  Once the caller received the "183 session in progress" message, this event will be triggered.
 *
 *  @param sessionId        The session ID of the call.
 *  @param audioCodecs      The matched audio codecs. It's separated by "#" if there are more than one codecs.
 *  @param videoCodecs      The matched video codecs. It's separated by "#" if there are more than one codecs.
 *  @param existsEarlyMedia By setting to true, it indicates that the call has early media.
 *  @param existsAudio      By setting to true, it indicates that this call includes the audio.
 *  @param existsVideo      By setting to true, it indicates that this call includes the video.
 *  @param sipMessage       The SIP message received.
 */
- (void)onInviteSessionProgress:(long)sessionId
                    audioCodecs:(NSString*)audioCodecs
                    videoCodecs:(NSString*)videoCodecs
               existsEarlyMedia:(BOOL)existsEarlyMedia
                    existsAudio:(BOOL)existsAudio
                    existsVideo:(BOOL)existsVideo
                     sipMessage:(NSString*)sipMessage;

/*!
 *  If the outgoing call is ringing, this event will be triggered.
 *
 *  @param sessionId  The session ID of the call.
 *  @param statusText The status text.
 *  @param statusCode The status code.
 *  @param sipMessage The SIP message received.
 */
- (void)onInviteRinging:(long)sessionId
             statusText:(NSString*)statusText
             statusCode:(int)statusCode
             sipMessage:(NSString*)sipMessage;

/*!
 *  If the remote party answered the call, this event would be triggered.
 *
 *  @param sessionId         The session ID of the call.
 *  @param callerDisplayName The display name of caller
 *  @param caller            The caller.
 *  @param calleeDisplayName The display name of callee.
 *  @param callee            The callee.
 *  @param audioCodecs       The matched audio codecs. It's separated by "#" if there are more than one codecs.
 *  @param videoCodecs       The matched video codecs. It's separated by "#" if there are more than one codecs.
 *  @param existsAudio       By setting to true, it indicates that this call includes the audio.
 *  @param existsVideo       By setting to true, it indicates that this call includes the video.
 *  @param sipMessage        The SIP message received.
 */
- (void)onInviteAnswered:(long)sessionId
       callerDisplayName:(NSString*)callerDisplayName
                  caller:(NSString*)caller
       calleeDisplayName:(NSString*)calleeDisplayName
                  callee:(NSString*)callee
             audioCodecs:(NSString*)audioCodecs
             videoCodecs:(NSString*)videoCodecs
             existsAudio:(BOOL)existsAudio
             existsVideo:(BOOL)existsVideo
              sipMessage:(NSString*)sipMessage;

/*!
 *  If the outgoing/incoming call fails, this event will be triggered.
 *
 *  @param sessionId The session ID of the call.
 *  @param callerDisplayName The display name of caller
 *  @param caller            The caller.
 *  @param calleeDisplayName The display name of callee.
 *  @param callee            The callee.
 *  @param reason    The failure reason.
 *  @param code      The failure code.
 *  @param sipMessage        The SIP message received.
 */
- (void)onInviteFailure:(long)sessionId
      callerDisplayName:(NSString*)callerDisplayName
                 caller:(NSString*)caller
      calleeDisplayName:(NSString*)calleeDisplayName
                 callee:(NSString*)callee
                 reason:(NSString*)reason
                   code:(int)code
             sipMessage:(NSString*)sipMessage;

/*!
 *  This event will be triggered when remote party updates this call.
 *
 *  @param sessionId   The session ID of the call.
 *  @param audioCodecs The matched audio codecs. It's separated by "#" if there are more than one codecs.
 *  @param videoCodecs The matched video codecs. It's separated by "#" if there are more than one codecs.
 *  @param screenCodecs The matched screen codecs. It's separated by "#" if there are more than one codecs.
 *  @param existsAudio By setting to true, it indicates that this call includes the audio.
 *  @param existsVideo By setting to true, it indicates that this call includes the video.
 *  @param existsScreen By setting to true, it indicates that this call includes the screen.
 *  @param sipMessage   The SIP message received.
 */
- (void)onInviteUpdated:(long)sessionId
            audioCodecs:(NSString*)audioCodecs
            videoCodecs:(NSString*)videoCodecs
           screenCodecs:(NSString*)screenCodecs
            existsAudio:(BOOL)existsAudio
            existsVideo:(BOOL)existsVideo
           existsScreen:(BOOL)existsScreen
             sipMessage:(NSString*)sipMessage;

/*!
 *  This event will be triggered when UAC sent/UAS received ACK (the call is connected). Some functions (hold, updateCall etc...) can be called only after the call is connected, otherwise it will return error.
 *
 *  @param sessionId The session ID of the call.
 */
- (void)onInviteConnected:(long)sessionId;

/*!
 *  If the enableCallForward method is called and a call is incoming, the call will be forwarded automatically and this event will be triggered.
 *
 *  @param forwardTo The target SIP URI for forwarding.
 */
- (void)onInviteBeginingForward:(NSString*)forwardTo;

/*!
 *  This event will be triggered once remote side closes the call.
 *
 *  @param sessionId The session ID of the call.
 *  @param sipMessage   The SIP message received.
 */
- (void)onInviteClosed:(long)sessionId
            sipMessage:(NSString*)sipMessage;

/*!
 *  If a user subscribed and his dialog status monitored, when the monitored user is holding a call
 *  or being rang, this event will be triggered.
 *
 *  @param BLFMonitoredUri the monitored user's URI
 *  @param BLFDialogState - the status of the call
 *  @param BLFDialogId - the id of the call
 *  @param BLFDialogDirection - the direction of the call
 */
- (void)onDialogStateUpdated:(NSString*)BLFMonitoredUri
              BLFDialogState:(NSString*)BLFDialogState
                 BLFDialogId:(NSString*) BLFDialogId
          BLFDialogDirection:(NSString*) BLFDialogDirection;

/*!
 *  If the remote side has placed the call on hold, this event will be triggered.
 *
 *  @param sessionId The session ID of the call.
 */
- (void)onRemoteHold:(long)sessionId;

/*!
 *  If the remote side un-holds the call, this event will be triggered.
 *
 *  @param sessionId   The session ID of the call.
 *  @param audioCodecs The matched audio codecs. It's separated by "#" if there are more than one codecs.
 *  @param videoCodecs The matched video codecs. It's separated by "#" if there are more than one codecs.
 *  @param existsAudio By setting to true, it indicates that this call includes the audio.
 *  @param existsVideo By setting to true, it indicates that this call includes the video.
 */
- (void)onRemoteUnHold:(long)sessionId
           audioCodecs:(NSString*)audioCodecs
           videoCodecs:(NSString*)videoCodecs
           existsAudio:(BOOL)existsAudio
           existsVideo:(BOOL)existsVideo;

/** @} */ // end of group22

/** @defgroup group23 Refer events
 * @{
 */

/*!
 *  This event will be triggered once receiving a REFER message.
 *
 *  @param sessionId       The session ID of the call.
 *  @param referId         The ID of the REFER message. Pass it to acceptRefer or rejectRefer.
 *  @param to              The refer target.
 *  @param from            The sender of REFER message.
 *  @param referSipMessage The SIP message of "REFER". Pass it to "acceptRefer" function.
 */
- (void)onReceivedRefer:(long)sessionId
                referId:(long)referId
                     to:(NSString*)to
                   from:(NSString*)from
        referSipMessage:(NSString*)referSipMessage;

/*!
 *  This callback will be triggered once remote side calls "acceptRefer" to accept the REFER.
 *
 *  @param sessionId The session ID of the call.
 */
- (void)onReferAccepted:(long)sessionId;

/*!
 *  This callback will be triggered once remote side calls "rejectRefer" to reject the REFER.
 *
 *  @param sessionId The session ID of the call.
 *  @param reason    Reason for rejecting.
 *  @param code      Rejecting code.
 */
- (void)onReferRejected:(long)sessionId reason:(NSString*)reason code:(int)code;

/*!
 *  When the refer call is being processed, this event will be trigged.
 *
 *  @param sessionId The session ID of the call.
 */
- (void)onTransferTrying:(long)sessionId;

/*!
 *  When the refer call rings, this event will be triggered.
 *
 *  @param sessionId The session ID of the call.
 */
- (void)onTransferRinging:(long)sessionId;

/*!
 *  When the refer call succeeds, this event will be triggered. ACTV means Active.
    For example: A starts the call with B, and A transfers B to C. When C accepts the refered call, A will receive this event.
 *
 *  @param sessionId The session ID of the call.
 */
- (void)onACTVTransferSuccess:(long)sessionId;

/*!
 *  When the refer call fails, this event will be triggered. ACTV means Active.
 For example: A starts the call with B, and A transfers B to C. When C rejects the refered call, A will receive this event.
 *
 *  @param sessionId The session ID of the call.
 *  @param reason    The error reason.
 *  @param code      The error code.
 */
- (void)onACTVTransferFailure:(long)sessionId reason:(NSString*)reason code:(int)code;

/** @} */ // end of group23

/** @defgroup group24 Signaling events
 * @{
 */
/*!
 *  This event will be triggered when receiving an SIP message.
 *  This event is disabled by default. To enable, use enableCallbackSignaling.
 *
 *  @param sessionId The session ID of the call.
 *  @param message   The SIP message received.
 */
- (void)onReceivedSignaling:(long)sessionId message:(NSString*)message;


/*!
 *  This event will be triggered when a SIP message is sent.
 *  This event is disabled by default. To enable, use enableCallbackSignaling.
 *
 *  @param sessionId The session ID of the call.
 *  @param message   The SIP message sent.
 */
- (void)onSendingSignaling:(long)sessionId message:(NSString*)message;

/** @} */ // end of group24

/** @defgroup group25 MWI events
 * @{
 */

/*!
 *  If there are any waiting voice messages (MWI), this event will be triggered.
 *
 *  @param messageAccount        Account for voice message.
 *  @param urgentNewMessageCount Count of new urgent messages.
 *  @param urgentOldMessageCount Count of old urgent messages.
 *  @param newMessageCount       Count of new messages.
 *  @param oldMessageCount       Count of old messages.
 */
- (void)onWaitingVoiceMessage:(NSString*)messageAccount
        urgentNewMessageCount:(int)urgentNewMessageCount
        urgentOldMessageCount:(int)urgentOldMessageCount
              newMessageCount:(int)newMessageCount
              oldMessageCount:(int)oldMessageCount;

/*!
 *  If there are any waiting fax messages (MWI), this event will be triggered.
 *
 *  @param messageAccount        Account for fax message.
 *  @param urgentNewMessageCount Count of new urgent messages.
 *  @param urgentOldMessageCount Count of old urgent messages.
 *  @param newMessageCount       Count of new messages.
 *  @param oldMessageCount       Count of old messages.
 */
- (void)onWaitingFaxMessage:(NSString*)messageAccount
        urgentNewMessageCount:(int)urgentNewMessageCount
        urgentOldMessageCount:(int)urgentOldMessageCount
              newMessageCount:(int)newMessageCount
              oldMessageCount:(int)oldMessageCount;

/** @} */ // end of group25

/** @defgroup group26 DTMF events
 * @{
 */

/*!
 *  This event will be triggered when receiving a DTMF tone from remote side.
 *
 *  @param sessionId The session ID of the call.
 *  @param tone      DTMF tone.
 * <p><table>
 * <tr><th>code</th><th>Description</th></tr>
 * <tr><td>0</td><td>The DTMF tone 0.</td></tr><tr><td>1</td><td>The DTMF tone 1.</td></tr><tr><td>2</td><td>The DTMF tone 2.</td></tr>
 * <tr><td>3</td><td>The DTMF tone 3.</td></tr><tr><td>4</td><td>The DTMF tone 4.</td></tr><tr><td>5</td><td>The DTMF tone 5.</td></tr>
 * <tr><td>6</td><td>The DTMF tone 6.</td></tr><tr><td>7</td><td>The DTMF tone 7.</td></tr><tr><td>8</td><td>The DTMF tone 8.</td></tr>
 * <tr><td>9</td><td>The DTMF tone 9.</td></tr><tr><td>10</td><td>The DTMF tone *.</td></tr><tr><td>11</td><td>The DTMF tone #.</td></tr>
 * <tr><td>12</td><td>The DTMF tone A.</td></tr><tr><td>13</td><td>The DTMF tone B.</td></tr><tr><td>14</td><td>The DTMF tone C.</td></tr>
 * <tr><td>15</td><td>The DTMF tone D.</td></tr><tr><td>16</td><td>The DTMF tone FLASH.</td></tr>
 * </table></p>
 */
- (void)onRecvDtmfTone:(long)sessionId tone:(int)tone;

/** @} */ // end of group26

/** @defgroup group27 INFO/OPTIONS message events
 * @{
 */

/*!
 *  This event will be triggered when receiving the OPTIONS message.
 *
 *  @param optionsMessage The whole received OPTIONS message in text format.
 */
- (void)onRecvOptions:(NSString*)optionsMessage;

/*!
 *  This event will be triggered when receiving the INFO message.
 *
 *  @param infoMessage The whole received INFO message in text format.
 */
- (void)onRecvInfo:(NSString*)infoMessage;

/*!
 *  This event will be triggered when receiving a NOTIFY message of the subscription.
 *
 *  @param subscribeId       The ID of SUBSCRIBE request.
 *  @param notifyMessage     The received INFO message in text format.
 *  @param messageData       The received message body. It can be either text or binary data.
 *  @param messageDataLength The length of "messageData".
 */
- (void)onRecvNotifyOfSubscription:(long)subscribeId
                      notifyMessage:(NSString*)notifyMessage
                        messageData:(unsigned char*)messageData
                  messageDataLength:(int)messageDataLength;
/** @} */ // end of group27

/** @defgroup group28 Presence events
 * @{
 */
/*!
 *  This event will be triggered when receiving the SUBSCRIBE request from a contact.
 *
 *  @param subscribeId     The ID of SUBSCRIBE request.
 *  @param fromDisplayName The display name of contact.
 *  @param from            The contact who sends the SUBSCRIBE request.
 *  @param subject         The subject of the SUBSCRIBE request.
 */
- (void)onPresenceRecvSubscribe:(long)subscribeId
                fromDisplayName:(NSString*)fromDisplayName
                           from:(NSString*)from
                        subject:(NSString*)subject;

/*!
 *  This event will be triggered when the contact is online or changes presence status.
 *
 *  @param fromDisplayName The display name of contact.
 *  @param from            The contact who sends the SUBSCRIBE request.
 *  @param stateText       The presence status text.
 */
- (void)onPresenceOnline:(NSString*)fromDisplayName
                    from:(NSString*)from
               stateText:(NSString*)stateText;

/*!
 *  When the contact status is changed to offline, this event will be triggered.
 *
 *  @param fromDisplayName The display name of contact.
 *  @param from            The contact who sends the SUBSCRIBE request
 */
- (void)onPresenceOffline:(NSString*)fromDisplayName from:(NSString*)from;


/** @} */ // end of group28

/** @defgroup group29 MESSAGE message events
 * @{
 */

/*!
 *  This event will be triggered when receiving a MESSAGE message in dialog.
 *
 *  @param sessionId         The session ID of the call.
 *  @param mimeType          The message mime type.
 *  @param subMimeType       The message sub mime type.
 *  @param messageData       The received message body. It can be either text or binary data.
 *  @param messageDataLength The length of "messageData".
 */
- (void)onRecvMessage:(long)sessionId
             mimeType:(NSString*)mimeType
          subMimeType:(NSString*)subMimeType
          messageData:(unsigned char*)messageData
    messageDataLength:(int)messageDataLength;

/*!
 *  This event will be triggered when receiving a MESSAGE message out of dialog. For example: pager message.
 *
 *  @param fromDisplayName   The display name of sender.
 *  @param from              The message sender.
 *  @param toDisplayName     The display name of receiver.
 *  @param to                The recipient.
 *  @param mimeType          The message mime type.
 *  @param subMimeType       The message sub mime type.
 *  @param messageData       The received message body. It can be text or binary data.
 *  @param messageDataLength The length of "messageData".
 *  @param sipMessage        The SIP message received.
 */
- (void)onRecvOutOfDialogMessage:(NSString*)fromDisplayName
                            from:(NSString*)from
                   toDisplayName:(NSString*)toDisplayName
                              to:(NSString*)to
                        mimeType:(NSString*)mimeType
                     subMimeType:(NSString*)subMimeType
                     messageData:(unsigned char*)messageData
               messageDataLength:(int)messageDataLength
                      sipMessage:(NSString*)sipMessage;

/*!
 *  This event will be triggered when the message is sent successfully in dialog.
 *
 *  @param sessionId The session ID of the call.
 *  @param messageId The message ID. It's equal to the return value of sendMessage function.
 *  @param sipMessage        The SIP message received.
 */
- (void)onSendMessageSuccess:(long)sessionId
                   messageId:(long)messageId
                  sipMessage:(NSString*)sipMessage;

/*!
 *  This event will be triggered when the message fails to be sent out of dialog.
 *
 *  @param sessionId The session ID of the call.
 *  @param messageId The message ID. It's equal to the return value of sendMessage function.
 *  @param reason    The failure reason.
 *  @param code      Failure code.
 *  @param sipMessage        The SIP message received.
 */
- (void)onSendMessageFailure:(long)sessionId
                   messageId:(long)messageId
                      reason:(NSString*)reason
                        code:(int)code
                  sipMessage:(NSString*)sipMessage;

/*!
 *  This event will be triggered when the message is sent successfully out of dialog.
 *
 *  @param messageId       The message ID. It's equal to the return value of SendOutOfDialogMessage function.
 *  @param fromDisplayName The display name of message sender.
 *  @param from            The message sender.
 *  @param toDisplayName   The display name of message receiver.
 *  @param to              The message receiver.
 *  @param sipMessage        The SIP message received.
 */
- (void)onSendOutOfDialogMessageSuccess:(long)messageId
                        fromDisplayName:(NSString*)fromDisplayName
                                   from:(NSString*)from
                          toDisplayName:(NSString*)toDisplayName
                                     to:(NSString*)to
                             sipMessage:(NSString*)sipMessage;

/*!
 *  This event will be triggered when the message fails to be sent out of dialog.
 *
 *  @param messageId       The message ID. It's equal to the return value of SendOutOfDialogMessage function.
 *  @param fromDisplayName The display name of message sender
 *  @param from            The message sender.
 *  @param toDisplayName   The display name of message receiver.
 *  @param to              The message recipient.
 *  @param reason          The failure reason.
 *  @param code            The failure code.
 *  @param sipMessage        The SIP message received.
 */
- (void)onSendOutOfDialogMessageFailure:(long)messageId
                        fromDisplayName:(NSString*)fromDisplayName
                                   from:(NSString*)from
                          toDisplayName:(NSString*)toDisplayName
                                     to:(NSString*)to
                                 reason:(NSString*)reason
                                   code:(int)code
                             sipMessage:(NSString*)sipMessage;

/*!
 *  This event will be triggered on sending SUBSCRIBE failure.
 *
 *  @param subscribeId     The ID of SUBSCRIBE request.
 *  @param statusCode The status code.
 */
- (void)onSubscriptionFailure:(long)subscribeId
                   statusCode:(int)statusCode;

/*!
 *  This event will be triggered when a SUBSCRIPTION is terminated or expired.
 *
 *  @param subscribeId     The ID of SUBSCRIBE request.
 */
- (void)onSubscriptionTerminated:(long)subscribeId;

/** @} */ // end of group29

/** @defgroup group30 Play audio and video file finished events
 * @{
 */

/*!
 *  If startPlayingFileToRemote function is called with no loop mode, this event will be triggered once the file play finished.
 *
 *  @param sessionId The session ID of the call.
 *  @param fileName  The play file name.
 */
- (void)onPlayFileFinished:(long)sessionId fileName:(NSString*)fileName;

/*!
*  If getStatistics function is called, this event will be triggered once receiving a RTP statistics .
*
*  @param sessionId The session ID of the call.
*  @param stat  RTP statistics of a json format.
*/
- (void)onStatistics:(long)sessionId stat:(NSString*)stat;

/** @} */ // end of group30

/** @defgroup group31 RTP callback events
 * @{
 */
/*!
 *  If enableRtpCallback function is called to enable the RTP callback, this event will be triggered once a RTP packet received.
 *
 *  @param sessionId  The session ID of the call.
 *  @param mediaType    If the received RTP packet is
 *     audio  this parameter is 0
 *     video  this parameter is 1
 *     screen  this parameter is 2
 *  @param direction  The RTP stream callback direction.
 * <p><table>
 * <tr><th>Type</th><th>Description</th></tr>
 * <tr><td>DIRECTION_SEND</td><td>Callback the send RTP stream for one channel based on the given sessionId. </td></tr>
 * <tr><td>DIRECTION_RECV</td><td>Callback the received  RTP stream for one channel based on the given sessionId.</td></tr>
 * </table></p>
 *  @param RTPPacket  The memory of whole RTP packet.
 *  @param packetSize The size of received RTP Packet.
  @note Don't call any SDK API functions in this event directly. If you want to call the API functions or other code, which is time-consuming, you should post a message to another thread and execute SDK API functions or other code in another thread.
 */
- (void)onRTPPacketCallback:(long)sessionId
                  mediaType:(int)mediaType
                  direction:(DIRECTION_MODE) direction
                  RTPPacket:(unsigned char *)RTPPacket
                 packetSize:(int)packetSize;


/** @} */ // end of group31

/** @defgroup group32 Audio and video stream callback events
* @{
*/

/*!
 *  This event will be triggered once receiving the audio packets when enableAudioStreamCallback function is called.
 *
 *  @param sessionId         The session ID of the call.
 *  @param audioCallbackMode The type that is passed in enableAudioStreamCallback function.
 *  @param data              The memory of audio stream. It's in PCM format.
 *  @param dataLength        The data size.
 *  @param samplingFreqHz    The audio stream sample in HZ. For example, it could be 8000 or 16000.
  @note Don't call any SDK API functions in this event directly. If you want to call the API functions or other code, which is time-consuming, you should post a message to another thread and execute SDK API functions or other code in another thread.
 */
- (void)onAudioRawCallback:(long)sessionId
         audioCallbackMode:(int)audioCallbackMode
                      data:(unsigned char *)data
                dataLength:(int)dataLength
            samplingFreqHz:(int)samplingFreqHz;

/*!
 *  This event will be triggered once received the video packets if called enableVideoStreamCallback function.
 *
 *  @param sessionId         The session ID of the call.
 *  @param videoCallbackMode The type passed in enableVideoStreamCallback function.
 *  @param width             The width of video image.
 *  @param height            The height of video image.
 *  @param data              The memory of video stream. It's in YUV420 format, such as YV12.
 *  @param dataLength        The data size.
 *  @return If you changed the sent video data, dataLength should be returned, otherwise 0.
  @note Don't call any SDK API functions in this event directly. If you want to call the API functions or other code, which is time-consuming, you should post a message to another thread and execute SDK API functions or other code in another thread.
 */
- (int)onVideoRawCallback:(long)sessionId
         videoCallbackMode:(int)videoCallbackMode
                     width:(int)width
                    height:(int)height
                      data:(unsigned char *)data
                dataLength:(int)dataLength;

/** @} */ // end of group32
/** @} */ // end of groupDelegate
@end

 



