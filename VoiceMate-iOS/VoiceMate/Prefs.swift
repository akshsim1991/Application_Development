import Foundation

/// Settings read by non-UI code. Keys match the @AppStorage keys in SettingsView.
enum Prefs {
    private static var d: UserDefaults { .standard }
    static func set(_ value: Any, _ key: String) { d.set(value, forKey: key) }

    // Voice & units
    static var speechRate: Float { Float(d.object(forKey: "speechRate") as? Double ?? 0.5) }
    static var voiceID: String { d.string(forKey: "voiceID") ?? "" }
    static var units: String { d.string(forKey: "units") ?? "auto" }
    static var imperial: Bool {
        switch units {
        case "imperial": return true
        case "metric": return false
        default: return Locale.current.measurementSystem == .us
        }
    }
    static var speedUnit: String { imperial ? "mph" : "km/h" }
    static var speedUnitSpoken: String { imperial ? "miles per hour" : "kilometres per hour" }
    static func displaySpeed(_ metresPerSecond: Double) -> Double { max(0, metresPerSecond) * (imperial ? 2.23694 : 3.6) }
    static var currencyCode: String { Locale.current.currency?.identifier ?? "INR" }

    // Safety
    static var speedLimit: Int { d.integer(forKey: "speedLimit") }
    static func setSpeedLimit(_ v: Int) { d.set(v, forKey: "speedLimit") }
    static var breakMinutes: Int { d.object(forKey: "breakMinutes") as? Int ?? 90 }
    static var keepScreenOn: Bool { d.object(forKey: "keepScreenOn") as? Bool ?? true }
    static var cameraAlerts: Bool { d.object(forKey: "cameraAlerts") as? Bool ?? true }
    static var autoLimitAlert: Bool { d.bool(forKey: "autoLimitAlert") }
    static var crashDetect: Bool { d.bool(forKey: "crashDetect") }

    // Navigation
    static var routeStyle: String { d.string(forKey: "routeStyle") ?? "fastest" }
    static var avoidTolls: Bool { d.bool(forKey: "avoidTolls") }
    static var avoidHighways: Bool { d.bool(forKey: "avoidHighways") }
    static var googleKey: String { (d.string(forKey: "googleKey") ?? "").trimmingCharacters(in: .whitespacesAndNewlines) }

    // Emergency contact (Telegram bot) & live tracking
    static var riderName: String { let n = d.string(forKey: "riderName") ?? ""; return n.isEmpty ? "Rider" : n }
    static var tgToken: String { (d.string(forKey: "tgToken") ?? "").trimmingCharacters(in: .whitespacesAndNewlines) }
    static var tgChat: String { (d.string(forKey: "tgChat") ?? "").trimmingCharacters(in: .whitespacesAndNewlines) }
    static var liveShareMinutes: Int { d.integer(forKey: "liveShareMinutes") }

    // AI assistant
    static var anthropicKey: String { (d.string(forKey: "anthropicKey") ?? "").trimmingCharacters(in: .whitespacesAndNewlines) }
}
