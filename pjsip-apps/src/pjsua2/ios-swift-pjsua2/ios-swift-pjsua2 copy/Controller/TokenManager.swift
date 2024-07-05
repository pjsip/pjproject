//
//  TokenManager.swift
//  ios-swift-pjsua2
//
//  Created by Matan Fridman on 12/06/2024.
//

import Foundation

class TokenManager {
    static let shared = TokenManager()
    private init() {}

    private(set) var voipToken: String?
    private(set) var devicetoken: String?

    func updateVoIPToken(_ token: String) {
        self.voipToken = token
    }
    
    func updateDevicetoken(_ token: String) {
        self.devicetoken = token
    }
}
