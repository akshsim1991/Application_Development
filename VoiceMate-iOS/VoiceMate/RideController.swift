import SwiftUI
import CoreLocation

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
    private let queue = SpeechQueue(engine: AppleSpeechEngine())
    private let previewEngine = AppleSpeechEngine()

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
        listener.onSleep = { [weak self] auto in
            guard let self else { return }
            let wake = self.wakeWord
            await self.say(auto ? "No commands for a while, going to standby. Say \(wake) when you need me."
                                : "Okay, going to standby. Say \(wake) when you need me.")
        }
        listener.onCommand = { [weak self] text, explicit in
            guard let self else { return false }
            let r = await Assistant.shared.handle(text, explicit: explicit)
            if r.silent { return false }
            self.add(user: true, text)
            self.phase = .thinking
            await self.say(r.text)
            if r.endRide { self.stop(); return false }
            return r.followUp
        }
        listener.reconfigureSession = { [weak self] in try self?.route.activate() }
        queue.onBusy = { [weak self] busy in
            guard let self else { return }
            if busy {
                self.phase = .speaking
                self.route.beginSpeaking()
            } else {
                self.route.endSpeaking()
                self.phase = self.isRiding ? .listening : .off
            }
        }
        Navigator.shared.speak = { [weak self] text in await self?.say(text, kind: .guidance) }
        TripRecorder.shared.speak = { [weak self] text in await self?.say(text, kind: .guidance) }
        TripRecorder.shared.speakUrgent = { [weak self] text in await self?.say(text, kind: .urgent) }
        Emergency.shared.speak = { [weak self] text in await self?.say(text, kind: .urgent) }
        Emergency.shared.expectReply = { [weak self] in self?.listener.expectFollowUp() }
        RoadInfo.shared.speak = { [weak self] text in await self?.say(text, kind: .guidance) }
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
            route.guidanceOnly = !Prefs.wakeEnabled
            try route.activate()
            queue.engine.prepareForRide()   // the voice is fixed for the whole ride
            LocationProvider.shared.startContinuous()
            TripRecorder.shared.start()
            Emergency.shared.rideStarted()
            RoadInfo.shared.start()
            isRiding = true
            phase = .listening
            UIApplication.shared.isIdleTimerDisabled = Prefs.keepScreenOn
            // Spoken before listening starts so the app never hears its own announcement.
            await say(Prefs.wakeEnabled ? "Ride mode on. Say \(wakeWord) when you need me." : "Ride mode on.")
            if let note = VoiceStore.shared.fallbackNote { await say(note) }
            let due = Garage.shared.dueMessages()
            if !due.isEmpty { await say(due.joined(separator: " ")) }
            if Prefs.wakeEnabled { try listener.start() }
            await handlePendingDestination()
        } catch {
            errorMessage = error.localizedDescription
            stop()
        }
    }

    func stop() {
        Navigator.shared.stop()
        Emergency.shared.rideEnded()
        RoadInfo.shared.stop()
        TripRecorder.shared.stop()
        listener.stop()
        queue.stopAll()
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

    /// Adds a stop to the current route (from the nearby list).
    func addStop(_ place: Place) async {
        guard isRiding, let loc = await LocationProvider.shared.freshLocation() else { return }
        add(user: true, "Add stop: \(place.name)")
        await say(await Navigator.shared.addStop(place, from: loc))
    }

    func skipStop() async {
        guard let loc = await LocationProvider.shared.freshLocation() else { return }
        await say(await Navigator.shared.skipStop(from: loc))
    }

    /// Follow a GPX route imported from a file.
    func followGPX(_ points: [CLLocationCoordinate2D], name: String) async {
        isSearching = true
        defer { isSearching = false }
        if !isRiding { await start() }
        guard isRiding, let loc = await LocationProvider.shared.freshLocation() else { return }
        add(user: true, "Follow route: \(name.isEmpty ? "imported GPX" : name)")
        await say(await Navigator.shared.beginRoute(points: points, name: name, from: loc))
    }

    private func handlePendingDestination() async {
        guard let dest = pendingDestination else { return }
        pendingDestination = nil
        let r = await Assistant.shared.requestNavigation(to: dest, autoStart: true)
        await say(r.text)
        if r.followUp, Prefs.wakeEnabled { listener.expectFollowUp() }
    }

    private func add(user: Bool, _ text: String) {
        log.append(LogEntry(isUser: user, text: text))
        if log.count > 30 { log.removeFirst(log.count - 30) }
    }

    /// Everything spoken goes through one queue: no overlaps, urgent first, stale guidance dropped.
    func say(_ text: String, kind: SpeechKind = .reply) async {
        if kind != .guidance { add(user: false, text) }
        await queue.say(text, kind: kind)
    }

    /// Plays a sample (voice gallery, audio test) without touching the ride queue.
    func previewVoice(_ text: String, voiceID: String? = nil) async {
        let useSession = !isRiding
        if useSession { route.preparePreview() }
        await previewEngine.speak(text, kind: .reply, voiceID: voiceID)
        if useSession { route.endPreview() }
    }

    func stopPreview() { previewEngine.stop() }
}
