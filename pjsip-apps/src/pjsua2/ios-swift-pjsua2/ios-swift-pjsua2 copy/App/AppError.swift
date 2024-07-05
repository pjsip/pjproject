//
//  AppError.swift
//  arnacon
//
//  Created by David Fassy on 21/03/2024.
//

import Foundation

class AppError: Error {
    let code: Int
    let description: String

    init(code: Int, description: String) {
        self.code = code
        self.description = description
        print("[AppError] \(code) - \(description)")
    }
}

extension AppError: LocalizedError {
    
    var errorDescription: String? {
        return "\(code) - \(description)"
    }
}
