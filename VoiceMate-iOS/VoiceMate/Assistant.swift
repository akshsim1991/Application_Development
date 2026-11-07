import UIKit

/// The "brain". Replace `reply(to:)` with a call to your own backend or an LLM API
/// if you want open-ended answers; everything else in the app stays the same.
@MainActor
final class Assistant {
    static let shared = Assistant()

    func reply(to rawQuestion: String) async -> String {
        let q = rawQuestion.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        if q.isEmpty { return "I didn't catch that. Please try again." }

        if q.contains("time") {
            return "It's " + Date().formatted(date: .omitted, time: .shortened) + "."
        }
        if q.contains("date") || q.contains("day is it") || q.contains("today") {
            return "Today is " + Date().formatted(date: .complete, time: .omitted) + "."
        }
        if q.contains("battery") {
            UIDevice.current.isBatteryMonitoringEnabled = true
            let level = UIDevice.current.batteryLevel
            return level < 0 ? "I can't read the battery level right now."
                             : "Your battery is at \(Int(level * 100)) percent."
        }
        if q.contains("hello") || q.contains("hi ") || q == "hi" || q.contains("hey") {
            return "Hello! How can I help?"
        }
        if q.contains("help") || q.contains("what can you do") {
            return "You can ask me for the time, the date, or your battery level."
        }
        return "You said: \(rawQuestion). I can tell you the time, the date, or your battery level."
    }
}
