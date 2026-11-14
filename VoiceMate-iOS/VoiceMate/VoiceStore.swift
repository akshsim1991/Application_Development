import AVFoundation

enum Verbosity: String, CaseIterable, Identifiable {
    case concise, normal, detailed
    var id: String { rawValue }
    var title: String {
        switch self {
        case .concise: return "Concise"
        case .normal: return "Normal"
        case .detailed: return "Detailed"
        }
    }
}

/// A speaking-style configuration (rate, pitch, pause, how much it says). Not a promise of emotion:
/// system voices only support rate, pitch, volume and pauses.
struct VoicePreset: Identifiable, Hashable {
    let id: String
    let name: String
    let blurb: String
    let rate: Double
    let pitch: Double
    let pause: Double
    let verbosity: Verbosity
}

struct VoiceInfo: Identifiable {
    let id: String
    let name: String
    let language: String
    let accent: String
    let quality: String
    let qualityRank: Int
    let gender: String
    let source: String
    let isPersonal: Bool
}

/// Voice choice, speaking style, pronunciation overrides: everything that shapes how the app sounds.
/// Saved between rides.
@MainActor
final class VoiceStore: ObservableObject {
    static let shared = VoiceStore()

    static let rateRange = 0.38...0.60
    static let pitchRange = 0.85...1.15
    static let pauseRange = 0.0...0.6

    static let presets: [VoicePreset] = [
        VoicePreset(id: "calm", name: "Calm Navigator", blurb: "Unhurried, steady, clear pauses.",
                    rate: 0.46, pitch: 1.0, pause: 0.30, verbosity: .normal),
        VoicePreset(id: "friendly", name: "Friendly Co-rider", blurb: "Relaxed and chatty, tells you a bit more.",
                    rate: 0.50, pitch: 1.04, pause: 0.20, verbosity: .detailed),
        VoicePreset(id: "concise", name: "Concise Professional", blurb: "Short and brisk. Only what you need.",
                    rate: 0.52, pitch: 0.98, pause: 0.10, verbosity: .concise),
        VoicePreset(id: "warm", name: "Warm Guide", blurb: "Soft and gentle, a little slower.",
                    rate: 0.47, pitch: 1.04, pause: 0.35, verbosity: .normal),
        VoicePreset(id: "energetic", name: "Energetic Touring Companion", blurb: "Upbeat and quick, with early heads-ups.",
                    rate: 0.54, pitch: 1.08, pause: 0.15, verbosity: .detailed)
    ]

    /// Realistic navigation sentences for previews.
    static let samples = [
        "In five hundred metres, turn left.",
        "Stay on the main road, not the service road.",
        "At the roundabout, take the second exit and turn right.",
        "You have missed the turn. Finding another route.",
        "Continue straight on NH 48 for twelve kilometres."
    ]

    @Published var voiceID: String { didSet { save("voiceID", voiceID) } }
    @Published var presetID: String { didSet { save("voicePreset", presetID) } }
    @Published var rate: Double { didSet { rate = clamp(rate, Self.rateRange); save("speechRate", rate) } }
    @Published var pitch: Double { didSet { pitch = clamp(pitch, Self.pitchRange); save("voicePitch", pitch) } }
    @Published var pause: Double { didSet { pause = clamp(pause, Self.pauseRange); save("voicePause", pause) } }
    @Published var verbosity: Verbosity { didSet { save("verbosity", verbosity.rawValue) } }
    @Published var pronunciations: [Pronunciation] { didSet { savePronunciations() } }
    /// Set when the chosen voice wasn't available and a similar one was used.
    @Published private(set) var fallbackNote: String?

    private let d = UserDefaults.standard

    init() {
        let d = UserDefaults.standard
        voiceID = d.string(forKey: "voiceID") ?? ""
        presetID = d.string(forKey: "voicePreset") ?? "calm"
        rate = d.object(forKey: "speechRate") as? Double ?? 0.46
        pitch = d.object(forKey: "voicePitch") as? Double ?? 1.0
        pause = d.object(forKey: "voicePause") as? Double ?? 0.25
        verbosity = Verbosity(rawValue: d.string(forKey: "verbosity") ?? "") ?? .normal
        if let data = d.data(forKey: "pronunciations"),
           let v = try? JSONDecoder().decode([Pronunciation].self, from: data) { pronunciations = v } else { pronunciations = [] }
    }

    func apply(_ p: VoicePreset) {
        presetID = p.id
        rate = p.rate
        pitch = p.pitch
        pause = p.pause
        verbosity = p.verbosity
    }

    // MARK: Voices

    /// Voices installed on this iPhone. (Voices that need downloading aren't listed by iOS.)
    static func catalogue() -> [VoiceInfo] {
        AVSpeechSynthesisVoice.speechVoices().map { v in
            let q: (String, Int)
            switch v.quality {
            case .premium: q = ("Premium", 3)
            case .enhanced: q = ("Enhanced", 2)
            default: q = ("Standard", 1)
            }
            let g: String
            switch v.gender {
            case .male: g = "Male"
            case .female: g = "Female"
            default: g = "Not specified"
            }
            let personal = v.voiceTraits.contains(.isPersonalVoice)
            return VoiceInfo(id: v.identifier, name: v.name, language: v.language, accent: accentName(v.language),
                             quality: q.0, qualityRank: q.1, gender: g,
                             source: personal ? "Personal Voice (on-device)" : "Apple system voice (on-device)",
                             isPersonal: personal)
        }
        .sorted { a, b in
            let ai = a.language == "en-IN", bi = b.language == "en-IN"
            if ai != bi { return ai }
            if a.qualityRank != b.qualityRank { return a.qualityRank > b.qualityRank }
            return a.name < b.name
        }
    }

    static func accentName(_ tag: String) -> String {
        let names = ["en-IN": "Indian English", "en-GB": "British English", "en-US": "American English",
                     "en-AU": "Australian English", "en-ZA": "South African English", "en-IE": "Irish English",
                     "hi-IN": "Hindi", "ta-IN": "Tamil", "te-IN": "Telugu", "kn-IN": "Kannada", "ml-IN": "Malayalam",
                     "mr-IN": "Marathi", "bn-IN": "Bengali", "gu-IN": "Gujarati", "pa-IN": "Punjabi", "ur-IN": "Urdu"]
        return names[tag] ?? Locale.current.localizedString(forIdentifier: tag) ?? tag
    }

    /// The voice to use. If the chosen one is gone (iOS update, removed download) a similar one is used
    /// and `fallbackNote` is set so the rider can be told when it is safe.
    func resolveVoice() -> AVSpeechSynthesisVoice? {
        fallbackNote = nil
        let all = AVSpeechSynthesisVoice.speechVoices()
        if !voiceID.isEmpty {
            if let v = AVSpeechSynthesisVoice(identifier: voiceID) { return v }
            let wantedLang = d.string(forKey: "voiceLanguage") ?? "en-IN"
            if let best = best(in: all, language: wantedLang) ?? best(in: all, prefix: String(wantedLang.prefix(2))) {
                fallbackNote = "Your chosen voice isn't available, so I'm using \(best.name) instead."
                return best
            }
        }
        // Nothing chosen yet: best Indian English voice, else best voice for the phone's language.
        return best(in: all, language: "en-IN")
            ?? best(in: all, prefix: Locale.current.language.languageCode?.identifier ?? "en")
            ?? AVSpeechSynthesisVoice(language: "en-IN")
            ?? AVSpeechSynthesisVoice(language: "en-US")
    }

    private func best(in voices: [AVSpeechSynthesisVoice], language: String) -> AVSpeechSynthesisVoice? {
        voices.filter { $0.language == language }.max { $0.quality.rawValue < $1.quality.rawValue }
    }

    private func best(in voices: [AVSpeechSynthesisVoice], prefix: String) -> AVSpeechSynthesisVoice? {
        voices.filter { $0.language.hasPrefix(prefix) }.max { $0.quality.rawValue < $1.quality.rawValue }
    }

    func select(_ v: VoiceInfo) {
        voiceID = v.id
        d.set(v.language, forKey: "voiceLanguage")
    }

    // MARK: Persistence

    private func clamp(_ v: Double, _ r: ClosedRange<Double>) -> Double { min(max(v, r.lowerBound), r.upperBound) }
    private func save(_ key: String, _ value: Any) { UserDefaults.standard.set(value, forKey: key) }
    private func savePronunciations() {
        if let data = try? JSONEncoder().encode(pronunciations) { UserDefaults.standard.set(data, forKey: "pronunciations") }
    }
}
