import CoreLocation

struct Trip: Codable, Identifiable {
    var id = UUID()
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

/// Records each ride (distance, time, speeds, path), raises speed alerts and break reminders.
@MainActor
final class TripRecorder: ObservableObject {
    static let shared = TripRecorder()

    @Published private(set) var isRecording = false
    @Published private(set) var distance = 0.0
    @Published private(set) var movingTime = 0.0
    @Published private(set) var maxSpeed = 0.0
    @Published private(set) var elapsed = 0.0
    @Published private(set) var trips: [Trip] = []

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

    var avgSpeed: Double { movingTime > 0 ? distance / movingTime : 0 }

    func start() {
        guard !isRecording else { return }
        distance = 0; movingTime = 0; maxSpeed = 0; elapsed = 0
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
        if distance >= 200 {
            trips.insert(Trip(start: startDate, end: Date(), distance: distance, movingTime: movingTime,
                              maxSpeed: maxSpeed, points: points), at: 0)
            save()
        }
    }

    func delete(at offsets: IndexSet) { trips.remove(atOffsets: offsets); save() }

    func summary() -> String {
        guard isRecording else { return "No ride in progress." }
        let avg = Int(Prefs.displaySpeed(avgSpeed).rounded()), top = Int(Prefs.displaySpeed(maxSpeed).rounded())
        return "\(spokenDistance(distance)) in \(spokenDuration(elapsed)). Average \(avg), top speed \(top) \(Prefs.speedUnitSpoken)."
    }

    private func tick() {
        elapsed = Date().timeIntervalSince(startDate)
        if elapsed >= nextBreak {
            nextBreak += TimeInterval(max(Prefs.breakMinutes, 15) * 60)
            Task { await speak?("You've been riding for \(spokenDuration(elapsed)). Consider taking a break.") }
        }
    }

    private func handle(_ l: CLLocation) {
        guard l.horizontalAccuracy >= 0, l.horizontalAccuracy < 50 else { return }
        if let last = lastLoc {
            let d = l.distance(from: last), dt = l.timestamp.timeIntervalSince(last.timestamp)
            if dt > 0, dt < 15, d / dt < 80 {   // ignore GPS jumps
                distance += d
                if d / dt > 0.5 { movingTime += dt }
            }
        }
        if l.speed >= 0 { maxSpeed = max(maxSpeed, l.speed) }
        if let p = points.last {
            if l.distance(from: CLLocation(latitude: p[0], longitude: p[1])) >= 25 { points.append([l.coordinate.latitude, l.coordinate.longitude]) }
        } else { points.append([l.coordinate.latitude, l.coordinate.longitude]) }
        lastLoc = l

        let limit = Prefs.speedLimit
        if limit > 0, l.speed >= 0, Prefs.displaySpeed(l.speed) > Double(limit), Date().timeIntervalSince(lastAlert) > 45 {
            lastAlert = Date()
            let now = Int(Prefs.displaySpeed(l.speed).rounded())
            Task { await speak?("Speed alert. \(now) \(Prefs.speedUnitSpoken).") }
        }
    }

    private func save() {
        if let d = try? JSONEncoder().encode(trips) { try? d.write(to: Self.fileURL, options: .atomic) }
    }
}
