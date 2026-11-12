import SwiftUI
import CoreLocation
import UniformTypeIdentifiers

private struct NearbyCategory: Identifiable {
    let id = UUID()
    let title: String
    let icon: String
    let query: String
}

private let categories: [NearbyCategory] = [
    .init(title: "Petrol", icon: "fuelpump.fill", query: "petrol pump"),
    .init(title: "Food", icon: "fork.knife", query: "restaurant"),
    .init(title: "Chai", icon: "cup.and.saucer.fill", query: "tea stall cafe"),
    .init(title: "ATM", icon: "banknote.fill", query: "ATM"),
    .init(title: "Hospital", icon: "cross.case.fill", query: "hospital"),
    .init(title: "Mechanic", icon: "wrench.and.screwdriver.fill", query: "motorcycle repair shop"),
    .init(title: "Puncture", icon: "circle.dashed", query: "tyre puncture repair"),
    .init(title: "Hotel", icon: "bed.double.fill", query: "hotel")
]

struct NavigateView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var nav = Navigator.shared
    @ObservedObject private var places = PlaceStore.shared
    @AppStorage("routeStyle") private var routeStyle = "fastest"
    @AppStorage("avoidTolls") private var avoidTolls = false
    @AppStorage("avoidHighways") private var avoidHighways = false

    @State private var query = ""
    @State private var showSave = false
    @State private var saveName = ""
    @State private var showImport = false
    @State private var showNearby = false
    @State private var nearbyTitle = ""
    @State private var nearbyResults: [Place] = []
    @State private var nearbyFrom: CLLocation?
    @State private var loadingNearby = false

    var body: some View {
        NavigationStack {
            List {
                if nav.isNavigating { activeSection }

                Section("Where to?") {
                    HStack {
                        TextField("Search a place or address", text: $query)
                            .submitLabel(.go)
                            .onSubmit(go)
                        if ride.isSearching { ProgressView() }
                        else { Button("Go", action: go).buttonStyle(.borderedProminent).disabled(query.isEmpty) }
                    }
                    Text("You can also just say “hey rider, navigate to …”.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section("Find nearby") {
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 8), count: 4), spacing: 8) {
                        ForEach(categories) { c in
                            Button { Task { await loadNearby(c) } } label: {
                                VStack(spacing: 4) {
                                    Image(systemName: c.icon).font(.title3)
                                    Text(c.title).font(.caption2.weight(.semibold))
                                }
                                .frame(maxWidth: .infinity, minHeight: 56)
                                .background(RoundedRectangle(cornerRadius: 12).fill(Theme.bg))
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    if loadingNearby { ProgressView() }
                }

                Section("Route options") {
                    Picker("Style", selection: $routeStyle) {
                        Text("Fastest").tag("fastest")
                        Text("Curvy").tag("curvy")
                    }
                    .pickerStyle(.segmented)
                    Toggle("Avoid tolls", isOn: $avoidTolls)
                    Toggle("Avoid highways", isOn: $avoidHighways)
                    Text("Curvy picks the twistiest of Google's alternative routes (experimental, needs a Google key). Changes apply to the next route, or tap Update route while navigating.")
                        .font(.caption).foregroundStyle(Theme.dim)
                    if nav.isNavigating {
                        Button("Update route now") { Task { await ride.say(await nav.replan()) } }
                    }
                }

                Section("Saved places") {
                    ForEach(places.saved) { p in
                        Button { Task { await ride.navigate(to: p.place) } } label: {
                            Label(p.name, systemImage: icon(for: p.name))
                        }
                    }
                    .onDelete { places.delete(at: $0) }
                    Button { showSave = true } label: { Label("Save current location…", systemImage: "plus.circle.fill") }
                    Text("Voice: “save parking”, “set home”, “save this place as <name>”, “where did I park”.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section("Routes") {
                    Button { showImport = true } label: { Label("Import GPX route…", systemImage: "square.and.arrow.down") }
                    Text("Follow a route from Kurviger, Rever, Komoot and similar apps. Works best with a Google key.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                if !places.recents.isEmpty {
                    Section("Recent") {
                        ForEach(places.recents) { p in
                            Button { Task { await ride.navigate(to: p.place) } } label: {
                                Label(p.name, systemImage: "clock.arrow.circlepath")
                            }
                        }
                        .onDelete { places.deleteRecent(at: $0) }
                    }
                }
            }
            .scrollContentBackground(.hidden)
            .background(Theme.bg.ignoresSafeArea())
            .navigationTitle("Navigate")
            .alert("Save current location", isPresented: $showSave) {
                TextField("Name (e.g. Office)", text: $saveName)
                Button("Save") { Task { await saveCurrent() } }
                Button("Cancel", role: .cancel) { saveName = "" }
            }
            .fileImporter(isPresented: $showImport,
                          allowedContentTypes: [UTType(filenameExtension: "gpx") ?? .xml, .xml],
                          allowsMultipleSelection: false) { result in importGPX(result) }
            .sheet(isPresented: $showNearby) { nearbySheet }
        }
    }

    // MARK: Pieces

    private var activeSection: some View {
        Section("Navigating") {
            HStack(spacing: 16) {
                Image(systemName: maneuverSymbol(nav.upcomingPhrase))
                    .font(.system(size: 44, weight: .bold)).foregroundStyle(Theme.accent).frame(width: 60)
                VStack(alignment: .leading, spacing: 2) {
                    Text(shortDistance(nav.upcomingMeters)).font(.system(size: 30, weight: .heavy, design: .rounded))
                    Text(nav.upcomingPhrase.prefix(1).uppercased() + nav.upcomingPhrase.dropFirst()).font(.headline)
                }
            }
            LabeledContent("Destination", value: nav.destinationName)
            LabeledContent("Remaining", value: "\(shortDistance(nav.remainingMeters)) · \(Int((nav.remainingSeconds / 60).rounded())) min")
            LabeledContent("Arrive", value: Date().addingTimeInterval(nav.remainingSeconds).formatted(date: .omitted, time: .shortened))
            if !nav.queue.isEmpty {
                LabeledContent("Then", value: nav.queue.map(\.name).joined(separator: " → "))
                Button("Skip to next stop") { Task { await ride.skipStop() } }
            }
            Button(role: .destructive) { nav.stop() } label: { Label("Stop navigation", systemImage: "xmark.circle.fill") }
        }
    }

    private var nearbySheet: some View {
        NavigationStack {
            List {
                if nearbyResults.isEmpty { Text("Nothing found nearby.").foregroundStyle(Theme.dim) }
                ForEach(0..<nearbyResults.count, id: \.self) { i in
                    nearbyRow(nearbyResults[i])
                }
            }
            .navigationTitle(nearbyTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .cancellationAction) { Button("Close") { showNearby = false } } }
        }
        .presentationDetents([.medium, .large])
        .preferredColorScheme(.dark)
    }

    private func nearbyRow(_ p: Place) -> some View {
        let actionLabel: String = nav.isNavigating ? "Add stop" : "Go"
        return Button {
            showNearby = false
            Task {
                if nav.isNavigating { await ride.addStop(p) } else { await ride.navigate(to: p) }
            }
        } label: {
            HStack {
                VStack(alignment: .leading) {
                    Text(p.name).font(.headline)
                    Text(distanceText(to: p)).font(.caption).foregroundStyle(Theme.dim)
                }
                Spacer()
                Text(actionLabel).font(.subheadline.weight(.semibold)).foregroundStyle(Theme.accent)
            }
        }
    }

    private func distanceText(to p: Place) -> String {
        guard let from = nearbyFrom else { return "" }
        let target = CLLocation(latitude: p.coordinate.latitude, longitude: p.coordinate.longitude)
        return shortDistance(from.distance(from: target))
    }

    // MARK: Actions

    private func go() {
        let q = query
        query = ""
        Task { await ride.navigate(toQuery: q) }
    }

    private func loadNearby(_ c: NearbyCategory) async {
        loadingNearby = true
        defer { loadingNearby = false }
        guard let l = await LocationProvider.shared.freshLocation() else { return }
        nearbyFrom = l
        nearbyTitle = c.title
        nearbyResults = await nav.nearby(c.query, near: l, limit: 6)
        showNearby = true
    }

    private func saveCurrent() async {
        let name = saveName
        saveName = ""
        if let l = await LocationProvider.shared.freshLocation() { places.save(name: name, at: l.coordinate) }
    }

    private func importGPX(_ result: Result<[URL], Error>) {
        guard case .success(let urls) = result, let url = urls.first else { return }
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        guard let data = try? Data(contentsOf: url) else { return }
        let parsed = GPX.parse(data)
        Task { await ride.followGPX(parsed.points, name: parsed.name) }
    }

    private func icon(for name: String) -> String {
        switch name.lowercased() {
        case "home": return "house.fill"
        case "work": return "briefcase.fill"
        case "parking": return "parkingsign.circle.fill"
        default: return "star.fill"
        }
    }
}
