import CoreLocation

/// Export rides as GPX and import GPX routes.
enum GPX {
    static func xml(for trip: Trip) -> String {
        let iso = ISO8601DateFormatter()
        var s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\" creator=\"RideVoice\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
        s += "<trk><name>RideVoice ride \(iso.string(from: trip.start))</name><trkseg>\n"
        for p in trip.points where p.count == 2 {
            s += "<trkpt lat=\"\(p[0])\" lon=\"\(p[1])\"></trkpt>\n"
        }
        s += "</trkseg></trk>\n</gpx>\n"
        return s
    }

    static func writeFile(for trip: Trip) -> URL? {
        let f = DateFormatter()
        f.dateFormat = "yyyy-MM-dd-HHmm"
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("RideVoice-\(f.string(from: trip.start)).gpx")
        do { try xml(for: trip).write(to: url, atomically: true, encoding: .utf8); return url } catch { return nil }
    }

    /// Prefers track points, then route points, then waypoints.
    static func parse(_ data: Data) -> (name: String, points: [CLLocationCoordinate2D]) {
        let delegate = GPXParser()
        let parser = XMLParser(data: data)
        parser.delegate = delegate
        parser.parse()
        let pts = !delegate.trk.isEmpty ? delegate.trk : (!delegate.rte.isEmpty ? delegate.rte : delegate.wpt)
        return (delegate.name, pts)
    }
}

private final class GPXParser: NSObject, XMLParserDelegate {
    var trk: [CLLocationCoordinate2D] = []
    var rte: [CLLocationCoordinate2D] = []
    var wpt: [CLLocationCoordinate2D] = []
    var name = ""
    private var text = ""
    private var capturing = false
    private var gotName = false

    func parser(_ parser: XMLParser, didStartElement element: String, namespaceURI: String?,
                qualifiedName: String?, attributes a: [String: String] = [:]) {
        if let lat = Double(a["lat"] ?? ""), let lon = Double(a["lon"] ?? "") {
            let c = CLLocationCoordinate2D(latitude: lat, longitude: lon)
            switch element {
            case "trkpt": trk.append(c)
            case "rtept": rte.append(c)
            case "wpt": wpt.append(c)
            default: break
            }
        }
        if element == "name", !gotName { capturing = true; text = "" }
    }

    func parser(_ parser: XMLParser, foundCharacters string: String) { if capturing { text += string } }

    func parser(_ parser: XMLParser, didEndElement element: String, namespaceURI: String?, qualifiedName: String?) {
        if element == "name", capturing {
            name = text.trimmingCharacters(in: .whitespacesAndNewlines)
            capturing = false
            gotName = true
        }
    }
}
