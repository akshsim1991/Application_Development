import SwiftUI

struct ContentView: View {
    @EnvironmentObject var ride: RideController
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        TabView {
            RideView().tabItem { Label("Ride", systemImage: "figure.outdoor.cycle") }
            NavigateView().tabItem { Label("Navigate", systemImage: "location.north.line.fill") }
            TripsView().tabItem { Label("Trips", systemImage: "chart.bar.xaxis") }
            SettingsView().tabItem { Label("Settings", systemImage: "gearshape.fill") }
        }
        .tint(Theme.accent)
        .preferredColorScheme(.dark)
        .onChange(of: scenePhase) { _, p in startIfRequested(p) }
        .onAppear { startIfRequested(scenePhase) }
    }

    private func startIfRequested(_ phase: ScenePhase) {
        guard phase == .active, ride.pendingStart || ride.pendingDestination != nil else { return }
        ride.pendingStart = false
        Task { await ride.start() }
    }
}
