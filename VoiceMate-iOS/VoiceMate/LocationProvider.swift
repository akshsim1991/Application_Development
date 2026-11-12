import CoreLocation

/// Speed / position / heading, usable in the background while Ride Mode is on.
@MainActor
final class LocationProvider: NSObject, CLLocationManagerDelegate {
    static let shared = LocationProvider()

    private let manager = CLLocationManager()
    private(set) var latest: CLLocation?
    /// Called for every new fix (used by Navigator).
    var onUpdate: ((CLLocation) -> Void)?
    private var waiters: [CheckedContinuation<CLLocation?, Never>] = []

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        manager.activityType = .automotiveNavigation
    }

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
        if let loc { latest = loc; onUpdate?(loc) }
        let w = waiters; waiters = []
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
