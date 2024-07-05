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
    
    
    func startCall(handle: String, video: Bool = false){
        let handle = CXHandle(type: .phoneNumber, value: handle)
        let uuid = UUID()
        ActiveCalls.shared.addCall(uuid)
        let startCallAction = CXStartCallAction(call: uuid, handle: handle)

        startCallAction.isVideo = video

        let transaction = CXTransaction()
        transaction.addAction(startCallAction)

        requestTransaction(transaction)
        provider?.reportOutgoingCall(with: uuid, connectedAt: Date())
    }
    
    func end(call:UUID) {
        let endCallAction = CXEndCallAction(call: call)
        let transaction = CXTransaction()
        transaction.addAction(endCallAction)

        requestTransaction(transaction)
    }
    
    private func requestTransaction(_ transaction: CXTransaction) {
        callController.request(transaction) { error in
            if let error = error {
                print("Error requesting transaction:", error.localizedDescription)
            } else {
                print("Requested transaction successfully")
            }
        }
    }
}
