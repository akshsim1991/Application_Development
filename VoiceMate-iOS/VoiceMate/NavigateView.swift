import SwiftUI

struct NavigateView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var nav = Navigator.shared
    @ObservedObject private var places = PlaceStore.shared
    @State private var query = ""
    @State private var showSave = false
    @State private var saveName = ""

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
        }
    }

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
            Button(role: .destructive) { nav.stop() } label: { Label("Stop navigation", systemImage: "xmark.circle.fill") }
        }
    }

    private func go() {
        let q = query
        query = ""
        Task { await ride.navigate(toQuery: q) }
    }

    private func saveCurrent() async {
        let name = saveName
        saveName = ""
        if let l = await LocationProvider.shared.freshLocation() { places.save(name: name, at: l.coordinate) }
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
