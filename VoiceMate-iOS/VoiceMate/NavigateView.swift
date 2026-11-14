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
    @State private var editingPlace: SavedPlace?
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
                if nav.isNavigating { ActiveNavSection() }

                GSection("Where to?") {
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

                GSection("Find nearby") {
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

                GSection("Route options") {
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

                GSection("Saved places") {
                    ForEach(places.saved) { p in
                        Button { Task { await ride.navigate(to: p.place) } } label: {
                            Label(p.name, systemImage: icon(for: p.name))
                        }
                        .swipeActions(edge: .leading) {
                            Button { editingPlace = p } label: { Label("Edit", systemImage: "pencil") }.tint(Theme.accent)
                        }
                        .swipeActions(edge: .trailing) {
                            Button(role: .destructive) { places.delete(id: p.id) } label: { Label("Delete", systemImage: "trash") }
                        }
                        .contextMenu {
                            Button { editingPlace = p } label: { Label("Edit", systemImage: "pencil") }
                        }
                    }
                    Button { showSave = true } label: { Label("Save current location…", systemImage: "plus.circle.fill") }
                    Text("Tap to navigate. Swipe right (or press and hold) to rename or move a place. Voice: “save parking”, “set home”, “save this place as <name>”, “where did I park”.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                GSection("Routes") {
                    Button { showImport = true } label: { Label("Import GPX route…", systemImage: "square.and.arrow.down") }
                    Text("Follow a route from Kurviger, Rever, Komoot and similar apps. Works best with a Google key.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                if !places.recents.isEmpty {
                    GSection("Recent") {
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
            .background(AppBackground())
            .keyboardDone()
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
            .sheet(item: $editingPlace) { p in
                PlaceEditor(place: p, onSave: { places.update($0) }, onDelete: { places.delete(id: p.id) })
            }
        }
    }

    // MARK: Pieces

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

struct PlaceEditor: View {
    @Environment(\.dismiss) private var dismiss
    let original: SavedPlace
    let onSave: (SavedPlace) -> Void
    let onDelete: () -> Void
    @State private var name: String
    @State private var lat: Double
    @State private var lon: Double
    @State private var moved = false
    @State private var locating = false

    init(place: SavedPlace, onSave: @escaping (SavedPlace) -> Void, onDelete: @escaping () -> Void) {
        original = place
        self.onSave = onSave
        self.onDelete = onDelete
        _name = State(initialValue: place.name)
        _lat = State(initialValue: place.lat)
        _lon = State(initialValue: place.lon)
    }

    var body: some View {
        NavigationStack {
            Form {
                GSection("Place") {
                    TextField("Name", text: $name)
                    Text(String(format: "%.5f, %.5f", lat, lon)).font(.footnote).foregroundStyle(Theme.dim)
                    Button {
                        Task {
                            locating = true
                            if let l = await LocationProvider.shared.freshLocation() {
                                lat = l.coordinate.latitude
                                lon = l.coordinate.longitude
                                moved = true
                            }
                            locating = false
                        }
                    } label: {
                        if locating { ProgressView() } else { Label("Move to my current location", systemImage: "location.fill") }
                    }
                    if moved { Text("Location updated. Tap Save.").font(.footnote).foregroundStyle(Theme.accent) }
                }
                GSection { Button("Delete place", role: .destructive) { onDelete(); dismiss() } }
            }
            .keyboardDone()
            .navigationTitle("Edit place")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        var p = original
                        p.name = name
                        p.lat = lat
                        p.lon = lon
                        onSave(p)
                        dismiss()
                    }
                    .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
        .presentationDetents([.medium])
        .preferredColorScheme(.dark)
    }
}

/// The live navigation card; it watches the per-second numbers so the rest of the screen doesn't redraw.
private struct ActiveNavSection: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var nav = Navigator.shared
    @ObservedObject private var live = Navigator.shared.live

    var body: some View {
        GSection("Navigating") {
            HStack(spacing: 16) {
                Image(systemName: maneuverSymbol(live.upcomingPhrase))
                    .font(.system(size: 44, weight: .bold)).foregroundStyle(Theme.good).frame(width: 60)
                VStack(alignment: .leading, spacing: 2) {
                    Text(shortDistance(live.upcomingMeters)).font(.system(size: 30, weight: .heavy, design: .rounded))
                    Text(live.upcomingPhrase.prefix(1).uppercased() + live.upcomingPhrase.dropFirst()).font(.headline)
                }
            }
            LabeledContent("Destination", value: nav.destinationName)
            LabeledContent("Remaining", value: "\(shortDistance(live.remainingMeters)) · \(Int((live.remainingSeconds / 60).rounded())) min")
            LabeledContent("Arrive", value: Date().addingTimeInterval(live.remainingSeconds).formatted(date: .omitted, time: .shortened))
            if !nav.queue.isEmpty {
                LabeledContent("Then", value: nav.queue.map(\.name).joined(separator: " → "))
                Button("Skip to next stop") { Task { await ride.skipStop() } }
            }
            Button(role: .destructive) { nav.stop() } label: { Label("Stop navigation", systemImage: "xmark.circle.fill") }
        }
    }
}
