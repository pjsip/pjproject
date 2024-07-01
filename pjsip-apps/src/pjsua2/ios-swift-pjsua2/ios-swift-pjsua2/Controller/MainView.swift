import SwiftUI
import AVFoundation

struct MainView: View {
    static var shared = MainView()

    @State private var showSidebar = false
    @State var selectedView: String?

    var body: some View {
        NavigationView {
            ZStack(alignment: .leading) {  // Align the ZStack to the leading edge
                _MainView(selectedView: $selectedView)

                if showSidebar {
                    Sidebar(showSidebar: $showSidebar, selectedView: $selectedView)
                        .transition(.move(edge: .leading))
                }
            }
            //            .navigationBarTitle("Main View", displayMode: .inline)
            .navigationBarItems(leading: Button(action: {
                withAnimation {
                    showSidebar.toggle()
                }
            }) {
                Image(systemName: "sidebar.leading")
                    .imageScale(.large)
            })
            .onAppear {
                
                initializeViews()
            }
        }
    }

    func initializeViews() {
        Task {
            await AppManager.shared.initializeWeb3Service()
            
//                let deviceToken:String = TokenManager.shared.devicetoken!
//                let voipToken:String = TokenManager.shared.voipToken!
//            print("tokens got")
//                try await AppManager.shared.web3Service?.sendTokens(_fcmToken: deviceToken, _voipToken: voipToken)
//                print("tokens sent")
            
            if let count = try? AppManager.shared.web3Service?.getENSList().count, count > 0 {
                selectedView = "View2"
            } else {
                selectedView = "View1"
            }
        }
    }
}

struct Sidebar: View {
    @Binding var showSidebar: Bool
    @Binding var selectedView: String?
    
    var body: some View {
        VStack(alignment: .leading) {
            Button(action: {
                selectedView = "View1"
                withAnimation {
                    showSidebar.toggle()
                }
            }) {
                Text("Shop")
                    .padding()
            }
            Button(action: {
                selectedView = "View2"
                withAnimation {
                    showSidebar.toggle()
                }
            }) {
                Text("Main")
                    .padding()
            }
            Button(action: {
                selectedView = "View3"
                withAnimation {
                    showSidebar.toggle()
                }
            }) {
                Text("Recent calls")
                    .padding()
            }
            Button(action: {
                selectedView = "View4"
                withAnimation {
                    showSidebar.toggle()
                }
            }) {
                Text("Profile")
                    .padding()
            }
            Spacer()
        }
        .frame(width: 200)
        .background(Color(.systemGray6))
        .offset(x: showSidebar ? 0 : -200)  // Offset to hide the sidebar when not visible
        .animation(.default, value: showSidebar)
    }
}

struct _MainView: View {
    @Binding var selectedView: String?
    
    var body: some View {
        VStack {
            if selectedView == "View1" {
                ShopView()
            } else if selectedView == "View2" {
                HomeView.shared
            } else if selectedView == "View3" {
                RecentCalls.shared
            } else if selectedView == "View4" {
                Text("This is the profile page.")
            } else {
                Text("Loading...")
            }
            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color(.systemBackground))
        .animation(.default, value: selectedView)
    }
}
