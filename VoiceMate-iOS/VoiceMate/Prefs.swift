import Foundation

/// Settings read by non-UI code. Keys match the @AppStorage keys in SettingsView.
enum Prefs {
    private static var d: UserDefaults { .standard }
    static var speechRate: Float { Float(d.object(forKey: "speechRate") as? Double ?? 0.5) }
    static var voiceID: String { d.string(forKey: "voiceID") ?? "" }
    static var units: String { d.string(forKey: "units") ?? "auto" }
    static var speedLimit: Int { d.integer(forKey: "speedLimit") }
    static var breakMinutes: Int { d.object(forKey: "breakMinutes") as? Int ?? 90 }
    static var keepScreenOn: Bool { d.object(forKey: "keepScreenOn") as? Bool ?? true }

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
    static func setSpeedLimit(_ v: Int) { d.set(v, forKey: "speedLimit") }
}
