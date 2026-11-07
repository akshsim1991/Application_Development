import SwiftUI

@main
struct VoiceMateApp: App {
    @StateObject private var controller = ConversationController.shared

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(controller)
        }
    }
}
