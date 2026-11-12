import SwiftUI

struct ContentView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var emergency = Emergency.shared
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        TabView {
            RideView().tabItem { Label("Ride", systemImage: "figure.outdoor.cycle") }
            NavigateView().tabItem { Label("Navigate", systemImage: "location.north.line.fill") }
            TripsView().tabItem { Label("Trips", systemImage: "chart.bar.xaxis") }
            GarageView().tabItem { Label("Garage", systemImage: "wrench.and.screwdriver.fill") }
            SettingsView().tabItem { Label("Settings", systemImage: "gearshape.fill") }
        }
        .tint(Theme.accent)
        .preferredColorScheme(.dark)
        .onChange(of: scenePhase) { _, p in startIfRequested(p) }
        .onAppear { startIfRequested(scenePhase) }
        .fullScreenCover(isPresented: Binding(get: { emergency.crashCountdown != nil }, set: { _ in })) {
            CrashAlertView()
        }
    }

    private func startIfRequested(_ phase: ScenePhase) {
        guard phase == .active, ride.pendingStart || ride.pendingDestination != nil else { return }
        ride.pendingStart = false
        Task { await ride.start() }
    }
}

struct CrashAlertView: View {
    @ObservedObject private var e = Emergency.shared

    var body: some View {
        ZStack {
            Color.red.ignoresSafeArea()
            VStack(spacing: 24) {
                Text("CRASH DETECTED").font(.system(size: 36, weight: .heavy, design: .rounded))
                Text("\(e.crashCountdown ?? 0)")
                    .font(.system(size: 130, weight: .heavy, design: .rounded)).monospacedDigit()
                Text("Sending an SOS with your location to your emergency contact").multilineTextAlignment(.center)
                Button {
                    e.cancelCrash()
                    Task { await RideController.shared.say("Alert cancelled.") }
                } label: {
                    Text("I'M OK")
                        .font(.system(size: 40, weight: .heavy, design: .rounded))
                        .foregroundStyle(.red)
                        .frame(maxWidth: .infinity, minHeight: 130)
                        .background(RoundedRectangle(cornerRadius: 28, style: .continuous).fill(.white))
                }
                Button("Send SOS now") { e.sendNow() }
                    .font(.headline).foregroundStyle(.white)
            }
            .foregroundStyle(.white)
            .padding()
        }
    }
}
