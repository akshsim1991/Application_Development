import SwiftUI

@main
struct VoiceMateApp: App {
    @StateObject private var ride = RideController.shared

    init() {
        // Solid dark bars: no blur to compute, and the gradient shows through the navigation bar.
        let tab = UITabBarAppearance()
        tab.configureWithOpaqueBackground()
        tab.backgroundColor = UIColor(red: 0.07, green: 0.05, blue: 0.15, alpha: 1)
        UITabBar.appearance().standardAppearance = tab
        UITabBar.appearance().scrollEdgeAppearance = tab

        let nav = UINavigationBarAppearance()
        nav.configureWithTransparentBackground()
        UINavigationBar.appearance().standardAppearance = nav
        UINavigationBar.appearance().scrollEdgeAppearance = nav
    }

    var body: some Scene {
        WindowGroup {
            ContentView().environmentObject(ride)
        }
    }
}
