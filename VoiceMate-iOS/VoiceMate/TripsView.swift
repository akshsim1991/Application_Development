import SwiftUI
import MapKit

struct TripsView: View {
    @ObservedObject private var trip = TripRecorder.shared

    var body: some View {
        NavigationStack {
            List {
                if !trip.trips.isEmpty {
                    Section {
                        HStack {
                            totals("Rides", "\(trip.trips.count)")
                            Divider()
                            totals("Distance", shortDistance(trip.trips.reduce(0) { $0 + $1.distance }))
                            Divider()
                            totals("Time", clockString(trip.trips.reduce(0) { $0 + $1.duration }))
                        }
                    }
                }
                Section("Rides") {
                    if trip.trips.isEmpty {
                        Text("Your rides appear here after you end a ride.").foregroundStyle(Theme.dim)
                    }
                    ForEach(trip.trips) { t in
                        NavigationLink { TripDetail(trip: t) } label: {
                            VStack(alignment: .leading, spacing: 3) {
                                Text(t.start.formatted(date: .abbreviated, time: .shortened)).font(.headline)
                                Text("\(shortDistance(t.distance)) · \(clockString(t.duration)) · avg \(Int(Prefs.displaySpeed(t.avgSpeed).rounded())) \(Prefs.speedUnit)")
                                    .font(.subheadline).foregroundStyle(Theme.dim)
                            }
                        }
                    }
                    .onDelete { trip.delete(at: $0) }
                }
            }
            .scrollContentBackground(.hidden)
            .background(Theme.bg.ignoresSafeArea())
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
    let trip: Trip

    var body: some View {
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
        .background(Theme.bg.ignoresSafeArea())
        .navigationTitle(trip.start.formatted(date: .abbreviated, time: .omitted))
        .navigationBarTitleDisplayMode(.inline)
    }
}
