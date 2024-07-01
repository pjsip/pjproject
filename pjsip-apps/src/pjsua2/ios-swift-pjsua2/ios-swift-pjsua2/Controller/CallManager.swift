//
//  CallManager.swift
//  arnacon
//
//  Created by David Fassy on 25/03/2024.
//

import Foundation
import CallKit


class CallSessionService:  SipServiceDelegate, CxProviderServiceDelegate {
    
    static let shared = CallSessionService()
    
    let sipService: SipService
    let cxProviderService: CxProviderService
    let callLogService: CallLogService
    var callSession: CallSession
    
    private init(sipService: SipService = .shared, cxProviderService: CxProviderService = .shared, callLogService: CallLogService = .shared, callSession: CallSession = .shared) {
        
        self.sipService = sipService
        self.cxProviderService = cxProviderService
        self.callLogService = callLogService
        self.callSession = callSession
        
        //super.init()
        
        self.sipService.delegate = self
        self.cxProviderService.delegate = self
    }
    
    // MARK: SipService delegate
    
    func onSipServiceRegisterSuccess() {
        
        refreshCall()
        
        checkOutgoingCallSession()
    }
    
    func onSipServiceRegisterFailure() {
        
        endCallbySystem(uuid: callSession.uuid, reason: CXCallEndedReason.failed)
    }
    
    func onSipServiceInviteIncoming(sessionId: Int, identifier: String, domain: String) {
        
        checkIncomingCallSession(sessionId: sessionId, identifier: identifier, domain: domain)
    }
    
    func onSipServiceInviteTrying(sessionId: Int) {
        
        if callSession.id == sessionId {
            
            self.callSession.state = .trying
            if !callSession.incoming {
                cxProviderService.cxProvider.reportOutgoingCall(with: callSession.uuid, startedConnectingAt: nil)
            }
        } else {
            print("[CallSessionService]:[onInviteTrying] SessionId not found = \(sessionId)")
        }
    }
    
    func onSipServiceInviteRinging(sessionId: Int) {
        
        if callSession.id == sessionId {
            
            self.callSession.state = .ringing
        } else {
            print("[CallSessionService]:[onInviteRinging] SessionId not found = \(sessionId)")
        }
    }
    
    func onSipServiceInviteAnswered(sessionId: Int) {
        
        if callSession.id == sessionId {
            callSession.state = .answered
        } else {
            print("[CallSessionService]:[onInviteAnswered] SessionId not found = \(sessionId)")
        }
    }
    
    func onSipServiceInviteFailure(sessionId: Int) {
        
        if callSession.id == sessionId {
            endCallbySystem(uuid: callSession.uuid, reason: .failed)
        } else {
            print("[CallSessionService]:[onInviteFailure] sessionId not found = \(sessionId)")
        }
    }
    
    func onSipServiceInviteConnected(sessionId: Int) {
        
        if callSession.id == sessionId {
            callSession.fromConnected = Date()
            self.callSession.state = .connected
            if !callSession.incoming {
                cxProviderService.cxProvider.reportOutgoingCall(with: callSession.uuid, connectedAt: nil)
            }
        } else {
            print("[CallSessionService]:[onInviteConnected] sessionId not found = \(sessionId)")
        }
    }
    
    func onSipServiceInviteClosed(sessionId: Int) {
        
        if callSession.id == sessionId {
            endCallbySystem(uuid: callSession.uuid, reason: .remoteEnded)
        } else {
            print("[CallSessionService]:[onInviteClosed] sessionId not found = \(sessionId)")
        }
    }
    
    // MARK: CxProvider delegate
    
    func onStartCall(uuid: UUID) -> Bool {
        
        return callSession.uuid == uuid && callSession.incoming == false && callSession.id > 0
    }
    
    func onAnswerCall(uuid: UUID, completion: @escaping (Bool) -> Void) {
        
        answerCallByUUID(uuid: uuid) { success in
            completion(success)
        }
    }
    
    func onHoldCall(uuid: UUID, onHold: Bool) -> Bool {
        
        return holdCallByUUID(uuid: uuid, onHold: onHold)
    }
    
    func onMuteCall(uuid: UUID, onMute: Bool) -> Bool {
        
        return muteCallByUUID(uuid: uuid, onMute : onMute)
    }
    
//    func onPlayDtmf(uuid: UUID, dtmf: Int32) -> Bool {
//        
//        return playDtmfByUUID(uuid: uuid, dtmf: dtmf)
//    }
    
    func onEndCall(uuid: UUID) {
        
        hangUpCallByUUID(uuid: uuid)
    }
    
    func onDidReset() {
        
        stopAudio()
        
        callSession.reset()
        
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            self.sipService.portSIPSDKReset()
        }
    }
    
    func onStartAudio() {
        
        startAudio()
    }
    
    func onStopAudio() {
        
        stopAudio()
    }
    
    // MARK: UI delegate
    
    func pressHangUp() {
        
        print("[CallSessionService]:[pressPressHangUp]")
        cxProviderService.reportEndCall(uuid: callSession.uuid)
    }
    
    func pressHold() {
        
        print("[CallSessionService]:[pressHold]")
        if callSession.state == .connected {
            
            cxProviderService.reportSetHold(uuid: callSession.uuid, onHold: !callSession.holdState)
        }
    }
    
    func pressMute() {
        
        print("[CallSessionService]:[pressMute]")
        if callSession.state == .connected {
            
            cxProviderService.reportSetMute(uuid: callSession.uuid, muted: !callSession.muteState)
        }
    }
    
    func pressNumpad(val: Int32) {
        
        print("[CallSessionService]:[pressNumpad]")
        if callSession.state == .connected {
            
            cxProviderService.reportPlayDtmf(uuid: callSession.uuid, dtmf: Int(val))
        }
    }
    
    //MARK: Sip actions
    
    func hangUpCallByUUID(uuid: UUID) {
        
        print("[CallSessionService]:[hangUpCallByUUID] uuid \(uuid)")
        
        if callSession.uuid == uuid {
            
           
            if callSession.id > 0 {
                
                if callSession.state == .connected || !callSession.incoming {
                    _ = sipService.portSIPSDK.hangUp(callSession.id)
                } else {
                    _ = sipService.portSIPSDK.rejectCall(callSession.id, code: 486)
                }
            }
            
            //session end
            callSession.reset()
            
            //reset speaker
            resetSpeaker()
            
            //reset sip sdk
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                self.sipService.portSIPSDKReset()
            }
            
        }
    }
    
    func holdCallByUUID(uuid: UUID, onHold: Bool) -> Bool {
        
        print("[CallSessionService]:[holdCallByUUID] onHold \(onHold), uuid \(uuid)")
        
        if callSession.uuid == uuid {
            
            if callSession.state == .connected && callSession.holdState != onHold {
                
                var ret:Int32 = -1
                if onHold {
                    ret = sipService.portSIPSDK.hold(callSession.id)
                } else {
                    ret = sipService.portSIPSDK.unHold(callSession.id)
                }
                if ret == 0 {
                    callSession.holdState = onHold
                    return true
                }
            }
        }
        
        return false
    }
    
    func muteCallByUUID(uuid: UUID, onMute: Bool) -> Bool {
        
        print("[CallSessionService]:[muteCall] mute \(onMute), uuid \(uuid)")
        
        if callSession.uuid == uuid {
            
            if callSession.state == .connected && callSession.muteState != onMute {
                
                var ret:Int32 = -1
                if onMute {
                    ret = sipService.portSIPSDK.muteSession(callSession.id, muteIncomingAudio: false, muteOutgoingAudio: true, muteIncomingVideo: false, muteOutgoingVideo: true)
                } else {
                    ret = sipService.portSIPSDK.muteSession(callSession.id, muteIncomingAudio: false, muteOutgoingAudio: false, muteIncomingVideo: false, muteOutgoingVideo: false)
                }
                if ret == 0 {
                    callSession.muteState = onMute
                    return true
                }
            }
        }
        
        return false
    }
    
//    func playDtmfByUUID(uuid: UUID, dtmf: Int32) -> Bool {
//        
//        print("[CallSessionService]:[playDtmfByUUID] uuid \(uuid), dtmf \(dtmf)")
//
//        if callSession.uuid == uuid {
//            
//            if callSession.state == .connected {
//                let ret = sipService.portSIPSDK.sendDtmf(callSession.id, dtmfMethod: DTMF_RFC2833, code: dtmf, dtmfDration: 160, playDtmfTone: true)
//                if ret == 0 {
//                    return true
//                }
//            }
//        }
//        
//        return false
//    }
//    
    // MARK: Outgoing call

    
//    func dialOutgoingCall(identifier: String, isWeb3: Bool, completionHandler: @escaping (AppError?) -> Void) {
//        
//        
//        print("[SipManager]:[dialOutgoingCall] Identifier - \(identifier)")
//        
//        let onContactResolved: (ContactModel) -> Void = { contact in
//            
//            self.callSession.setWaitingOutgoingCallSession(isWeb3: isWeb3, identifier: contact.identifier, domain: contact.domain, displayName: contact.displayName)
//            
//            Task {
//                self.sipService.processSipServer(register: false)
//            }
//           
//           completionHandler(nil)
//        }
//        
//        let onMicrophoneAccessGranted = {
//            
//            AppManager.shared.contactService.resolveContact(identifier: identifier, isWeb3: isWeb3) { result in
//                switch result {
//                case .success(let contact):
//                    onContactResolved(contact)
//                case .failure(let error):
//                    completionHandler(error)
//                }
//            }
//        }
        
//        guard callSession.state == .none else {
//            
//            return completionHandler(AppError(code: 10, description: "A call is currently active"))
//        }
//        
//        sipService.soundService.requestMicrophoneAccessIfNeeded { granted in
//            if granted {
//                onMicrophoneAccessGranted()
//            } else {
//                completionHandler(AppError(code: 0, description: "Please authorize access to microphone in system settings"))
//            }
//        }
//    }
    
    func checkOutgoingCallSession() {
        
        if callSession.state == .registering && !callSession.incoming {
            
            print("[CallSessionService]:[checkOutgoingCallSession] Outgoing call found")
            
            if Calendar.current.dateComponents([.second], from: callSession.fromStart, to: Date()).second! > 30 {
                print("[CallSessionService]:[checkOutgoingCallSession] Outgoing call obsolete")
                
                endCallbySystem(uuid: callSession.uuid, reason: CXCallEndedReason.failed)
            }
            else {
                
                performOutgoingCall(uuid: callSession.uuid)
            }
        }
    }

    func performOutgoingCall(uuid: UUID) {
        
        print("[CallSessionService]:[performOutgoingCall] UUID \(uuid)")
        
        if callSession.uuid == uuid {
        
            //let callee = currentCallSession.isWeb3 ? currentCallSession.identifier + "@" + currentCallSession.
            let sessionId = sipService.portSIPSDK.call(callSession.identifier, sendSdp: true, videoCall: false)
            if sessionId > 0 {
                callSession.id = sessionId
                callSession.state = .inviting
                callLogService.addCallLogItem(callLogItem: CallLogItemModel(isWeb3: callSession.isWeb3, isIncoming: callSession.incoming, date: Date(), identifier: callSession.identifier, displayName: callSession.displayName))
                
                cxProviderService.reportOutgoingCall(uuid: callSession.uuid, displayName: callSession.displayName, complete: { (error) in
                    
                    if error == nil {
                        
                        self.callSession.callKitReported = true
                        
                        DispatchQueue.global(qos: .background).asyncAfter(deadline: .now() + 30.0) {
                            if self.callSession.uuid == uuid && self.callSession.state != .connected {
                                
                                print("[CallSessionService]:[performOutgoingCall] UUID \(uuid) - Not established after 30 sec")
                                self.endCallbySystem(uuid: uuid, reason: CXCallEndedReason.unanswered)
                            }
                        }
                    } else {
                        print("[CallSessionService]:[makeCall] error: \(String(describing: error))")
                        self.endCallbySystem(uuid: uuid, reason: CXCallEndedReason.failed)
                    }
                })
            } else {
                print("[CallSessionService]:[performOutgoingCall] UUID \(uuid) -  Sip call error")
                self.endCallbySystem(uuid: uuid, reason: CXCallEndedReason.failed)
            }
        } else {
            print("[CallSessionManager]:[performOutgoingCall] UUID \(uuid) - Session not found")
        }
    }
    
    // MARK: Incoming call
    
    func receivedIncomingCall(uuid: UUID, callee: String, identifier: String, domain: String, isWeb3: Bool) {
        
        print("[CallSessionService]:[receivedIncomingCall] uuid: \(uuid), identifier: \(identifier), domain: \(domain)")
        
        guard self.callSession.state == .none else {
            print("[CallSessionService]:[receivedIncomingCall] A call is currently active")
            return
        }
        
        cxProviderService.reportInComingCall(uuid: uuid, identifier: identifier, complete: { (error) in
            if error == nil {
                
                self.callSession.setWaitingIncomingCallSession(uuid: uuid, isWeb3: isWeb3, identifier: identifier, domain: domain)
                
                self.callLogService.addCallLogItem(callLogItem: CallLogItemModel(isWeb3: self.callSession.isWeb3, isIncoming: self.callSession.incoming, date: Date(), identifier: self.callSession.identifier, displayName: self.callSession.displayName))
                
                self.setNoInviteReceivedIncomingCallSession(uuid: uuid)
                
                Task {
                    try await Helper.retry(maxRetries: 10, retryDelay: 1, name: "processSipServer") {
                        guard AppManager.shared.web3Service != nil else {
                            throw AppError(code: 0, description: "Failed to processSipServer, Web3 not initialized")
                        }
                        self.sipService.processSipServer(register: true)
                    }
                }
            }
            else {
                print("[CallSessionService]:[receivedIncomingCall] Error: \(String(describing: error))")
            }
        })
    }
    
    func checkIncomingCallSession(sessionId: Int, identifier: String, domain: String) {
        
        if callSession.state == .registering && callSession.incoming && callSession.identifier == identifier && callSession.domain == domain {
            
            print("[CallSessionService]:[checkIncomingCallSession] Incoming call found")
            
            callSession.id = sessionId
            callSession.state = .inviting
            if callSession.callKitAnswered {
                print("[CallSessionService]:[checkIncomingCallSession] Call already answered with callkit")
                
                let ret = performAnswerCall(uuid: callSession.uuid)
                callSession.callKitCompletionCallback?(ret)
            }
        }
        else {
            //voip not received yet, reject with code 480 for retry
            print("[CallSessionService]:[checkIncomingCallSession] reject call with retry")
            sipService.portSIPSDK.rejectCall(sessionId, code: 480)
        }
    }
    
    func answerCallByUUID(uuid: UUID, completion completionHandler: @escaping (_ success: Bool) -> Void) {
        
        if callSession.uuid == uuid {
            
            if callSession.state == .registering {
                
                print("[CallSessionService]:[performAnswerCall] Invite not yet received")
                callSession.callKitAnswered = true
                callSession.callKitCompletionCallback = completionHandler
            } else {
                print("[CallSessionService]:[performAnswerCall] uuid \(uuid)")
                if performAnswerCall(uuid: uuid) {
                    completionHandler(true)
                } else {
                    completionHandler(false)
                }
            }
        
        } else {
            completionHandler(false)
        }
    }
    
    func performAnswerCall(uuid: UUID) -> Bool {
        
        if callSession.uuid == uuid {
            
            if callSession.state == .registering {
                print("[CallSessionService]:[answerCallWithUUID] Invite not yet received")
                callSession.callKitAnswered = true
                return true
            } else {
                print("[CallSessionService]:[answerCallWithUUID] uuid \(uuid)")
                
                let ret = sipService.portSIPSDK.answerCall(callSession.id, videoCall: false)
                if ret == 0 {
                    return true
                } else {
                    endCallbySystem(uuid: uuid, reason: .failed)
                    return false
                }
            }
        } else {
            print("[CallSessionService]:[answerCallWithUUID] Session not found")
        }
        
        return false
    }
    
    func setNoInviteReceivedIncomingCallSession(uuid: UUID) {
        
        print("[CallSessionService]:[setNoInviteReceivedIncomingCallSession]")
        
        DispatchQueue.global(qos: .background).asyncAfter(deadline: .now() + 20.0) {
            if self.callSession.uuid == uuid && self.callSession.state == .registering {
                print("[CallSessionService]:[receivedIncomingCall] Invite not received after 20 sec")
                self.endCallbySystem(uuid: uuid, reason: .failed)
            }
        }
    }
    
    func refreshCall() {
        
        print("[CallSessionService]:[refreshCall]")
        
        if callSession.state == .connected {
           
            print(String(format: "[CallManager]:[refreshCall] session %d", callSession.id))
            sipService.portSIPSDK.updateCall(callSession.id, enableAudio: true, enableVideo: false)
        }
    }
    
    func endCallbySystem(uuid: UUID, reason: CXCallEndedReason?) {
        
        print("[CallSessionService]:[endCall] uuid \(uuid)")
        
        if callSession.uuid == uuid {
            
            //callkit end
            if callSession.callKitReported && reason != nil {
                
                cxProviderService.cxProvider.reportCall(with: uuid, endedAt: nil, reason: reason!)
            }
            
            hangUpCallByUUID(uuid: uuid)
        }
    }
  
    // MARK: Audio
    
    func startAudio() {
        
        print("[CallSessionService]:[startAudio]")
        if sipService.portSIPSDK.startAudio() {
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                self.setSpeaker(on: self.callSession.speakerState)
            }
        }
    }

    func stopAudio() {
        
        print("[CallSessionService]:[stopAudio]")
        sipService.portSIPSDK.stopAudio()
    }
    
    func resetSpeaker() {
        
        print("[CallSessionService]:[resetSpeaker]")
        sipService.portSIPSDK.setLoudspeakerStatus(true)
    }
    
    func setSpeaker(on: Bool) {
        
        print("[CallSessionService]:[setSpeaker] on \(on)")
        let ret = sipService.portSIPSDK.setLoudspeakerStatus(on)
        if ret == 0 {
            callSession.speakerState = on
        }
    }
    
    func pressSpeaker() {
        
        print("[CallSessionService]:[pressSpeaker]")
        
        if callSession.state == .connected {
            setSpeaker(on: !callSession.speakerState)
        }
    }
}

protocol CxProviderServiceDelegate: AnyObject {
    
    func onStartCall(uuid: UUID) -> Bool
    func onAnswerCall(uuid: UUID, completion: @escaping (Bool) -> Void)
    func onHoldCall(uuid: UUID, onHold: Bool) -> Bool
    func onMuteCall(uuid: UUID, onMute: Bool) -> Bool
    func onPlayDtmf(uuid: UUID, dtmf: Int32) -> Bool
    func onEndCall(uuid: UUID)
    func onDidReset()
    func onStartAudio()
    func onStopAudio()
}

protocol SipServiceDelegate: AnyObject {
    
    func onSipServiceRegisterSuccess()
    func onSipServiceRegisterFailure()
    func onSipServiceInviteIncoming(sessionId: Int, identifier: String, domain: String)
    func onSipServiceInviteTrying(sessionId: Int)
    func onSipServiceInviteRinging(sessionId: Int)
    func onSipServiceInviteAnswered(sessionId: Int)
    func onSipServiceInviteFailure(sessionId: Int)
    func onSipServiceInviteConnected(sessionId: Int)
    func onSipServiceInviteClosed(sessionId: Int)
}
