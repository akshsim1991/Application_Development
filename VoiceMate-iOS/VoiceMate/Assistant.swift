import UIKit
import CoreLocation
import MediaPlayer

struct Reply {
    var text: String
    /// Nothing recognised while the assistant is on: stay quiet instead of answering background chatter.
    var silent = false
    /// Ride Mode should end after this reply is spoken.
    var endRide = false
    /// True when the assistant asked a question: keep listening for the answer without the wake phrase.
    var followUp = false
}

/// Voice commands for riding. Everything here must work with no screen interaction.
@MainActor
final class Assistant {
    static let shared = Assistant()
    private var lastReply = ""
    private var pending: Place?
    private var pendingAsStop = false
    private var pendingSOS = false

    func reply(to question: String) async -> String { await handle(question).text }

    /// `explicit` is false when the words weren't preceded by the wake phrase (assistant already on).
    func handle(_ raw: String, explicit: Bool = true) async -> Reply {
        let r = await compute(raw, explicit: explicit)
        if !r.silent, !r.text.hasPrefix("I didn't catch") { lastReply = r.text }
        return r
    }

    // MARK: Helpers

    /// Phrases (with a space) and long words match anywhere; short single words must be whole words,
    /// so "play" doesn't fire on "display".
    private func has(_ q: String, _ needles: String...) -> Bool {
        let tokens = Set(q.split(whereSeparator: { !($0.isLetter || $0.isNumber || $0 == "'") }).map(String.init))
        return needles.contains { n in n.contains(" ") || n.count >= 8 ? q.contains(n) : tokens.contains(n) }
    }

    private func after(_ q: String, _ prefixes: [String]) -> String? {
        for p in prefixes {
            if let r = q.range(of: p) {
                let s = q[r.upperBound...].trimmingCharacters(in: .whitespacesAndNewlines)
                return s.isEmpty ? nil : s
            }
        }
        return nil
    }

    private func destination(from q: String) -> String? {
        if q.hasSuffix("go home") || q.hasSuffix("take me home") { return "home" }
        return after(q, ["navigate to ", "take me to ", "directions to ", "go to ", "navigate "])
    }

    private func number(_ q: String, _ pattern: String) -> Double? {
        guard let re = try? NSRegularExpression(pattern: pattern),
              let m = re.firstMatch(in: q, range: NSRange(q.startIndex..., in: q)), m.numberOfRanges > 1,
              let r = Range(m.range(at: 1), in: q) else { return nil }
        return Double(q[r])
    }

    private func parseDelay(_ q: String) -> TimeInterval? {
        if q.contains("half an hour") { return 1800 }
        guard let m = q.range(of: #"(\d+)\s*(hours?|hrs?|minutes?|mins?)"#, options: .regularExpression) else { return nil }
        let s = String(q[m])
        let n = Double(s.prefix { $0.isNumber }) ?? 0
        return s.contains("h") ? n * 3600 : n * 60
    }

    private func placeNameToSave(_ q: String) -> String? {
        if has(q, "set home", "save home", "this is home") { return "Home" }
        if has(q, "set work", "save work", "this is work", "this is my office") { return "Work" }
        if has(q, "save parking", "save my parking", "i parked here", "remember parking", "parked here") { return "Parking" }
        if let named = after(q, ["save this place as ", "save this location as ", "save location as ", "remember this place as ",
                                 "save this as ", "save place as "])?.capitalized { return named }
        if has(q, "save this location", "save this place", "save location", "save my location") {
            return "Saved place \(PlaceStore.shared.saved.count + 1)"
        }
        return nil
    }

    /// "nearest petrol pump", "find a hospital", ...
    private func nearbyCategory(_ q: String, _ words: Set<String>) -> (query: String, label: String)? {
        guard has(q, "nearest", "nearby", "closest", "near me", "find a ", "find the ", "find me ", "find an ") else { return nil }
        let table: [([String], String, String)] = [
            (["petrol pump", "petrol bunk", "petrol station", "fuel station", "gas station", "fuel pump", "petrol", "fuel"], "petrol pump", "petrol pump"),
            (["atm", "cash machine"], "ATM", "ATM"),
            (["hospital", "emergency room"], "hospital", "hospital"),
            (["pharmacy", "chemist", "medical store"], "pharmacy", "pharmacy"),
            (["mechanic", "garage", "bike repair", "service centre", "service center", "workshop"], "motorcycle repair shop", "mechanic"),
            (["puncture", "tyre shop", "tire shop"], "tyre puncture repair", "puncture shop"),
            (["restaurant", "food", "dhaba", "eat"], "restaurant", "restaurant"),
            (["tea", "chai", "coffee", "cafe"], "tea stall cafe", "cafe"),
            (["toilet", "restroom", "washroom"], "public toilet", "toilet"),
            (["hotel", "lodge", "stay", "accommodation"], "hotel", "hotel"),
            (["police"], "police station", "police station")
        ]
        for (keys, query, label) in table {
            if keys.contains(where: { $0.contains(" ") ? q.contains($0) : words.contains($0) }) { return (query, label) }
        }
        return nil
    }

    private func yes(_ words: Set<String>) -> Bool {
        !words.isDisjoint(with: ["yes", "yeah", "yep", "start", "go", "ok", "okay", "sure", "confirm", "send"])
    }
    private func no(_ words: Set<String>) -> Bool {
        !words.isDisjoint(with: ["no", "cancel", "nope", "stop", "don't"])
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
        pendingAsStop = false
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

    private func addStop(query: String) async -> Reply {
        guard Navigator.shared.isNavigating else { return Reply(text: "Start navigation first, then add a stop.") }
        guard let loc = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location yet.") }
        let place: Place
        if let saved = PlaceStore.shared.find(query) { place = saved.place }
        else if let found = await Navigator.shared.search(query, near: loc) { place = found }
        else { return Reply(text: "I couldn't find \(query).") }
        return Reply(text: await Navigator.shared.addStop(place, from: loc))
    }

    // MARK: Commands

    private func compute(_ raw: String, explicit: Bool) async -> Reply {
        let q = raw.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        if q.isEmpty { return Reply(text: "I didn't catch that.") }
        let words = Set(q.split(separator: " ").map(String.init))

        // Crash countdown: any "I'm OK" cancels.
        if Emergency.shared.crashCountdown != nil {
            if !words.isDisjoint(with: ["ok", "okay", "fine", "cancel", "alright", "stop", "no"]) || q.contains("i'm good") {
                Emergency.shared.cancelCrash()
                return Reply(text: "Okay, alert cancelled. Glad you're alright.")
            }
            return Reply(text: "Say I'm OK to cancel the alert.", followUp: true)
        }

        // Confirmations
        if pendingSOS {
            pendingSOS = false
            if yes(words) { return Reply(text: await Emergency.shared.sendSOS(manual: true)) }
            return Reply(text: "Okay, not sending.")
        }
        let dest = destination(from: q)
        let stopQuery = after(q, ["add a stop at ", "add a stop to ", "add a stop ", "add stop at ", "add stop ", "stop at "])
        if let item = pending, dest == nil, stopQuery == nil {
            pending = nil
            if no(words) && !yes(words) { return Reply(text: "Okay, cancelled.") }
            if yes(words) {
                if pendingAsStop, let loc = await LocationProvider.shared.freshLocation() {
                    return Reply(text: await Navigator.shared.addStop(item, from: loc))
                }
                return await startNavigation(to: item)
            }
        }

        if q.hasPrefix("ask "), let ans = await Ask.answer(String(q.dropFirst(4))) { return Reply(text: ans) }

        // Ending the ride by voice (the microphone can't hear "hey rider" again afterwards).
        if has(q, "end ride", "end the ride", "stop ride mode", "turn off ride mode", "finish ride", "stop the ride") {
            let summary = TripRecorder.shared.isRecording ? TripRecorder.shared.summary() + " " : ""
            return Reply(text: summary + "Ride ended. Ride safe.", endRide: true)
        }

        // Safety
        if q.replacingOccurrences(of: " ", with: "").contains("sos") || has(q, "emergency", "send help", "i need help", "i crashed", "call for help") {
            guard Emergency.shared.isConfigured else { return Reply(text: "Emergency contact isn't set up. Open Settings, then Emergency.") }
            pendingSOS = true
            return Reply(text: "Send an emergency message with your location? Say yes to confirm.", followUp: true)
        }
        if has(q, "share my location", "share location", "send my location") {
            return Reply(text: await Emergency.shared.shareLocation())
        }

        // Stops & destinations
        if let stopQuery { return await addStop(query: stopQuery) }
        if has(q, "skip stop", "skip this stop", "next stop") {
            guard let loc = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location.") }
            return Reply(text: await Navigator.shared.skipStop(from: loc))
        }
        if let dest { return await requestNavigation(to: dest) }

        // Nearby places
        if let cat = nearbyCategory(q, words) {
            guard RideController.shared.isRiding else { return Reply(text: "Start ride mode first.") }
            guard let loc = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location yet.") }
            guard let first = await Navigator.shared.nearby(cat.query, near: loc, limit: 3).first else {
                return Reply(text: "I couldn't find a \(cat.label) nearby.")
            }
            pending = first
            pendingAsStop = Navigator.shared.isNavigating
            let d = loc.distance(from: CLLocation(latitude: first.coordinate.latitude, longitude: first.coordinate.longitude))
            return Reply(text: "Nearest \(cat.label): \(first.name), \(spokenDistance(d)) away. Say yes to \(pendingAsStop ? "add it as a stop" : "navigate").",
                         followUp: true)
        }

        // Saved places
        if let name = placeNameToSave(q) {
            guard let l = await LocationProvider.shared.freshLocation() else { return Reply(text: "I can't get your location.") }
            PlaceStore.shared.save(name: name, at: l.coordinate)
            return Reply(text: "\(name) saved.")
        }
        if has(q, "where did i park", "where is my bike", "find my bike", "find my parking") {
            return await requestNavigation(to: "parking")
        }

        // Route preferences
        if has(q, "avoid toll", "no toll", "skip toll") { return await routePref("avoidTolls", true, "Avoiding tolls.") }
        if has(q, "allow toll", "use toll", "with toll") { return await routePref("avoidTolls", false, "Tolls allowed.") }
        if has(q, "avoid highway", "avoid expressway", "no highway") { return await routePref("avoidHighways", true, "Avoiding highways.") }
        if has(q, "allow highway", "use highway", "with highway") { return await routePref("avoidHighways", false, "Highways allowed.") }
        if has(q, "curvy", "scenic", "twisty"), has(q, "route", "road", "mode", "prefer", "roads") {
            return await routePref("routeStyle", "curvy", "Curvy routes on. Needs a Google key to compare alternatives.")
        }
        if has(q, "fastest route", "shortest route", "normal route") { return await routePref("routeStyle", "fastest", "Fastest route.") }

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

        // Garage: fuel, odometer, service, documents
        if has(q, "log fuel", "filled", "fill up", "fill-up", "fuel entry", "petrol entry") {
            guard let litres = number(q, #"(\d+(?:\.\d+)?)\s*(?:litres?|liters?|l\b)"#) else {
                return Reply(text: "Say, for example, log fuel 8 litres at 107.")
            }
            var price = number(q, #"(?:at|@)\s*(?:rs\.?|rupees?|₹)?\s*(\d+(?:\.\d+)?)"#) ?? 0
            if price == 0, let total = number(q, #"(?:for|paid)\s*(?:rs\.?|rupees?|₹)?\s*(\d+(?:\.\d+)?)"#), litres > 0 { price = total / litres }
            return Reply(text: Garage.shared.logFuel(litres: litres, pricePerLitre: price))
        }
        if has(q, "mileage", "fuel economy", "per litre", "per liter", "how efficient") {
            return Reply(text: Garage.shared.mileageSummary())
        }
        if has(q, "odometer") || words.contains("odo") {
            if q.contains("set"), let v = number(q, #"(\d+(?:\.\d+)?)"#) {
                Garage.shared.setOdometer(v)
                return Reply(text: "Odometer set to \(Int(v)) kilometres.")
            }
            return Reply(text: "Odometer is \(Int(Garage.shared.odometerKm)) kilometres.")
        }
        if has(q, "service status", "next service", "service due", "maintenance", "is my bike due", "service reminders") {
            return Reply(text: Garage.shared.serviceSummary())
        }
        if has(q, "done", "changed", "replaced", "serviced", "lubed", "lubricated", "lubricant"), let name = Garage.shared.markDone(matching: q) {
            return Reply(text: "\(name) marked as done at \(Int(Garage.shared.odometerKm)) kilometres.")
        }
        if has(q, "insurance", "puc", "pollution", "license", "licence", "documents"), has(q, "expire", "expiry", "valid", "due", "when", "status") {
            return Reply(text: Garage.shared.docSummary())
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

        if has(q, "route summary", "summary of the route", "summarise the route", "summarize the route") {
            return Reply(text: Navigator.shared.summary())
        }
        if has(q, "repeat", "say that again") {
            // "Repeat the last instruction" repeats the last turn prompt; plain "repeat" repeats the last answer.
            let nav = Navigator.shared.lastInstruction
            if has(q, "instruction", "direction", "turn"), !nav.isEmpty { return Reply(text: nav) }
            return Reply(text: lastReply.isEmpty ? "Nothing to repeat." : lastReply)
        }

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

        if has(q, "gps", "satellite", "signal") {
            let g = LocationProvider.shared
            let acc = g.gpsAccuracy.map { ", accurate to about \(Int($0)) metres" } ?? ""
            return Reply(text: "\(g.gpsLevel.title)\(acc).")
        }
        if has(q, "speed", "how fast") {
            _ = await LocationProvider.shared.freshLocation()
            let gps = LocationProvider.shared
            guard gps.speedKnown else { return Reply(text: "I can't get your speed yet. \(gps.gpsLevel.title).") }
            if gps.speedMps < 1.5 { return Reply(text: "You're stationary.") }
            return Reply(text: "You're doing \(Int(Prefs.displaySpeed(gps.speedMps).rounded())) \(Prefs.speedUnitSpoken).")
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
            return Reply(text: "Try: navigate to a place, nearest petrol pump, add a stop, avoid tolls, next turn, weather, trip stats, log fuel, mileage, service status, save parking, set a timer, share my location, or ask a question.")
        }
        if explicit, has(q, "call", "message", "text") {
            return Reply(text: "For calls and messages, hold your helmet button to use Siri.")
        }

        // Anything else: ask the AI assistant if a key is set. While the assistant is simply on, ignore
        // chatter it doesn't understand (only words addressed to it with the wake phrase get an answer).
        if !explicit { return Reply(text: "", silent: true) }
        if !Prefs.anthropicKey.isEmpty {
            if let ans = await Ask.answer(q) { return Reply(text: ans) }
            return Reply(text: "I couldn't reach the AI assistant. Check your internet and key.")
        }
        return Reply(text: "Sorry, I can't do that. Say help for what I can do.")
    }

    private func routePref(_ key: String, _ value: Any, _ msg: String) async -> Reply {
        Prefs.set(value, key)
        let note = Navigator.shared.isNavigating ? await Navigator.shared.replan() : ""
        return Reply(text: note.isEmpty ? msg : msg + " " + note)
    }
}
