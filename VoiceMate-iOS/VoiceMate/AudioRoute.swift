import AVFoundation
import Combine

/// Configures the shared audio session so the microphone and speaker are the
/// Bluetooth headphones whenever they are connected, and reports the current route.
@MainActor
final class AudioRoute: ObservableObject {
    @Published private(set) var description = "No audio route"
    @Published private(set) var isBluetooth = false

    private var observer: NSObjectProtocol?

    init() {
        observer = NotificationCenter.default.addObserver(
            forName: AVAudioSession.routeChangeNotification, object: nil, queue: .main
        ) { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
        refresh()
    }

    deinit { if let observer { NotificationCenter.default.removeObserver(observer) } }

    /// `.allowBluetooth` enables the headset (HFP) microphone; `.allowBluetoothA2DP`
    /// keeps high-quality output when the mic is idle.
    func activate() throws {
        let session = AVAudioSession.sharedInstance()
        try session.setCategory(
            .playAndRecord,
            mode: .default,
            options: [.allowBluetooth, .allowBluetoothA2DP, .duckOthers]
        )
        try session.setActive(true)
        refresh()
    }

    func deactivate() {
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        refresh()
    }

    func refresh() {
        let route = AVAudioSession.sharedInstance().currentRoute
        let bt: Set<AVAudioSession.Port> = [.bluetoothA2DP, .bluetoothHFP, .bluetoothLE]
        let output = route.outputs.first
        isBluetooth = route.outputs.contains { bt.contains($0.portType) }
            || route.inputs.contains { bt.contains($0.portType) }
        description = output.map { "\($0.portName)" } ?? "No audio route"
    }
}
