//
//  AppDataSaver.swift
//  arnacon
//
//  Created by David Fassy on 17/03/2024.
//

import Foundation
import iArnaconSDK


public class AppDataSaver: PDataSaveHelper {
    
    public init() {}
    
    public func setPreference<T>(key: String, value: T) {
        UserDefaults.standard.set(value, forKey: key)
    }
    
    public func getPreference<T>(key: String, defaultValue: T) -> T {
        
        if let value = UserDefaults.standard.object(forKey: key) as? T {
            return value
        }
        return defaultValue
    }
}
