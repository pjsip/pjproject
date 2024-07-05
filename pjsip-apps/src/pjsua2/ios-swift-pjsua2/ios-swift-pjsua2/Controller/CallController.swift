//
//  CallController.swift
//  ios-swift-pjsua2
//
//  Created by Matan Fridman on 02/07/2024.
//

import Foundation
import CallKit


struct ActiveCalls {
    static var shared = ActiveCalls()
    
    var calls: [UUID] = []
    
    mutating func addCall(_ call: UUID) {
        calls.append(call)
    }

    /// Removes a call from the array of active calls if it exists.
    /// - Parameter call: The call to remove.
    mutating func removeCall(_ call: UUID) {
        calls.removeAll { $0 == call }
    }

    /// Empties the array of active calls.
    mutating func removeAllCalls() {
        calls.removeAll()
    }
}

struct CallController {
    static var shared = CallController()
    var provider: CXProvider?
    let callController = CXCallController()
    var currentCaller = ""

    func startCall(name: String, video: Bool = false) {
        let handle = CXHandle(type: .emailAddress, value: name)
        let uuid = UUID()
        print("requesting action: start call, uuid: ", uuid, "handle: ", name)
        ActiveCalls.shared.addCall(uuid)
        print("current calls: ", ActiveCalls.shared.calls)
        
        // Initialize the start call action
        let startCallAction = CXStartCallAction(call: uuid, handle: handle)
        startCallAction.isVideo = video

        // Create a transaction and add the start call action
        let transaction = CXTransaction(action: startCallAction)
        
        // Request the transaction
        requestTransaction(transaction)
        
        // Report the outgoing call to the provider
//        provider?.reportOutgoingCall(with: uuid, connectedAt: nil)
    }
    
    func endCall(call:UUID) {
        print("requesting action: end call, uuid: ", call)
        let endCallAction = CXEndCallAction(call: call)
        let transaction = CXTransaction()
        transaction.addAction(endCallAction)
        ActiveCalls.shared.removeCall(call)

        requestTransaction(transaction)
    }
    
    func requestTransaction(_ transaction: CXTransaction) {
        let callController = CXCallController()
        callController.request(transaction) { error in
            if let error = error {
                print("Error requesting transaction: \(error.localizedDescription)")
            } else {
                print("Requested transaction successfully")
            }
        }
    }
}
