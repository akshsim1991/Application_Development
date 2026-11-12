import SwiftUI

/// Start/stop Ride Mode and glue listener -> Assistant -> speaker.
@MainActor
final class RideController: ObservableObject {
    static let shared = RideController()

    let route = AudioRoute()
    let listener = RideListener()
    private let speaker = Speaker()
    private var speechTail: Task<Void, Never>?

    @Published private(set) var isRiding = false
    @Published var lastHeard = ""
    @Published var lastReply = ""
    @Published var errorMessage: String?
    @Published var wakeWord: String {
        didSet { UserDefaults.standard.set(wakeWord, forKey: "wakeWord"); listener.wakeWord = wakeWord }
    }

    /// Set by Siri intents; the UI acts on them when the app appears.
    var pendingStart = false
    var pendingDestination: String?

    init() {
        let saved = UserDefaults.standard.string(forKey: "wakeWord") ?? "hey rider"
        wakeWord = saved
        listener.wakeWord = saved
        listener.onWake = { [weak self] in await self?.say("Yes?") }
        listener.onCommand = { [weak self] text in
            guard let self else { return false }
            self.lastHeard = text
            let r = await Assistant.shared.handle(text)
            await self.say(r.text)
            return r.followUp
        }
        Navigator.shared.speak = { [weak self] text in await self?.say(text) }
    }

    func toggle() {
        if isRiding { stop() } else { Task { await start() } }
    }

    func start() async {
        guard !isRiding else { await handlePendingDestination(); return }
        errorMessage = nil
        guard await RideListener.requestPermissions() else {
            errorMessage = "Allow Microphone and Speech Recognition in Settings > VoiceMate."
            return
        }
        do {
            try route.activate()
            LocationProvider.shared.startContinuous()
            isRiding = true
            UIApplication.shared.isIdleTimerDisabled = true
            // Spoken before listening starts so the app never hears its own announcement.
            await say("Ride mode on.")
            try listener.start()
            await handlePendingDestination()
        } catch {
            errorMessage = error.localizedDescription
            stop()
        }
    }

    func stop() {
        Navigator.shared.stop()
        listener.stop()
        speaker.stop()
        LocationProvider.shared.stopContinuous()
        route.deactivate()
        UIApplication.shared.isIdleTimerDisabled = false
        isRiding = false
    }

    private func handlePendingDestination() async {
        guard let dest = pendingDestination else { return }
        pendingDestination = nil
        let r = await Assistant.shared.requestNavigation(to: dest)
        await say(r.text)
        if r.followUp { listener.expectFollowUp() }
    }

    /// Speech is queued so navigation prompts never cut off an answer (or each other).
    private func say(_ text: String) async {
        lastReply = text
        let previous = speechTail
        let task = Task { await previous?.value; await speaker.speak(text) }
        speechTail = task
        await task.value
    }
}
