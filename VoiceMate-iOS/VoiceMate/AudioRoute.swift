import AVFoundation
import Combine

/// Audio session and route manager (the "BluetoothAudioManager").
///
/// Two modes:
/// - **Hands-free** (default): `playAndRecord` so the helmet microphone can hear the wake phrase. Bluetooth
///   headsets then use the hands-free (HFP) profile, which is mono and lower quality. This is how Bluetooth
///   works; no app can avoid it while the microphone is open.
/// - **Guidance only**: `playback` / `voicePrompt`, held only while speaking, ducking music and releasing it
///   afterwards. Bluetooth keeps its high-quality stereo profile. Voice commands then go through Siri.
///
/// With no Bluetooth or wired headphones, speech goes to the iPhone's loudspeaker, never the earpiece.
@MainActor
final class AudioRoute: ObservableObject {
    @Published private(set) var description = "No audio route"
    @Published private(set) var outputKind = "Unknown"
    @Published private(set) var inputName = "—"
    @Published private(set) var isBluetooth = false
    @Published private(set) var status = ""

    var guidanceOnly = false

    private var observers: [NSObjectProtocol] = []

    init() {
        let nc = NotificationCenter.default
        observers = [
            nc.addObserver(forName: AVAudioSession.routeChangeNotification, object: nil, queue: .main) { [weak self] _ in
                Task { @MainActor in self?.routeChanged() }
            },
            nc.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: .main) { [weak self] n in
                let type = (n.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt).flatMap(AVAudioSession.InterruptionType.init)
                Task { @MainActor in self?.interruption(type) }
            }
        ]
        refresh()
    }

    deinit { observers.forEach(NotificationCenter.default.removeObserver) }

    // MARK: Hands-free mode

    /// `.defaultToSpeaker` puts sound on the loudspeaker (not the earpiece) when no Bluetooth headset or
    /// wired headphones are connected; Bluetooth still wins when present.
    func activate() throws {
        if guidanceOnly { try configureGuidance(); refresh(); return }
        let session = AVAudioSession.sharedInstance()
        try session.setCategory(.playAndRecord, mode: .default,
                                options: [.allowBluetooth, .allowBluetoothA2DP, .mixWithOthers, .defaultToSpeaker])
        try session.setActive(true)
        refresh()
        enforceSpeaker()
    }

    func deactivate() {
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        refresh()
    }

    // MARK: Guidance-only mode and previews

    private func configureGuidance() throws {
        try AVAudioSession.sharedInstance().setCategory(.playback, mode: .voicePrompt, options: [.duckOthers])
    }

    /// Called when speech starts. Only does anything in guidance-only mode.
    func beginSpeaking() {
        guard guidanceOnly else { return }
        try? configureGuidance()
        try? AVAudioSession.sharedInstance().setActive(true)
        refresh()
    }

    /// Called when speech ends: gives music its full volume back.
    func endSpeaking() {
        guard guidanceOnly else { return }
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }

    /// Previews and the audio test also work with the silent switch on and when no ride is running.
    func preparePreview() {
        try? AVAudioSession.sharedInstance().setCategory(.playback, mode: .spokenAudio, options: [.duckOthers])
        try? AVAudioSession.sharedInstance().setActive(true)
        refresh()
    }

    func endPreview() {
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
    }

    // MARK: Route monitoring

    private func routeChanged() {
        refresh()
        enforceSpeaker()
    }

    private func interruption(_ type: AVAudioSession.InterruptionType?) {
        switch type {
        case .began:
            status = "Audio interrupted (call or another app)"
        case .ended:
            status = ""
            if !guidanceOnly { try? activate() }
        default:
            break
        }
    }

    /// If iOS ever picks the earpiece while recording is enabled, push the sound back to the loudspeaker.
    private func enforceSpeaker() {
        let s = AVAudioSession.sharedInstance()
        guard s.category == .playAndRecord,
              s.currentRoute.outputs.contains(where: { $0.portType == .builtInReceiver }) else { return }
        try? s.overrideOutputAudioPort(.speaker)
        refresh()
    }

    func refresh() {
        let route = AVAudioSession.sharedInstance().currentRoute
        let bt: Set<AVAudioSession.Port> = [.bluetoothA2DP, .bluetoothHFP, .bluetoothLE]
        let out = route.outputs.first
        isBluetooth = route.outputs.contains { bt.contains($0.portType) } || route.inputs.contains { bt.contains($0.portType) }
        description = out.map { $0.portName } ?? "No audio route"
        inputName = route.inputs.first?.portName ?? "—"
        switch out?.portType {
        case .some(.bluetoothA2DP): outputKind = "Bluetooth (stereo)"
        case .some(.bluetoothHFP), .some(.bluetoothLE): outputKind = "Bluetooth (hands-free)"
        case .some(.headphones): outputKind = "Wired headphones"
        case .some(.builtInSpeaker): outputKind = "iPhone loudspeaker"
        case .some(.builtInReceiver): outputKind = "Earpiece"
        case .some: outputKind = "Other"
        case .none: outputKind = "None"
        }
    }
}
