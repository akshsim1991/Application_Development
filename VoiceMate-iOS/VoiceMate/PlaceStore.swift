import CoreLocation

struct SavedPlace: Codable, Identifiable, Hashable {
    var id = UUID()
    var name: String
    var lat: Double
    var lon: Double
    var coordinate: CLLocationCoordinate2D { CLLocationCoordinate2D(latitude: lat, longitude: lon) }
    var place: Place { Place(name: name, coordinate: coordinate) }
    static func == (a: SavedPlace, b: SavedPlace) -> Bool { a.id == b.id }
    func hash(into h: inout Hasher) { h.combine(id) }
}

/// Saved places (Home, Work, Parking, anything you name) and recent destinations.
@MainActor
final class PlaceStore: ObservableObject {
    static let shared = PlaceStore()

    @Published private(set) var saved: [SavedPlace] = []
    @Published private(set) var recents: [SavedPlace] = []

    init() {
        saved = Self.load("savedPlaces")
        recents = Self.load("recentPlaces")
        // Migrate the "home" saved by the first version.
        let d = UserDefaults.standard
        if d.object(forKey: "homeLat") != nil, !saved.contains(where: { $0.name.lowercased() == "home" }) {
            saved.append(SavedPlace(name: "Home", lat: d.double(forKey: "homeLat"), lon: d.double(forKey: "homeLon")))
            persist()
        }
    }

    func save(name: String, at c: CLLocationCoordinate2D) {
        let n = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !n.isEmpty else { return }
        saved.removeAll { $0.name.lowercased() == n.lowercased() }
        saved.append(SavedPlace(name: n, lat: c.latitude, lon: c.longitude))
        persist()
    }

    func delete(at offsets: IndexSet) { saved.remove(atOffsets: offsets); persist() }
    func deleteRecent(at offsets: IndexSet) { recents.remove(atOffsets: offsets); persist() }

    func addRecent(_ p: Place) {
        recents.removeAll { $0.name == p.name }
        recents.insert(SavedPlace(name: p.name, lat: p.coordinate.latitude, lon: p.coordinate.longitude), at: 0)
        recents = Array(recents.prefix(10))
        persist()
    }

    /// Exact (case-insensitive) name match, ignoring "my / the / back / to".
    func find(_ query: String) -> SavedPlace? {
        let q = Self.clean(query)
        return saved.first { Self.clean($0.name) == q }
    }

    private static func clean(_ s: String) -> String {
        var t = s.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
        for p in ["to ", "back ", "my ", "the "] where t.hasPrefix(p) { t.removeFirst(p.count) }
        return t
    }

    private static func load(_ key: String) -> [SavedPlace] {
        guard let data = UserDefaults.standard.data(forKey: key),
              let v = try? JSONDecoder().decode([SavedPlace].self, from: data) else { return [] }
        return v
    }

    private func persist() {
        if let d = try? JSONEncoder().encode(saved) { UserDefaults.standard.set(d, forKey: "savedPlaces") }
        if let d = try? JSONEncoder().encode(recents) { UserDefaults.standard.set(d, forKey: "recentPlaces") }
    }
}
