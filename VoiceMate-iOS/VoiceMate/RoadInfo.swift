import CoreLocation

/// Speed cameras and posted speed limits from OpenStreetMap (community data: coverage in India is
/// patchy, so treat it as a helpful extra, not a guarantee).
@MainActor
final class RoadInfo: ObservableObject {
    static let shared = RoadInfo()

    @Published private(set) var limitKmh: Int?
    var speak: ((String) async -> Void)?

    private var cameras: [Int: CLLocationCoordinate2D] = [:]
    private var warned: Set<Int> = []
    private var lastFetchAt = Date.distantPast
    private var lastFetchLoc: CLLocation?
    private var fetching = false
    private var observerID: UUID?

    /// Posted limit in the units you use.
    var limitDisplay: Double? { limitKmh.map { Prefs.imperial ? Double($0) / 1.609 : Double($0) } }

    func start() {
        guard observerID == nil else { return }
        observerID = LocationProvider.shared.observe { [weak self] l in self?.handle(l) }
    }

    func stop() {
        if let id = observerID { LocationProvider.shared.removeObserver(id); observerID = nil }
        limitKmh = nil; cameras = [:]; warned = []
        lastFetchAt = .distantPast; lastFetchLoc = nil
    }

    private func handle(_ l: CLLocation) {
        guard Prefs.cameraAlerts || Prefs.autoLimitAlert else { limitKmh = nil; return }
        let moved = lastFetchLoc.map { l.distance(from: $0) } ?? .infinity
        if !fetching, Date().timeIntervalSince(lastFetchAt) > 60 || moved > 1000 {
            Task { await fetch(l) }
        }
        guard Prefs.cameraAlerts, l.speed > 4, l.course >= 0 else { return }
        for (id, c) in cameras where !warned.contains(id) {
            let d = l.distance(from: CLLocation(latitude: c.latitude, longitude: c.longitude))
            guard d < 450 else { continue }
            var diff = abs(bearing(l.coordinate, c) - l.course)
            if diff > 180 { diff = 360 - diff }
            if diff < 50 {
                warned.insert(id)
                Task { await speak?("Speed camera ahead, \(spokenDistance(d)).") }
            }
        }
    }

    private struct Response: Decodable {
        struct E: Decodable { let type: String; let id: Int; let lat: Double?; let lon: Double?; let tags: [String: String]? }
        let elements: [E]
    }

    private func fetch(_ l: CLLocation) async {
        fetching = true
        defer { fetching = false }
        lastFetchAt = Date()
        lastFetchLoc = l
        let lat = l.coordinate.latitude, lon = l.coordinate.longitude
        let query = "[out:json][timeout:10];(node(around:2500,\(lat),\(lon))[\"highway\"=\"speed_camera\"];"
            + "way(around:25,\(lat),\(lon))[\"maxspeed\"];);out tags center;"
        guard let url = URL(string: "https://overpass-api.de/api/interpreter"),
              let encoded = query.addingPercentEncoding(withAllowedCharacters: .alphanumerics) else { return }
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.timeoutInterval = 12
        req.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        req.httpBody = "data=\(encoded)".data(using: .utf8)
        guard let (data, resp) = try? await URLSession.shared.data(for: req),
              (resp as? HTTPURLResponse)?.statusCode == 200,
              let r = try? JSONDecoder().decode(Response.self, from: data) else { return }
        var limit: Int?
        for e in r.elements {
            if e.type == "node", let la = e.lat, let lo = e.lon {
                cameras[e.id] = CLLocationCoordinate2D(latitude: la, longitude: lo)
            } else if e.type == "way", limit == nil, let ms = e.tags?["maxspeed"] {
                limit = kmh(ms)
            }
        }
        limitKmh = limit
    }

    private func kmh(_ s: String) -> Int? {
        guard let v = Int(s.prefix { $0.isNumber }) else { return nil }
        return s.lowercased().contains("mph") ? Int((Double(v) * 1.609).rounded()) : v
    }
}
