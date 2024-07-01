import Foundation
import iArnaconSDK
import UIKit


class AppManager {
    
    static let shared = AppManager()
    @Published var isInitialized: Bool = false
    

    var web3Service: Web3IS?
    private let dataSaver: AppDataSaver = AppDataSaver()
    private let dataLogger: AppDataLogger = AppDataLogger()
    
    private var tokenHash: String = UserDefaults.standard.object(forKey: "tokenHash") as? String ?? "" {
        didSet { UserDefaults.standard.set(tokenHash, forKey: "tokenHash") }
    }
    
    private init() {
    }
    
    func initializeWeb3Service() {
        self.web3Service = Web3IS(_dataSaveHelper: dataSaver, _logger: dataLogger);
        self.web3Service?.isMatan()
        print("[AppManager] Web3 service initialized, pk:", self.web3Service?.WALLET.getPublicKey()!)
        self.isInitialized = true
//        dataSaver.setPreference(key: "recent-calls", value: "")
    }
    
    func addRecentCall(call: CallInfo) {
        let callsListString = dataSaver.getPreference(key: "recent-calls", defaultValue: "")
        
        var callsList: [CallInfo] = []
        if !callsListString.isEmpty {
            if let data = callsListString.data(using: .utf8) {
                let decoder = JSONDecoder()
                do {
                    callsList = try decoder.decode([CallInfo].self, from: data)
                } catch {
                    print("Error decoding JSON: \(error)")
                }
            }
        }
        
        callsList.insert(call, at: 0)
        
        let encoder = JSONEncoder()
        if let data = try? encoder.encode(callsList) {
            let newCallListString = String(data: data, encoding: .utf8) ?? ""
            dataSaver.setPreference(key: "recent-calls", value: newCallListString)
        }
        print("added call: ", call)
    }


    
    func getRecentCalls() -> [CallInfo] {
        let callsListString = dataSaver.getPreference(key: "recent-calls", defaultValue: "")
        
        var callsList: [CallInfo] = []
        if !callsListString.isEmpty {
            if let data = callsListString.data(using: .utf8) {
                let decoder = JSONDecoder()
                do {
                    callsList = try decoder.decode([CallInfo].self, from: data)
                } catch {
                    print("Error decoding JSON: \(error)")
                }
            }
        }
        return callsList
    }

    
    func registerUserEns() throws -> [String] {
            guard let userENS = try? self.web3Service?.getENSList() else {
                throw AppError(code: 0, description: "Failed to register user ENS")
            }
            print("[AppManager][registerUserEns] User registered: \(userENS)")
            return userENS
    }
    
    func getStore() async throws -> StoreModel {
        
        try await Helper.retry(maxRetries: 30, retryDelay: 2, name: "getStore") {
          
            guard let jsonStore = try await self.web3Service?.fetchStore() else {
                throw AppError(code: 0, description: "Store not ready")
            }
            print("[AppManager][getStore] Store: \(jsonStore)")
            return try JSONDecoder().decode(StoreModel.self, from: Data(jsonStore.utf8))
        }
    }
}

struct CallInfo: Codable, Identifiable {
    var id = UUID()  // Unique identifier for each call
    let caller: String
    let callee: String
    let timestamp: Date
    let callDuration: TimeInterval
}

