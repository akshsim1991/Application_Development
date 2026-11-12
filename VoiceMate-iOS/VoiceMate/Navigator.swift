import MapKit
import CoreLocation

// MARK: - Geometry helpers

private extension MKPolyline {
    var coords: [CLLocationCoordinate2D] {
        var c = [CLLocationCoordinate2D](repeating: kCLLocationCoordinate2DInvalid, count: pointCount)
        getCoordinates(&c, range: NSRange(location: 0, length: pointCount))
        return c
    }
}

private func meters(_ a: CLLocationCoordinate2D, _ b: CLLocationCoordinate2D) -> Double {
    CLLocation(latitude: a.latitude, longitude: a.longitude)
        .distance(from: CLLocation(latitude: b.latitude, longitude: b.longitude))
}

private func bearing(_ a: CLLocationCoordinate2D, _ b: CLLocationCoordinate2D) -> Double {
    let la1 = a.latitude * .pi / 180, la2 = b.latitude * .pi / 180
    let dl = (b.longitude - a.longitude) * .pi / 180
    let y = sin(dl) * cos(la2)
    let x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dl)
    return (atan2(y, x) * 180 / .pi + 360).truncatingRemainder(dividingBy: 360)
}

/// Direction of travel as a path arrives at its end (looks back at least 10 m).
private func endBearing(_ pts: [CLLocationCoordinate2D]) -> Double? {
    guard pts.count >= 2, let last = pts.last else { return nil }
    for p in pts.dropLast().reversed() where meters(p, last) >= 10 { return bearing(p, last) }
    return bearing(pts[0], last)
}

/// Direction of travel as a path leaves its start (looks ahead at least 10 m).
private func startBearing(_ pts: [CLLocationCoordinate2D]) -> Double? {
    guard pts.count >= 2, let first = pts.first else { return nil }
    for p in pts.dropFirst() where meters(first, p) >= 10 { return bearing(first, p) }
    return bearing(first, pts[pts.count - 1])
}

/// Distance in metres from `p` to the segment a-b.
private func distToSegment(_ p: CLLocationCoordinate2D, _ a: CLLocationCoordinate2D, _ b: CLLocationCoordinate2D) -> Double {
    let kx = cos(p.latitude * .pi / 180) * 111_320, ky = 110_540.0
    func xy(_ c: CLLocationCoordinate2D) -> (Double, Double) { ((c.longitude - p.longitude) * kx, (c.latitude - p.latitude) * ky) }
    let (ax, ay) = xy(a), (bx, by) = xy(b)
    let dx = bx - ax, dy = by - ay
    let len2 = dx * dx + dy * dy
    let t = len2 == 0 ? 0 : max(0, min(1, -(ax * dx + ay * dy) / len2))
    return hypot(ax + t * dx, ay + t * dy)
}

func spokenDistance(_ m: Double) -> String {
    if Prefs.imperial {
        let ft = m * 3.28084
        if ft < 1000 { return "\(max(50, Int((ft / 50).rounded()) * 50)) feet" }
        let mi = m / 1609.34
        return mi < 10 ? String(format: "%.1f miles", mi) : "\(Int(mi.rounded())) miles"
    }
    if m < 1000 { return "\(max(50, Int((m / 50).rounded()) * 50)) metres" }
    let km = m / 1000
    return km < 10 ? String(format: "%.1f kilometres", km) : "\(Int(km.rounded())) kilometres"
}

func spokenDuration(_ s: TimeInterval) -> String {
    let mins = max(1, Int((s / 60).rounded()))
    if mins < 60 { return "\(mins) minute" + (mins == 1 ? "" : "s") }
    let h = mins / 60, m = mins % 60
    return "\(h) hour" + (h == 1 ? "" : "s") + (m == 0 ? "" : " \(m) minutes")
}

/// Turns visual instructions ("take the second exit") into plain directions based on the
/// maneuver type and the real geometry: go straight / turn left / turn right.
private func plainPhrase(prev: NavStep, next: NavStep, isLast: Bool) -> String {
    let text = next.instruction.lowercased()
    let m = next.maneuver ?? ""
    if isLast || m == "ARRIVE" || text.hasPrefix("arrive") { return "you will arrive at your destination" }

    let roundabout = m.hasPrefix("ROUNDABOUT") || text.contains("roundabout") || text.contains("traffic circle")
    let head = String(text.prefix(30))
    let side = head.contains("left") ? "left" : (head.contains("right") ? "right" : nil)

    var angle: Double?
    if let a = endBearing(prev.points) {
        // Through a roundabout the exit direction is where the step finally ends up.
        if let b = roundabout ? endBearing(next.points) : startBearing(next.points) {
            var d = (b - a).truncatingRemainder(dividingBy: 360)
            if d > 180 { d -= 360 }
            if d < -180 { d += 360 }
            angle = d
        }
    }

    func turn(_ d: Double) -> String {
        let s = d < 0 ? "left" : "right", mag = abs(d)
        if mag < 25 { return "go straight" }
        if mag < 60 { return "bear \(s)" }
        if mag < 120 { return "turn \(s)" }
        if mag < 165 { return "take a sharp \(s)" }
        return "make a U-turn"
    }

    var core: String
    switch m {
    case "TURN_LEFT": core = "turn left"
    case "TURN_RIGHT": core = "turn right"
    case "TURN_SLIGHT_LEFT": core = "bear left"
    case "TURN_SLIGHT_RIGHT": core = "bear right"
    case "TURN_SHARP_LEFT": core = "take a sharp left"
    case "TURN_SHARP_RIGHT": core = "take a sharp right"
    case "UTURN_LEFT", "UTURN_RIGHT": core = "make a U-turn"
    case "STRAIGHT", "NAME_CHANGE", "DEPART": core = "go straight"
    case "RAMP_LEFT": core = "take the exit on the left"
    case "RAMP_RIGHT": core = "take the exit on the right"
    case "FORK_LEFT": core = "keep left"
    case "FORK_RIGHT": core = "keep right"
    default:
        if text.contains("u-turn") { core = "make a U-turn" }
        else if text.contains("keep left") { core = "keep left" }
        else if text.contains("keep right") { core = "keep right" }
        else if text.contains("exit") && !roundabout { core = "take the exit" + (side.map { " on the \($0)" } ?? "") }
        else if let angle { core = turn(angle) }
        else if let side { core = "turn \(side)" }
        else { core = "continue straight" }
    }
    if roundabout {
        if let angle { core = turn(angle) } else if m == "ROUNDABOUT_LEFT" { core = "turn left" } else if m == "ROUNDABOUT_RIGHT" { core = "turn right" }
        core = "at the roundabout, " + core
    }
    return core
}

private func capitalised(_ s: String) -> String { s.prefix(1).uppercased() + s.dropFirst() }

/// Compact distance for the UI ("300 m", "1.5 km").
func shortDistance(_ m: Double) -> String {
    if Prefs.imperial {
        let ft = m * 3.28084
        if ft < 1000 { return "\(Int((ft / 10).rounded()) * 10) ft" }
        return String(format: "%.1f mi", m / 1609.34)
    }
    if m < 1000 { return "\(Int((m / 10).rounded()) * 10) m" }
    return String(format: "%.1f km", m / 1000)
}

/// SF Symbol for a plain-language maneuver ("turn left", "go straight", ...).
func maneuverSymbol(_ phrase: String) -> String {
    let p = phrase.lowercased()
    if p.contains("arrive") { return "flag.checkered" }
    if p.contains("u-turn") { return "arrow.uturn.left" }
    if p.contains("roundabout") { return "arrow.triangle.turn.up.right.circle" }
    if p.contains("left") { return "arrow.turn.up.left" }
    if p.contains("right") { return "arrow.turn.up.right" }
    return "arrow.up"
}

// MARK: - Provider-neutral models

struct Place { var name: String; var coordinate: CLLocationCoordinate2D }

struct NavStep {
    var instruction: String
    /// Google maneuver code (e.g. "TURN_LEFT"); nil for Apple Maps.
    var maneuver: String?
    var points: [CLLocationCoordinate2D]
    var distance: Double
}

struct NavRoute {
    var distance: Double
    var duration: TimeInterval
    var points: [CLLocationCoordinate2D]
    var steps: [NavStep]
}

protocol RoutingProvider {
    func search(_ query: String, near loc: CLLocation) async -> Place?
    func route(from loc: CLLocation, to place: Place) async -> NavRoute?
}

/// Apple Maps (MapKit) routing: no key needed, but weaker coverage in India.
struct AppleProvider: RoutingProvider {
    func search(_ query: String, near loc: CLLocation) async -> Place? {
        let req = MKLocalSearch.Request()
        req.naturalLanguageQuery = query
        req.region = MKCoordinateRegion(center: loc.coordinate, latitudinalMeters: 60_000, longitudinalMeters: 60_000)
        req.resultTypes = [.pointOfInterest, .address]
        guard let item = try? await MKLocalSearch(request: req).start().mapItems.first else { return nil }
        return Place(name: item.name ?? query, coordinate: item.placemark.coordinate)
    }

    func route(from loc: CLLocation, to place: Place) async -> NavRoute? {
        let req = MKDirections.Request()
        req.source = MKMapItem(placemark: MKPlacemark(coordinate: loc.coordinate))
        req.destination = MKMapItem(placemark: MKPlacemark(coordinate: place.coordinate))
        req.transportType = .automobile
        guard let r = try? await MKDirections(request: req).calculate().routes.first else { return nil }
        let steps = r.steps.filter { !$0.instructions.isEmpty }.map {
            NavStep(instruction: $0.instructions, maneuver: nil, points: $0.polyline.coords, distance: $0.distance)
        }
        return NavRoute(distance: r.distance, duration: r.expectedTravelTime, points: r.polyline.coords, steps: steps)
    }
}

// MARK: - Navigator

/// Audio-only turn-by-turn navigation: announces plain directions with distances, reroutes when
/// you leave the route, and answers "what's next" / "how far". Routing comes from Google (if a key is
/// set) or Apple Maps.
@MainActor
final class Navigator: ObservableObject {
    static let shared = Navigator()

    @Published private(set) var isNavigating = false
    @Published private(set) var destinationName = ""
    @Published private(set) var providerStatus = "Apple Maps"
    @Published private(set) var upcomingPhrase = ""
    @Published private(set) var upcomingMeters = 0.0
    @Published private(set) var remainingMeters = 0.0
    @Published private(set) var remainingSeconds = 0.0

    /// Set by RideController; speaks through the helmet.
    var speak: ((String) async -> Void)?

    private var route: NavRoute?
    private var destination: Place?
    private var steps: [NavStep] = []
    private var junctions: [CLLocationCoordinate2D] = []   // junctions[i] = where step i's maneuver happens
    private var phrases: [String] = []
    private var routeCoords: [CLLocationCoordinate2D] = []
    private var nextIdx = 1
    private var announced: [Int: Int] = [:]
    private var prevDistance = Double.infinity
    private var offRouteCount = 0
    private var rerouting = false
    private var rerouteCooldown = Date.distantPast
    private var lastLoc: CLLocation?
    private var observerID: UUID?

    // MARK: Search & start

    private var google: GoogleProvider? {
        let k = (UserDefaults.standard.string(forKey: "googleKey") ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        return k.isEmpty ? nil : GoogleProvider(key: k)
    }

    func search(_ query: String, near loc: CLLocation) async -> Place? {
        if let g = google {
            if let p = await g.search(query, near: loc) { providerStatus = "Google Maps"; return p }
            providerStatus = "Google failed, using Apple Maps"
        } else { providerStatus = "Apple Maps" }
        return await AppleProvider().search(query, near: loc)
    }

    /// Returns what to say to the rider.
    func begin(to place: Place, from loc: CLLocation) async -> String {
        guard let r = await calculate(to: place, from: loc) else {
            return "I couldn't get directions. Check your internet connection."
        }
        install(r, place)
        var msg = "Navigation started. \(spokenDistance(r.distance)), about \(spokenDuration(r.duration))."
        if let first = steps.first?.instruction, !first.isEmpty { msg += " " + first + "." }
        return msg
    }

    func stop() {
        isNavigating = false
        route = nil; destination = nil; steps = []; phrases = []; junctions = []; routeCoords = []
        if let id = observerID { LocationProvider.shared.removeObserver(id); observerID = nil }
        upcomingPhrase = ""; upcomingMeters = 0; remainingMeters = 0; remainingSeconds = 0
    }

    // MARK: Queries

    func upcoming() -> String {
        guard isNavigating, nextIdx < steps.count, let loc = lastLoc else { return "I'm not navigating." }
        let d = loc.distance(from: CLLocation(latitude: junctions[nextIdx].latitude, longitude: junctions[nextIdx].longitude))
        return "In \(spokenDistance(d)), \(phrases[nextIdx])."
    }

    func remaining() -> String {
        guard let r = remainingValues(lastLoc) else { return "I'm not navigating." }
        return "\(spokenDistance(r.distance)) to go, about \(spokenDuration(r.seconds))."
    }

    private func remainingValues(_ loc: CLLocation?) -> (distance: Double, seconds: TimeInterval)? {
        guard isNavigating, let route, route.distance > 0 else { return nil }
        var rem = steps.dropFirst(nextIdx).reduce(0.0) { $0 + $1.distance }
        if nextIdx < steps.count, let loc {
            rem += loc.distance(from: CLLocation(latitude: junctions[nextIdx].latitude, longitude: junctions[nextIdx].longitude))
        }
        return (rem, route.duration * min(1, rem / route.distance))
    }

    // MARK: Internals

    private func calculate(to place: Place, from loc: CLLocation) async -> NavRoute? {
        if let g = google {
            if let r = await g.route(from: loc, to: place), r.steps.count > 1 { providerStatus = "Google Maps"; return r }
            providerStatus = "Google failed, using Apple Maps"
        } else { providerStatus = "Apple Maps" }
        return await AppleProvider().route(from: loc, to: place)
    }

    private func install(_ r: NavRoute, _ place: Place) {
        route = r
        destination = place
        destinationName = place.name
        steps = r.steps
        routeCoords = r.points
        junctions = [steps.first?.points.first ?? kCLLocationCoordinate2DInvalid]
        phrases = [""]
        for i in 1..<max(steps.count, 1) {
            junctions.append(steps[i - 1].points.last ?? kCLLocationCoordinate2DInvalid)
            phrases.append(plainPhrase(prev: steps[i - 1], next: steps[i], isLast: i == steps.count - 1))
        }
        nextIdx = 1
        announced = [:]
        prevDistance = .infinity
        offRouteCount = 0
        rerouting = false
        isNavigating = steps.count > 1
        if let id = observerID { LocationProvider.shared.removeObserver(id) }
        observerID = LocationProvider.shared.observe { [weak self] loc in self?.update(loc) }
    }

    private func say(_ text: String) { Task { await speak?(text) } }

    private func update(_ loc: CLLocation) {
        guard isNavigating, !rerouting, nextIdx < steps.count else { return }
        lastLoc = loc

        if isOffRoute(loc) {
            offRouteCount += 1
            if offRouteCount >= 4, Date() > rerouteCooldown { Task { await reroute(from: loc) }; return }
        } else { offRouteCount = 0 }

        let j = junctions[nextIdx]
        let d = loc.distance(from: CLLocation(latitude: j.latitude, longitude: j.longitude))
        upcomingPhrase = phrases[nextIdx]
        upcomingMeters = d
        if let r = remainingValues(loc) { remainingMeters = r.distance; remainingSeconds = r.seconds }
        let v = max(loc.speed, 0)
        let far = max(500, v * 25), mid = max(200, v * 10), near = max(50, v * 3.5)
        let level = d <= near ? 3 : d <= mid ? 2 : d <= far ? 1 : 0
        let isLast = nextIdx == steps.count - 1

        if isLast && d < 40 {
            let name = destinationName
            stop()
            say("You have arrived at \(name).")
            return
        }
        if level > (announced[nextIdx] ?? 0) {
            announced[nextIdx] = level
            if !(isLast && level == 3) {
                let p = phrases[nextIdx]
                say(level == 3 ? capitalised(p) + " now." : "In \(spokenDistance(d)), \(p).")
            }
        }
        // Passed the junction: move on to the next maneuver.
        if d < 25 || (level == 3 && d > prevDistance + 15) {
            nextIdx += 1
            prevDistance = .infinity
        } else {
            prevDistance = d
        }
    }

    private func isOffRoute(_ loc: CLLocation) -> Bool {
        guard loc.horizontalAccuracy >= 0, routeCoords.count >= 2 else { return false }
        var best = Double.infinity
        for i in 0..<(routeCoords.count - 1) {
            best = min(best, distToSegment(loc.coordinate, routeCoords[i], routeCoords[i + 1]))
            if best < 20 { return false }
        }
        return best > max(60, loc.horizontalAccuracy * 2)
    }

    private func reroute(from loc: CLLocation) async {
        guard let destination, !rerouting else { return }
        rerouting = true
        say("Rerouting.")
        if let r = await calculate(to: destination, from: loc) {
            install(r, destination)
            if let first = steps.first?.instruction, !first.isEmpty { say(first + ".") }
        } else {
            rerouting = false
            rerouteCooldown = Date().addingTimeInterval(20)
            offRouteCount = 0
            say("I can't reroute right now.")
        }
    }
}
