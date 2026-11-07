import SwiftUI

@main
struct VoiceMateApp: App {
    @StateObject private var ride = RideController.shared

    var body: some Scene {
        WindowGroup {
            ContentView().environmentObject(ride)
        }
    }
}
