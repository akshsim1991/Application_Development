import SwiftUI

/// Start/stop Ride Mode and glue listener -> Assistant -> speaker.
@MainActor
final class RideController: ObservableObject {
    static let shared = RideController()

    let route = AudioRoute()
    let listener = RideListener()
    private let speaker = Speaker()

    @Published private(set) var isRiding = false
    @Published var lastHeard = ""
    @Published var lastReply = ""
    @Published var errorMessage: String?
    @Published var wakeWord: String {
        didSet { UserDefaults.standard.set(wakeWord, forKey: "wakeWord"); listener.wakeWord = wakeWord }
    }

    /// Set by the "Start Ride Mode" Siri intent; the UI starts it when the app appears.
    var pendingStart = false

    init() {
        let saved = UserDefaults.standard.string(forKey: "wakeWord") ?? "hey rider"
        wakeWord = saved
        listener.wakeWord = saved
        listener.onWake = { [weak self] in await self?.say("Yes?") }
        listener.onCommand = { [weak self] text in
            guard let self else { return }
            self.lastHeard = text
            await self.say(await Assistant.shared.reply(to: text))
        }
    }

    func toggle() { isRiding ? stop() : Task { await start() } }

    func start() async {
        guard !isRiding else { return }
        errorMessage = nil
        guard await RideListener.requestPermissions() else {
            errorMessage = "Allow Microphone and Speech Recognition in Settings > VoiceMate."
            return
        }
        do {
            try route.activate()
            LocationProvider.shared.startContinuous()
            try listener.start()
            isRiding = true
            UIApplication.shared.isIdleTimerDisabled = true
            await say("Ride mode on. Say \(wakeWord) and then a command.")
        } catch {
            errorMessage = error.localizedDescription
            stop()
        }
    }

    func stop() {
        listener.stop()
        speaker.stop()
        LocationProvider.shared.stopContinuous()
        route.deactivate()
        UIApplication.shared.isIdleTimerDisabled = false
        isRiding = false
    }

    private func say(_ text: String) async {
        lastReply = text
        await speaker.speak(text)
    }
}
