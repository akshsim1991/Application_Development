import AVFoundation
import Speech

/// Thread-safe hand-off between the audio-thread tap and the current recognition request.
final class AudioFeed: @unchecked Sendable {
    private let lock = NSLock()
    private var request: SFSpeechAudioBufferRecognitionRequest?
    private var lastAudio = Date()

    func set(_ r: SFSpeechAudioBufferRecognitionRequest?) { lock.lock(); request = r; lock.unlock() }
    func append(_ b: AVAudioPCMBuffer) {
        lock.lock(); let r = request; lastAudio = Date(); lock.unlock()
        r?.append(b)
    }
    func reset() { lock.lock(); lastAudio = Date(); lock.unlock() }
    func secondsSinceAudio() -> TimeInterval { lock.lock(); defer { lock.unlock() }; return Date().timeIntervalSince(lastAudio) }
}

private enum Heard {
    case nothing
    case goodbye
    case wakeOnly
    case command(String, Bool)   // text, explicit (addressed to the assistant)
}

/// Always-on listening through the helmet microphone.
///
/// Two states:
/// - **Standby**: only the wake phrase ("hey rider") is acted on.
/// - **Active** (after the wake phrase): you can just talk, no wake phrase needed, until you say
///   "bye rider" (or "that's all").
///
/// The audio engine runs for the whole ride (that is what lets iOS keep the app alive with the screen
/// locked); only the recognition request is recycled. A watchdog restarts the microphone and the
/// recogniser if either stops, for example after a call, an audio interruption or a helmet reconnect.
@MainActor
final class RideListener: ObservableObject {
    @Published private(set) var heard = ""
    @Published private(set) var isActive = false
    /// Shown in the UI when something needs attention ("Reconnecting microphone…").
    @Published private(set) var statusNote = ""

    var wakeWord = "hey rider"
    var onWake: (() async -> Void)?
    var onSleep: ((Bool) async -> Void)?              // true = automatic standby
    var onCommand: ((String, Bool) async -> Bool)?    // returns true if a follow-up answer is expected
    var reconfigureSession: (() throws -> Void)?

    private let recognizer = SFSpeechRecognizer(locale: .current) ?? SFSpeechRecognizer(locale: Locale(identifier: "en-US"))
    private let engine = AVAudioEngine()
    private let feed = AudioFeed()
    private var task: SFSpeechRecognitionTask?
    private var silenceTimer: Timer?
    private var cycleTimer: Timer?
    private var watchdog: Timer?
    private var cycleID = 0
    private var cycleStartedAt = Date()
    private var running = false
    private var paused = false
    private var recovering = false
    private var failStreak = 0
    private var preferOnDevice = true
    private var awaitingUntil = Date.distantPast
    private var lastActivity = Date()
    private var observers: [NSObjectProtocol] = []

    static func requestPermissions() async -> Bool {
        let speech = await withCheckedContinuation { (c: CheckedContinuation<Bool, Never>) in
            SFSpeechRecognizer.requestAuthorization { c.resume(returning: $0 == .authorized) }
        }
        let mic = await AVAudioApplication.requestRecordPermission()
        return speech && mic
    }

    // MARK: Start / stop

    func start() throws {
        guard !running else { return }
        guard let recognizer, recognizer.isAvailable else {
            throw NSError(domain: "VoiceMate", code: 1, userInfo: [NSLocalizedDescriptionKey: "Speech recognition is not available. Check your internet and Settings > Siri & Search."])
        }
        try startEngine()
        running = true
        paused = false
        isActive = false
        preferOnDevice = true
        failStreak = 0
        statusNote = ""
        let nc = NotificationCenter.default
        observers = [
            nc.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: .main) { [weak self] _ in
                Task { @MainActor in self?.recoverEngine(reason: "Audio route changed") }
            },
            nc.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: .main) { [weak self] n in
                let type = (n.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt).flatMap(AVAudioSession.InterruptionType.init)
                Task { @MainActor in
                    if type == .began { self?.statusNote = "Audio interrupted (call or another app)…" }
                    if type == .ended { self?.recoverEngine(reason: "Audio interruption ended") }
                }
            },
            nc.addObserver(forName: AVAudioSession.mediaServicesWereResetNotification, object: nil, queue: .main) { [weak self] _ in
                Task { @MainActor in self?.recoverEngine(reason: "Audio system restarted") }
            }
        ]
        watchdog = Timer.scheduledTimer(withTimeInterval: 4, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.watchdogTick() }
        }
        feed.reset()
        startCycle()
    }

    func stop() {
        running = false
        isActive = false
        statusNote = ""
        watchdog?.invalidate(); watchdog = nil
        observers.forEach(NotificationCenter.default.removeObserver)
        observers = []
        endCycle()
        engine.stop()
        engine.inputNode.removeTap(onBus: 0)
    }

    /// Next utterance is treated as a command even without the wake phrase (for yes/no answers in standby).
    func expectFollowUp() { awaitingUntil = Date().addingTimeInterval(8) }

    // MARK: Engine

    private func startEngine() throws {
        let input = engine.inputNode
        input.removeTap(onBus: 0)
        Self.installTap(input, feed)
        engine.prepare()
        try engine.start()
    }

    private nonisolated static func installTap(_ input: AVAudioInputNode, _ feed: AudioFeed) {
        input.installTap(onBus: 0, bufferSize: 1024, format: input.outputFormat(forBus: 0)) { buffer, _ in
            feed.append(buffer)
        }
    }

    /// Helmet reconnected, call ended, route changed: rebuild the engine and carry on.
    private func recoverEngine(reason: String) {
        guard running, !recovering else { return }
        recovering = true
        statusNote = "\(reason). Reconnecting microphone…"
        endCycle()
        engine.stop()
        engine.inputNode.removeTap(onBus: 0)
        Task {
            var ok = false
            for attempt in 0..<12 {
                guard running else { break }
                try? reconfigureSession?()
                do { try startEngine(); ok = true; break }
                catch { try? await Task.sleep(for: .seconds(Double(min(2 + attempt, 6)))) }
            }
            feed.reset()
            recovering = false
            guard running else { return }
            if ok {
                statusNote = ""
                if !paused { startCycle() }
            } else {
                statusNote = "Microphone unavailable. End the ride and start again."
            }
        }
    }

    private func watchdogTick() {
        guard running, !recovering else { return }
        if !engine.isRunning { recoverEngine(reason: "Microphone stopped"); return }
        if feed.secondsSinceAudio() > 6 { recoverEngine(reason: "No audio from microphone"); return }
        if !paused, task == nil { startCycle() }
        let limit = Prefs.activeTimeoutMinutes
        if limit > 0, isActive, !paused, Date().timeIntervalSince(lastActivity) > Double(limit * 60) {
            goStandby(auto: true)
        }
    }

    // MARK: Recognition cycles

    private func startCycle() {
        guard running, !paused, !recovering, let recognizer else { return }
        task?.cancel()
        cycleTimer?.invalidate()
        let request = SFSpeechAudioBufferRecognitionRequest()
        request.shouldReportPartialResults = true
        request.taskHint = .dictation
        if preferOnDevice, recognizer.supportsOnDeviceRecognition { request.requiresOnDeviceRecognition = true }
        feed.set(request)
        heard = ""
        cycleID += 1
        cycleStartedAt = Date()
        let id = cycleID
        task = recognizer.recognitionTask(with: request) { [weak self] result, error in
            Task { @MainActor in
                guard let self, id == self.cycleID else { return }
                if let result {
                    self.failStreak = 0
                    if !self.recovering, !self.statusNote.isEmpty { self.statusNote = "" }
                    self.heard = result.bestTranscription.formattedString
                    if result.isFinal { self.utteranceEnded() } else { self.armSilence(1.3) }
                } else if let error {
                    self.recognitionFailed(error)
                }
            }
        }
        // Recognition requests are time-limited, so recycle before the limit.
        cycleTimer = Timer.scheduledTimer(withTimeInterval: 50, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.restartCycle() }
        }
    }

    private func recognitionFailed(_ error: Error) {
        // Quick failures mean recognition itself is broken (not just "no speech for a while").
        if Date().timeIntervalSince(cycleStartedAt) < 2 { failStreak += 1 } else { failStreak = 0 }
        if failStreak >= 3 {
            if preferOnDevice {
                preferOnDevice = false
                failStreak = 0
                statusNote = "Switching to online speech recognition…"
            } else {
                let msg = error.localizedDescription
                statusNote = msg.contains("Siri and Dictation")
                    ? "Turn on Siri or Dictation: iPhone Settings > Siri & Search, and General > Keyboard > Enable Dictation."
                    : "Speech recognition problem: \(msg). Check your internet."
            }
        }
        restartCycle()
    }

    private func endCycle() {
        cycleID += 1
        silenceTimer?.invalidate(); silenceTimer = nil
        cycleTimer?.invalidate(); cycleTimer = nil
        feed.set(nil)
        task?.cancel(); task = nil
    }

    private func restartCycle() {
        endCycle()
        guard running, !paused, !recovering else { return }
        let delay: Duration = failStreak >= 3 ? .seconds(2) : .milliseconds(400)
        Task { try? await Task.sleep(for: delay); self.startCycle() }
    }

    private func armSilence(_ s: TimeInterval) {
        silenceTimer?.invalidate()
        silenceTimer = Timer.scheduledTimer(withTimeInterval: s, repeats: false) { [weak self] _ in
            Task { @MainActor in self?.utteranceEnded() }
        }
    }

    // MARK: Wake phrase, goodbye, conversation state

    private var assistantName: String { wakeWord.lowercased().split(separator: " ").last.map(String.init) ?? "rider" }

    /// Finds "hey rider" (also "hi rider", "a rider", ... as speech recognition often hears it).
    private func stripWake(_ text: String) -> (Bool, String) {
        let phrase = wakeWord.lowercased().trimmingCharacters(in: .whitespaces)
        var found: Range<String.Index>?
        if !phrase.isEmpty, let r = text.range(of: phrase, options: .backwards) {
            found = r
        } else {
            var leads: Set<String> = ["hey", "hi", "hay", "hello", "okay", "ok", "a"]
            let words = phrase.split(separator: " ").map(String.init)
            if words.count > 1 { leads.insert(words[0]) }
            let leadPattern = leads.map { NSRegularExpression.escapedPattern(for: $0) }.joined(separator: "|")
            let pattern = "\\b(?:\(leadPattern))[\\s,.-]+\(NSRegularExpression.escapedPattern(for: assistantName))\\b"
            if let re = try? NSRegularExpression(pattern: pattern),
               let m = re.matches(in: text, range: NSRange(text.startIndex..., in: text)).last,
               let r = Range(m.range, in: text) { found = r }
        }
        guard let r = found else { return (false, text) }
        let rest = text[r.upperBound...].trimmingCharacters(in: CharacterSet.whitespacesAndNewlines.union(.punctuationCharacters))
        return (true, rest)
    }

    private func isGoodbye(_ t: String) -> Bool {
        let tokens = Set(t.split(whereSeparator: { !($0.isLetter || $0.isNumber || $0 == "'") }).map(String.init))
        let byes: Set<String> = ["bye", "by", "buy", "goodbye", "bye-bye"]
        if !tokens.isDisjoint(with: byes), tokens.contains(assistantName) { return true }
        if isActive {
            if tokens.contains("goodbye") || (tokens.contains("bye") && tokens.count <= 3) { return true }
            for p in ["that's all", "that is all", "stop listening", "go to sleep", "go to standby", "standby"] where t.contains(p) { return true }
        }
        return false
    }

    private func goStandby(auto: Bool) {
        guard isActive else { return }
        isActive = false
        awaitingUntil = .distantPast
        paused = true
        endCycle()
        Task {
            await onSleep?(auto)
            paused = false
            if running { startCycle() }
        }
    }

    private func utteranceEnded() {
        let text = heard.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        let (hadWake, rest) = stripWake(text)
        let following = Date() < awaitingUntil
        awaitingUntil = .distantPast

        var action = Heard.nothing
        if (isActive || hadWake) && isGoodbye(text) {
            action = .goodbye
        } else if hadWake {
            action = rest.isEmpty ? .wakeOnly : .command(rest, true)
        } else if (isActive || following) && !text.isEmpty {
            action = .command(text, following)
        }
        if case .nothing = action { restartCycle(); return }

        // Stop listening while we speak so the app never hears itself.
        paused = true
        endCycle()
        Task {
            switch action {
            case .nothing:
                break
            case .goodbye:
                isActive = false
                await onSleep?(false)
            case .wakeOnly:
                isActive = true
                lastActivity = Date()
                await onWake?()
            case .command(let c, let explicit):
                if hadWake { isActive = true }
                lastActivity = Date()
                if await onCommand?(c, explicit) == true { awaitingUntil = Date().addingTimeInterval(8) }
                lastActivity = Date()
            }
            paused = false
            if running { startCycle() }
        }
    }
}
