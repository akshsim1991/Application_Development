import Foundation

struct FuelEntry: Codable, Identifiable {
    var id = UUID()
    var date: Date
    var litres: Double
    var pricePerLitre: Double
    var odometer: Double   // km at the time of the fill-up
    var total: Double { litres * pricePerLitre }
}

struct ServiceItem: Codable, Identifiable {
    var id = UUID()
    var name: String
    var intervalKm: Double
    var lastDoneKm: Double
    func remainingKm(odometer: Double) -> Double { lastDoneKm + intervalKm - odometer }
}

struct DocItem: Codable, Identifiable {
    var id = UUID()
    var name: String
    var expiry: Date
    var daysLeft: Int { Calendar.current.dateComponents([.day], from: Calendar.current.startOfDay(for: Date()),
                                                       to: Calendar.current.startOfDay(for: expiry)).day ?? 0 }
}

/// Odometer (grows with your rides), fuel log & mileage, service reminders, document expiry dates.
/// Distances here are in kilometres.
@MainActor
final class Garage: ObservableObject {
    static let shared = Garage()

    @Published var odometerKm: Double { didSet { UserDefaults.standard.set(odometerKm, forKey: "odometerKm") } }
    @Published private(set) var fuel: [FuelEntry] = []
    @Published private(set) var services: [ServiceItem] = []
    @Published private(set) var docs: [DocItem] = []

    init() {
        odometerKm = UserDefaults.standard.double(forKey: "odometerKm")
        fuel = Self.load("fuelLog")
        services = Self.load("services")
        docs = Self.load("docs")
        if !UserDefaults.standard.bool(forKey: "garageSeeded") {
            UserDefaults.standard.set(true, forKey: "garageSeeded")
            services = [
                ServiceItem(name: "Engine oil", intervalKm: 3000, lastDoneKm: odometerKm),
                ServiceItem(name: "Chain lube", intervalKm: 500, lastDoneKm: odometerKm),
                ServiceItem(name: "General service", intervalKm: 6000, lastDoneKm: odometerKm)
            ]
            persist()
        }
    }

    // MARK: Odometer

    func addDistance(meters: Double) { odometerKm += meters / 1000 }

    // MARK: Fuel

    var fuelSorted: [FuelEntry] { fuel.sorted { $0.date < $1.date } }

    /// km per litre for each fill-up after the first (assumes fill-to-full).
    var mileages: [Double] {
        let f = fuelSorted
        guard f.count >= 2 else { return [] }
        return (1..<f.count).compactMap { i in
            let dist = f[i].odometer - f[i - 1].odometer
            return dist > 0 && f[i].litres > 0 ? dist / f[i].litres : nil
        }
    }
    var lastMileage: Double? { mileages.last }
    var averageMileage: Double? {
        let f = fuelSorted
        guard f.count >= 2 else { return nil }
        let dist = f.last!.odometer - f.first!.odometer
        let litres = f.dropFirst().reduce(0) { $0 + $1.litres }
        return dist > 0 && litres > 0 ? dist / litres : nil
    }
    var totalSpent: Double { fuel.reduce(0) { $0 + $1.total } }
    var costPerKm: Double? {
        let f = fuelSorted
        guard f.count >= 2 else { return nil }
        let dist = f.last!.odometer - f.first!.odometer
        let spent = f.dropFirst().reduce(0) { $0 + $1.total }
        return dist > 0 ? spent / dist : nil
    }

    @discardableResult
    func logFuel(litres: Double, pricePerLitre: Double) -> String {
        fuel.append(FuelEntry(date: Date(), litres: litres, pricePerLitre: pricePerLitre, odometer: odometerKm))
        persist()
        var msg = "Logged \(trim(litres)) litres"
        if pricePerLitre > 0 { msg += " at \(trim(pricePerLitre))" }
        msg += "."
        if let m = lastMileage { msg += " Mileage \(trim(m)) kilometres per litre." }
        return msg
    }

    func deleteFuel(ids: [UUID]) { fuel.removeAll { ids.contains($0.id) }; persist() }

    func mileageSummary() -> String {
        guard let avg = averageMileage else { return "I need at least two fill-ups to work out mileage. Say log fuel, 8 litres." }
        var s = "Average mileage \(trim(avg)) kilometres per litre."
        if let l = lastMileage { s += " Last fill-up \(trim(l))." }
        if let c = costPerKm { s += " Fuel costs about \(trim(c * 100)) per 100 kilometres." }
        return s
    }

    // MARK: Service & documents

    func addService(name: String, intervalKm: Double) {
        services.append(ServiceItem(name: name, intervalKm: intervalKm, lastDoneKm: odometerKm)); persist()
    }
    func markDone(_ id: UUID) {
        if let i = services.firstIndex(where: { $0.id == id }) { services[i].lastDoneKm = odometerKm; persist() }
    }
    func deleteService(at offsets: IndexSet) { services.remove(atOffsets: offsets); persist() }

    /// "I changed the engine oil" -> marks the matching reminder done.
    func markDone(matching q: String) -> String? {
        let text = q.lowercased()
        guard let i = services.firstIndex(where: { s in
            s.name.lowercased().split(separator: " ").contains { $0.count >= 4 && text.contains($0) }
        }) else { return nil }
        services[i].lastDoneKm = odometerKm
        persist()
        return services[i].name
    }

    func addDoc(name: String, expiry: Date) { docs.append(DocItem(name: name, expiry: expiry)); persist() }
    func deleteDoc(at offsets: IndexSet) { docs.remove(atOffsets: offsets); persist() }

    func serviceSummary() -> String {
        guard !services.isEmpty else { return "You have no service reminders. Add some in the Garage tab." }
        return services.map { s in
            let left = s.remainingKm(odometer: odometerKm)
            return left <= 0 ? "\(s.name) is overdue by \(Int(-left)) kilometres." : "\(s.name), \(Int(left)) kilometres left."
        }.joined(separator: " ")
    }

    func docSummary() -> String {
        guard !docs.isEmpty else { return "You haven't added any documents. Add insurance or PUC dates in the Garage tab." }
        return docs.map { d in
            let n = d.daysLeft
            return n < 0 ? "\(d.name) expired \(-n) days ago." : "\(d.name) expires in \(n) days."
        }.joined(separator: " ")
    }

    /// Spoken at the start of every ride.
    func dueMessages() -> [String] {
        var out: [String] = []
        for s in services {
            let left = s.remainingKm(odometer: odometerKm)
            if left <= 0 { out.append("\(s.name) is overdue by \(Int(-left)) kilometres.") }
            else if left <= 200 { out.append("\(s.name) is due in \(Int(left)) kilometres.") }
        }
        for d in docs {
            let n = d.daysLeft
            if n < 0 { out.append("\(d.name) has expired.") }
            else if n <= 30 { out.append("\(d.name) expires in \(n) days.") }
        }
        return out
    }

    // MARK: Storage

    private func trim(_ v: Double) -> String { v == v.rounded() ? "\(Int(v))" : String(format: "%.1f", v) }

    private static func load<T: Decodable>(_ key: String) -> [T] {
        guard let data = UserDefaults.standard.data(forKey: key),
              let v = try? JSONDecoder().decode([T].self, from: data) else { return [] }
        return v
    }

    private func persist() {
        if let d = try? JSONEncoder().encode(fuel) { UserDefaults.standard.set(d, forKey: "fuelLog") }
        if let d = try? JSONEncoder().encode(services) { UserDefaults.standard.set(d, forKey: "services") }
        if let d = try? JSONEncoder().encode(docs) { UserDefaults.standard.set(d, forKey: "docs") }
    }
}
