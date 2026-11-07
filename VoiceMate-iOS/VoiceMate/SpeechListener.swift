import AVFoundation
import Speech

enum ListenError: LocalizedError {
    case notAuthorized, recognizerUnavailable
    var errorDescription: String? {
        switch self {
        case .notAuthorized: return "Microphone or speech recognition permission was denied. Enable both in Settings > VoiceMate."
        case .recognizerUnavailable: return "Speech recognition is not available right now."
        }
    }
}

/// Live speech-to-text from the active input (the Bluetooth headset mic when connected).
/// Finishes automatically after a short silence.
@MainActor
final class SpeechListener: ObservableObject {
    @Published private(set) var transcript = ""
    @Published private(set) var isListening = false

    private let recognizer = SFSpeechRecognizer(locale: .current) ?? SFSpeechRecognizer(locale: Locale(identifier: "en-US"))
    private let engine = AVAudioEngine()
    private var request: SFSpeechAudioBufferRecognitionRequest?
    private var task: SFSpeechRecognitionTask?
    private var silenceTimer: Timer?
    private var completion: ((String) -> Void)?

    static func requestPermissions() async -> Bool {
        let speech = await withCheckedContinuation { (c: CheckedContinuation<Bool, Never>) in
            SFSpeechRecognizer.requestAuthorization { c.resume(returning: $0 == .authorized) }
        }
        let mic = await AVAudioApplication.requestRecordPermission()
        return speech && mic
    }

    /// Starts listening; `onFinished` is called once with the final text (possibly empty).
    func start(onFinished: @escaping (String) -> Void) throws {
        stop()
        guard let recognizer, recognizer.isAvailable else { throw ListenError.recognizerUnavailable }

        let request = SFSpeechAudioBufferRecognitionRequest()
        request.shouldReportPartialResults = true
        self.request = request
        completion = onFinished
        transcript = ""

        try Self.installTap(on: engine, feeding: request)
        engine.prepare()
        try engine.start()
        isListening = true
        armSilenceTimer(seconds: 6) // generous wait for the first word

        task = recognizer.recognitionTask(with: request) { [weak self] result, error in
            Task { @MainActor in
                guard let self else { return }
                if let result {
                    self.transcript = result.bestTranscription.formattedString
                    if result.isFinal { self.finish(); return }
                    self.armSilenceTimer(seconds: 1.5)
                }
                if error != nil { self.finish() }
            }
        }
    }

    func stop() {
        guard isListening || task != nil else { return }
        completion = nil
        teardown()
    }

    // Built outside the main actor so the audio-thread callback has no isolation requirements.
    private nonisolated static func installTap(on engine: AVAudioEngine, feeding request: SFSpeechAudioBufferRecognitionRequest) throws {
        let input = engine.inputNode
        input.removeTap(onBus: 0)
        input.installTap(onBus: 0, bufferSize: 1024, format: input.outputFormat(forBus: 0)) { buffer, _ in
            request.append(buffer)
        }
    }

    private func armSilenceTimer(seconds: TimeInterval) {
        silenceTimer?.invalidate()
        silenceTimer = Timer.scheduledTimer(withTimeInterval: seconds, repeats: false) { [weak self] _ in
            Task { @MainActor in self?.finish() }
        }
    }

    private func finish() {
        guard isListening else { return }
        let text = transcript.trimmingCharacters(in: .whitespacesAndNewlines)
        let done = completion
        completion = nil
        teardown()
        done?(text)
    }

    private func teardown() {
        silenceTimer?.invalidate()
        silenceTimer = nil
        if engine.isRunning { engine.stop() }
        engine.inputNode.removeTap(onBus: 0)
        request?.endAudio()
        task?.cancel()
        request = nil
        task = nil
        isListening = false
    }
}
