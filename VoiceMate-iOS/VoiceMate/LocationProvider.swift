import CoreLocation

enum GPSLevel {
    case off          // no location permission
    case searching    // permission OK, no fix yet / fix lost
    case poor, fair, good, strong

    var title: String {
        switch self {
        case .off: return "Location off"
        case .searching: return "Searching for GPS"
        case .poor: return "GPS poor"
        case .fair: return "GPS fair"
        case .good: return "GPS good"
        case .strong: return "GPS strong"
        }
    }
    /// 0...1 for the signal-bars icon.
    var bars: Double {
        switch self {
        case .off, .searching: return 0
        case .poor: return 0.25
        case .fair: return 0.5
        case .good: return 0.75
        case .strong: return 1
        }
    }
}

/// Speed / position, usable in the background while Ride Mode is on. Several parts of the app
/// (navigation, trip recorder, dashboard) observe it.
///
/// `speedMps` is the best speed available: the GPS speed when the phone reports one, otherwise speed
/// computed from consecutive positions (so it still works in a train or bus with a weak signal).
@MainActor
final class LocationProvider: NSObject, ObservableObject, CLLocationManagerDelegate {
    static let shared = LocationProvider()

    @Published private(set) var latest: CLLocation?
    @Published private(set) var speedMps = 0.0
    @Published private(set) var speedKnown = false
    @Published private(set) var gpsLevel: GPSLevel = .searching
    @Published private(set) var gpsAccuracy: Double?
    @Published private(set) var authorized = false
    @Published private(set) var reducedAccuracy = false

    private let manager = CLLocationManager()
    private var waiters: [CheckedContinuation<CLLocation?, Never>] = []
    private var observers: [UUID: (CLLocation) -> Void] = [:]
    private var previous: CLLocation?
    private var lastFixAt = Date.distantPast
    private var wantsUpdates = false
    private var background = false
    private var staleTimer: Timer?

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        manager.activityType = .automotiveNavigation
        manager.pausesLocationUpdatesAutomatically = false
        refreshAuthorization()
    }

    @discardableResult
    func observe(_ f: @escaping (CLLocation) -> Void) -> UUID {
        let id = UUID()
        observers[id] = f
        return id
    }

    func removeObserver(_ id: UUID) { observers[id] = nil }

    // MARK: Start / stop

    /// Foreground-only updates so the speedometer works even before you start a ride.
    func startPreview() {
        guard !wantsUpdates else { return }
        wantsUpdates = true
        manager.requestWhenInUseAuthorization()
        manager.startUpdatingLocation()
        startStaleTimer()
        refreshAuthorization()
    }

    func stopPreview() {
        guard !background else { return }
        stopUpdates()
    }

    /// Ride Mode: keeps running with the screen locked.
    func startContinuous() {
        background = true
        wantsUpdates = true
        manager.requestWhenInUseAuthorization()
        manager.allowsBackgroundLocationUpdates = true
        manager.showsBackgroundLocationIndicator = true
        manager.startUpdatingLocation()
        startStaleTimer()
        refreshAuthorization()
    }

    func stopContinuous() {
        background = false
        manager.allowsBackgroundLocationUpdates = false
        stopUpdates()
    }

    private func stopUpdates() {
        wantsUpdates = false
        manager.stopUpdatingLocation()
        staleTimer?.invalidate(); staleTimer = nil
        previous = nil
        speedMps = 0; speedKnown = false; gpsAccuracy = nil
        gpsLevel = authorized ? .searching : .off
    }

    private func startStaleTimer() {
        staleTimer?.invalidate()
        staleTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.tick() }
        }
    }

    /// Latest fix if it is recent, otherwise a one-shot request (nil if it fails).
    func freshLocation() async -> CLLocation? {
        if let l = latest, -l.timestamp.timeIntervalSinceNow < 10 { return l }
        manager.requestWhenInUseAuthorization()
        return await withCheckedContinuation { c in
            waiters.append(c)
            manager.requestLocation()
        }
    }

    // MARK: State

    private func refreshAuthorization() {
        let s = manager.authorizationStatus
        authorized = (s == .authorizedWhenInUse || s == .authorizedAlways)
        reducedAccuracy = authorized && manager.accuracyAuthorization == .reducedAccuracy
        if authorized, wantsUpdates { manager.startUpdatingLocation() }
        if reducedAccuracy { manager.requestTemporaryFullAccuracyAuthorization(withPurposeKey: "Navigation") }
        updateLevel()
    }

    private func updateLevel() {
        guard authorized else { gpsLevel = .off; return }
        guard wantsUpdates, Date().timeIntervalSince(lastFixAt) < 10, let a = gpsAccuracy, a >= 0 else {
            gpsLevel = .searching
            return
        }
        switch a {
        case ..<10: gpsLevel = .strong
        case ..<30: gpsLevel = .good
        case ..<100: gpsLevel = .fair
        default: gpsLevel = .poor
        }
    }

    /// Runs every 2 s: speed falls back to 0 and the signal to "searching" if fixes stop arriving.
    private func tick() {
        if Date().timeIntervalSince(lastFixAt) > 5 { speedMps = 0; speedKnown = false }
        updateLevel()
    }

    private func deliver(_ loc: CLLocation?) {
        if let loc {
            latest = loc
            lastFixAt = Date()
            gpsAccuracy = loc.horizontalAccuracy >= 0 ? loc.horizontalAccuracy : nil

            // GPS speed when valid; otherwise worked out from the last two positions.
            if loc.speed >= 0 {
                speedMps = loc.speed
                speedKnown = true
            } else if let p = previous {
                let dt = loc.timestamp.timeIntervalSince(p.timestamp)
                if dt >= 0.5, dt <= 10, loc.horizontalAccuracy >= 0, loc.horizontalAccuracy < 150 {
                    speedMps = loc.distance(from: p) / dt
                    speedKnown = true
                } else { speedKnown = false }
            } else { speedKnown = false }
            previous = loc
            updateLevel()
            for f in Array(observers.values) { f(loc) }
        }
        let w = waiters
        waiters = []
        w.forEach { $0.resume(returning: loc ?? latest) }
    }

    nonisolated func locationManager(_ m: CLLocationManager, didUpdateLocations locs: [CLLocation]) {
        guard let l = locs.last else { return }
        Task { @MainActor in self.deliver(l) }
    }

    nonisolated func locationManager(_ m: CLLocationManager, didFailWithError error: Error) {
        Task { @MainActor in self.deliver(nil) }
    }

    nonisolated func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        Task { @MainActor in self.refreshAuthorization() }
    }
}
