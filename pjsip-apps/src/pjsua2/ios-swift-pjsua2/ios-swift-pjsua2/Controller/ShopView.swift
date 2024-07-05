import SwiftUI
import WebKit

struct ShopView: View {
    @State var webView: WKWebView? = WKWebView()

    var walletAddress: String {
    if AppManager.shared.isInitialized,
       let publicKey = AppManager.shared.web3Service?.WALLET.getPublicKey() {
        return publicKey
    } else {
        return ""
    }
}

var body: some View {
    
    VStack {
        _WebView(url: URL(string: "https://arnacon-shop.vercel.app/?user_address=\(walletAddress)")!, webView: $webView)
            .edgesIgnoringSafeArea(.all)
    }
}

}
