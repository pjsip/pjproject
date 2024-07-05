import SwiftUI
import WebKit
import Combine

class HomeViewModel:ObservableObject {
//    static let shared = HomeViewModel()
    @Published var options: [String] = []
    @Published var selectedOption: String = ""
    @Published var optionsUpdated: Bool = false

    var cancellables = Set<AnyCancellable>()

    init() {
        AppManager.shared.$isInitialized
            .filter { $0 }  // Proceed only if it's true (initialized)
            .sink { [weak self] _ in
                self?.loadOptions()
            }
            .store(in: &cancellables)
    }
    
    func updateOptions(newOptions:[String]){
        self.options = newOptions
        self.selectedOption = self.options[0]
        print("options count: ", self.options.count)
        if(!self.optionsUpdated){
            self.optionsUpdated = true
        }
        print("options: ", self.options)
    }
    
    func loadOptions() {
        print("loading options...")
        Task {
            do {
                if let fetchedOptions = try AppManager.shared.web3Service?.getENSList() {
                    DispatchQueue.main.async {
                        self.updateOptions(newOptions: fetchedOptions)
                        
                        if self.options.isEmpty {
                            print("options is empty")
                            // Dummy options for testing
                            self.updateOptions(newOptions: ["matan.cellact", "matan.eth", "0612345678"])
                        }
                    }
                } else {
                    print("Failed to fetch options.")
                }
            } catch {
                print("Error fetching options: \(error)")
            }
            
        }
    }
}

struct HomeView: View {
    static var shared = HomeView()
    @ObservedObject private var viewModel = HomeViewModel()
    @State private var webViewUrl: URL?
    @State var webView: WKWebView? = WKWebView()
    
    // Add this method to HomeView
    func performActionOnWebView(data: Request) {
        if let webView = webView {
            // Assuming _WebView's Coordinator is accessible
            if let coordinator = webView.navigationDelegate as? _WebView.Coordinator {
                coordinator.performAction(data: data)
            }
        }
    }
    
    var body: some View {
        VStack {
            Picker("Select an option", selection: $viewModel.selectedOption) {
                ForEach(viewModel.options, id: \.self) { option in
                    Text(option).tag(option)
                }
            }
            .pickerStyle(MenuPickerStyle())
            .onChange(of: viewModel.selectedOption) { newValue in
                print("change in selection", newValue)
                webViewUrl = getProductDetails(ens: newValue)
                if viewModel.options.count > 0 {
                    let xData = "B89FC85E-DD58-456D-A84A-702AB22D7ED7:1719906408227"//(AppManager.shared.web3Service?.getXData())!  // Example XData, replace with real data
                    let xSign = "0xd3154ed6b15d786478ae77cb852075afb8f65cf6be113b4401328de6effcb3444202cc918b153223fff3d22b2d2a6beba91540911ec586b73c439205294797721c"//AppManager.shared.web3Service?.getXSign(data: xData)
                    Task{
                        var domain = try await AppManager.shared.web3Service?.getCalleeDomain(callee: newValue)
                        if (domain == "Error"){
                            domain = "test2.cellact.nl"
                        }
                        print("domain:", domain)
//                        registerAccount(ens: newValue, domain: domain!, xData: xData, xSign: xSign!)
                    }
                }
            }

            if let url = webViewUrl {
                _WebView(url:url, webView: $webView)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                Text("Use the shop to register your first product")
                    .foregroundColor(.gray)
            }
            Spacer()
            Button(action: {
                AppManager.shared.getRecentCalls()
            }) {
                Text("Recent calls")
            }
            Spacer()
            Button(action: {
                CPPWrapper().unregisterAccountWrapper()
            }) {
                Text("Unregister Account")
            }
            Spacer()
            Button(action: {
                let xData = (AppManager.shared.web3Service?.getXData())!
                let xSign = AppManager.shared.web3Service?.getXSign(data: xData)
                registerAccount(ens: viewModel.selectedOption, domain: "test.cellact.nl", xData: xData, xSign: xSign!)
                print("registered")
            }) {
                Text("Register Account")
            }
        }
        .onAppear {
            viewModel.loadOptions()
                    setupBindings()
                }
    }

    func getProductDetails(ens: String) -> URL {
//        let providerUrls = {"test.cellact.nl": "https://proof-files-git-main-matans-projects-4d48d79b.vercel.app/background.html?ens=\(viewModel.selectedOption)"}
        
        let urlList: [String] = ["https://arnacon-stores.vercel.app/shop-1.html", "https://arnacon-stores.vercel.app/shop-2.html", "https://arnacon-stores.vercel.app/shop-3.html", "https://proof-files-git-main-matans-projects-4d48d79b.vercel.app/background.html?ens=\(viewModel.selectedOption)"]
        if var index = viewModel.options.firstIndex(of: ens) {
            print("idx: ", index)
            if index > urlList.count - 1 {
                index = urlList.count - 1
            }
            return URL(string: urlList[3])!
        } else {
            return URL(string: "https://example.com")!
        }
    }
    
    func registerAccount(ens:String, domain:String,  xData:String, xSign:String){
        print("registering account: ", ens, xData, xSign)
        CPPWrapper().createAccountWrapper(ens,
                                          "password",
                                          domain,   // test.cellact.nl
                                          "5060",
                                          xSign,
                                          xData)
    }
    private func setupBindings() {
        viewModel.$optionsUpdated
            .filter { $0 }  // Proceed only if it's true (initialized)
            .sink { _ in
                print("count: ", viewModel.options.count)
                if viewModel.options.count > 0 {
                    webViewUrl = getProductDetails(ens: viewModel.selectedOption)
                } else {
                    webViewUrl = URL(string: "https://example.com")
                }
                print("url: ", webViewUrl as Any)
            }
            .store(in: &viewModel.cancellables)
    }
}
