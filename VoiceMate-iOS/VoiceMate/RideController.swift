import SwiftUI

enum RidePhase { case off, listening, thinking, speaking }

struct LogEntry: Identifiable {
    let id = UUID()
    let isUser: Bool
    let text: String
}

/// Start/stop Ride Mode and glue listener -> Assistant -> speaker.
@MainActor
final class RideController: ObservableObject {
    static let shared = RideController()

    let route = AudioRoute()
    let listener = RideListener()
    private let speaker = Speaker()
    private var speechTail: Task<Void, Never>?

    @Published private(set) var isRiding = false
    @Published private(set) var phase: RidePhase = .off
    @Published private(set) var log: [LogEntry] = []
    @Published var errorMessage: String?
    @Published var isSearching = false
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
            self.add(user: true, text)
            self.phase = .thinking
            let r = await Assistant.shared.handle(text)
            await self.say(r.text)
            return r.followUp
        }
        Navigator.shared.speak = { [weak self] text in await self?.say(text) }
        TripRecorder.shared.speak = { [weak self] text in await self?.say(text) }
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
            TripRecorder.shared.start()
            isRiding = true
            phase = .listening
            UIApplication.shared.isIdleTimerDisabled = Prefs.keepScreenOn
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
        TripRecorder.shared.stop()
        listener.stop()
        speaker.stop()
        LocationProvider.shared.stopContinuous()
        route.deactivate()
        UIApplication.shared.isIdleTimerDisabled = false
        isRiding = false
        phase = .off
    }

    /// Used by the quick-action buttons: runs a command as if you had said it.
    func runCommand(_ text: String) async {
        add(user: true, text)
        phase = .thinking
        let r = await Assistant.shared.handle(text)
        await say(r.text)
        if r.followUp, isRiding { listener.expectFollowUp() }
    }

    /// Navigate from a typed destination (starts Ride Mode first if needed).
    func navigate(toQuery q: String) async {
        let query = q.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !query.isEmpty else { return }
        isSearching = true
        defer { isSearching = false }
        if !isRiding { await start() }
        guard isRiding else { return }
        add(user: true, "Navigate to \(query)")
        await say(await Assistant.shared.requestNavigation(to: query, autoStart: true).text)
    }

    /// Navigate to a saved or recent place (starts Ride Mode first if needed).
    func navigate(to place: Place) async {
        isSearching = true
        defer { isSearching = false }
        if !isRiding { await start() }
        guard isRiding else { return }
        add(user: true, "Navigate to \(place.name)")
        await say(await Assistant.shared.startNavigation(to: place).text)
    }

    private func handlePendingDestination() async {
        guard let dest = pendingDestination else { return }
        pendingDestination = nil
        let r = await Assistant.shared.requestNavigation(to: dest)
        await say(r.text)
        if r.followUp { listener.expectFollowUp() }
    }

    private func add(user: Bool, _ text: String) {
        log.append(LogEntry(isUser: user, text: text))
        if log.count > 30 { log.removeFirst(log.count - 30) }
    }

    /// Speech is queued so navigation prompts never cut off an answer (or each other).
    func say(_ text: String) async {
        add(user: false, text)
        let previous = speechTail
        let task = Task { [weak self] in
            await previous?.value
            self?.phase = .speaking
            await self?.speaker.speak(text)
        }
        speechTail = task
        await task.value
        if speechTail == task { phase = isRiding ? .listening : .off }
    }
}
