import CoreLocation

struct Trip: Codable, Identifiable {
    var id = UUID()
    var name: String? = nil
    var start: Date
    var end: Date
    var distance: Double
    var movingTime: TimeInterval
    var maxSpeed: Double
    var points: [[Double]]   // [lat, lon]
    var duration: TimeInterval { end.timeIntervalSince(start) }
    var avgSpeed: Double { movingTime > 0 ? distance / movingTime : 0 }
    var coordinates: [CLLocationCoordinate2D] { points.map { CLLocationCoordinate2D(latitude: $0[0], longitude: $0[1]) } }
}

/// Numbers that change every second. Kept apart from the trip list so that only the live stat tiles
/// redraw, not every screen that shows trips.
@MainActor
final class LiveStats: ObservableObject {
    @Published var distance = 0.0
    @Published var movingTime = 0.0
    @Published var maxSpeed = 0.0
    @Published var elapsed = 0.0
    var avgSpeed: Double { movingTime > 0 ? distance / movingTime : 0 }
}

/// Records each ride (distance, time, speeds, path), raises speed alerts and break reminders.
@MainActor
final class TripRecorder: ObservableObject {
    static let shared = TripRecorder()

    @Published private(set) var isRecording = false
    @Published private(set) var trips: [Trip] = []
    let live = LiveStats()

    /// Set by RideController; speaks through the helmet.
    var speak: ((String) async -> Void)?

    private var startDate = Date()
    private var lastLoc: CLLocation?
    private var points: [[Double]] = []
    private var observerID: UUID?
    private var timer: Timer?
    private var lastAlert = Date.distantPast
    private var nextBreak = TimeInterval.infinity

    private static var fileURL: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("trips.json")
    }

    init() {
        if let data = try? Data(contentsOf: Self.fileURL),
           let v = try? JSONDecoder().decode([Trip].self, from: data) { trips = v }
    }

    func start() {
        guard !isRecording else { return }
        live.distance = 0; live.movingTime = 0; live.maxSpeed = 0; live.elapsed = 0
        points = []; lastLoc = nil
        startDate = Date()
        lastAlert = .distantPast
        let b = Prefs.breakMinutes
        nextBreak = b > 0 ? TimeInterval(b * 60) : .infinity
        isRecording = true
        observerID = LocationProvider.shared.observe { [weak self] l in self?.handle(l) }
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.tick() }
        }
    }

    func stop() {
        guard isRecording else { return }
        isRecording = false
        timer?.invalidate(); timer = nil
        if let id = observerID { LocationProvider.shared.removeObserver(id); observerID = nil }
        Garage.shared.addDistance(meters: live.distance)
        if live.distance >= 200 {
            trips.insert(Trip(start: startDate, end: Date(), distance: live.distance, movingTime: live.movingTime,
                              maxSpeed: live.maxSpeed, points: points), at: 0)
            save()
        }
        live.distance = 0; live.movingTime = 0; live.maxSpeed = 0; live.elapsed = 0
    }

    func delete(at offsets: IndexSet) { trips.remove(atOffsets: offsets); save() }
    func delete(id: UUID) { trips.removeAll { $0.id == id }; save() }

    func rename(_ id: UUID, to name: String) {
        guard let i = trips.firstIndex(where: { $0.id == id }) else { return }
        let n = name.trimmingCharacters(in: .whitespacesAndNewlines)
        trips[i].name = n.isEmpty ? nil : n
        save()
    }

    func summary() -> String {
        guard isRecording else { return "No ride in progress." }
        let avg = Int(Prefs.displaySpeed(live.avgSpeed).rounded()), top = Int(Prefs.displaySpeed(live.maxSpeed).rounded())
        return "\(spokenDistance(live.distance)) in \(spokenDuration(live.elapsed)). Average \(avg), top speed \(top) \(Prefs.speedUnitSpoken)."
    }

    private func tick() {
        live.elapsed = Date().timeIntervalSince(startDate)
        if live.elapsed >= nextBreak {
            nextBreak += TimeInterval(max(Prefs.breakMinutes, 15) * 60)
            Task { await speak?("You've been riding for \(spokenDuration(live.elapsed)). Consider taking a break.") }
        }
    }

    private func handle(_ l: CLLocation) {
        // Poor accuracy (train or bus window, tunnels) is still fine for speed-based tracking.
        guard l.horizontalAccuracy >= 0, l.horizontalAccuracy < 200 else { return }
        let gps = LocationProvider.shared
        let v = gps.speedMps
        // GPS jitter while standing still reports small speeds and wandering positions: only count real movement.
        let moving = gps.speedKnown && v >= 1.5
        if let last = lastLoc {
            let dt = l.timestamp.timeIntervalSince(last.timestamp)
            if moving, dt > 0, dt < 15 {
                let good = l.horizontalAccuracy < 50 && last.horizontalAccuracy < 50
                let d = good ? l.distance(from: last) : v * dt   // weak signal: integrate speed instead of positions
                if d / dt < 100 {                                // ignore GPS jumps
                    live.distance += d
                    live.movingTime += dt
                }
            }
        }
        if moving, v < 100, v > live.maxSpeed { live.maxSpeed = v }
        if moving, l.horizontalAccuracy < 100 {
            let p = [l.coordinate.latitude, l.coordinate.longitude]
            if let last = points.last {
                if l.distance(from: CLLocation(latitude: last[0], longitude: last[1])) >= 25 { points.append(p) }
            } else { points.append(p) }
        }
        lastLoc = l

        let manual = Prefs.speedLimit
        let limit: Double? = manual > 0 ? Double(manual) : (Prefs.autoLimitAlert ? RoadInfo.shared.limitDisplay : nil)
        if let limit, gps.speedKnown, Prefs.displaySpeed(v) > limit + (manual > 0 ? 0 : 5), Date().timeIntervalSince(lastAlert) > 45 {
            lastAlert = Date()
            let now = Int(Prefs.displaySpeed(v).rounded())
            Task { await speak?("Speed alert. \(now) \(Prefs.speedUnitSpoken).") }
        }
    }

    private func save() {
        if let d = try? JSONEncoder().encode(trips) { try? d.write(to: Self.fileURL, options: .atomic) }
    }
}
