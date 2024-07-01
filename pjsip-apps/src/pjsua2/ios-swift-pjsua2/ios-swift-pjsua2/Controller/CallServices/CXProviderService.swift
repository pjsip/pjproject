//
//  PortCxProviderService.swift
//  arnacon
//
//  Created by David Fassy on 25/03/2024.
//

import UIKit
import CallKit
import PortSIPVoIPSDK

class CxProviderService: NSObject, CXProviderDelegate {
    
    var cxProvider: CXProvider
    let callController = CXCallController()
    
    static let shared = CxProviderService()
    
    weak var delegate: CxProviderServiceDelegate?
    
    private override init() {
        
        let providerConfiguration = CXProviderConfiguration()
        providerConfiguration.supportedHandleTypes = [.generic]
        providerConfiguration.iconTemplateImageData = #imageLiteral(resourceName: "WelcomeIcon").pngData()
        cxProvider = CXProvider(configuration: providerConfiguration)
        
        super.init()
        
        cxProvider.setDelegate(self, queue: nil)
    }
    
    func reportOutgoingCall(uuid: UUID, displayName: String, complete: @escaping (Error?) -> Void) {
        
        print("[CxProviderService]:[reportOutgoingCall] uuid \(uuid), displayName \(displayName)")
        
        let handle = CXHandle(type: .generic, value: displayName)
        let startCallAction = CXStartCallAction(call: uuid, handle: handle)
        startCallAction.isVideo = false
        let transaction = CXTransaction()
        transaction.addAction(startCallAction)
            
        callController.request(transaction) { error in
            complete(error)
        }
    }
    
    func reportInComingCall(uuid: UUID, identifier: String, complete: @escaping (Error?) -> Void) {
        
        print("[CxProviderService]:[reportInComingCall] uuid: \(uuid), identifier: \(identifier)")
        
        let handle = CXHandle(type: .generic, value: identifier)
        let update = CXCallUpdate()
        update.remoteHandle = handle
        update.hasVideo = false
        update.supportsGrouping = true
        update.supportsDTMF = true
        update.supportsHolding = true
        update.supportsUngrouping = true

        cxProvider.reportNewIncomingCall(with: uuid, update: update, completion: { error in
            complete(error)
        })
    }
    
    func reportUpdateCall(uuid: UUID, displayName: String) {
        
        print("[CxProviderService]:[reportUpdateCall] uuid \(uuid)")
        
        let handle = CXHandle(type: .generic, value: displayName)
        let update = CXCallUpdate()
        update.remoteHandle = handle
        update.hasVideo = false
        update.supportsGrouping = true
        update.supportsDTMF = true
        update.supportsHolding = true
        update.supportsUngrouping = true

        cxProvider.reportCall(with: uuid, updated: update)
    }
    
    func reportSetHold(uuid: UUID, onHold: Bool) {
        
        print("[CallSessionService]:[reportSetHold] uuid \(uuid), onHold \(onHold)")

        let setHeldCallAction = CXSetHeldCallAction(call: uuid, onHold: onHold)
        let transaction = CXTransaction()
        transaction.addAction(setHeldCallAction)
            
        callController.request(transaction) { error in
            if error != nil {
                print("[CallManager]:[reportSetHold] error: \(String(describing: error))")
            }
        }
    }
    
    func reportSetMute(uuid: UUID, muted: Bool) {
        
        print("[CallSessionService]:[reportSetMute] uuid \(uuid), muted \(muted)")
        
        let setMutedCallAction = CXSetMutedCallAction(call: uuid, muted: muted)
        let transaction = CXTransaction()
        transaction.addAction(setMutedCallAction)
        callController.request(transaction) { error in
            if error != nil {
                print("[CallSessionService]:[reportSetMute] error: \(String(describing: error))")
            }
        }
    }
    
    func reportPlayDtmf(uuid: UUID, dtmf: Int) {
        
        print("[CallSessionService]:[reportPlayDtmf] uuid \(uuid), dtmf \(dtmf)")
        
        var digits: String
        if dtmf == 10 {
            digits = "*"
        } else if dtmf == 11 {
            digits = "#"
        } else {
            digits = String(dtmf)
        }
        
        let dtmfCallAction = CXPlayDTMFCallAction(call: uuid, digits: digits, type: .singleTone)
        let transaction = CXTransaction()
        transaction.addAction(dtmfCallAction)
        let callController = CXCallController()
        callController.request(transaction) { error in
            if error != nil {
                print("[CallSessionService]:[reportPlayDtmf] error: \(String(describing: error))")
            }
        }
    }
    
    func reportEndCall(uuid: UUID) {
        
        print("[CallSessionService]:[reportEndCall]")
       
        let endCallAction = CXEndCallAction(call: uuid)
        let transaction = CXTransaction()
        transaction.addAction(endCallAction)
        
        callController.request(transaction) { error in
            if error != nil {
                print("[CallSessionService]:[reportEndCall] error: \(String(describing: error))")
                //if error, try to hang up sip
                self.delegate?.onEndCall(uuid: uuid)
            }
        }
    }
    
    // MARK: CXProviderDelegate
    
    func providerDidReset(_ provider: CXProvider) {
        
        delegate?.onDidReset()
    }
    
    func provider(_: CXProvider, perform action: CXAnswerCallAction) {
        
        print("[PortCxProviderService]:[CXAnswerCallAction]")
        
        delegate?.onAnswerCall(uuid: action.callUUID) { success in
            if success {
                action.fulfill()
            } else {
                action.fail()
            }
        }
    }

    func provider(_: CXProvider, perform action: CXStartCallAction) {
        
        print("[PortCxProviderService]:[CXStartCallAction]")

        if let delegate = delegate, delegate.onStartCall(uuid: action.callUUID) {
            action.fulfill()
        }
        else {
            action.fail()
        }
    }
    
    func provider(_: CXProvider, perform action: CXEndCallAction) {
        
        print("[PortCxProviderService]:[CXEndCallAction]")
        
        delegate?.onEndCall(uuid: action.callUUID)
        action.fulfill()
    }
   
    func provider(_: CXProvider, perform action: CXSetHeldCallAction) {
        
        print("[PortCxProviderService]:[CXSetHeldCallAction]")
       
        if let delegate = delegate, delegate.onHoldCall(uuid: action.callUUID, onHold: action.isOnHold) {
            action.fulfill()
        }
        else {
            action.fail()
        }
    }

    func provider(_: CXProvider, perform action: CXSetMutedCallAction) {
        
        print("[PortCxProviderService]:[CXSetMutedCallAction]")
      
        if let delegate = delegate, delegate.onMuteCall(uuid: action.callUUID, onMute: action.isMuted) {
            action.fulfill()
        }
        else {
            action.fail()
        }
    }

    func provider(_: CXProvider, didActivate _: AVAudioSession) {
        
        print("[PortCxProviderService]:[AVAudioSession] activate")
        delegate?.onStartAudio()
    }

    func provider(_: CXProvider, didDeactivate _: AVAudioSession) {
        
        print("[PortCxProviderService]:[AVAudioSession] deactivate")
        delegate?.onStopAudio()
    }
    
    func provider(_: CXProvider, perform action: CXPlayDTMFCallAction) {
        
        print("[PortCxProviderService]:[CXPlayDTMFCallAction]")

        var dtmf: Int32 = 0
        switch action.digits {
        case "0":
            dtmf = 0
        case "1":
            dtmf = 1
        case "2":
            dtmf = 2
        case "3":
            dtmf = 3
        case "4":
            dtmf = 4
        case "5":
            dtmf = 5
        case "6":
            dtmf = 6
        case "7":
            dtmf = 7
        case "8":
            dtmf = 8
        case "9":
            dtmf = 9
        case "*":
            dtmf = 10
        case "#":
            dtmf = 11
        default:
            return
        }
        
        if let delegate = delegate, delegate.onPlayDtmf(uuid: action.callUUID, dtmf: dtmf) {
            action.fulfill()
        }
        else {
            action.fail()
        }
    }
}
