//
//  AppUtils.swift
//  arnacon
//
//  Created by David Fassy on 06/03/2024.
//

import Foundation


class Helper {
    
    static func retry<T>(maxRetries: Int, retryDelay: TimeInterval, name: String, operation: @escaping () async throws -> T) async throws -> T {
        
        var attempts = 0
        
        print("[Helper][retry][\(name)] Trying \(maxRetries) attempts every \(retryDelay) seconds")
        
        while attempts < maxRetries {
            attempts += 1
            do {
                return try await operation()
            } catch {
                print("[Helper][retry][\(name)] Attempt \(attempts) failed")
                if attempts < maxRetries {
                    try? await Task.sleep(nanoseconds: UInt64(retryDelay * 1_000_000_000))
                }
            }
        }
        throw AppError(code: 0, description: "Operation failed after \(maxRetries) attempts")
    }
    
    static func getUrlScheme() -> String {
        
        if let urlTypes = Bundle.main.infoDictionary?["CFBundleURLTypes"] as? [AnyObject],
            let urlName = urlTypes.first?["CFBundleURLName"] as? String,
            let urlSchemes = urlTypes.first?["CFBundleURLSchemes"] as? [AnyObject],
           let urlScheme = urlSchemes.first as? String {
            return String(format: "%@://%@", urlScheme, urlName)
        }
        
        return "arnacon://"
    }
    
    static func getVersion() -> String {
       
        if let shortVersion = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String,
           let version = Bundle.main.infoDictionary?["CFBundleVersion"] as? String {
            return String(format: "%@.%@", shortVersion, version)
        }
        
        return "1.0.0"
    }
}
