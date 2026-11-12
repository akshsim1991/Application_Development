import CoreLocation
import CoreMotion

/// SOS and location sharing through a Telegram bot (free, no server), crash detection with a
/// 30-second "I'm OK" countdown, and optional live ride tracking.
@MainActor
final class Emergency: ObservableObject {
    static let shared = Emergency()

    /// Seconds left in a crash countdown, nil when idle.
    @Published private(set) var crashCountdown: Int?

    var speak: ((String) async -> Void)?
    /// Asks the listener to accept the next utterance without the wake phrase.
    var expectReply: (() -> Void)?

    private let motion = CMMotionManager()
    private var observerID: UUID?
    private var recentSpeeds: [(Date, Double)] = []
    private var checking = false
    private var countdownTimer: Timer?
    private var shareTimer: Timer?

    var isConfigured: Bool { !Prefs.tgToken.isEmpty && !Prefs.tgChat.isEmpty }

    // MARK: Telegram

    func send(_ text: String) async -> Bool {
        guard isConfigured, let url = URL(string: "https://api.telegram.org/bot\(Prefs.tgToken)/sendMessage") else { return false }
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.timeoutInterval = 15
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.httpBody = try? JSONSerialization.data(withJSONObject: ["chat_id": Prefs.tgChat, "text": text])
        guard let (_, resp) = try? await URLSession.shared.data(for: req) else { return false }
        return (resp as? HTTPURLResponse)?.statusCode == 200
    }

    /// Reads the chat id of whoever last messaged the bot (send the bot any message first).
    static func findChatID(token: String) async -> String? {
        guard let url = URL(string: "https://api.telegram.org/bot\(token.trimmingCharacters(in: .whitespacesAndNewlines))/getUpdates"),
              let (data, _) = try? await URLSession.shared.data(from: url),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let result = json["result"] as? [[String: Any]] else { return nil }
        for update in result.reversed() {
            for key in ["message", "channel_post", "my_chat_member"] {
                if let m = update[key] as? [String: Any], let chat = m["chat"] as? [String: Any], let id = chat["id"] as? Int {
                    return String(id)
                }
            }
        }
        return nil
    }

    func mapsLink(_ l: CLLocation) -> String { "https://maps.google.com/?q=\(l.coordinate.latitude),\(l.coordinate.longitude)" }

    func shareLocation(note: String = "Location") async -> String {
        guard isConfigured else { return "Emergency contact isn't set up. Open Settings." }
        guard let l = await LocationProvider.shared.freshLocation() else { return "I can't get your location." }
        let speed = Int(Prefs.displaySpeed(l.speed).rounded())
        let ok = await send("📍 \(Prefs.riderName): \(note)\n\(mapsLink(l))\nSpeed \(speed) \(Prefs.speedUnit)")
        return ok ? "Location shared." : "I couldn't send the message. Check your internet."
    }

    func sendSOS(manual: Bool) async -> String {
        guard isConfigured else { return "Emergency contact isn't set up. Open Settings." }
        let l = await LocationProvider.shared.freshLocation()
        var text = "🚨 SOS from \(Prefs.riderName). "
            + (manual ? "Manual alert." : "A possible crash was detected and there was no response.")
        if let l { text += "\nLocation: \(mapsLink(l))" }
        let ok = await send(text)
        return ok ? "Emergency message sent." : "I couldn't send the emergency message. Check your internet."
    }

    // MARK: Ride lifecycle

    func rideStarted() {
        startMonitoring()
        guard isConfigured, Prefs.liveShareMinutes > 0 else { return }
        Task { _ = await shareLocation(note: "Ride started") }
        shareTimer = Timer.scheduledTimer(withTimeInterval: TimeInterval(Prefs.liveShareMinutes * 60), repeats: true) { [weak self] _ in
            Task { @MainActor in _ = await self?.shareLocation(note: "Ride update") }
        }
    }

    func rideEnded() {
        stopMonitoring()
        let wasSharing = shareTimer != nil
        shareTimer?.invalidate(); shareTimer = nil
        if wasSharing { Task { _ = await shareLocation(note: "Ride ended") } }
    }

    // MARK: Crash detection

    func startMonitoring() {
        guard Prefs.crashDetect, motion.isDeviceMotionAvailable, observerID == nil else { return }
        observerID = LocationProvider.shared.observe { [weak self] l in self?.noteSpeed(l) }
        motion.deviceMotionUpdateInterval = 0.05
        motion.startDeviceMotionUpdates(to: .main) { [weak self] dm, _ in
            guard let a = dm?.userAcceleration else { return }
            let g = (a.x * a.x + a.y * a.y + a.z * a.z).squareRoot()
            if g > 4.0 { Task { @MainActor in self?.impact() } }
        }
    }

    func stopMonitoring() {
        motion.stopDeviceMotionUpdates()
        if let id = observerID { LocationProvider.shared.removeObserver(id); observerID = nil }
        recentSpeeds = []
        countdownTimer?.invalidate(); countdownTimer = nil
        crashCountdown = nil
    }

    private func noteSpeed(_ l: CLLocation) {
        recentSpeeds.append((Date(), l.speed))
        recentSpeeds.removeAll { Date().timeIntervalSince($0.0) > 6 }
    }

    /// A hard jolt while moving fast, followed by standing still, starts the countdown.
    private func impact() {
        guard !checking, crashCountdown == nil, (recentSpeeds.map { $0.1 }.max() ?? 0) > 8 else { return }
        checking = true
        Task {
            try? await Task.sleep(for: .seconds(6))
            let v = LocationProvider.shared.latest?.speed ?? 0
            checking = false
            if v < 2.5 { startCountdown() }
        }
    }

    /// For the Test button in Settings.
    func simulateCrash() { if crashCountdown == nil { startCountdown() } }

    private func startCountdown() {
        crashCountdown = 30
        Task {
            await speak?("Crash detected. Say I'm OK to cancel. Sending an alert in 30 seconds.")
            expectReply?()
        }
        countdownTimer?.invalidate()
        countdownTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.tickCountdown() }
        }
    }

    private func tickCountdown() {
        guard let c = crashCountdown else { return }
        if c <= 1 { finishCountdown(); return }
        crashCountdown = c - 1
        if (c - 1) % 10 == 0 {
            Task { await speak?("Alert in \(c - 1) seconds. Say I'm OK to cancel."); expectReply?() }
        }
    }

    private func finishCountdown() {
        countdownTimer?.invalidate(); countdownTimer = nil
        crashCountdown = nil
        Task {
            let msg = await sendSOS(manual: false)
            await speak?(msg)
        }
    }

    func sendNow() { finishCountdown() }

    func cancelCrash() {
        countdownTimer?.invalidate(); countdownTimer = nil
        crashCountdown = nil
    }
}
