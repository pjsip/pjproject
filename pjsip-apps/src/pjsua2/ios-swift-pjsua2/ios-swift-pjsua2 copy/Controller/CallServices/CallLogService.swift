//
//  CallLogManager.swift
//  arnacon
//
//  Created by David Fassy on 07/03/2024.
//

import Foundation

class CallLogService: ObservableObject {
    
    static let shared = CallLogService()
    
    private var callLogItems: [CallLogItemModel] = []
        
    init() {
        
        loadCallLogItems()
    }
    
    private func loadCallLogItems() {
        
        guard let data = UserDefaults.standard.data(forKey: "callLogItems"),
              let storedCallLogItems = try? JSONDecoder().decode([CallLogItemModel].self, from: data) else { return }
        
        self.callLogItems = storedCallLogItems
    }
    
    private func saveCallLogItems() {
        
        if let data = try? JSONEncoder().encode(self.callLogItems) {
            UserDefaults.standard.set(data, forKey: "callLogItems")
        }
    }
    
    func addCallLogItem(callLogItem: CallLogItemModel) {
        
        self.callLogItems.append(callLogItem)
        
        let web3Logs = callLogItems.filter { $0.isWeb3 }
        let gsmLogs = callLogItems.filter { !$0.isWeb3 }
        
//        callLogItems = Array(web3Logs.suffix(AppManager.shared.config.maxCallLogs)) + Array(gsmLogs.suffix(AppManager.shared.config.maxCallLogs))
        
        saveCallLogItems()
    }
    
    func getCallLogItems(_ isWeb3: Bool) -> [CallLogItemModel] {
        
        return callLogItems.reversed().filter { $0.isWeb3 == isWeb3 }
    }
}

struct CallLogItemModel: Codable, Identifiable {
    
    var id = UUID()
    var isWeb3: Bool
    var isIncoming: Bool
    var date: Date
    var identifier: String
    var displayName: String
    
}
