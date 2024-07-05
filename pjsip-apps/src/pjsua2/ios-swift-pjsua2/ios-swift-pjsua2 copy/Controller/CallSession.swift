//
//  CallSession.swift
//  arnacon
//
//  Created by David Fassy on 25/03/2024.
//

import Foundation

enum CallSessionState: Equatable {
    
    case none
    case registering
    case inviting
    case trying
    case ringing
    case answered
    case connected
}

class CallSession: ObservableObject {
    
    static let shared = CallSession()
    
    var id: Int
    var uuid: UUID
    var state: CallSessionState
    var incoming: Bool
    var isWeb3: Bool
    var fromStart: Date
    var fromConnected: Date?
    var holdState: Bool
    var muteState: Bool
    var speakerState: Bool
    var identifier: String
    var displayName: String
    var domain: String
    var callKitReported: Bool
    var callKitAnswered: Bool
    var callKitCompletionCallback: ((Bool) -> Void)?
    @Published var showView: Bool
 
    private init() {
        
        id = 0
        uuid = UUID()
        state = .none
        incoming = false
        isWeb3 = false
        fromStart = Date()
        fromConnected = nil
        holdState = false
        muteState = false
        speakerState = false
        identifier = ""
        displayName = ""
        domain = ""
        callKitReported = false
        callKitAnswered = false
        callKitCompletionCallback = nil
        showView = false
    }
    
    func setWaitingOutgoingCallSession(isWeb3: Bool, identifier: String, domain: String,  displayName: String) {
        
        id = 0
        uuid = UUID()
        state = .registering
        incoming = false
        self.isWeb3 = isWeb3
        fromStart = Date()
        fromConnected = nil
        holdState = false
        muteState = false
        speakerState = false
        self.identifier = identifier
        self.displayName = displayName
        self.domain = domain
        callKitReported = false
        callKitAnswered = false
        callKitCompletionCallback = nil
        DispatchQueue.main.async {
            self.showView = true
        }
    }
    
    func setWaitingIncomingCallSession(uuid: UUID, isWeb3: Bool, identifier: String, domain: String) {
        
        id = 0
        self.uuid = uuid
        state = .registering
        incoming = true
        self.isWeb3 = isWeb3
        fromStart = Date()
        fromConnected = nil
        holdState = false
        muteState = false
        speakerState = false
        self.identifier = identifier
        self.displayName = identifier
        self.domain = domain
        callKitReported = true
        callKitAnswered = false
        callKitCompletionCallback = nil
        DispatchQueue.main.async {
            self.showView = true
        }
    }
    
    func reset() {
        
        print("[CallSession][reset]")
        
        id = 0
        uuid = UUID()
        state = .none
        incoming = false
        isWeb3 = false
        fromStart = Date()
        fromConnected = nil
        holdState = false
        muteState = false
        speakerState = false
        identifier = ""
        displayName = ""
        domain = ""
        callKitReported = false
        callKitAnswered = false
        callKitCompletionCallback = nil
        DispatchQueue.main.async {
            self.showView = false
        }
    }
}
