import SwiftUI

struct ProfileView: View {
    @EnvironmentObject var appState: AppState
    @State private var username: String = "John Doe"
    
    var body: some View {
        VStack(spacing: 20) {
            Text("Profile")
                .font(.largeTitle)
                .fontWeight(.bold)
            
            TextField("Username", text: $username)
                .textFieldStyle(RoundedBorderTextFieldStyle())
                .padding(.horizontal)
            
            Toggle("Service provider mode", isOn: $appState.isServiceProvider)
                .padding(.horizontal)
            
            Spacer()
        }
        .padding()
        .onAppear {
            if let walletAddress = AppManager.shared.web3Service?.WALLET.getPublicKey() {
                username = walletAddress
                    }
            
        }
    }
}
