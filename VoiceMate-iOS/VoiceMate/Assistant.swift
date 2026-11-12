import UIKit
import CoreLocation
import MapKit
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
    private var pending: MKMapItem?

    func reply(to question: String) async -> String { await handle(question).text }

    func handle(_ raw: String) async -> Reply {
        let r = await compute(raw)
        if !r.text.hasPrefix("I didn't catch") { lastReply = r.text }
        return r
    }

    private func has(_ q: String, _ words: String...) -> Bool { words.contains { q.contains($0) } }

    private func destination(from q: String) -> String? {
        for p in ["navigate to ", "take me to ", "directions to ", "go to ", "navigate "] {
            if let r = q.range(of: p) {
                let d = q[r.upperBound...].trimmingCharacters(in: .whitespacesAndNewlines)
                return d.isEmpty ? nil : d
            }
        }
        return nil
    }

    // MARK: Navigation

    func requestNavigation(to dest: String) async -> Reply {
        guard RideController.shared.isRiding else { return Reply(text: "Start ride mode first.") }
        guard let loc = await LocationProvider.shared.freshLocation() else {
            return Reply(text: "I can't get your location yet.")
        }
        let item: MKMapItem?
        if ["home", "my home", "back home"].contains(dest) {
            guard let h = homeItem() else { return Reply(text: "You haven't set a home. Say set home when you're there.") }
            item = h
        } else {
            item = await Navigator.shared.search(dest, near: loc)
        }
        guard let item else { return Reply(text: "I couldn't find \(dest).") }
        pending = item
        let d = loc.distance(from: item.placemark.location ?? loc)
        return Reply(text: "\(item.name ?? dest), about \(spokenDistance(d)) away. Say yes to start.", followUp: true)
    }

    private func startNavigation(to item: MKMapItem) async -> Reply {
        guard let loc = await LocationProvider.shared.freshLocation() else {
            return Reply(text: "I can't get your location yet.")
        }
        return Reply(text: await Navigator.shared.begin(to: item, from: loc))
    }

    private func homeItem() -> MKMapItem? {
        let d = UserDefaults.standard
        guard d.object(forKey: "homeLat") != nil else { return nil }
        let item = MKMapItem(placemark: MKPlacemark(coordinate: CLLocationCoordinate2D(
            latitude: d.double(forKey: "homeLat"), longitude: d.double(forKey: "homeLon"))))
        item.name = "Home"
        return item
    }

    // MARK: Commands

    private func compute(_ raw: String) async -> Reply {
        let q = raw.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        if q.isEmpty { return Reply(text: "I didn't catch that.") }
        let words = Set(q.split(separator: " ").map(String.init))

        let dest = destination(from: q)
        if let item = pending, dest == nil {
            pending = nil
            if !words.isDisjoint(with: ["no", "cancel", "nope", "stop"]) { return Reply(text: "Okay, cancelled.") }
            if !words.isDisjoint(with: ["yes", "yeah", "yep", "start", "go", "ok", "okay", "sure", "confirm"]) {
                return await startNavigation(to: item)
            }
        }
        if let dest { return await requestNavigation(to: dest) }

        if has(q, "stop navigation", "cancel navigation", "end navigation", "stop navigating") {
            Navigator.shared.stop()
            return Reply(text: "Navigation stopped.")
        }
        if Navigator.shared.isNavigating {
            if has(q, "next turn", "what's next", "whats next", "next instruction") { return Reply(text: Navigator.shared.upcoming()) }
            if has(q, "how far", "how long") || words.contains("eta") || has(q, "arrive") { return Reply(text: Navigator.shared.remaining()) }
        }
        if has(q, "set home", "save home", "this is home") {
            guard let l = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location.") }
            UserDefaults.standard.set(l.coordinate.latitude, forKey: "homeLat")
            UserDefaults.standard.set(l.coordinate.longitude, forKey: "homeLon")
            return Reply(text: "Home saved. Say navigate home any time.")
        }

        if has(q, "repeat", "say that again") { return Reply(text: lastReply.isEmpty ? "Nothing to repeat." : lastReply) }

        // Music (Apple Music / Music app)
        let player = MPMusicPlayerController.systemMusicPlayer
        if has(q, "next song", "next track", "skip") { player.skipToNextItem(); return Reply(text: "Next track.") }
        if has(q, "previous song", "previous track") { player.skipToPreviousItem(); return Reply(text: "Previous track.") }
        if has(q, "pause", "stop music", "stop the music") { player.pause(); return Reply(text: "Paused.") }
        if has(q, "play music", "resume", "play") { player.play(); return Reply(text: "Playing.") }

        if has(q, "speed", "how fast") {
            guard let l = await LocationProvider.shared.freshLocation(), l.speed >= 0 else { return Reply(text: "I can't get your speed right now.") }
            if Locale.current.measurementSystem == .us {
                return Reply(text: "You're doing \(Int((l.speed * 2.23694).rounded())) miles per hour.")
            }
            return Reply(text: "You're doing \(Int((l.speed * 3.6).rounded())) kilometres per hour.")
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
        if has(q, "time") { return Reply(text: "It's " + Date().formatted(date: .omitted, time: .shortened) + ".") }
        if has(q, "date", "what day") { return Reply(text: "Today is " + Date().formatted(date: .complete, time: .omitted) + ".") }
        if has(q, "battery") {
            UIDevice.current.isBatteryMonitoringEnabled = true
            let level = UIDevice.current.batteryLevel
            return Reply(text: level < 0 ? "I can't read the battery." : "Phone battery is at \(Int(level * 100)) percent.")
        }
        if has(q, "help", "what can you do") {
            return Reply(text: "Say navigate to a place, next turn, how far, stop navigation, speed, location, battery, or play, pause, next song.")
        }
        if has(q, "call ", "message", "text ") {
            return Reply(text: "For calls and messages, hold your helmet button to use Siri.")
        }
        return Reply(text: "Sorry, I can't do that. Say help for what I can do.")
    }
}
