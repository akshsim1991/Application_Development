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
    @AppStorage("cameraAlerts") private var cameraAlerts = true
    @AppStorage("autoLimitAlert") private var autoLimitAlert = false
    @AppStorage("crashDetect") private var crashDetect = false
    @AppStorage("googleKey") private var googleKey = ""
    @AppStorage("riderName") private var riderName = ""
    @AppStorage("tgToken") private var tgToken = ""
    @AppStorage("tgChat") private var tgChat = ""
    @AppStorage("liveShareMinutes") private var liveShareMinutes = 0
    @AppStorage("anthropicKey") private var anthropicKey = ""
    @State private var status = ""

    private var version: String {
        let v = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "2.0"
        return "VoiceMate \(v)"
    }

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

                Section("Safety alerts") {
                    Stepper(value: $speedLimit, in: 0...200, step: 5) {
                        Text(speedLimit == 0 ? "Speed alert: off" : "Speed alert: \(speedLimit) \(Prefs.speedUnit)")
                    }
                    Stepper(value: $breakMinutes, in: 0...240, step: 15) {
                        Text(breakMinutes == 0 ? "Break reminder: off" : "Break reminder: every \(breakMinutes) min")
                    }
                    Toggle("Speed camera warnings", isOn: $cameraAlerts)
                    Toggle("Warn above posted speed limit", isOn: $autoLimitAlert)
                    Text("Camera and speed-limit data come from OpenStreetMap volunteers. Coverage in India is incomplete, so don't rely on it.")
                        .font(.caption).foregroundStyle(Theme.dim)
                    Toggle("Keep screen on while riding", isOn: $keepScreenOn)
                }

                Section("Emergency contact (Telegram)") {
                    TextField("Your name", text: $riderName)
                    SecureField("Bot token", text: $tgToken)
                        .textInputAutocapitalization(.never).autocorrectionDisabled()
                    TextField("Chat ID", text: $tgChat)
                        .keyboardType(.numbersAndPunctuation).autocorrectionDisabled()
                    Button("Find chat ID (message your bot first)") {
                        Task {
                            if let id = await Emergency.findChatID(token: tgToken) { tgChat = id; status = "Chat ID found." }
                            else { status = "No message found. Send your bot a message, then try again." }
                        }
                    }
                    Button("Send test message") {
                        Task { status = await Emergency.shared.send("✅ Test from VoiceMate. Emergency alerts are working.") ? "Test message sent." : "Failed. Check token, chat ID and internet." }
                    }
                    if !status.isEmpty { Text(status).font(.caption).foregroundStyle(Theme.accent) }
                    Toggle("Crash detection", isOn: $crashDetect)
                    Button("Test crash alert (30 s countdown)") { Emergency.shared.simulateCrash() }
                    Stepper(value: $liveShareMinutes, in: 0...60, step: 5) {
                        Text(liveShareMinutes == 0 ? "Live tracking: off" : "Share location every \(liveShareMinutes) min")
                    }
                    Text("Crash detection is experimental: a hard jolt at speed followed by stopping starts a 30-second “I'm OK” countdown, then messages your contact with your location. Setup steps are in the README.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section("Navigation") {
                    SecureField("Google Maps API key", text: $googleKey)
                        .textInputAutocapitalization(.never).autocorrectionDisabled()
                    LabeledContent("Routing", value: nav.providerStatus)
                    Text("With a key, routing uses Google Maps two-wheeler mode (best for India). Without one, Apple Maps is used.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section("AI assistant") {
                    SecureField("Anthropic API key", text: $anthropicKey)
                        .textInputAutocapitalization(.never).autocorrectionDisabled()
                    Text("With a key, anything VoiceMate doesn't understand is answered by Claude (for example “hey rider, how do I fix a punctured tubeless tyre”). Each answer costs a tiny amount on your Anthropic account.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section {
                    NavigationLink("Voice commands") { CommandsView() }
                    Button("Open iPhone settings (permissions)") {
                        if let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) }
                    }
                }

                Section("About") {
                    Text(version).font(.headline)
                    Text("Designed and developed by Akshaya Simha").foregroundStyle(Theme.dim)
                }
            }
            .scrollContentBackground(.hidden)
            .background(Theme.bg.ignoresSafeArea())
            .keyboardDone()
            .navigationTitle("Settings")
        }
    }
}

struct CommandsView: View {
    private let groups: [(String, [String])] = [
        ("Navigation", ["navigate to <place>", "take me home / navigate home", "nearest petrol pump / hospital / ATM / mechanic", "add a stop at <place>", "skip stop", "next turn", "how far / ETA", "avoid tolls / avoid highways", "curvy route / fastest route", "stop navigation"]),
        ("Places", ["save parking / I parked here", "where did I park", "set home / set work", "save this place as <name>"]),
        ("Ride info", ["what's my speed", "where am I", "which direction", "trip stats / average speed", "weather"]),
        ("Garage", ["log fuel 8 litres at 107", "what's my mileage", "set odometer to 12000", "service status", "I changed the engine oil", "when does my insurance expire"]),
        ("Safety", ["SOS / I need help", "share my location", "I'm OK (cancels a crash alert)", "set speed alert to 80 / speed alert off"]),
        ("Timers", ["set a timer for 10 minutes", "remind me in 30 minutes to refuel"]),
        ("Music", ["play / pause", "next song / previous song"]),
        ("Other", ["what time is it", "battery", "repeat", "ask <any question> (needs Anthropic key)", "help"])
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
