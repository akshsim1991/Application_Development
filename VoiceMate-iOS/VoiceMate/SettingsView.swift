import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var nav = Navigator.shared
    @AppStorage("speechRate") private var speechRate = 0.5
    @AppStorage("voiceID") private var voiceID = ""
    @AppStorage("units") private var units = "auto"
    @AppStorage("speedLimit") private var speedLimit = 0
    @AppStorage("breakMinutes") private var breakMinutes = 90
    @AppStorage("keepScreenOn") private var keepScreenOn = true
    @AppStorage("googleKey") private var googleKey = ""

    var body: some View {
        NavigationStack {
            Form {
                Section("Voice") {
                    TextField("Wake phrase", text: $ride.wakeWord)
                        .textInputAutocapitalization(.never).autocorrectionDisabled()
                        .disabled(ride.isRiding)
                    VStack(alignment: .leading) {
                        Text("Speaking speed")
                        Slider(value: $speechRate, in: 0.35...0.65)
                    }
                    Picker("Voice", selection: $voiceID) {
                        Text("Default").tag("")
                        ForEach(Speaker.voiceOptions()) { Text($0.name).tag($0.id) }
                    }
                    Button("Test voice") { Task { await ride.say("This is how I will sound while you ride.") } }
                }

                Section("Units") {
                    Picker("Units", selection: $units) {
                        Text("Auto").tag("auto")
                        Text("km").tag("metric")
                        Text("miles").tag("imperial")
                    }
                    .pickerStyle(.segmented)
                }

                Section("Safety") {
                    Stepper(value: $speedLimit, in: 0...200, step: 5) {
                        Text(speedLimit == 0 ? "Speed alert: off" : "Speed alert: \(speedLimit) \(Prefs.speedUnit)")
                    }
                    Stepper(value: $breakMinutes, in: 0...240, step: 15) {
                        Text(breakMinutes == 0 ? "Break reminder: off" : "Break reminder: every \(breakMinutes) min")
                    }
                    Toggle("Keep screen on while riding", isOn: $keepScreenOn)
                }

                Section("Navigation") {
                    SecureField("Google Maps API key", text: $googleKey)
                        .textInputAutocapitalization(.never).autocorrectionDisabled()
                    LabeledContent("Routing", value: nav.providerStatus)
                    Text("With a key, routing uses Google Maps two-wheeler mode (best for India). Without one, Apple Maps is used.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section {
                    NavigationLink("Voice commands") { CommandsView() }
                    Button("Open iPhone settings (permissions)") {
                        if let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) }
                    }
                }
            }
            .scrollContentBackground(.hidden)
            .background(Theme.bg.ignoresSafeArea())
            .navigationTitle("Settings")
        }
    }
}

struct CommandsView: View {
    private let groups: [(String, [String])] = [
        ("Navigation", ["navigate to <place>", "take me home / navigate home", "next turn", "how far / ETA", "stop navigation"]),
        ("Places", ["save parking / I parked here", "where did I park", "set home / set work", "save this place as <name>"]),
        ("Ride info", ["what's my speed", "where am I", "which direction", "trip stats / average speed", "weather"]),
        ("Alerts", ["set speed alert to 80", "speed alert off", "set a timer for 10 minutes", "remind me in 30 minutes to refuel"]),
        ("Music", ["play / pause", "next song / previous song"]),
        ("Phone", ["what time is it", "battery", "repeat", "help"])
    ]

    var body: some View {
        List {
            Text("Say the wake phrase first (default “hey rider”), then the command. After a question like “Say yes to start” just answer.")
                .font(.footnote).foregroundStyle(Theme.dim)
            ForEach(groups, id: \.0) { g in
                Section(g.0) { ForEach(g.1, id: \.self) { Text($0) } }
            }
        }
        .navigationTitle("Voice commands")
    }
}
