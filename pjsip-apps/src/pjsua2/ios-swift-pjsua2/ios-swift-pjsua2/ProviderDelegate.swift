//import Foundation
//
//class CallManager {
//    var calls: [Call] = []
//
//    func add(call: Call) {
//        calls.append(call)
//    }
//
//    func remove(call: Call) {
//        if let index = calls.firstIndex(where: { $0 === call }) {
//            calls.remove(at: index)
//        }
//    }
//}
//
//class Call {
//    let uuid: UUID
//
//    init(uuid: UUID) {
//        self.uuid = uuid
//    }
//}
//
//import CallKit
//
//class ProviderDelegate: NSObject, CXProviderDelegate {
//
//    private let callManager = CallManager()
//    private let provider: CXProvider
//
//    override init() {
//        provider = CXProvider(configuration: type(of: self).providerConfiguration)
//        super.init()
//        provider.setDelegate(self, queue: nil)
//    }
//
//    static var providerConfiguration: CXProviderConfiguration {
//        let providerConfiguration = CXProviderConfiguration(localizedName: "YourApp")
//        providerConfiguration.supportsVideo = false
//        providerConfiguration.maximumCallsPerCallGroup = 1
//        providerConfiguration.supportedHandleTypes = [.generic]
//        providerConfiguration.iconTemplateImageData = UIImage(named: "your_app_icon")?.pngData()
//        providerConfiguration.ringtoneSound = "ringtone.caf"
//        return providerConfiguration
//    }
//
//    func reportIncomingCall(uuid: UUID, handle: String, hasVideo: Bool = false, completion: ((Error?) -> Void)? = nil) {
//        let update = CXCallUpdate()
//        update.remoteHandle = CXHandle(type: .generic, value: handle)
//        update.hasVideo = hasVideo
//
//        provider.reportNewIncomingCall(with: uuid, update: update) { error in
//            if let error = error {
//                print("Failed to report incoming call: \(error.localizedDescription).")
//            } else {
//                let call = Call(uuid: uuid)
//                self.callManager.add(call: call)
//            }
//            completion?(error)
//        }
//    }
//
//    // Implement other CXProviderDelegate methods as needed
//
//    func provider(_ provider: CXProvider, perform action: CXAnswerCallAction) {
//        // Handle answer call action
//        action.fulfill()
//    }
//
//    func provider(_ provider: CXProvider, perform action: CXEndCallAction) {
//        // Handle end call action
//        action.fulfill()
//    }
//
//    func provider(_ provider: CXProvider, perform action: CXSetHeldCallAction) {
//        // Handle hold call action
//        action.fulfill()
//    }
//
//    func provider(_ provider: CXProvider, didActivate audioSession: AVAudioSession) {
//        // Handle audio session activation
//    }
//
//    func provider(_ provider: CXProvider, didDeactivate audioSession: AVAudioSession) {
//        // Handle audio session deactivation
//    }
//}
