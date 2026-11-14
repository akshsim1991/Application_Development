import AVFoundation

/// Speech through AVSpeechSynthesizer using the voices installed on the iPhone.
@MainActor
final class AppleSpeechEngine: NSObject, SpeechEngine, AVSpeechSynthesizerDelegate {
    let name = "Apple system voices"
    let isOnDevice = true

    private let synth = AVSpeechSynthesizer()
    private var continuation: CheckedContinuation<Void, Never>?
    private var pending: Set<ObjectIdentifier> = []
    private var lockedVoice: AVSpeechSynthesisVoice?

    override init() {
        super.init()
        synth.delegate = self
    }

    func prepareForRide() { lockedVoice = VoiceStore.shared.resolveVoice() }

    func speak(_ text: String, kind: SpeechKind) async {
        await speak(text, kind: kind, voiceID: nil)
    }

    /// `voiceID` is used by the voice gallery to preview a voice without selecting it.
    func speak(_ text: String, kind: SpeechKind, voiceID: String?) async {
        stop()
        let store = VoiceStore.shared
        let chunks = PhraseFormatter.prepare(text, overrides: store.pronunciations)
        guard !chunks.isEmpty else { return }
        let voice: AVSpeechSynthesisVoice?
        if let voiceID { voice = AVSpeechSynthesisVoice(identifier: voiceID) }
        else { voice = lockedVoice ?? store.resolveVoice() }

        // Urgent warnings are a touch slower, lower and louder: prominent, not shouted.
        let urgent = kind == .urgent
        let rate = Float(store.rate) * (urgent ? 0.95 : 1)
        let pitch = Float(store.pitch) + (urgent ? 0.03 : 0)

        await withCheckedContinuation { (c: CheckedContinuation<Void, Never>) in
            continuation = c
            for chunk in chunks {
                let u = AVSpeechUtterance(string: chunk.text)
                u.voice = voice
                u.rate = max(AVSpeechUtteranceMinimumSpeechRate, min(AVSpeechUtteranceMaximumSpeechRate, rate))
                u.pitchMultiplier = max(0.5, min(2.0, pitch))
                u.volume = urgent ? 1.0 : 0.95
                u.preUtteranceDelay = 0
                u.postUtteranceDelay = store.pause * chunk.pauseFactor
                pending.insert(ObjectIdentifier(u))
                synth.speak(u)
            }
        }
    }

    func stop() {
        pending.removeAll()   // late callbacks from cancelled utterances are then ignored
        if synth.isSpeaking { synth.stopSpeaking(at: .immediate) }
        finish()
    }

    private func finish() {
        continuation?.resume()
        continuation = nil
    }

    private func done(_ id: ObjectIdentifier) {
        guard pending.remove(id) != nil else { return }
        if pending.isEmpty { finish() }
    }

    nonisolated func speechSynthesizer(_ s: AVSpeechSynthesizer, didFinish u: AVSpeechUtterance) {
        let id = ObjectIdentifier(u)
        Task { @MainActor in self.done(id) }
    }

    nonisolated func speechSynthesizer(_ s: AVSpeechSynthesizer, didCancel u: AVSpeechUtterance) {
        let id = ObjectIdentifier(u)
        Task { @MainActor in self.done(id) }
    }
}
