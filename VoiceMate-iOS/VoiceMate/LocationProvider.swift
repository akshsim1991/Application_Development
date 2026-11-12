import CoreLocation

/// Speed / position, usable in the background while Ride Mode is on. Several parts of the app
/// (navigation, trip recorder, dashboard) observe it.
@MainActor
final class LocationProvider: NSObject, ObservableObject, CLLocationManagerDelegate {
    static let shared = LocationProvider()

    @Published private(set) var latest: CLLocation?

    private let manager = CLLocationManager()
    private var waiters: [CheckedContinuation<CLLocation?, Never>] = []
    private var observers: [UUID: (CLLocation) -> Void] = [:]

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        manager.activityType = .automotiveNavigation
    }

    @discardableResult
    func observe(_ f: @escaping (CLLocation) -> Void) -> UUID {
        let id = UUID()
        observers[id] = f
        return id
    }

    func removeObserver(_ id: UUID) { observers[id] = nil }

    func startContinuous() {
        manager.requestWhenInUseAuthorization()
        manager.allowsBackgroundLocationUpdates = true
        manager.pausesLocationUpdatesAutomatically = false
        manager.showsBackgroundLocationIndicator = true
        manager.startUpdatingLocation()
    }

    func stopContinuous() {
        manager.stopUpdatingLocation()
        manager.allowsBackgroundLocationUpdates = false
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

    private func deliver(_ loc: CLLocation?) {
        if let loc {
            latest = loc
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
}
