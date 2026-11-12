import UIKit
import CoreLocation
import MediaPlayer

struct Reply {
    var text: String
    /// True when the assistant asked a question: keep listening for the answer without the wake phrase.
    var followUp = false
}

/// Voice commands for riding. Everything here must work with no screen interaction.
@MainActor
final class Assistant {
    static let shared = Assistant()
    private var lastReply = ""
    private var pending: Place?

    func reply(to question: String) async -> String { await handle(question).text }

    func handle(_ raw: String) async -> Reply {
        let r = await compute(raw)
        if !r.text.hasPrefix("I didn't catch") { lastReply = r.text }
        return r
    }

    private func has(_ q: String, _ words: String...) -> Bool { words.contains { q.contains($0) } }

    private func destination(from q: String) -> String? {
        if q.hasSuffix("go home") || q.hasSuffix("take me home") { return "home" }
        for p in ["navigate to ", "take me to ", "directions to ", "go to ", "navigate "] {
            if let r = q.range(of: p) {
                let d = q[r.upperBound...].trimmingCharacters(in: .whitespacesAndNewlines)
                return d.isEmpty ? nil : d
            }
        }
        return nil
    }

    // MARK: Navigation

    /// Resolves a destination (saved place first, then map search). With `autoStart` it begins at once,
    /// otherwise it asks "Say yes to start".
    func requestNavigation(to dest: String, autoStart: Bool = false) async -> Reply {
        guard RideController.shared.isRiding else { return Reply(text: "Start ride mode first.") }
        guard let loc = await LocationProvider.shared.freshLocation() else {
            return Reply(text: "I can't get your location yet.")
        }
        let item: Place
        if let saved = PlaceStore.shared.find(dest) {
            item = saved.place
        } else if ["home", "work", "parking"].contains(dest.lowercased().replacingOccurrences(of: "my ", with: "")) {
            return Reply(text: "You haven't saved \(dest). Say save \(dest) when you're there.")
        } else if let found = await Navigator.shared.search(dest, near: loc) {
            item = found
        } else {
            return Reply(text: "I couldn't find \(dest).")
        }
        if autoStart { return await startNavigation(to: item) }
        pending = item
        let d = loc.distance(from: CLLocation(latitude: item.coordinate.latitude, longitude: item.coordinate.longitude))
        return Reply(text: "\(item.name), about \(spokenDistance(d)) away. Say yes to start.", followUp: true)
    }

    func startNavigation(to item: Place) async -> Reply {
        guard let loc = await LocationProvider.shared.freshLocation() else {
            return Reply(text: "I can't get your location yet.")
        }
        PlaceStore.shared.addRecent(item)
        return Reply(text: await Navigator.shared.begin(to: item, from: loc))
    }

    // MARK: Parsing helpers

    private func placeNameToSave(_ q: String) -> String? {
        if has(q, "set home", "save home", "this is home") { return "Home" }
        if has(q, "set work", "save work", "this is work", "this is my office") { return "Work" }
        if has(q, "save parking", "save my parking", "i parked here", "remember parking", "parked here") { return "Parking" }
        for p in ["save this place as ", "save this location as ", "save location as ", "remember this place as ", "save this as ", "save place as "] {
            if let r = q.range(of: p) {
                let n = q[r.upperBound...].trimmingCharacters(in: .whitespacesAndNewlines)
                return n.isEmpty ? nil : n.capitalized
            }
        }
        return nil
    }

    private func parseDelay(_ q: String) -> TimeInterval? {
        if q.contains("half an hour") { return 1800 }
        guard let m = q.range(of: #"(\d+)\s*(hours?|hrs?|minutes?|mins?)"#, options: .regularExpression) else { return nil }
        let s = String(q[m])
        let n = Double(s.prefix { $0.isNumber }) ?? 0
        return s.contains("h") ? n * 3600 : n * 60
    }

    // MARK: Commands

    private func compute(_ raw: String) async -> Reply {
        let q = raw.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        if q.isEmpty { return Reply(text: "I didn't catch that.") }
        let words = Set(q.split(separator: " ").map(String.init))

        // Answer to "Say yes to start"
        let dest = destination(from: q)
        if let item = pending, dest == nil {
            pending = nil
            if !words.isDisjoint(with: ["no", "cancel", "nope", "stop"]) { return Reply(text: "Okay, cancelled.") }
            if !words.isDisjoint(with: ["yes", "yeah", "yep", "start", "go", "ok", "okay", "sure", "confirm"]) {
                return await startNavigation(to: item)
            }
        }
        if let dest { return await requestNavigation(to: dest) }

        // Saved places
        if let name = placeNameToSave(q) {
            guard let l = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location.") }
            PlaceStore.shared.save(name: name, at: l.coordinate)
            return Reply(text: "\(name) saved.")
        }
        if has(q, "where did i park", "where is my bike", "find my bike", "find my parking") {
            return await requestNavigation(to: "parking")
        }

        // Timers and reminders
        if has(q, "timer", "remind me", "reminder") {
            guard let delay = parseDelay(q) else { return Reply(text: "Say, for example, set a timer for 10 minutes.") }
            var label = q.range(of: " to ", options: .backwards).map { String(q[$0.upperBound...]) } ?? ""
            label = label.replacingOccurrences(of: #"(in|after|for)?\s*(\d+)\s*(hours?|hrs?|minutes?|mins?)"#, with: "",
                                               options: .regularExpression).trimmingCharacters(in: .whitespaces)
            let msg = label.isEmpty ? "Your \(spokenDuration(delay)) timer is up." : "Reminder: \(label)."
            Task {
                try? await Task.sleep(for: .seconds(delay))
                await RideController.shared.say(msg)
            }
            return Reply(text: label.isEmpty ? "Timer set for \(spokenDuration(delay))." : "Okay, I'll remind you in \(spokenDuration(delay)).")
        }

        // Speed alert settings
        if has(q, "speed alert", "speed limit", "speed warning") {
            if has(q, "off", "disable", "cancel", "stop") { Prefs.setSpeedLimit(0); return Reply(text: "Speed alert off.") }
            if let m = q.range(of: #"\d+"#, options: .regularExpression), let v = Int(q[m]) {
                Prefs.setSpeedLimit(v)
                return Reply(text: "Speed alert set to \(v) \(Prefs.speedUnitSpoken).")
            }
            let l = Prefs.speedLimit
            return Reply(text: l == 0 ? "Speed alert is off." : "Speed alert is \(l) \(Prefs.speedUnitSpoken).")
        }

        // Trip stats
        if has(q, "ridden", "been riding", "travelled", "traveled", "trip", "average speed", "max speed", "top speed") {
            return Reply(text: TripRecorder.shared.summary())
        }

        // Navigation control
        if has(q, "stop navigation", "cancel navigation", "end navigation", "stop navigating") {
            Navigator.shared.stop()
            return Reply(text: "Navigation stopped.")
        }
        if Navigator.shared.isNavigating {
            if has(q, "next turn", "what's next", "whats next", "next instruction") { return Reply(text: Navigator.shared.upcoming()) }
            if has(q, "how far", "how long") || words.contains("eta") || has(q, "arrive") { return Reply(text: Navigator.shared.remaining()) }
        } else if has(q, "next turn") {
            return Reply(text: "I'm not navigating.")
        }

        if has(q, "weather", "temperature", "will it rain", "is it raining") {
            guard let l = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location for the weather.") }
            return Reply(text: await Weather.summary(at: l))
        }

        if has(q, "repeat", "say that again") { return Reply(text: lastReply.isEmpty ? "Nothing to repeat." : lastReply) }

        // Music (Apple Music / Music app)
        let player = MPMusicPlayerController.systemMusicPlayer
        if has(q, "toggle music") {
            if player.playbackState == .playing { player.pause(); return Reply(text: "Paused.") }
            player.play(); return Reply(text: "Playing.")
        }
        if has(q, "next song", "next track", "skip") { player.skipToNextItem(); return Reply(text: "Next track.") }
        if has(q, "previous song", "previous track") { player.skipToPreviousItem(); return Reply(text: "Previous track.") }
        if has(q, "pause", "stop music", "stop the music") { player.pause(); return Reply(text: "Paused.") }
        if has(q, "play music", "resume", "play") { player.play(); return Reply(text: "Playing.") }

        if has(q, "speed", "how fast") {
            guard let l = await LocationProvider.shared.freshLocation(), l.speed >= 0 else { return Reply(text: "I can't get your speed right now.") }
            return Reply(text: "You're doing \(Int(Prefs.displaySpeed(l.speed).rounded())) \(Prefs.speedUnitSpoken).")
        }
        if has(q, "where am i", "location", "which road", "what road") {
            guard let l = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location right now.") }
            let marks = try? await CLGeocoder().reverseGeocodeLocation(l)
            if let p = marks?.first {
                let parts = [p.thoroughfare, p.subLocality, p.locality].compactMap { $0 }
                if !parts.isEmpty { return Reply(text: "You're near " + parts.joined(separator: ", ") + ".") }
            }
            return Reply(text: "I have your position but can't name the road.")
        }
        if has(q, "heading", "direction", "compass") {
            guard let l = await LocationProvider.shared.freshLocation(), l.course >= 0 else { return Reply(text: "I need you to be moving to know your heading.") }
            let names = ["north", "north east", "east", "south east", "south", "south west", "west", "north west"]
            return Reply(text: "You're heading \(names[Int(((l.course + 22.5) / 45).rounded(.down)) % 8]).")
        }
        if has(q, "what time", "the time", "current time", "time now") { return Reply(text: "It's " + Date().formatted(date: .omitted, time: .shortened) + ".") }
        if has(q, "date", "what day") { return Reply(text: "Today is " + Date().formatted(date: .complete, time: .omitted) + ".") }
        if has(q, "battery") {
            UIDevice.current.isBatteryMonitoringEnabled = true
            let level = UIDevice.current.batteryLevel
            return Reply(text: level < 0 ? "I can't read the battery." : "Phone battery is at \(Int(level * 100)) percent.")
        }
        if has(q, "help", "what can you do") {
            return Reply(text: "Try: navigate to a place, next turn, how far, weather, trip stats, set a timer, save parking, speed, battery, or play, pause, next song.")
        }
        if has(q, "call ", "message", "text ") {
            return Reply(text: "For calls and messages, hold your helmet button to use Siri.")
        }
        return Reply(text: "Sorry, I can't do that. Say help for what I can do.")
    }
}
