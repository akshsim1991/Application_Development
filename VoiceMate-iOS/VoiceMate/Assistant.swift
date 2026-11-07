import UIKit
import CoreLocation
import MediaPlayer

/// Voice commands for riding. Everything here must work with no screen interaction.
/// Swap `reply(to:)` for a backend / LLM call if you want open-ended answers.
@MainActor
final class Assistant {
    static let shared = Assistant()
    private var lastReply = ""

    func reply(to rawQuestion: String) async -> String {
        let answer = await compute(rawQuestion)
        if !answer.hasPrefix("Say") { lastReply = answer }
        return answer
    }

    private func has(_ q: String, _ words: String...) -> Bool { words.contains { q.contains($0) } }

    private func compute(_ raw: String) async -> String {
        let q = raw.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        if q.isEmpty { return "I didn't catch that." }

        if has(q, "repeat", "say that again") { return lastReply.isEmpty ? "Nothing to repeat." : lastReply }

        // Music (Apple Music / Music app)
        let player = MPMusicPlayerController.systemMusicPlayer
        if has(q, "next song", "next track", "skip") { player.skipToNextItem(); return "Next track." }
        if has(q, "previous song", "previous track", "go back") { player.skipToPreviousItem(); return "Previous track." }
        if has(q, "pause", "stop music", "stop the music") { player.pause(); return "Paused." }
        if has(q, "play music", "resume", "play") { player.play(); return "Playing." }

        if has(q, "speed", "how fast") {
            guard let l = await LocationProvider.shared.freshLocation(), l.speed >= 0 else { return "I can't get your speed right now." }
            if Locale.current.measurementSystem == .us {
                return "You're doing \(Int((l.speed * 2.23694).rounded())) miles per hour."
            }
            return "You're doing \(Int((l.speed * 3.6).rounded())) kilometres per hour."
        }
        if has(q, "where am i", "location", "which road", "what road") {
            guard let l = await LocationProvider.shared.freshLocation() else { return "I can't get your location right now." }
            let marks = try? await CLGeocoder().reverseGeocodeLocation(l)
            if let p = marks?.first {
                let parts = [p.thoroughfare, p.subLocality, p.locality].compactMap { $0 }
                if !parts.isEmpty { return "You're near " + parts.joined(separator: ", ") + "." }
            }
            return "I have your position but can't name the road."
        }
        if has(q, "heading", "direction", "compass") {
            guard let l = await LocationProvider.shared.freshLocation(), l.course >= 0 else { return "I need you to be moving to know your heading." }
            let names = ["north", "north east", "east", "south east", "south", "south west", "west", "north west"]
            return "You're heading \(names[Int(((l.course + 22.5) / 45).rounded(.down)) % 8])."
        }
        if has(q, "time") { return "It's " + Date().formatted(date: .omitted, time: .shortened) + "." }
        if has(q, "date", "what day") { return "Today is " + Date().formatted(date: .complete, time: .omitted) + "." }
        if has(q, "battery") {
            UIDevice.current.isBatteryMonitoringEnabled = true
            let level = UIDevice.current.batteryLevel
            return level < 0 ? "I can't read the battery." : "Phone battery is at \(Int(level * 100)) percent."
        }
        if has(q, "help", "what can you do") {
            return "Ask for speed, location, heading, time, battery, or say play, pause, next song."
        }
        if has(q, "navigate", "call ", "message", "text ") {
            return "For navigation and calls, hold your helmet button to use Siri."
        }
        return "Sorry, I can't do that. Say help for what I can do."
    }
}
