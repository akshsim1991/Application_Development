import SwiftUI

struct QuickAction: Identifiable {
    let id = UUID()
    let title: String
    let icon: String
    let command: String
}

private let quickActions: [QuickAction] = [
    .init(title: "Speed", icon: "speedometer", command: "what's my speed"),
    .init(title: "Where am I", icon: "location.fill", command: "where am i"),
    .init(title: "Weather", icon: "cloud.sun.fill", command: "weather"),
    .init(title: "Next turn", icon: "arrow.turn.up.right", command: "next turn"),
    .init(title: "ETA", icon: "flag.checkered", command: "how far"),
    .init(title: "Trip", icon: "chart.bar.fill", command: "trip stats"),
    .init(title: "Petrol", icon: "fuelpump.fill", command: "nearest petrol pump"),
    .init(title: "Hospital", icon: "cross.case.fill", command: "nearest hospital"),
    .init(title: "Music", icon: "playpause.fill", command: "toggle music"),
    .init(title: "Park here", icon: "parkingsign.circle.fill", command: "save parking"),
    .init(title: "Service", icon: "wrench.and.screwdriver.fill", command: "service status"),
    .init(title: "Share", icon: "square.and.arrow.up", command: "share my location")
]

struct RideView: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var route = RideController.shared.route
    @ObservedObject private var listener = RideController.shared.listener
    @ObservedObject private var loc = LocationProvider.shared
    @ObservedObject private var trip = TripRecorder.shared
    @ObservedObject private var nav = Navigator.shared
    @ObservedObject private var road = RoadInfo.shared
    @State private var showSOS = false
    @AppStorage("speedLimit") private var speedLimit = 0

    private var moving: Bool { ride.isRiding && (loc.latest?.speed ?? 0) >= 1.5 }
    private var speed: Double { moving ? Prefs.displaySpeed(loc.latest?.speed ?? 0) : 0 }

    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                statusRow
                speedCard
                if nav.isNavigating { navCard }
                statsGrid
                voiceCard
                startButton
                if let e = ride.errorMessage {
                    Text(e).font(.footnote).foregroundStyle(Theme.bad).multilineTextAlignment(.center)
                }
                quickActionGrid
                sosButton
                if !ride.log.isEmpty { logCard }
            }
            .padding()
        }
        .background(Theme.bg.ignoresSafeArea())
    }

    // MARK: Pieces

    private var statusRow: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                chip(route.isBluetooth ? "Helmet: \(route.description)" : "No helmet (\(route.description))",
                     icon: route.isBluetooth ? "headphones" : "exclamationmark.triangle.fill",
                     color: route.isBluetooth ? Theme.good : Theme.accent)
                chip(ride.isRiding ? "GPS on" : "GPS off", icon: "location.fill", color: ride.isRiding ? Theme.good : Theme.dim)
                chip(nav.providerStatus, icon: "map.fill", color: nav.providerStatus.hasPrefix("Google") ? Theme.good : Theme.dim)
            }
        }
    }

    private func chip(_ text: String, icon: String, color: Color) -> some View {
        Label(text, systemImage: icon)
            .font(.caption.weight(.semibold))
            .foregroundStyle(color)
            .padding(.horizontal, 12).padding(.vertical, 7)
            .background(Capsule().fill(color.opacity(0.15)))
    }

    private var speedCard: some View {
        VStack(spacing: 6) {
            SpeedGauge(speed: speed, maxSpeed: Prefs.imperial ? 100 : 160, unit: Prefs.speedUnit,
                       over: speedLimit > 0 && speed > Double(speedLimit))
            HStack(spacing: 18) {
                Label(cardinal(ride.isRiding ? (loc.latest?.course ?? -1) : -1), systemImage: "safari")
                if speedLimit > 0 { Label("Alert \(speedLimit)", systemImage: "exclamationmark.triangle") }
                if ride.isRiding, let lim = road.limitDisplay { limitSign(Int(lim.rounded())) }
            }
            .font(.subheadline.weight(.semibold)).foregroundStyle(Theme.dim)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 12)
        .background(RoundedRectangle(cornerRadius: 22, style: .continuous).fill(Theme.card))
    }

    private func limitSign(_ v: Int) -> some View {
        Text("\(v)")
            .font(.system(size: 18, weight: .heavy, design: .rounded))
            .foregroundStyle(.black)
            .frame(width: 40, height: 40)
            .background(Circle().fill(.white))
            .overlay(Circle().stroke(Theme.bad, lineWidth: 4))
    }

    private var sosButton: some View {
        Button { showSOS = true } label: {
            Label("SOS", systemImage: "sos")
                .font(.headline.weight(.heavy))
                .foregroundStyle(.white)
                .frame(maxWidth: .infinity, minHeight: 54)
                .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(Theme.bad.opacity(0.85)))
        }
        .confirmationDialog("Send an emergency message with your location?", isPresented: $showSOS, titleVisibility: .visible) {
            Button("Send SOS now", role: .destructive) {
                Task { await ride.say(await Emergency.shared.sendSOS(manual: true)) }
            }
        }
    }

    private var navCard: some View {
        HStack(spacing: 16) {
            Image(systemName: maneuverSymbol(nav.upcomingPhrase))
                .font(.system(size: 44, weight: .bold)).foregroundStyle(Theme.accent).frame(width: 60)
            VStack(alignment: .leading, spacing: 2) {
                Text(shortDistance(nav.upcomingMeters)).font(.system(size: 30, weight: .heavy, design: .rounded))
                Text(nav.upcomingPhrase.prefix(1).uppercased() + nav.upcomingPhrase.dropFirst())
                    .font(.headline).foregroundStyle(Theme.dim)
                Text("\(nav.destinationName) · \(shortDistance(nav.remainingMeters)) · \(Int((nav.remainingSeconds / 60).rounded())) min")
                    .font(.caption).foregroundStyle(Theme.dim).lineLimit(1)
            }
            Spacer()
        }
        .card()
    }

    private var statsGrid: some View {
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
            StatTile(title: "Distance", value: trip.isRecording ? shortDistance(trip.distance) : "—")
            StatTile(title: "Riding time", value: trip.isRecording ? clockString(trip.movingTime) : "—")
            StatTile(title: "Average", value: trip.isRecording && trip.movingTime > 5 ? "\(Int(Prefs.displaySpeed(trip.avgSpeed).rounded())) \(Prefs.speedUnit)" : "—")
            StatTile(title: "Top speed", value: trip.isRecording && trip.maxSpeed > 0 ? "\(Int(Prefs.displaySpeed(trip.maxSpeed).rounded())) \(Prefs.speedUnit)" : "—")
        }
    }

    private var voiceCard: some View {
        HStack(spacing: 14) {
            VoiceOrb(phase: ride.phase, active: listener.isActive)
            VStack(alignment: .leading, spacing: 3) {
                Text(stateTitle).font(.headline)
                Text(stateDetail).font(.subheadline).foregroundStyle(Theme.dim).lineLimit(3)
                if !listener.statusNote.isEmpty {
                    Text(listener.statusNote).font(.footnote.weight(.semibold)).foregroundStyle(Theme.accent).lineLimit(3)
                }
            }
            Spacer()
        }
        .card()
    }

    private var stateTitle: String {
        switch ride.phase {
        case .off: return "Voice assistant off"
        case .listening: return listener.isActive ? "Assistant ON" : "Standby"
        case .thinking: return "Working on it…"
        case .speaking: return "Speaking"
        }
    }

    private var stateDetail: String {
        if ride.phase == .off { return "Start a ride, then say “\(ride.wakeWord)”." }
        if !listener.heard.isEmpty && ride.phase == .listening { return "“\(listener.heard)”" }
        if listener.isActive { return "Just talk. Say “bye \(ride.wakeWord.split(separator: " ").last.map(String.init) ?? "rider")” to turn me off." }
        return "Say “\(ride.wakeWord)” to wake me."
    }

    private var startButton: some View {
        Button(action: ride.toggle) {
            HStack(spacing: 12) {
                Image(systemName: ride.isRiding ? "stop.fill" : "play.fill").font(.title)
                Text(ride.isRiding ? "END RIDE" : "START RIDE")
                    .font(.system(size: 26, weight: .heavy, design: .rounded))
            }
            .foregroundStyle(.black)
            .frame(maxWidth: .infinity, minHeight: 84)
            .background(RoundedRectangle(cornerRadius: 26, style: .continuous).fill(ride.isRiding ? Theme.bad : Theme.good))
        }
    }

    private var quickActionGrid: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("QUICK ACTIONS").font(.caption.weight(.semibold)).foregroundStyle(Theme.dim)
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 10), count: 3), spacing: 10) {
                ForEach(quickActions) { a in
                    Button { Task { await ride.runCommand(a.command) } } label: {
                        VStack(spacing: 6) {
                            Image(systemName: a.icon).font(.title2)
                            Text(a.title).font(.caption.weight(.semibold))
                        }
                        .foregroundStyle(.white)
                        .frame(maxWidth: .infinity, minHeight: 72)
                        .background(RoundedRectangle(cornerRadius: 16, style: .continuous).fill(Theme.card))
                    }
                }
            }
        }
    }

    private var logCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("CONVERSATION").font(.caption.weight(.semibold)).foregroundStyle(Theme.dim)
            ForEach(ride.log.suffix(6)) { e in
                HStack {
                    if e.isUser { Spacer(minLength: 40) }
                    Text(e.text)
                        .font(.subheadline)
                        .padding(.horizontal, 12).padding(.vertical, 8)
                        .background(RoundedRectangle(cornerRadius: 14, style: .continuous)
                            .fill(e.isUser ? Theme.accent.opacity(0.25) : Theme.card))
                    if !e.isUser { Spacer(minLength: 40) }
                }
            }
        }
    }
}

struct SpeedGauge: View {
    let speed: Double
    let maxSpeed: Double
    let unit: String
    let over: Bool

    var body: some View {
        ZStack {
            Circle().trim(from: 0.1, to: 0.9)
                .stroke(Color.white.opacity(0.10), style: StrokeStyle(lineWidth: 18, lineCap: .round))
                .rotationEffect(.degrees(90))
            Circle().trim(from: 0.1, to: 0.1 + 0.8 * min(1, speed / maxSpeed))
                .stroke(over ? Theme.bad : Theme.accent, style: StrokeStyle(lineWidth: 18, lineCap: .round))
                .rotationEffect(.degrees(90))
            VStack(spacing: 0) {
                Text("\(Int(speed.rounded()))")
                    .font(.system(size: 92, weight: .heavy, design: .rounded)).monospacedDigit()
                    .minimumScaleFactor(0.5).foregroundStyle(over ? Theme.bad : .white)
                Text(unit).font(.title3.weight(.semibold)).foregroundStyle(Theme.dim)
            }
        }
        .frame(width: 250, height: 250)
        .animation(.easeOut(duration: 0.4), value: speed)
    }
}

struct VoiceOrb: View {
    let phase: RidePhase
    var active = false
    @State private var pulse = false

    private var color: Color {
        switch phase {
        case .off: return .gray
        case .listening: return active ? Theme.good : Color.blue.opacity(0.7)
        case .thinking: return Theme.accent
        case .speaking: return .cyan
        }
    }

    private var icon: String {
        switch phase {
        case .off: return "mic.slash.fill"
        case .listening: return active ? "mic.fill" : "ear.fill"
        case .thinking: return "ellipsis"
        case .speaking: return "speaker.wave.2.fill"
        }
    }

    var body: some View {
        ZStack {
            Circle().fill(color.opacity(0.18)).scaleEffect(phase == .off ? 0.9 : (pulse ? 1.2 : 0.9))
            Circle().fill(color).frame(width: 46, height: 46)
            Image(systemName: icon).font(.headline).foregroundStyle(.black)
        }
        .frame(width: 80, height: 80)
        .onAppear { withAnimation(.easeInOut(duration: 1.1).repeatForever(autoreverses: true)) { pulse = true } }
    }
}
