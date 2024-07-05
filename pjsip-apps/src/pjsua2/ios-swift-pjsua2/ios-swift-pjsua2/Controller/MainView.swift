import SwiftUI
import AVFoundation



class AppState: ObservableObject {
    @Published var isServiceProvider: Bool = false
}

struct MainView: View {
    @StateObject private var appState = AppState()
    
    static var shared = MainView()
    @State private var showSidebar = false
    @State var selectedView: String?

    var body: some View {
        NavigationView {
            ZStack(alignment: .leading) {
                _MainView(selectedView: $selectedView)

                if showSidebar {
                    Sidebar(showSidebar: $showSidebar, selectedView: $selectedView)
                        .transition(.move(edge: .leading))
                }
            }
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
        .environmentObject(appState)
    }

    func initializeViews() {
        Task {
            AppManager.shared.initializeWeb3Service()
            
            if let count = try? AppManager.shared.web3Service?.getENSList().count, count > 0 {
                selectedView = "View2"
            } else {
                selectedView = "View1"
            }
        }
    }
}

struct Sidebar: View {
    @EnvironmentObject var appState: AppState
    @Binding var showSidebar: Bool
    @Binding var selectedView: String?
    
    var body: some View {
        VStack(alignment: .leading) {
            if appState.isServiceProvider {
                serviceProviderSidebarContent
            } else {
                regularSidebarContent
            }
            Spacer()
        }
        .frame(width: 200)
        .background(Color(.systemGray6))
        .offset(x: showSidebar ? 0 : -200)
        .animation(.default, value: showSidebar)
    }
    
    var serviceProviderSidebarContent: some View {
        Group {
            sidebarButton(title: "My Products", view: "MyProducts")
            sidebarButton(title: "My Payments", view: "MyPayments")
            sidebarButton(title: "Clients", view: "Clients")
            sidebarButton(title: "Profile", view: "View4")
        }
    }
    
    var regularSidebarContent: some View {
        Group {
            sidebarButton(title: "Shop", view: "View1")
            sidebarButton(title: "Main", view: "View2")
            sidebarButton(title: "Recent calls", view: "View3")
            sidebarButton(title: "Profile", view: "View4")
        }
    }
    
    func sidebarButton(title: String, view: String) -> some View {
        Button(action: {
            selectedView = view
            withAnimation {
                showSidebar.toggle()
            }
        }) {
            Text(title)
                .padding()
        }
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
                ProfileView()
            } else if selectedView == "MyPayments" {
                ProfileView()
            }
            else if selectedView == "MyProducts" {
                MyProductsView()
            }
            else {
                Text("Loading...")
            }
            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color(.systemBackground))
        .animation(.default, value: selectedView)
    }
}
