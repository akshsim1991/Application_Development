import CoreLocation

/// Spoken weather from Open-Meteo (free, no key).
enum Weather {
    private struct Response: Decodable {
        struct Current: Decodable {
            let temperature_2m: Double
            let weather_code: Int
            let wind_speed_10m: Double
        }
        struct Hourly: Decodable { let precipitation_probability: [Int?]? }
        let current: Current
        let hourly: Hourly?
    }

    static func summary(at loc: CLLocation) async -> String {
        let imp = Prefs.imperial
        var comps = URLComponents(string: "https://api.open-meteo.com/v1/forecast")!
        comps.queryItems = [
            .init(name: "latitude", value: String(loc.coordinate.latitude)),
            .init(name: "longitude", value: String(loc.coordinate.longitude)),
            .init(name: "current", value: "temperature_2m,weather_code,wind_speed_10m"),
            .init(name: "hourly", value: "precipitation_probability"),
            .init(name: "forecast_hours", value: "3"),
            .init(name: "timezone", value: "auto"),
            .init(name: "temperature_unit", value: imp ? "fahrenheit" : "celsius"),
            .init(name: "wind_speed_unit", value: imp ? "mph" : "kmh")
        ]
        guard let url = comps.url,
              let (data, resp) = try? await URLSession.shared.data(from: url),
              (resp as? HTTPURLResponse)?.statusCode == 200,
              let r = try? JSONDecoder().decode(Response.self, from: data) else {
            return "I couldn't get the weather. Check your internet connection."
        }
        var text = "It's \(Int(r.current.temperature_2m.rounded())) degrees and \(describe(r.current.weather_code)), "
            + "wind \(Int(r.current.wind_speed_10m.rounded())) \(imp ? "miles" : "kilometres") per hour."
        if let p = r.hourly?.precipitation_probability?.compactMap({ $0 }).max(), p >= 20 {
            text += " Chance of rain in the next 3 hours: \(p) percent."
        }
        return text
    }

    private static func describe(_ code: Int) -> String {
        switch code {
        case 0: return "clear"
        case 1, 2: return "partly cloudy"
        case 3: return "overcast"
        case 45, 48: return "foggy"
        case 51...57: return "drizzling"
        case 61...67: return "raining"
        case 71...77: return "snowing"
        case 80...82: return "showery"
        case 95...99: return "thunderstorms"
        default: return "unsettled"
        }
    }
}
