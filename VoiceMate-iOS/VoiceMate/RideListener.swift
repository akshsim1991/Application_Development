import AVFoundation
import Speech

/// Thread-safe hand-off between the audio-thread tap and the current recognition request.
final class AudioFeed: @unchecked Sendable {
    private let lock = NSLock()
    private var request: SFSpeechAudioBufferRecognitionRequest?
    func set(_ r: SFSpeechAudioBufferRecognitionRequest?) { lock.lock(); request = r; lock.unlock() }
    func append(_ b: AVAudioPCMBuffer) { lock.lock(); let r = request; lock.unlock(); r?.append(b) }
}

/// Always-on listening through the helmet microphone. The audio engine runs for the whole ride
/// (that is what lets iOS keep the app alive with the screen locked); only the recognition
/// request is recycled. Say the wake word followed by a command, e.g. "hey rider, what's my speed".
@MainActor
final class RideListener: ObservableObject {
    @Published private(set) var heard = ""

    var wakeWord = "hey rider"
    var onWake: (() async -> Void)?
    var onCommand: ((String) async -> Void)?

    private let recognizer = SFSpeechRecognizer(locale: .current) ?? SFSpeechRecognizer(locale: Locale(identifier: "en-US"))
    private let engine = AVAudioEngine()
    private let feed = AudioFeed()
    private var task: SFSpeechRecognitionTask?
    private var silenceTimer: Timer?
    private var cycleTimer: Timer?
    private var cycleID = 0
    private var running = false
    private var paused = false
    private var awaitingUntil = Date.distantPast
    private var observers: [NSObjectProtocol] = []

    static func requestPermissions() async -> Bool {
        let speech = await withCheckedContinuation { (c: CheckedContinuation<Bool, Never>) in
            SFSpeechRecognizer.requestAuthorization { c.resume(returning: $0 == .authorized) }
        }
        let mic = await AVAudioApplication.requestRecordPermission()
        return speech && mic
    }

    func start() throws {
        guard !running else { return }
        guard let recognizer, recognizer.isAvailable else {
            throw NSError(domain: "VoiceMate", code: 1, userInfo: [NSLocalizedDescriptionKey: "Speech recognition is not available."])
        }
        try startEngine()
        running = true
        paused = false
        let nc = NotificationCenter.default
        observers = [
            nc.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: .main) { [weak self] _ in
                Task { @MainActor in self?.recoverEngine() }
            },
            nc.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: .main) { [weak self] n in
                let type = (n.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt).flatMap(AVAudioSession.InterruptionType.init)
                if type == .ended { Task { @MainActor in self?.recoverEngine() } }
            }
        ]
        startCycle()
    }

    func stop() {
        running = false
        observers.forEach(NotificationCenter.default.removeObserver)
        observers = []
        endCycle()
        engine.stop()
        engine.inputNode.removeTap(onBus: 0)
    }

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
    private func recoverEngine() {
        guard running else { return }
        endCycle()
        engine.stop()
        try? AVAudioSession.sharedInstance().setActive(true)
        do { try startEngine() } catch {
            Task { try? await Task.sleep(for: .seconds(2)); self.recoverEngine() }
            return
        }
        startCycle()
    }

    // MARK: Recognition cycles

    private func startCycle() {
        guard running, !paused, let recognizer else { return }
        let request = SFSpeechAudioBufferRecognitionRequest()
        request.shouldReportPartialResults = true
        request.taskHint = .dictation
        if recognizer.supportsOnDeviceRecognition { request.requiresOnDeviceRecognition = true }
        feed.set(request)
        heard = ""
        cycleID += 1
        let id = cycleID
        task = recognizer.recognitionTask(with: request) { [weak self] result, error in
            Task { @MainActor in
                guard let self, id == self.cycleID else { return }
                if let result {
                    self.heard = result.bestTranscription.formattedString
                    if result.isFinal { self.utteranceEnded() } else { self.armSilence(1.3) }
                } else if error != nil {
                    self.restartCycle()
                }
            }
        }
        // Recognition requests are time-limited, so recycle before the limit.
        cycleTimer = Timer.scheduledTimer(withTimeInterval: 50, repeats: false) { [weak self] _ in
            Task { @MainActor in self?.restartCycle() }
        }
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
        guard running, !paused else { return }
        Task { try? await Task.sleep(for: .milliseconds(400)); self.startCycle() }
    }

    private func armSilence(_ s: TimeInterval) {
        silenceTimer?.invalidate()
        silenceTimer = Timer.scheduledTimer(withTimeInterval: s, repeats: false) { [weak self] _ in
            Task { @MainActor in self?.utteranceEnded() }
        }
    }

    // MARK: Wake word handling

    private func utteranceEnded() {
        let text = heard.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        var command: String?
        var wakeOnly = false

        if Date() < awaitingUntil {
            command = text
        } else if let r = text.range(of: wakeWord.lowercased(), options: .backwards) {
            let after = text[r.upperBound...].trimmingCharacters(in: .whitespacesAndNewlines.union(.punctuationCharacters))
            if after.isEmpty { wakeOnly = true } else { command = after }
        }
        awaitingUntil = .distantPast

        guard (command?.isEmpty == false) || wakeOnly else { restartCycle(); return }

        // Stop listening while we speak so the app never hears itself.
        paused = true
        endCycle()
        Task {
            if wakeOnly {
                awaitingUntil = Date().addingTimeInterval(10)
                await onWake?()
                awaitingUntil = Date().addingTimeInterval(8)
            } else if let command {
                await onCommand?(command)
            }
            paused = false
            if running { startCycle() }
        }
    }
}
