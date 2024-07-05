import SwiftUI
import Combine


class RecentCallsViewModel: ObservableObject {
    @Published var recentCalls: [CallInfo] = []
    
    init() {
        fetchRecentCalls()
    }
    
    func fetchRecentCalls() {
        self.recentCalls = AppManager.shared.getRecentCalls()
    }
    
    func addRecentCall(call: CallInfo) {
        AppManager.shared.addRecentCall(call: call)
    }
}

struct RecentCalls: View {
    static var shared = RecentCalls()
    @StateObject private var viewModel = RecentCallsViewModel()
    @State private var selectedCall: CallInfo?
    
    var body: some View {
        NavigationView {
            ScrollView {
                VStack {
                    ForEach(viewModel.recentCalls) { call in
                        Button(action: {
                            selectedCall = call
                        }) {
                            Text(call.caller)
                                .padding()
                                .background(Color.blue)
                                .foregroundColor(.white)
                                .cornerRadius(8)
                        }
                        .padding(.vertical, 4)
                    }
                }
                .padding()
            }
            .navigationTitle("Recent Calls")
            .sheet(item: $selectedCall) { call in
                CallDetailView(call: call)
            }
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: {
                        addNewCall()
                    }) {
                        Image(systemName: "plus")
                    }
                }
            }
        }
    }
    
    func addNewCall() {
        let newCall = CallInfo(caller: "New Caller", callee:"callee", timestamp: Date(), callDuration: 120)
        viewModel.addRecentCall(call: newCall)
    }
}

struct CallDetailView: View {
    var call: CallInfo
    
    var body: some View {
        VStack {
            Text("Caller: \(call.caller)")
                .font(.title)
            Text("Timestamp: \(call.timestamp)")
                .font(.subheadline)
            Text("Duration: \(call.callDuration) seconds")
                .font(.subheadline)
            Spacer()
        }
        .padding()
    }
}
