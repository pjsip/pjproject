//
//  AppLogger.swift
//  arnacon
//
//  Created by David Fassy on 21/05/2024.
//

import Foundation
import iArnaconSDK

public class AppDataLogger: PLogger {
    
    public init() {}
    
    public func debug(_ value: String) {
        print("[iArnaconSDK] " + value)
    }
    public func error(_ error: String, exception: Error) {
        print("[iArnaconSDK] Error: " + error)
    }
}
