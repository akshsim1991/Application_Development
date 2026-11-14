import SwiftUI

struct QuickAction: Identifiable {
    let id = UUID()
    let title: String
    let icon: String
    let command: String
    let color: Color
}

private let quickActions: [QuickAction] = [
    .init(title: "Speed", icon: "speedometer", command: "what's my speed", color: Theme.accent),
    .init(title: "Where am I", icon: "location.fill", command: "where am i", color: Theme.cyan),
    .init(title: "Weather", icon: "cloud.sun.fill", command: "weather", color: Color.yellow),
    .init(title: "Next turn", icon: "arrow.turn.up.right", command: "next turn", color: Theme.good),
    .init(title: "ETA", icon: "flag.checkered", command: "how far", color: Theme.violet),
    .init(title: "Trip", icon: "chart.bar.fill", command: "trip stats", color: Theme.pink),
    .init(title: "Petrol", icon: "fuelpump.fill", command: "nearest petrol pump", color: Theme.accent),
    .init(title: "Hospital", icon: "cross.case.fill", command: "nearest hospital", color: Theme.bad),
    .init(title: "Music", icon: "playpause.fill", command: "toggle music", color: Theme.pink),
    .init(title: "Park here", icon: "parkingsign.circle.fill", command: "save parking", color: Theme.cyan),
    .init(title: "Service", icon: "wrench.and.screwdriver.fill", command: "service status", color: Theme.violet),
    .init(title: "Share", icon: "square.and.arrow.up", command: "share my location", color: Theme.good)
]

/// The Ride screen is split into small views that each watch only what they show, so a GPS update every
/// second redraws the speedometer and not the whole screen.
struct RideView: View {
    @EnvironmentObject var ride: RideController

    var body: some View {
        ScrollView {
            VStack(spacing: 14) {
                StatusRow()
                SpeedPanel()
                NavMiniCard()
                StatsGrid()
                VoicePanel()
                StartButton()
                QuickActionGrid()
                SOSButton()
                LogPanel()
            }
            .padding()
        }
        .background(AppBackground())
        .onAppear { if !ride.isRiding { LocationProvider.shared.startPreview() } }
        .onDisappear { LocationProvider.shared.stopPreview() }
        .onChange(of: ride.isRiding) { _, riding in if !riding { LocationProvider.shared.startPreview() } }
    }
}

// MARK: - Status chips

private struct StatusRow: View {
    @ObservedObject private var route = RideController.shared.route
    @ObservedObject private var loc = LocationProvider.shared
    @ObservedObject private var nav = Navigator.shared

    private var gpsColor: Color {
        switch loc.gpsLevel {
        case .strong, .good: return Theme.good
        case .fair: return Theme.accent
        case .poor, .searching, .off: return Theme.bad
        }
    }

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                chip(route.isBluetooth ? "Helmet: \(route.description)" : "No helmet (\(route.description))",
                     icon: route.isBluetooth ? "headphones" : "exclamationmark.triangle.fill",
                     color: route.isBluetooth ? Theme.good : Theme.accent)
                gpsChip
                chip(nav.providerStatus, icon: "map.fill", color: nav.providerStatus.hasPrefix("Google") ? Theme.cyan : Theme.dim)
            }
        }
    }

    private var gpsChip: some View {
        Button {
            if !loc.authorized, let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) }
        } label: {
            HStack(spacing: 6) {
                Image(systemName: "cellularbars", variableValue: loc.gpsLevel.bars)
                Text(loc.gpsAccuracy.map { "\(loc.gpsLevel.title) ±\(Int($0)) m" } ?? loc.gpsLevel.title)
            }
            .font(.caption.weight(.semibold))
            .foregroundStyle(gpsColor)
            .padding(.horizontal, 12).padding(.vertical, 7)
            .background(Capsule().fill(gpsColor.opacity(0.18)))
        }
        .buttonStyle(.plain)
    }

    private func chip(_ text: String, icon: String, color: Color) -> some View {
        Label(text, systemImage: icon)
            .font(.caption.weight(.semibold))
            .foregroundStyle(color)
            .padding(.horizontal, 12).padding(.vertical, 7)
            .background(Capsule().fill(color.opacity(0.18)))
    }
}

// MARK: - Speedometer

private struct SpeedPanel: View {
    @ObservedObject private var loc = LocationProvider.shared
    @ObservedObject private var road = RoadInfo.shared
    @EnvironmentObject var ride: RideController
    @AppStorage("speedLimit") private var speedLimit = 0

    private var moving: Bool { loc.speedKnown && loc.speedMps >= 1.5 }
    private var speed: Double { moving ? Prefs.displaySpeed(loc.speedMps) : 0 }

    private var hint: String? {
        if !loc.authorized { return "Location is off for RideVoice. Tap the GPS badge above and choose “While Using” with Precise Location on." }
        if loc.reducedAccuracy { return "Precise Location is off. Turn it on: Settings > Privacy > Location Services > RideVoice." }
        if loc.gpsLevel == .searching { return "Waiting for GPS… move near a window or outdoors." }
        if loc.gpsLevel == .poor { return "Weak GPS signal. Speed may be late or jumpy." }
        return nil
    }

    var body: some View {
        VStack(spacing: 8) {
            SpeedGauge(speed: speed,
                       maxSpeed: Prefs.imperial ? (speed > 100 ? 200 : 100) : (speed > 160 ? 320 : 160),
                       unit: Prefs.speedUnit,
                       over: speedLimit > 0 && speed > Double(speedLimit))
            HStack(spacing: 14) {
                Label(cardinal(moving ? (loc.latest?.course ?? -1) : -1), systemImage: "safari")
                if speedLimit > 0 { Label("Alert \(speedLimit)", systemImage: "exclamationmark.triangle") }
                if ride.isRiding, let lim = road.limitDisplay { limitSign(Int(lim.rounded())) }
            }
            .font(.subheadline.weight(.semibold)).foregroundStyle(Theme.dim)
            if let hint {
                Text(hint).font(.footnote.weight(.semibold)).foregroundStyle(Theme.accent)
                    .multilineTextAlignment(.center).padding(.horizontal)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 14)
        .background(GlassBackground(radius: 30, tint: Theme.violet))
    }

    private func limitSign(_ v: Int) -> some View {
        Text("\(v)")
            .font(.system(size: 18, weight: .heavy, design: .rounded))
            .foregroundStyle(.black)
            .frame(width: 40, height: 40)
            .background(Circle().fill(.white))
            .overlay(Circle().stroke(Theme.bad, lineWidth: 4))
    }
}

struct SpeedGauge: View {
    let speed: Double
    let maxSpeed: Double
    let unit: String
    let over: Bool

    private var fraction: Double { min(1, max(0, speed / maxSpeed)) }
    // The circle is rotated 90 degrees, so bottom -> top here reads left -> right on screen.
    private var gradient: LinearGradient {
        LinearGradient(colors: [Theme.cyan, Theme.good, Theme.accent, Theme.pink], startPoint: .bottom, endPoint: .top)
    }

    var body: some View {
        ZStack {
            Circle().trim(from: 0.1, to: 0.9)
                .stroke(Color.white.opacity(0.08), style: StrokeStyle(lineWidth: 20, lineCap: .round))
                .rotationEffect(.degrees(90))
            Circle().trim(from: 0.1, to: 0.1 + 0.8 * fraction)
                .stroke(over ? AnyShapeStyle(Theme.redGrad) : AnyShapeStyle(gradient),
                        style: StrokeStyle(lineWidth: 20, lineCap: .round))
                .rotationEffect(.degrees(90))
                .shadow(color: (over ? Theme.bad : Theme.accent).opacity(0.45), radius: 10)
            VStack(spacing: 0) {
                Text("\(Int(speed.rounded()))")
                    .font(.system(size: 96, weight: .heavy, design: .rounded)).monospacedDigit()
                    .minimumScaleFactor(0.5).foregroundStyle(over ? Theme.bad : .white)
                Text(unit).font(.title3.weight(.semibold)).foregroundStyle(Theme.dim)
            }
        }
        .frame(width: 250, height: 250)
        .animation(.easeOut(duration: 0.25), value: fraction)
    }
}

// MARK: - Navigation & stats

private struct NavMiniCard: View {
    @ObservedObject private var nav = Navigator.shared

    var body: some View {
        if nav.isNavigating { NavMiniContent(destination: nav.destinationName) }
    }
}

private struct NavMiniContent: View {
    let destination: String
    @ObservedObject private var live = Navigator.shared.live

    var body: some View {
        HStack(spacing: 16) {
            Image(systemName: maneuverSymbol(live.upcomingPhrase))
                .font(.system(size: 42, weight: .bold)).foregroundStyle(Theme.good).frame(width: 60)
            VStack(alignment: .leading, spacing: 2) {
                Text(shortDistance(live.upcomingMeters)).font(.system(size: 30, weight: .heavy, design: .rounded))
                Text(live.upcomingPhrase.prefix(1).uppercased() + live.upcomingPhrase.dropFirst())
                    .font(.headline).foregroundStyle(Theme.dim)
                Text("\(destination) · \(shortDistance(live.remainingMeters)) · \(Int((live.remainingSeconds / 60).rounded())) min")
                    .font(.caption).foregroundStyle(Theme.dim).lineLimit(1)
            }
            Spacer()
        }
        .padding(16)
        .background(GlassBackground(tint: Theme.good))
    }
}

private struct StatsGrid: View {
    @ObservedObject private var trip = TripRecorder.shared
    @ObservedObject private var live = TripRecorder.shared.live

    var body: some View {
        let on = trip.isRecording
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
            StatTile(title: "Distance", value: on ? shortDistance(live.distance) : "—", icon: "road.lanes", color: Theme.cyan)
            StatTile(title: "Riding time", value: on ? clockString(live.movingTime) : "—", icon: "timer", color: Theme.violet)
            StatTile(title: "Average", value: on && live.movingTime > 5 ? "\(Int(Prefs.displaySpeed(live.avgSpeed).rounded())) \(Prefs.speedUnit)" : "—",
                     icon: "gauge.medium", color: Theme.accent)
            StatTile(title: "Top speed", value: on && live.maxSpeed > 0 ? "\(Int(Prefs.displaySpeed(live.maxSpeed).rounded())) \(Prefs.speedUnit)" : "—",
                     icon: "bolt.fill", color: Theme.pink)
        }
    }
}

// MARK: - Voice assistant

private struct VoicePanel: View {
    @EnvironmentObject var ride: RideController
    @ObservedObject private var listener = RideController.shared.listener

    private var tint: Color {
        switch ride.phase {
        case .off: return .gray
        case .listening: return listener.isActive ? Theme.good : Theme.cyan
        case .thinking: return Theme.accent
        case .speaking: return Theme.pink
        }
    }

    @AppStorage("wakeEnabled") private var wakeEnabled = true

    private var title: String {
        if ride.isRiding && !wakeEnabled && ride.phase == .listening { return "Guidance only" }
        switch ride.phase {
        case .off: return "Voice assistant off"
        case .listening: return listener.isActive ? "Assistant ON" : "Standby"
        case .thinking: return "Working on it…"
        case .speaking: return "Speaking"
        }
    }

    private var detail: String {
        let name = ride.wakeWord.split(separator: " ").last.map(String.init) ?? "rider"
        if ride.isRiding && !wakeEnabled { return "Spoken directions only. Hold your helmet button to use Siri." }
        if ride.phase == .off { return "Start a ride, then say “\(ride.wakeWord)”." }
        if !listener.heard.isEmpty && ride.phase == .listening { return "“\(listener.heard)”" }
        if listener.isActive { return "Just talk. Say “bye \(name)” to turn me off." }
        return "Say “\(ride.wakeWord)” to wake me."
    }

    var body: some View {
        HStack(spacing: 14) {
            VoiceOrb(phase: ride.phase, active: listener.isActive)
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.headline)
                Text(detail).font(.subheadline).foregroundStyle(Theme.dim).lineLimit(3)
                if !listener.statusNote.isEmpty {
                    Text(listener.statusNote).font(.footnote.weight(.semibold)).foregroundStyle(Theme.accent).lineLimit(3)
                }
                if let e = ride.errorMessage {
                    Text(e).font(.footnote).foregroundStyle(Theme.bad).lineLimit(3)
                }
            }
            Spacer()
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(GlassBackground(tint: tint))
    }
}

struct VoiceOrb: View {
    let phase: RidePhase
    var active = false
    @State private var pulse = false

    private var color: Color {
        switch phase {
        case .off: return .gray
        case .listening: return active ? Theme.good : Theme.cyan
        case .thinking: return Theme.accent
        case .speaking: return Theme.pink
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
            Circle().fill(color.opacity(0.20)).scaleEffect(phase == .off ? 0.9 : (pulse ? 1.2 : 0.9))
            Circle().fill(color).frame(width: 46, height: 46)
            Image(systemName: icon).font(.headline).foregroundStyle(.black)
        }
        .frame(width: 80, height: 80)
        .onAppear { withAnimation(.easeInOut(duration: 1.1).repeatForever(autoreverses: true)) { pulse = true } }
    }
}

// MARK: - Buttons

private struct StartButton: View {
    @EnvironmentObject var ride: RideController

    var body: some View {
        Button {
            tapHaptic()
            ride.toggle()
        } label: {
            HStack(spacing: 12) {
                Image(systemName: ride.isRiding ? "stop.fill" : "play.fill").font(.title)
                Text(ride.isRiding ? "END RIDE" : "START RIDE")
                    .font(.system(size: 26, weight: .heavy, design: .rounded))
            }
            .foregroundStyle(.black)
            .frame(maxWidth: .infinity, minHeight: 84)
            .background(RoundedRectangle(cornerRadius: 28, style: .continuous).fill(ride.isRiding ? Theme.redGrad : Theme.greenGrad))
            .shadow(color: (ride.isRiding ? Theme.bad : Theme.good).opacity(0.45), radius: 14, y: 6)
        }
        .buttonStyle(PressStyle())
    }
}

private struct QuickActionGrid: View {
    @EnvironmentObject var ride: RideController

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("QUICK ACTIONS").font(.caption.weight(.semibold)).foregroundStyle(Theme.dim)
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 10), count: 3), spacing: 10) {
                ForEach(quickActions) { a in
                    Button {
                        tapHaptic()
                        Task { await ride.runCommand(a.command) }
                    } label: {
                        VStack(spacing: 6) {
                            Image(systemName: a.icon).font(.title2).foregroundStyle(a.color)
                            Text(a.title).font(.caption.weight(.semibold)).foregroundStyle(.white)
                        }
                        .frame(maxWidth: .infinity, minHeight: 76)
                        .background(GlassBackground(radius: 18, tint: a.color))
                    }
                    .buttonStyle(PressStyle())
                }
            }
        }
    }
}

private struct SOSButton: View {
    @EnvironmentObject var ride: RideController
    @State private var showSOS = false

    var body: some View {
        Button { tapHaptic(); showSOS = true } label: {
            Label("SOS", systemImage: "sos")
                .font(.headline.weight(.heavy))
                .foregroundStyle(.white)
                .frame(maxWidth: .infinity, minHeight: 54)
                .background(RoundedRectangle(cornerRadius: 18, style: .continuous).fill(Theme.redGrad.opacity(0.85)))
        }
        .buttonStyle(PressStyle())
        .confirmationDialog("Send an emergency message with your location?", isPresented: $showSOS, titleVisibility: .visible) {
            Button("Send SOS now", role: .destructive) {
                Task { await ride.say(await Emergency.shared.sendSOS(manual: true)) }
            }
        }
    }
}

private struct LogPanel: View {
    @EnvironmentObject var ride: RideController

    var body: some View {
        if !ride.log.isEmpty {
            VStack(alignment: .leading, spacing: 8) {
                Text("CONVERSATION").font(.caption.weight(.semibold)).foregroundStyle(Theme.dim)
                ForEach(ride.log.suffix(6)) { e in
                    HStack {
                        if e.isUser { Spacer(minLength: 40) }
                        Text(e.text)
                            .font(.subheadline)
                            .padding(.horizontal, 12).padding(.vertical, 8)
                            .background(
                                RoundedRectangle(cornerRadius: 16, style: .continuous)
                                    .fill(e.isUser ? AnyShapeStyle(Theme.brand.opacity(0.45)) : AnyShapeStyle(Theme.card))
                            )
                        if !e.isUser { Spacer(minLength: 40) }
                    }
                }
            }
        }
    }
}
