//
//  WebviewController.swift
//  ios-swift-pjsua2
//
//  Created by Matan Fridman on 14/06/2024.
//

import Foundation
import WebKit
import SwiftUI
import CallKit

struct _WebView: UIViewRepresentable {
    let url: URL
    @Binding var webView: WKWebView?;
    var inCallWith:String = ""

    func makeUIView(context: Context) -> WKWebView {
        print(url)

        let userContentController = WKUserContentController()
        userContentController.add(context.coordinator, name: "buttonPressed")

        let config = WKWebViewConfiguration()
        config.userContentController = userContentController

        let webView = WKWebView(frame: .zero, configuration: config)
        webView.navigationDelegate = context.coordinator
        webView.load(URLRequest(url: url))
        
        return webView
    }

    func updateUIView(_ uiView: WKWebView, context: Context) {
        if context.coordinator.currentURL != url {
            uiView.load(URLRequest(url: url))
            context.coordinator.currentURL = url
        }
        
        // Bind the created webView to the state property
        if self.webView != uiView {
            DispatchQueue.main.async {
                self.webView = uiView
            }
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }

    class Coordinator: NSObject, WKScriptMessageHandler, WKNavigationDelegate {
        var parent: _WebView
        var currentURL: URL?

        init(_ parent: _WebView) {
            self.parent = parent
            self.currentURL = parent.url
        }

        func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
            if message.name == "buttonPressed", let body = message.body as? String {
                print("JavaScript message received: \(body)")

                // Parse JSON
                if let data = body.data(using: .utf8) {
                    do {
                        let request = try JSONDecoder().decode(Request.self, from: data)
                        print("Parsed Request: \(request)")
                        if request.action == "ENS" {
                            if let ens = request.body?.ens {
                                print("ENS: ", ens)
                                let sp = request.body!.sp!
                                print("service provider: ", sp)
                                Task{
                                    try await AppManager.shared.web3Service?.saveENSItem(ens, sp)
                                }
                            }
                        }
                        else if request.action == "GSM" {
                            if let gsm = request.body?.gsm {
                                print("GSM: ", gsm)
                                Task{
                                    try await AppManager.shared.web3Service?.saveENSItem(gsm)
                                }
                            }
                        }
                        else {
                            performAction(data: request)
                        }
                        MainView.shared.selectedView = "View2"
                        Task{
                            do {
                                let deviceToken:String = TokenManager.shared.devicetoken!
                                let voipToken:String = TokenManager.shared.voipToken!
                                try await AppManager.shared.web3Service?.sendTokens(_fcmToken: deviceToken, _voipToken: voipToken)
                                print("tokens sent")
                            }
                            catch {
                                print("Tokens already sent.")
                            }
                        }

                    } catch {
                        print("Failed to parse JSON: \(error.localizedDescription)")
                    }
                }
            } else {
                print("Unexpected message name: \(message.name)")
            }
        }

        func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
            print("WebView did fail provisional navigation with error: \(error.localizedDescription)")
        }
        
        public func performAction(data:Request){
            if data.action == "call" {
                if let to = data.body?.to {
                    print("To: \(to)")
                    let ens = to
                    // resolve ens to sip address
                    let sip = "sip:\(ens)@test.cellact.nl" //let sip = "sip:\(ens)@\(domain)"
                    let xData = "B89FC85E-DD58-456D-A84A-702AB22D7ED7:1719906408227"//(AppManager.shared.web3Service?.getXData())!  // Example XData, replace with real data
                    let xSign = "0xd3154ed6b15d786478ae77cb852075afb8f65cf6be113b4401328de6effcb3444202cc918b153223fff3d22b2d2a6beba91540911ec586b73c439205294797721c"//AppManager.shared.web3Service?.getXSign(data: xData)

                    CallController.shared.startCall(handle: ens)
                    let update = CXCallUpdate()
                    update.remoteHandle = CXHandle(type: .generic, value: ens)
                    
                    CPPWrapper().outgoingCall(sip, xData: xData, xSign: xSign)
                    CPPWrapper().call_listener_wrapper(call_status_listener_swift)
                    parent.webView?.sendDataToHTML("{\"action\": \"ringing\", \"body\": {\"to\": \"\(ens)\", \"from\": \"csda\"}}")
                    print("mtn Parent in call with is: ", ens)
                    parent.inCallWith = ens
                }
            // end current call
            } else if data.action == "end-call" {
                CPPWrapper().hangupCall()
                parent.webView?.sendDataToHTML("{\"action\": \"call-ended\", \"body\": {\"to\": \"\(parent.inCallWith)\", \"from\": \"csda\"}}")
            } else if data.action == "accept-call" {
                print("accept")
                if let from = data.body?.from {
                    CPPWrapper().answerCall()
                    parent.webView?.sendDataToHTML("{\"action\": \"call-started\", \"body\": {\"from\": \"\(from)\"}}")
                    parent.inCallWith = from
                }
            } else if data.action == "reject-call" {
                print("reject")
                CPPWrapper().hangupCall()
                
                parent.webView?.sendDataToHTML("{\"action\": \"call-ended\", \"body\": {\"from\": \"\(parent.inCallWith)\", \"from\": \"csda\"}}")
            }
        }
    }
}

extension WKWebView {
    func sendDataToHTML(_ data: String) {
        let jsCode = "controller.receiveData('\(data)')"
        print("mtn Executing JavaScript: \(jsCode)")
        self.evaluateJavaScript(jsCode) { (result, error) in
            if let error = error {
                print("Failed to send data to HTML: \(error.localizedDescription)")
            } else {
                print("mtn Data sent to HTML successfully: \(String(describing: result))")
            }
        }
    }

    func receivingCall(_ from: String) {
        print("here.")
        sendDataToHTML("{\"action\": \"receiving-call\", \"body\": {\"from\": \"\(from)\"}}")
    }
}
