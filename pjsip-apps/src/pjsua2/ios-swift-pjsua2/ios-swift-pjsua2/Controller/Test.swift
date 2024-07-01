import SwiftUI
import WebKit
import Combine

class CallData: ObservableObject {
    @Published var inCallWith: String = ""
}


class MainController: UIViewController {

//    static let shared: MainController = {
//        // Load MainController from storyboard
//        let storyboard = UIStoryboard(name: "Main", bundle: nil)
//        guard let viewController = storyboard.instantiateViewController(withIdentifier: "mainController") as? MainController else {
//            fatalError("MainController not found in storyboard")
//        }
//        return viewController
//    }()
    
    private override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
        super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
        // Initialization code here
    }
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        // Initialization code here
    }
    
    @IBOutlet weak var viewObj: UIView!
    @IBOutlet weak var selection: UISegmentedControl!
    
    var contentView: ContentView?
    var hostingController: UIHostingController<ContentView>?
    var callData = CallData() // Shared call data
    
    override func viewDidLoad() {
        super.viewDidLoad()
        print("reloading view")
        // Create a UIHostingController to host the SwiftUI view
        hostingController = UIHostingController(rootView: ContentView(callData: callData))
        // Add the hostingController's view to your UIKit view
        addChild(hostingController!)
        viewObj.addSubview(hostingController!.view)
        hostingController!.view.frame = viewObj.bounds
        
        hostingController!.didMove(toParent: self)
    }
    
    
    @IBAction func productChanged(_ sender: UISegmentedControl){
        print(sender);
        var newValueIndex:Int = sender.selectedSegmentIndex;
        var newValue:String = sender.titleForSegment(at: newValueIndex)!;
        print("new value:", newValue );
    }
}


struct ContentView: View {
    @State private var message = "No message received"
    var webView: WKWebView = WV.shared
    @State var loaded: Bool = false
    @ObservedObject var callData: CallData // Observe the data model
    
    var body: some View {
        VStack {
            Button("Simulate incoming call") {
                webView.receivingCall("ddtetre")
            }
            WebView(
                messageReceived: $message,
                url: URL(string: "https://proof-files-git-main-matans-projects-4d48d79b.vercel.app/background.html")!,
                inCallWith: $callData.inCallWith,
                loaded: $loaded
            )
            .edgesIgnoringSafeArea(.all)
            Text(message)
                .padding()
        }
    }
    
    func sendDataToHTML(_ data: String) {
        webView.sendDataToHTML(data)
    }
}

class WV {
    static let shared: WKWebView = WKWebView()
}

struct WebView: UIViewRepresentable {
    @Binding var messageReceived: String
    var webView: WKWebView = WV.shared
    let url: URL
    @Binding var inCallWith: String
    @Binding var loaded: Bool
    
    func makeUIView(context: Context) -> WKWebView {
        let contentController = webView.configuration.userContentController
        contentController.add(context.coordinator, name: "buttonPressed")
        webView.configuration.userContentController = contentController
        
        webView.navigationDelegate = context.coordinator
        print("mtn webview is connected")
        return webView
    }
    
    
    func updateUIView(_ uiView: WKWebView, context: Context) {
        // Only load the URL if the WebView has not finished loading
        if !loaded {
            let request = URLRequest(url: url)
            uiView.load(request)
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }

    class Coordinator: NSObject, WKScriptMessageHandler, WKNavigationDelegate {
        var parent: WebView
        var hasLoadedInitialURL = false

        init(_ parent: WebView) {
            self.parent = parent
        }

        func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
            if message.name == "buttonPressed", let body = message.body as? String {
                print("mtn JavaScript message received: \(body)")
                parent.messageReceived = body
                parent.loaded = true
                // Parse JSON
                if let data = body.data(using: .utf8) {
                    do {
                        let request = try JSONDecoder().decode(Request.self, from: data)
                        print("Parsed Request: \(request)")
                        // invite someone to call
                        performAction(data: request)
                    } catch {
                        print("Failed to parse JSON: \(error.localizedDescription)")
                    }
                }
            } else {
                print("Unexpected message name: \(message.name)")
            }
        }

        func performAction(data:Request){
            if data.action == "call" {
                if let to = data.body?.to {
                    print("To: \(to)")
                    let ens = to
                    // resolve ens to sip address
                    let sip = "sip:\(to)@test.cellact.nl"
//                    CPPWrapper().outgoingCall(sip)
                    CPPWrapper().call_listener_wrapper(call_status_listener_swift)
                    parent.messageReceived = "Sending call to: \(to)"
                    parent.webView.sendDataToHTML("{\"action\": \"ringing\", \"body\": {\"to\": \"\(to)\", \"from\": \"csda\"}}")
                    print("mtn Parent in call with is: ", to)
                    parent.inCallWith = to
                }
            // end current call
            } else if data.action == "end-call" {
                parent.messageReceived = "Ending call..."
                CPPWrapper().hangupCall()
                parent.webView.sendDataToHTML("{\"action\": \"call-ended\", \"body\": {\"to\": \"\(parent.inCallWith)\", \"from\": \"csda\"}}")
            } else if data.action == "accept-call" {
                print("accept")
                if let from = data.body?.from {
                    parent.messageReceived = "Accepting call..."
                    CPPWrapper().answerCall()
                    parent.webView.sendDataToHTML("{\"action\": \"call-started\", \"body\": {\"from\": \"\(from)\"}}")
                    parent.inCallWith = from
                }
            } else if data.action == "reject-call" {
                print("reject")
                parent.messageReceived = "Rejecting call..."
                CPPWrapper().hangupCall()
                parent.webView.sendDataToHTML("{\"action\": \"call-ended\", \"body\": {\"from\": \"\(parent.inCallWith)\", \"from\": \"csda\"}}")
            }
        }
        
        
        func receivingCall(_ from: String) {
            parent.webView.sendDataToHTML("{\"action\": \"receiving-call\", \"body\": {\"from\": \"\(from)\"}}")
        }

        func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
            print("WebView did finish navigation")
        }

        func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
            print("WebView did fail navigation with error: \(error.localizedDescription)")
        }

        func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
            print("WebView did fail provisional navigation with error: \(error.localizedDescription)")
        }
    }
}



struct Request: Codable {
    let action: String
    let body: RequestBody?
    
    init(action: String, body: RequestBody?) {
        self.action = action
        self.body = body
    }
}

struct RequestBody: Codable {
    let from: String?
    let to: String?
    let ens: String?
    let gsm: String?
    let sp: String?
    
    init(from: String? = nil, to: String? = nil, ens: String? = nil, gsm: String? = nil, sp: String? = nil) {
        self.from = from
        self.to = to
        self.ens = ens
        self.gsm = gsm
        self.sp = sp
    }
}
