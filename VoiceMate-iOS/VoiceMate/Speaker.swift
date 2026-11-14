import AVFoundation

/// Speaks text through the current audio route (your Bluetooth headphones).
@MainActor
final class Speaker: NSObject, AVSpeechSynthesizerDelegate {
    private let synth = AVSpeechSynthesizer()
    private var continuation: CheckedContinuation<Void, Never>?

    override init() {
        super.init()
        synth.delegate = self
    }

    /// Returns when speech has finished (or was cancelled).
    func speak(_ text: String) async {
        stop()
        let utterance = AVSpeechUtterance(string: text)
        if !Prefs.voiceID.isEmpty, let v = AVSpeechSynthesisVoice(identifier: Prefs.voiceID) {
            utterance.voice = v
        } else {
            utterance.voice = AVSpeechSynthesisVoice(language: Locale.current.identifier.replacingOccurrences(of: "_", with: "-"))
                ?? AVSpeechSynthesisVoice(language: "en-US")
        }
        utterance.rate = Prefs.speechRate
        await withCheckedContinuation { (c: CheckedContinuation<Void, Never>) in
            continuation = c
            synth.speak(utterance)
        }
    }

    func stop() {
        if synth.isSpeaking { synth.stopSpeaking(at: .immediate) }
        finish()
    }

    private func finish() {
        continuation?.resume()
        continuation = nil
    }

    nonisolated func speechSynthesizer(_ s: AVSpeechSynthesizer, didFinish u: AVSpeechUtterance) {
        Task { @MainActor in self.finish() }
    }

    nonisolated func speechSynthesizer(_ s: AVSpeechSynthesizer, didCancel u: AVSpeechUtterance) {
        Task { @MainActor in self.finish() }
    }
}

struct VoiceOption: Identifiable {
    let id: String
    let name: String
}

extension Speaker {
    /// Installed voices for the phone's language, best quality first. Download "Enhanced" voices in
    /// Settings > Accessibility > Spoken Content > Voices for a more natural sound.
    static func voiceOptions() -> [VoiceOption] { cachedVoices }

    private static let cachedVoices: [VoiceOption] = {
        let lang = Locale.current.language.languageCode?.identifier ?? "en"
        return AVSpeechSynthesisVoice.speechVoices()
            .filter { $0.language.hasPrefix(lang) }
            .sorted { $0.quality.rawValue > $1.quality.rawValue }
            .map { VoiceOption(id: $0.identifier, name: "\($0.name) (\($0.language))" + ($0.quality.rawValue > 1 ? " - enhanced" : "")) }
    }()
}
