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
    if Locale.current.measurementSystem == .us {
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

/// Turns MapKit's visual instructions ("take the second exit") into plain directions
/// based on the real geometry: go straight / turn left / turn right.
private func plainPhrase(prev: MKRoute.Step, next: MKRoute.Step, isLast: Bool) -> String {
    let text = next.instructions.lowercased()
    if isLast || text.contains("arrive") || text.contains("destination") { return "you will arrive at your destination" }
    if text.contains("u-turn") { return "make a U-turn" }

    let roundabout = text.contains("roundabout") || text.contains("traffic circle")
    let side = text.contains("left") ? "left" : (text.contains("right") ? "right" : nil)

    var angle: Double?
    if let a = endBearing(prev.polyline.coords) {
        let nextPts = next.polyline.coords
        // Through a roundabout the exit direction is where the step finally ends up.
        if let b = roundabout ? endBearing(nextPts) : startBearing(nextPts) {
            var d = (b - a).truncatingRemainder(dividingBy: 360)
            if d > 180 { d -= 360 }
            if d < -180 { d += 360 }
            angle = d
        }
    }

    func turn(_ d: Double) -> String {
        let s = d < 0 ? "left" : "right", m = abs(d)
        if m < 25 { return "go straight" }
        if m < 60 { return "bear \(s)" }
        if m < 120 { return "turn \(s)" }
        if m < 165 { return "take a sharp \(s)" }
        return "make a U-turn"
    }

    var core: String
    if text.contains("keep left") { core = "keep left" }
    else if text.contains("keep right") { core = "keep right" }
    else if text.contains("exit") && !roundabout { core = "take the exit" + (side.map { " on the \($0)" } ?? "") }
    else if let angle { core = turn(angle) }
    else if let side { core = "turn \(side)" }
    else { core = "continue straight" }
    if roundabout { core = "at the roundabout, " + core }
    return core
}

private func capitalised(_ s: String) -> String { s.prefix(1).uppercased() + s.dropFirst() }

// MARK: - Navigator

/// Audio-only turn-by-turn navigation: announces plain directions with distances, reroutes when
/// you leave the route, and answers "what's next" / "how far". Uses Apple Maps routing (MapKit).
@MainActor
final class Navigator: ObservableObject {
    static let shared = Navigator()

    @Published private(set) var isNavigating = false
    @Published private(set) var destinationName = ""

    /// Set by RideController; speaks through the helmet.
    var speak: ((String) async -> Void)?

    private var route: MKRoute?
    private var destination: MKMapItem?
    private var steps: [MKRoute.Step] = []
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

    // MARK: Search & start

    func search(_ query: String, near loc: CLLocation) async -> MKMapItem? {
        let req = MKLocalSearch.Request()
        req.naturalLanguageQuery = query
        req.region = MKCoordinateRegion(center: loc.coordinate, latitudinalMeters: 60_000, longitudinalMeters: 60_000)
        req.resultTypes = [.pointOfInterest, .address]
        return try? await MKLocalSearch(request: req).start().mapItems.first
    }

    /// Returns what to say to the rider.
    func begin(to item: MKMapItem, from loc: CLLocation) async -> String {
        guard let r = await calculate(to: item, from: loc) else {
            return "I couldn't get directions. Check your internet connection."
        }
        install(r, item)
        var msg = "Navigation started. \(spokenDistance(r.distance)), about \(spokenDuration(r.expectedTravelTime))."
        if let first = steps.first?.instructions, !first.isEmpty { msg += " " + first + "." }
        return msg
    }

    func stop() {
        isNavigating = false
        route = nil; destination = nil; steps = []; phrases = []; junctions = []; routeCoords = []
        LocationProvider.shared.onUpdate = nil
    }

    // MARK: Queries

    func upcoming() -> String {
        guard isNavigating, nextIdx < steps.count, let loc = lastLoc else { return "I'm not navigating." }
        let d = loc.distance(from: CLLocation(latitude: junctions[nextIdx].latitude, longitude: junctions[nextIdx].longitude))
        return "In \(spokenDistance(d)), \(phrases[nextIdx])."
    }

    func remaining() -> String {
        guard isNavigating, let route, route.distance > 0 else { return "I'm not navigating." }
        var rem = steps.dropFirst(nextIdx).reduce(0) { $0 + $1.distance }
        if nextIdx < steps.count, let loc = lastLoc {
            rem += loc.distance(from: CLLocation(latitude: junctions[nextIdx].latitude, longitude: junctions[nextIdx].longitude))
        }
        let secs = route.expectedTravelTime * min(1, rem / route.distance)
        return "\(spokenDistance(rem)) to go, about \(spokenDuration(secs))."
    }

    // MARK: Internals

    private func calculate(to item: MKMapItem, from loc: CLLocation) async -> MKRoute? {
        let req = MKDirections.Request()
        req.source = MKMapItem(placemark: MKPlacemark(coordinate: loc.coordinate))
        req.destination = item
        req.transportType = .automobile
        return try? await MKDirections(request: req).calculate().routes.first
    }

    private func install(_ r: MKRoute, _ item: MKMapItem) {
        route = r
        destination = item
        destinationName = item.name ?? "destination"
        steps = r.steps.filter { !$0.instructions.isEmpty }
        routeCoords = r.polyline.coords
        junctions = [steps.first?.polyline.coords.first ?? kCLLocationCoordinate2DInvalid]
        phrases = [""]
        for i in 1..<max(steps.count, 1) {
            junctions.append(steps[i - 1].polyline.coords.last ?? kCLLocationCoordinate2DInvalid)
            phrases.append(plainPhrase(prev: steps[i - 1], next: steps[i], isLast: i == steps.count - 1))
        }
        nextIdx = 1
        announced = [:]
        prevDistance = .infinity
        offRouteCount = 0
        rerouting = false
        isNavigating = steps.count > 1
        LocationProvider.shared.onUpdate = { [weak self] loc in self?.update(loc) }
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
            if let first = steps.first?.instructions, !first.isEmpty { say(first + ".") }
        } else {
            rerouting = false
            rerouteCooldown = Date().addingTimeInterval(20)
            offRouteCount = 0
            say("I can't reroute right now.")
        }
    }
}
