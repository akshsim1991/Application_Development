import SwiftUI
import MapKit
import Charts

private struct WeekBar: Identifiable {
    let id = UUID()
    let weekStart: Date
    let value: Double
}

struct TripsView: View {
    @ObservedObject private var trip = TripRecorder.shared

    private var weekly: [WeekBar] {
        let cal = Calendar.current
        var sums: [Date: Double] = [:]
        for t in trip.trips {
            guard let start = cal.dateInterval(of: .weekOfYear, for: t.start)?.start else { continue }
            sums[start, default: 0] += t.distance / 1000
        }
        let k = Prefs.imperial ? 0.621371 : 1
        return sums.map { WeekBar(weekStart: $0.key, value: $0.value * k) }.sorted { $0.weekStart < $1.weekStart }.suffix(8).map { $0 }
    }

    var body: some View {
        NavigationStack {
            List {
                if !trip.trips.isEmpty {
                    GSection {
                        HStack {
                            totals("Rides", "\(trip.trips.count)")
                            Divider()
                            totals("Distance", shortDistance(trip.trips.reduce(0) { $0 + $1.distance }))
                            Divider()
                            totals("Time", clockString(trip.trips.reduce(0) { $0 + $1.duration }))
                        }
                    }
                    GSection("Distance per week") {
                        Chart(weekly) { w in
                            BarMark(x: .value("Week", w.weekStart, unit: .weekOfYear), y: .value("Distance", w.value))
                                .foregroundStyle(Theme.accent)
                        }
                        .chartYAxisLabel(Prefs.imperial ? "mi" : "km")
                        .frame(height: 160)
                    }
                    GSection("Records") {
                        if let longest = trip.trips.max(by: { $0.distance < $1.distance }) {
                            LabeledContent("Longest ride", value: shortDistance(longest.distance))
                        }
                        if let fastest = trip.trips.max(by: { $0.maxSpeed < $1.maxSpeed }) {
                            LabeledContent("Top speed", value: "\(Int(Prefs.displaySpeed(fastest.maxSpeed).rounded())) \(Prefs.speedUnit)")
                        }
                        if let long = trip.trips.max(by: { $0.duration < $1.duration }) {
                            LabeledContent("Longest time", value: clockString(long.duration))
                        }
                    }
                }
                GSection("Rides") {
                    if trip.trips.isEmpty {
                        Text("Your rides appear here after you end a ride.").foregroundStyle(Theme.dim)
                    }
                    ForEach(trip.trips) { t in
                        NavigationLink { TripDetail(id: t.id) } label: {
                            VStack(alignment: .leading, spacing: 3) {
                                Text(t.name ?? t.start.formatted(date: .abbreviated, time: .shortened)).font(.headline)
                                Text("\(t.name != nil ? t.start.formatted(date: .abbreviated, time: .omitted) + " · " : "")\(shortDistance(t.distance)) · \(clockString(t.duration)) · avg \(Int(Prefs.displaySpeed(t.avgSpeed).rounded())) \(Prefs.speedUnit)")
                                    .font(.subheadline).foregroundStyle(Theme.dim)
                            }
                        }
                    }
                    .onDelete { trip.delete(at: $0) }
                }
            }
            .scrollContentBackground(.hidden)
            .background(AppBackground())
            .navigationTitle("Trips")
        }
    }

    private func totals(_ title: String, _ value: String) -> some View {
        VStack(spacing: 2) {
            Text(value).font(.title3.weight(.bold)).monospacedDigit().minimumScaleFactor(0.6).lineLimit(1)
            Text(title).font(.caption).foregroundStyle(Theme.dim)
        }
        .frame(maxWidth: .infinity)
    }
}

struct TripDetail: View {
    let id: UUID
    @ObservedObject private var rec = TripRecorder.shared
    @Environment(\.dismiss) private var dismiss
    @State private var showRename = false
    @State private var newName = ""
    @State private var showDelete = false

    private var trip: Trip? { rec.trips.first { $0.id == id } }

    var body: some View {
        Group {
            if let trip { content(trip) } else { Text("This ride was deleted.").foregroundStyle(Theme.dim) }
        }
        .background(AppBackground())
        .navigationTitle(trip?.name ?? trip?.start.formatted(date: .abbreviated, time: .omitted) ?? "Ride")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            if let trip {
                ToolbarItem(placement: .topBarTrailing) {
                    Menu {
                        Button { newName = trip.name ?? ""; showRename = true } label: { Label("Rename", systemImage: "pencil") }
                        if let url = GPX.writeFile(for: trip) {
                            ShareLink(item: url) { Label("Export GPX", systemImage: "square.and.arrow.up") }
                        }
                        Button(role: .destructive) { showDelete = true } label: { Label("Delete ride", systemImage: "trash") }
                    } label: { Image(systemName: "ellipsis.circle") }
                }
            }
        }
        .alert("Rename ride", isPresented: $showRename) {
            TextField("Name (e.g. Sunday ghat ride)", text: $newName)
            Button("Save") { rec.rename(id, to: newName) }
            Button("Cancel", role: .cancel) {}
        }
        .confirmationDialog("Delete this ride?", isPresented: $showDelete, titleVisibility: .visible) {
            Button("Delete", role: .destructive) { rec.delete(id: id); dismiss() }
        }
    }

    private func content(_ trip: Trip) -> some View {
        ScrollView {
            VStack(spacing: 14) {
                if trip.coordinates.count >= 2 {
                    Map {
                        MapPolyline(coordinates: trip.coordinates).stroke(Theme.accent, lineWidth: 5)
                    }
                    .frame(height: 300)
                    .clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
                }
                LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
                    StatTile(title: "Distance", value: shortDistance(trip.distance))
                    StatTile(title: "Duration", value: clockString(trip.duration))
                    StatTile(title: "Moving time", value: clockString(trip.movingTime))
                    StatTile(title: "Average", value: "\(Int(Prefs.displaySpeed(trip.avgSpeed).rounded())) \(Prefs.speedUnit)")
                    StatTile(title: "Top speed", value: "\(Int(Prefs.displaySpeed(trip.maxSpeed).rounded())) \(Prefs.speedUnit)")
                    StatTile(title: "Started", value: trip.start.formatted(date: .omitted, time: .shortened))
                }
            }
            .padding()
        }
    }
}
