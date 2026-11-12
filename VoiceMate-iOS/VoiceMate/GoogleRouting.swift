import CoreLocation

/// Google Routes API (two-wheeler mode, which works in India) + Places API (New) for destination search.
/// Needs an API key with billing enabled; paste it into the app (see README).
struct GoogleProvider: RoutingProvider {
    let key: String

    // MARK: Search

    func search(_ query: String, near loc: CLLocation, nearest: Bool, limit: Int) async -> [Place] {
        var body: [String: Any] = [
            "textQuery": query,
            "pageSize": limit,
            "locationBias": ["circle": [
                "center": ["latitude": loc.coordinate.latitude, "longitude": loc.coordinate.longitude],
                "radius": 50_000.0
            ]]
        ]
        if nearest { body["rankPreference"] = "DISTANCE" }
        if let region = Locale.current.region?.identifier { body["regionCode"] = region }
        guard let data = await post("https://places.googleapis.com/v1/places:searchText",
                                    mask: "places.displayName,places.formattedAddress,places.location", body: body),
              let r = try? JSONDecoder().decode(PlacesResponse.self, from: data) else { return [] }
        return (r.places ?? []).compactMap { p in
            guard let l = p.location else { return nil }
            return Place(name: p.displayName?.text ?? p.formattedAddress ?? query,
                         coordinate: CLLocationCoordinate2D(latitude: l.latitude, longitude: l.longitude))
        }
    }

    // MARK: Routes

    func routes(from loc: CLLocation, to place: Place, options: RouteOptions) async -> [NavRoute] {
        let mask = "routes.duration,routes.distanceMeters,routes.polyline.encodedPolyline,"
            + "routes.legs.steps.navigationInstruction,routes.legs.steps.distanceMeters,routes.legs.steps.polyline.encodedPolyline"
        for mode in ["TWO_WHEELER", "DRIVE"] {
            var origin: [String: Any] = ["latLng": ["latitude": loc.coordinate.latitude, "longitude": loc.coordinate.longitude]]
            if loc.course >= 0, loc.speed > 2 { origin["heading"] = Int(loc.course) }
            var body: [String: Any] = [
                "origin": ["location": origin],
                "destination": ["location": ["latLng": ["latitude": place.coordinate.latitude, "longitude": place.coordinate.longitude]]],
                "travelMode": mode,
                "polylineEncoding": "ENCODED_POLYLINE",
                "languageCode": "en-IN",
                "units": "METRIC"
            ]
            if options.avoidTolls || options.avoidHighways {
                body["routeModifiers"] = ["avoidTolls": options.avoidTolls, "avoidHighways": options.avoidHighways]
            }
            if !options.vias.isEmpty {
                body["intermediates"] = options.vias.prefix(25).map { v in
                    ["location": ["latLng": ["latitude": v.latitude, "longitude": v.longitude]], "via": true] as [String: Any]
                }
            } else if options.alternatives {
                body["computeAlternativeRoutes"] = true
            }
            guard let data = await post("https://routes.googleapis.com/directions/v2:computeRoutes", mask: mask, body: body),
                  let r = try? JSONDecoder().decode(RoutesResponse.self, from: data),
                  let routes = r.routes, !routes.isEmpty else { continue }
            return routes.map(convert)
        }
        return []
    }

    private func convert(_ r: RoutesResponse.Route) -> NavRoute {
        var steps: [NavStep] = []
        for s in (r.legs ?? []).flatMap({ $0.steps ?? [] }) {
            let pts = decodePolyline(s.polyline?.encodedPolyline ?? "")
            let text = s.navigationInstruction?.instructions ?? ""
            var man = s.navigationInstruction?.maneuver
            if man == "MANEUVER_UNSPECIFIED" { man = nil }
            let d = Double(s.distanceMeters ?? 0)
            if text.isEmpty, var last = steps.popLast() {   // no instruction: just a continuation
                last.points += pts
                last.distance += d
                steps.append(last)
            } else {
                steps.append(NavStep(instruction: text, maneuver: man, points: pts, distance: d))
            }
        }
        if let end = steps.last?.points.last {
            steps.append(NavStep(instruction: "Arrive at destination", maneuver: "ARRIVE", points: [end], distance: 0))
        }
        let secs = Double(r.duration?.dropLast() ?? "") ?? 0
        let pts = decodePolyline(r.polyline?.encodedPolyline ?? "")
        return NavRoute(distance: Double(r.distanceMeters ?? 0), duration: secs, points: pts, steps: steps,
                        curviness: routeCurviness(pts))
    }

    // MARK: HTTP

    private func post(_ url: String, mask: String, body: [String: Any]) async -> Data? {
        guard let u = URL(string: url), let payload = try? JSONSerialization.data(withJSONObject: body) else { return nil }
        var req = URLRequest(url: u)
        req.httpMethod = "POST"
        req.httpBody = payload
        req.timeoutInterval = 15
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")
        req.setValue(key, forHTTPHeaderField: "X-Goog-Api-Key")
        req.setValue(mask, forHTTPHeaderField: "X-Goog-FieldMask")
        // Lets you restrict the key to this iOS app in Google Cloud.
        if let id = Bundle.main.bundleIdentifier { req.setValue(id, forHTTPHeaderField: "X-Ios-Bundle-Identifier") }
        guard let (data, resp) = try? await URLSession.shared.data(for: req),
              (resp as? HTTPURLResponse)?.statusCode == 200 else { return nil }
        return data
    }
}

// MARK: - JSON

private struct PlacesResponse: Decodable {
    struct Name: Decodable { let text: String }
    struct Loc: Decodable { let latitude: Double; let longitude: Double }
    struct P: Decodable { let displayName: Name?; let formattedAddress: String?; let location: Loc? }
    let places: [P]?
}

private struct RoutesResponse: Decodable {
    struct Poly: Decodable { let encodedPolyline: String? }
    struct Instr: Decodable { let maneuver: String?; let instructions: String? }
    struct Step: Decodable { let distanceMeters: Int?; let polyline: Poly?; let navigationInstruction: Instr? }
    struct Leg: Decodable { let steps: [Step]? }
    struct Route: Decodable { let distanceMeters: Int?; let duration: String?; let polyline: Poly?; let legs: [Leg]? }
    let routes: [Route]?
}

/// Standard Google encoded-polyline decoder (precision 1e5).
func decodePolyline(_ s: String) -> [CLLocationCoordinate2D] {
    let u = Array(s.utf8)
    var i = 0, lat = 0, lon = 0
    var out: [CLLocationCoordinate2D] = []

    func nextValue() -> Int? {
        var result = 0, shift = 0, b = 0
        repeat {
            guard i < u.count else { return nil }
            b = Int(u[i]) - 63
            i += 1
            result |= (b & 0x1f) << shift
            shift += 5
        } while b >= 0x20
        return (result & 1) != 0 ? ~(result >> 1) : (result >> 1)
    }

    while i < u.count {
        guard let dLat = nextValue(), let dLon = nextValue() else { break }
        lat += dLat
        lon += dLon
        out.append(CLLocationCoordinate2D(latitude: Double(lat) / 1e5, longitude: Double(lon) / 1e5))
    }
    return out
}
