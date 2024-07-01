//
//  SipManager.swift
//  arnacon
//
//  Created by David Fassy on 25/03/2024.
//

import Foundation
import PortSIPVoIPSDK
import CoreTelephony
import Combine

class SipService: NSObject, ObservableObject, PortSIPEventDelegate, MonitorNetworkDelegate {
  
    static let shared = SipService()
    
    let portSIPSDK = PortSIPSDK()
    let monitorNetworkService: MonitorNetworkService
    let soundService: SoundService
    
    private let debugSipMessage = false
   
    var sipRegistrationState: SipRegistrationState {
        didSet {
            print("[SipRegistrationState] \(sipRegistrationState.description)")
        }
    }
    
    weak var delegate: SipServiceDelegate?
    
    private init(monitorNetworkService: MonitorNetworkService = .shared, soundService: SoundService = .shared) {
        
        self.monitorNetworkService = monitorNetworkService
        self.soundService = soundService
        self.sipRegistrationState = .notInitialized(description: "Not initialized")
        
        super.init()
        
        //Monitor network
        self.monitorNetworkService.delegate = self
        
        //portSIPSDK
        portSIPSDK.delegate = self
    }
    
    func portSIPSDKReset() {
        
        portSIPSDK.unRegisterServer(0)
        portSIPSDK.unInitialize()
        self.sipRegistrationState = .notInitialized(description: "Not initialized")
    }
    
    func portSIPSDKInit() {
        
        portSIPSDKReset()
        
        var ret = portSIPSDK.initialize(TRANSPORT_UDP, localIP: "0.0.0.0", localSIPPort: Int32(10002), loglevel: PORTSIP_LOG_NONE, logPath: "", maxLine: Int32(1), agent: "PortSIP SDK for IOS", audioDeviceLayer: 0, videoDeviceLayer: 0, tlsCertificatesRootPath: "", tlsCipherList: "", verifyTLSCertificate: false, dnsServers: "")
        
        if ret != 0 {
            self.sipRegistrationState = .notInitialized(description: String(format: "Initialization error: %d", ret))
            return
        }
        
        ret = portSIPSDK.setLicenseKey("M4iOSMh40NUJCNEE2RTE3NzFFNzJDREFCM0YwNEVFRTRCRTlDOUBCRTc2MTEwMDA1RDVGNEY3NjdBNDFCMkU3ODYzRDY5MEBDRkZBQjY5REEzQTJFMkVCRkQwQ0VDQzRDNDMxRTQ2OUBERTdCNjM5NjhBNzAzQkQ4QTAzOEIyMjk5OTZGOEUxNA")
        
        if ret != 0 {
            self.sipRegistrationState = .notInitialized(description: String(format: "License error: %d", ret))
            return
        }
        
        portSIPSDK.addAudioCodec(AUDIOCODEC_OPUS)
        portSIPSDK.addAudioCodec(AUDIOCODEC_G729)
        portSIPSDK.addAudioCodec(AUDIOCODEC_PCMA)
        portSIPSDK.addAudioCodec(AUDIOCODEC_PCMU)
        
        portSIPSDK.addAudioCodec(AUDIOCODEC_GSM);
        portSIPSDK.addAudioCodec(AUDIOCODEC_ILBC);
        portSIPSDK.addAudioCodec(AUDIOCODEC_AMR);
        portSIPSDK.addAudioCodec(AUDIOCODEC_SPEEX);
        portSIPSDK.addAudioCodec(AUDIOCODEC_SPEEXWB);
        
        portSIPSDK.setAudioSamples(20, maxPtime: 60)
        portSIPSDK.setInstanceId(UIDevice.current.identifierForVendor?.uuidString)
        portSIPSDK.setSrtpPolicy(SRTP_POLICY_NONE)
        portSIPSDK.setKeepAliveTime(50)
        portSIPSDK.setRtpKeepAlive(true, keepAlivePayloadType: 126, deltaTransmitTimeMS: 15000);
        portSIPSDK.enableCallbackSignaling(true, enableReceived: true)
        portSIPSDK.enableCallKit(true)
        
        self.sipRegistrationState = .initialized
    }
    
    public func processSipServer(register: Bool = true, server: String = "") {
        
        //Init sdk
        print("[SipManager][processSipServer] Init PortSip SDK")
        portSIPSDKInit()
        
        self.sipRegistrationState = .inProgress
        
        //Set user
        print("[SipManager][processSipServer] Set user")
        let username = AppManager.shared.state.userEns
        let displayName = AppManager.shared.state.userEns
        let sipServer = server.isEmpty ? AppManager.shared.config.serviceProviderName : server
        let sipServerPort = AppManager.shared.config.serviceProviderPort
        
        var ret = portSIPSDK.setUser(username, displayName: displayName, authName: username, password: "", userDomain: "", sipServer: sipServer, sipServerPort: Int32(sipServerPort), stunServer: "", stunServerPort: 0, outboundServer: "", outboundServerPort: 0)
        if ret != 0 {
            self.sipRegistrationState = .failed(description: String(format: "Set user error: %d", ret))
            delegate?.onSipServiceRegisterFailure()
            return
        }
        
        //Authentication
        print("[SipManager][processSipServer] Set authentication headers")
        if let web3Service = AppManager.shared.web3Service {
            let data = web3Service.getXData()
            let sign = web3Service.getXSign(data: data)
            if !data.isEmpty && !sign.isEmpty {
                print(String(format: "[SipManager][registerSipServer] User token - [%@, %@]", data, sign))
                if register {
                    portSIPSDK.addSipMessageHeader(-1, methodName: "REGISTER", msgType: 1, headerName: "X-Data", headerValue: data)
                    portSIPSDK.addSipMessageHeader(-1, methodName: "REGISTER", msgType: 1, headerName: "X-Sign", headerValue: sign)
                } else {
                    portSIPSDK.addSipMessageHeader(-1, methodName: "INVITE", msgType: 1, headerName: "X-Data", headerValue: data)
                    portSIPSDK.addSipMessageHeader(-1, methodName: "INVITE", msgType: 1, headerName: "X-Sign", headerValue: sign)
                }
            }
            else {
                self.sipRegistrationState = .failed(description: "User token error")
                delegate?.onSipServiceRegisterFailure()
                return
            }
        }
        else {
            self.sipRegistrationState = .failed(description: "Web3 not initialized")
            delegate?.onSipServiceRegisterFailure()
            return
        }
        
        if register {
            print(String(format: "[SipManager][registerSipServer] Register Sip server - [%@, %@]", username, displayName))
            ret = portSIPSDK.registerServer(3600, retryTimes: 0)
            if ret != 0 {
                self.sipRegistrationState = .failed(description: String(format: "Register error: %d", ret))
                delegate?.onSipServiceRegisterFailure()
                return
            }
        }
        else {
            print(String(format: "[SipManager][registerSipServer] Invite Sip server - [%@, %@]", username, displayName))
            self.sipRegistrationState = .success
            
            delegate?.onSipServiceRegisterSuccess()
        }
    }
    
    // MARK: MonitorNetworkServiceDelegate
    
    func networkStatusDidChange(to connection: MonitorNetworkConnection) {
        
        print("Network status changed: \(connection.description)")
    }
    
    // MARK: PortSIPEventDelegate
    
    func onReceivedSignaling(_ sessionId: Int, message: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onReceivedSignaling][SessionId:%d] ==> %@", sessionId, message.components(separatedBy: "\r\n").first!))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onReceivedSignaling][SessionId:%d] ==> %@", sessionId, message))
        }
    }
    
    func onSendingSignaling(_ sessionId: Int, message: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onSendingSignaling][SessionId:%d] ==> %@", sessionId, message.components(separatedBy: "\r\n").first!))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onSendingSignaling][SessionId:%d] ==> %@", sessionId, message))
        }
    }
    
    func onRegisterSuccess(_ statusText: String!, statusCode: Int32, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onRegisterSuccess] %d %@", statusCode, statusText))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onRegisterSuccess] %@", sipMessage))
        }
        
        self.sipRegistrationState = .success
        
        delegate?.onSipServiceRegisterSuccess()
    }
    
    func onRegisterFailure(_ statusText: String!, statusCode: Int32, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onRegisterFailure] %d %@", statusCode, statusText))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onRegisterFailure] %@", sipMessage))
        }
        
        self.sipRegistrationState = .failed(description: statusText)
        
        delegate?.onSipServiceRegisterFailure()
    }
    
    func onInviteIncoming(_ sessionId: Int, callerDisplayName: String!, caller: String!, calleeDisplayName: String!, callee: String!, audioCodecs: String!, videoCodecs: String!, existsAudio: Bool, existsVideo: Bool, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteIncoming] SessionId:%d", sessionId))
        print(String(format: "[PortSIPEventDelegate]:[onInviteIncoming] caller:%@", caller))
        print(String(format: "[PortSIPEventDelegate]:[onInviteIncoming] callerDisplayName:%@", callerDisplayName))
        print(String(format: "[PortSIPEventDelegate]:[onInviteIncoming] callee:%@", callee))
        print(String(format: "[PortSIPEventDelegate]:[onInviteIncoming] calleeDisplayName:%@", calleeDisplayName))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onInviteIncoming] %@", sipMessage))
        }
  
        delegate?.onSipServiceInviteIncoming(sessionId: sessionId, identifier: String(caller.split(separator: "@")[0].replacingOccurrences(of: "sip:", with: "")), domain: String(caller.split(separator: "@")[1]))
    }
    
    func onInviteTrying(_ sessionId: Int) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteTrying] %d", sessionId))
        
        delegate?.onSipServiceInviteTrying(sessionId: sessionId)
    }
    
    func onInviteSessionProgress(_ sessionId: Int, audioCodecs: String!, videoCodecs: String!, existsEarlyMedia: Bool, existsAudio: Bool, existsVideo: Bool, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteSessionProgress] SessionId:%d", sessionId))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onInviteSessionProgress] %@", sipMessage))
        }
    }
    
    func onInviteRinging(_ sessionId: Int, statusText: String!, statusCode: Int32, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteRinging] %d %@", statusCode, statusText))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onInviteRinging] %@", sipMessage))
        }
        
        delegate?.onSipServiceInviteRinging(sessionId: sessionId)
    }
    
    func onInviteAnswered(_ sessionId: Int, callerDisplayName: String!, caller: String!, calleeDisplayName: String!, callee: String!, audioCodecs: String!, videoCodecs: String!, existsAudio: Bool, existsVideo: Bool, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteAnswered] SessionId: %d", sessionId))
        print(String(format: "[PortSIPEventDelegate]:[onInviteAnswered] caller: %@", caller))
        print(String(format: "[PortSIPEventDelegate]:[onInviteAnswered] callerDisplayName: %@", callerDisplayName))
        print(String(format: "[PortSIPEventDelegate]:[onInviteAnswered] callee: %@", callee))
        print(String(format: "[PortSIPEventDelegate]:[onInviteAnswered] calleeDisplayName: %@", calleeDisplayName))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onInviteAnswered] %@", sipMessage))
        }
        
        delegate?.onSipServiceInviteAnswered(sessionId: sessionId)
    }
    
    func onInviteFailure(_ sessionId: Int, callerDisplayName: String!, caller: String!, calleeDisplayName: String!, callee: String!, reason: String!, code: Int32, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteFailure] %d %@", code, reason))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onInviteFailure] %@", sipMessage))
        }
        
        delegate?.onSipServiceInviteFailure(sessionId: sessionId)
    }
    
    func onInviteUpdated(_ sessionId: Int, audioCodecs: String!, videoCodecs: String!, screenCodecs: String!, existsAudio: Bool, existsVideo: Bool, existsScreen: Bool, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteUpdated] SessionId:%d", sessionId))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onInviteUpdated] %@", sipMessage))
        }
    }
    
    func onInviteConnected(_ sessionId: Int) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteConnected] %d", sessionId))
        
        delegate?.onSipServiceInviteConnected(sessionId: sessionId)
    }
    
    func onInviteBeginingForward(_ forwardTo: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteBeginingForward] %@", forwardTo))
    }
    
    func onInviteClosed(_ sessionId: Int, sipMessage: String!) {
        
        print(String(format: "[PortSIPEventDelegate]:[onInviteClosed] %d", sessionId))
        if debugSipMessage {
            print(String(format: "[PortSIPEventDelegate]:[onInviteClosed] %@", sipMessage))
        }
        
        delegate?.onSipServiceInviteClosed(sessionId: sessionId)
    }
    
    func onDialogStateUpdated(_ BLFMonitoredUri: String!, blfDialogState BLFDialogState: String!, blfDialogId BLFDialogId: String!, blfDialogDirection BLFDialogDirection: String!) {
        
    }
    
    func onRemoteHold(_ sessionId: Int) {
        
    }
    
    func onRemoteUnHold(_ sessionId: Int, audioCodecs: String!, videoCodecs: String!, existsAudio: Bool, existsVideo: Bool) {
        
    }
    
    func onReceivedRefer(_ sessionId: Int, referId: Int, to: String!, from: String!, referSipMessage: String!) {
        
    }
    
    func onReferAccepted(_ sessionId: Int) {
        
    }
    
    func onReferRejected(_ sessionId: Int, reason: String!, code: Int32) {
        
    }
    
    func onTransferTrying(_ sessionId: Int) {
        
    }
    
    func onTransferRinging(_ sessionId: Int) {
        
    }
    
    func onACTVTransferSuccess(_ sessionId: Int) {
        
    }
    
    func onACTVTransferFailure(_ sessionId: Int, reason: String!, code: Int32) {
        
    }
    
    func onWaitingVoiceMessage(_ messageAccount: String!, urgentNewMessageCount: Int32, urgentOldMessageCount: Int32, newMessageCount: Int32, oldMessageCount: Int32) {
        
    }
    
    func onWaitingFaxMessage(_ messageAccount: String!, urgentNewMessageCount: Int32, urgentOldMessageCount: Int32, newMessageCount: Int32, oldMessageCount: Int32) {
        
    }
    
    func onRecvDtmfTone(_ sessionId: Int, tone: Int32) {
        
    }
    
    func onRecvOptions(_ optionsMessage: String!) {
        
    }
    
    func onRecvInfo(_ infoMessage: String!) {
        
    }
    
    func onRecvNotifyOfSubscription(_ subscribeId: Int, notifyMessage: String!, messageData: UnsafeMutablePointer<UInt8>!, messageDataLength: Int32) {
        
    }
    
    func onPresenceRecvSubscribe(_ subscribeId: Int, fromDisplayName: String!, from: String!, subject: String!) {
        
    }
    
    func onPresenceOnline(_ fromDisplayName: String!, from: String!, stateText: String!) {
        
    }
    
    func onPresenceOffline(_ fromDisplayName: String!, from: String!) {
        
    }
    
    func onRecvMessage(_ sessionId: Int, mimeType: String!, subMimeType: String!, messageData: UnsafeMutablePointer<UInt8>!, messageDataLength: Int32) {
        
    }
    
    func onRecvOutOfDialogMessage(_ fromDisplayName: String!, from: String!, toDisplayName: String!, to: String!, mimeType: String!, subMimeType: String!, messageData: UnsafeMutablePointer<UInt8>!, messageDataLength: Int32, sipMessage: String!) {
        
    }
    
    func onSendMessageSuccess(_ sessionId: Int, messageId: Int, sipMessage: String!) {
        
    }
    
    func onSendMessageFailure(_ sessionId: Int, messageId: Int, reason: String!, code: Int32, sipMessage: String!) {
        
    }
    
    func onSendOutOfDialogMessageSuccess(_ messageId: Int, fromDisplayName: String!, from: String!, toDisplayName: String!, to: String!, sipMessage: String!) {
        
    }
    
    func onSendOutOfDialogMessageFailure(_ messageId: Int, fromDisplayName: String!, from: String!, toDisplayName: String!, to: String!, reason: String!, code: Int32, sipMessage: String!) {
        
    }
    
    func onSubscriptionFailure(_ subscribeId: Int, statusCode: Int32) {
        
    }
    
    func onSubscriptionTerminated(_ subscribeId: Int) {
        
    }
    
    func onPlayFileFinished(_ sessionId: Int, fileName: String!) {
        
    }
    
    func onStatistics(_ sessionId: Int, stat: String!) {
        
    }
    
    func onRTPPacketCallback(_ sessionId: Int, mediaType: Int32, direction: DIRECTION_MODE, rtpPacket RTPPacket: UnsafeMutablePointer<UInt8>!, packetSize: Int32) {
        
    }
    
    func onAudioRawCallback(_ sessionId: Int, audioCallbackMode: Int32, data: UnsafeMutablePointer<UInt8>!, dataLength: Int32, samplingFreqHz: Int32) {
        
    }
    
    func onVideoRawCallback(_ sessionId: Int, videoCallbackMode: Int32, width: Int32, height: Int32, data: UnsafeMutablePointer<UInt8>!, dataLength: Int32) -> Int32 {
        return 0
    }
    
}
