import UIKit
import PushKit
import CallKit
import AVFoundation
import UserNotifications


class AppDelegate: UIResponder, UIApplicationDelegate, PKPushRegistryDelegate, CXProviderDelegate, UNUserNotificationCenterDelegate {

    var window: UIWindow?
    var provider: CXProvider?

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        UNUserNotificationCenter.current().delegate = self
        self.voipRegistration()
        self.setupCallKit()

        // Initialize CPPWrapper functions
        self.initializeCPPWrappers()

        return true
    }

    // Initialize CPPWrapper functions
    func initializeCPPWrappers() {
        CPPWrapper().createLibWrapper()
        CPPWrapper().incoming_call_wrapper(incoming_call_swift)
        CPPWrapper().acc_listener_wrapper(acc_listener_swift)
        CPPWrapper().update_video_wrapper(update_video_swift)
        CPPWrapper().call_listener_wrapper(call_status_listener_swift)
    }

    // VoIP registration function
    func voipRegistration() {
        let voipRegistry = PKPushRegistry(queue: DispatchQueue.main)
        voipRegistry.delegate = self
        voipRegistry.desiredPushTypes = [.voIP]
    }

    // CallKit setup function
    func setupCallKit() {
        let configuration = CXProviderConfiguration(localizedName: "YourApp")
        configuration.supportsVideo = true
        configuration.maximumCallGroups = 1
        configuration.maximumCallsPerCallGroup = 1
        CallController.shared.provider = CXProvider(configuration: configuration)
        CallController.shared.provider?.setDelegate(self, queue: nil)
    }

    // MARK: - PKPushRegistryDelegate

    func pushRegistry(_ registry: PKPushRegistry, didUpdate pushCredentials: PKPushCredentials, for type: PKPushType) {
        let voipToken = pushCredentials.token.map { String(format: "%02x", $0) }.joined()
        print("VoIP Token: \(voipToken)")
        // Send the token to your server
        TokenManager.shared.updateVoIPToken(voipToken)
    }

    func application(_ application: UIApplication, didRegisterForRemoteNotificationsWithDeviceToken deviceToken: Data) {
        let tokenParts = deviceToken.map { data in String(format: "%02.2hhx", data) }
        let token = tokenParts.joined()
        print("Device Token: \(token)")
        TokenManager.shared.updateDevicetoken(token)
        // You can save the device token on your server or use it for other purposes
    }
    
    func pushRegistry(_ registry: PKPushRegistry, didReceiveIncomingPushWith payload: PKPushPayload, for type: PKPushType, completion: @escaping () -> Void) {
        let payloadDictionary = payload.dictionaryPayload
        print("caller: ",  CPPWrapper().incomingCallInfoWrapper())

        if let aps = payloadDictionary["aps"] as? [String: Any],
           let handle = aps["caller"] as? String,
           let ens = aps["callee"] as? String {
            print("payload dictionary:", payloadDictionary, ens, handle)
            Task {
                do {
                    let domain = try await AppManager.shared.web3Service?.getCalleeDomain(callee: ens)
                    print("dmn:", domain)
                    
                    let xData = "B89FC85E-DD58-456D-A84A-702AB22D7ED7:1719906408227"//(AppManager.shared.web3Service?.getXData())!  // Example XData, replace with real data
                    let xSign = "0xd3154ed6b15d786478ae77cb852075afb8f65cf6be113b4401328de6effcb3444202cc918b153223fff3d22b2d2a6beba91540911ec586b73c439205294797721c"//AppManager.shared.web3Service?.getXSign(data: xData)
                    print(xData, xSign)
                    
                    CPPWrapper().createAccountWrapper(ens,
                                                      "password",
                                                      "test.cellact.nl",
                                                      "5060",
                                                      xSign,
                                                      xData)
                    CPPWrapper().call_listener_wrapper(call_status_listener_swift)
                } catch {
                    print("Error during asynchronous operations: \(error)")
                }
            }
            self.reportIncomingCall(uuid: UUID(), handle: handle)
            
        }
        completion()
    }

    // Report incoming call to CallKit
    func reportIncomingCall(uuid: UUID, handle: String) {
        let update = CXCallUpdate()
        update.remoteHandle = CXHandle(type: .generic, value: handle)
        CallController.shared.provider!.reportNewIncomingCall(with: uuid, update: update) { error in
            if let error = error {
                print("Error reporting new incoming call: \(error)")
            } else {
                print("Successfully reported new incoming call.")
            }
        }
    }

    // MARK: - CXProviderDelegate

    func providerDidReset(_ provider: CXProvider) {}

    func provider(_ provider: CXProvider, perform action: CXAnswerCallAction) {
        // Handle answering the call
        CPPWrapper().answerCall()
        action.fulfill()
    }

    func provider(_ provider: CXProvider, perform action: CXEndCallAction) {
        // Handle ending the call
        print("Declined call")
        CPPWrapper().hangupCall()
        
        // Ensure the action is fulfilled after handling the hangup
        action.fulfill()
    }
    
    func provider(_ provider: CXProvider, didActivate audioSession: AVAudioSession) {
        print("Received", #function)

        /*
         Start call audio media, now that the audio session is activated,
         after having its priority elevated.
         */
//        startAudio()
    }
}
