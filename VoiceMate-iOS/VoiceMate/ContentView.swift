import SwiftUI

struct ContentView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var route = RideController.shared.route
    @ObservedObject private var listener = RideController.shared.listener
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        VStack(spacing: 20) {
            Label(route.isBluetooth ? "Helmet connected: \(route.description)" : "Helmet not connected (using \(route.description))",
                  systemImage: route.isBluetooth ? "headphones" : "exclamationmark.triangle")
                .font(.footnote.weight(.medium))
                .padding(.horizontal, 12).padding(.vertical, 6)
                .background(Capsule().fill(route.isBluetooth ? Color.green.opacity(0.25) : Color.orange.opacity(0.25)))

            // Large target, easy to hit with gloves.
            Button(action: ride.toggle) {
                VStack(spacing: 8) {
                    Image(systemName: ride.isRiding ? "stop.fill" : "figure.outdoor.cycle").font(.system(size: 56))
                    Text(ride.isRiding ? "STOP RIDE MODE" : "START RIDE MODE").font(.title2.bold())
                }
                .foregroundStyle(.white)
                .frame(maxWidth: .infinity, minHeight: 200)
                .background(RoundedRectangle(cornerRadius: 28).fill(ride.isRiding ? Color.red : Color.green))
            }

            if ride.isRiding {
                Text("Listening. Say “\(ride.wakeWord)” then a command.").font(.headline).multilineTextAlignment(.center)
                if !listener.heard.isEmpty { Text(listener.heard).foregroundStyle(.secondary) }
            }
            if !ride.lastHeard.isEmpty { Text("You: \(ride.lastHeard)") }
            if !ride.lastReply.isEmpty { Text("VoiceMate: \(ride.lastReply)").bold() }
            if let e = ride.errorMessage { Text(e).font(.footnote).foregroundStyle(.red) }

            Spacer()

            HStack {
                Text("Wake phrase").font(.footnote)
                TextField("hey rider", text: $ride.wakeWord)
                    .textFieldStyle(.roundedBorder)
                    .textInputAutocapitalization(.never).autocorrectionDisabled()
                    .disabled(ride.isRiding)
            }
            Text("Commands: speed · where am I · heading · time · battery · play · pause · next song · repeat · help")
                .font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.center)
        }
        .padding()
        .onChange(of: scenePhase) { _, p in startIfRequested(p) }
        .onAppear { startIfRequested(scenePhase) }
    }

    private func startIfRequested(_ phase: ScenePhase) {
        guard phase == .active, ride.pendingStart else { return }
        ride.pendingStart = false
        Task { await ride.start() }
    }
}
