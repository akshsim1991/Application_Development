import SwiftUI

struct ContentView: View {
    @EnvironmentObject var c: ConversationController
    @ObservedObject private var route = ConversationController.shared.route
    @ObservedObject private var listener = ConversationController.shared.listener
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        VStack(spacing: 24) {
            routeBadge

            Spacer()

            if listener.isListening {
                Text(listener.transcript.isEmpty ? "Listening…" : listener.transcript)
                    .font(.title3).multilineTextAlignment(.center)
            } else {
                if !c.lastHeard.isEmpty { bubble("You", c.lastHeard) }
                if !c.lastReply.isEmpty { bubble("VoiceMate", c.lastReply) }
                if c.lastHeard.isEmpty && c.lastReply.isEmpty {
                    Text("Tap the mic and talk through your headphones.\nOr say “Hey Siri, ask VoiceMate”.")
                        .multilineTextAlignment(.center).foregroundStyle(.secondary)
                }
            }

            if let e = c.errorMessage {
                Text(e).font(.footnote).foregroundStyle(.red).multilineTextAlignment(.center)
            }

            Spacer()

            Button(action: c.toggle) {
                Image(systemName: listener.isListening ? "stop.fill" : (c.isBusy ? "speaker.wave.2.fill" : "mic.fill"))
                    .font(.system(size: 40, weight: .semibold))
                    .foregroundStyle(.white)
                    .frame(width: 110, height: 110)
                    .background(Circle().fill(listener.isListening ? Color.red : Color.accentColor))
                    .shadow(radius: 8, y: 4)
            }
            .accessibilityLabel(listener.isListening ? "Stop" : "Talk")

            Toggle("Keep listening after each reply", isOn: $c.continuous)
                .font(.footnote)
        }
        .padding()
        .onChange(of: scenePhase) { _, phase in startIfRequested(phase) }
        .onAppear { startIfRequested(scenePhase) }
    }

    private var routeBadge: some View {
        Label(route.isBluetooth ? "Using \(route.description)" : "No Bluetooth headphones — using \(route.description)",
              systemImage: route.isBluetooth ? "headphones" : "iphone")
            .font(.footnote.weight(.medium))
            .padding(.horizontal, 12).padding(.vertical, 6)
            .background(Capsule().fill(route.isBluetooth ? Color.green.opacity(0.2) : Color.orange.opacity(0.2)))
    }

    private func bubble(_ who: String, _ text: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(who).font(.caption).foregroundStyle(.secondary)
            Text(text).font(.title3)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding().background(RoundedRectangle(cornerRadius: 14).fill(.quaternary))
    }

    private func startIfRequested(_ phase: ScenePhase) {
        guard phase == .active, c.pendingListen else { return }
        c.pendingListen = false
        Task { await c.beginTurn() }
    }
}
