import SwiftUI
import AppKit

enum ThemeMode: String, CaseIterable, Identifiable {
    case system = "Automatic", colorful = "Colourful", dark = "Dark", amoled = "AMOLED black"
    var id: String { rawValue }
}
enum GlassTint: String, CaseIterable, Identifiable {
    case aero = "Aero blue", aqua = "Aqua green", rose = "Rose", amber = "Amber", graphite = "Graphite"
    var id: String { rawValue }
    var colors: [Color] {
        switch self {
        case .aero: return [Color(red: 0.30, green: 0.58, blue: 0.95), Color(red: 0.45, green: 0.75, blue: 0.95)]
        case .aqua: return [Color(red: 0.10, green: 0.70, blue: 0.55), Color(red: 0.45, green: 0.85, blue: 0.35)]
        case .rose: return [Color(red: 0.90, green: 0.35, blue: 0.60), Color(red: 0.65, green: 0.45, blue: 0.95)]
        case .amber: return [Color(red: 0.98, green: 0.60, blue: 0.15), Color(red: 0.95, green: 0.35, blue: 0.30)]
        case .graphite: return [Color(white: 0.45), Color(white: 0.65)]
        }
    }
}

/// App-wide look: a colour theme, an optional Vista-style glass effect, and colourful icons.
final class Theme: ObservableObject {
    static let shared = Theme()
    @Published var mode: ThemeMode { didSet { changed("themeMode", mode.rawValue) } }
    @Published var glass: Bool { didSet { changed("themeGlass", glass) } }
    @Published var tint: GlassTint { didSet { changed("themeTint", tint.rawValue) } }
    @Published var glassStrength: Double { didSet { changed("themeGlassStrength", glassStrength) } }
    @Published var colorIcons: Bool { didSet { changed("themeColorIcons", colorIcons) } }

    init() {
        let d = UserDefaults.standard
        mode = ThemeMode(rawValue: d.string(forKey: "themeMode") ?? "") ?? .colorful
        glass = d.bool(forKey: "themeGlass")
        tint = GlassTint(rawValue: d.string(forKey: "themeTint") ?? "") ?? .aero
        glassStrength = d.object(forKey: "themeGlassStrength") as? Double ?? 0.6
        colorIcons = d.object(forKey: "themeColorIcons") as? Bool ?? true
    }
    private func changed(_ key: String, _ value: Any) {
        if !Prefs.selfTest { UserDefaults.standard.set(value, forKey: key) }
        NotificationCenter.default.post(name: .init("FileGladeThemeChanged"), object: nil)
    }

    var colorScheme: ColorScheme? { switch mode { case .system: return nil; case .colorful: return .light; case .dark, .amoled: return .dark } }
    var appearance: NSAppearance? { switch mode { case .system: return nil; case .colorful: return NSAppearance(named: .aqua); case .dark, .amoled: return NSAppearance(named: .darkAqua) } }
    var isDark: Bool { mode == .dark || mode == .amoled || (mode == .system && NSApp.effectiveAppearance.bestMatch(from: [.darkAqua, .aqua]) == .darkAqua) }

    // MARK: Palette. In glass mode surfaces are translucent so the blurred desktop shows through.
    var window: Color {
        if glass { return .clear }
        switch mode { case .amoled: return .black; case .colorful: return Color(red: 0.95, green: 0.97, blue: 1.0); default: return Color(nsColor: .windowBackgroundColor) }
    }
    var content: Color {
        if glass { return isDark ? Color.black.opacity(0.52 + glassStrength * 0.30) : Color.white.opacity(0.70 + glassStrength * 0.25) }
        switch mode { case .amoled: return .black; case .colorful: return .white; default: return Color(nsColor: .textBackgroundColor) }
    }
    var panel: Color {
        if glass { return isDark ? Color.white.opacity(0.05) : Color.white.opacity(0.30) }
        switch mode { case .amoled: return Color(white: 0.04); case .colorful: return Color(red: 0.90, green: 0.94, blue: 1.0); default: return Color.primary.opacity(0.04) }
    }
    var control: Color {
        if glass { return isDark ? Color.white.opacity(0.10) : Color.white.opacity(0.65) }
        switch mode { case .amoled: return Color(white: 0.10); case .colorful: return .white; default: return Color(nsColor: .controlBackgroundColor) }
    }
    var line: Color { mode == .amoled ? Color(white: 0.16) : Color.primary.opacity(0.12) }
}

/// A quiet tint wash over the blurred desktop, with a thin highlight along the top edge. Static and cheap.
struct GlassBackdrop: View {
    @ObservedObject private var theme = Theme.shared
    var body: some View {
        ZStack(alignment: .top) {
            LinearGradient(colors: [theme.tint.colors[0].opacity(theme.isDark ? 0.16 : 0.18), theme.tint.colors[1].opacity(theme.isDark ? 0.06 : 0.08)], startPoint: .top, endPoint: .bottom)
            Rectangle().fill(Color.white.opacity(theme.isDark ? 0.10 : 0.55)).frame(height: 1)
        }.allowsHitTesting(false)
    }
}

/// Backing view for the window: a single blurred-desktop layer (glass) beneath the SwiftUI content.
final class ThemedContainer: NSView {
    let effect = NSVisualEffectView()
    init(content: NSView) {
        super.init(frame: .zero)
        effect.blendingMode = .behindWindow; effect.state = .active; effect.material = .underWindowBackground
        effect.autoresizingMask = [.width, .height]; effect.frame = bounds
        content.autoresizingMask = [.width, .height]; content.frame = bounds
        addSubview(effect); addSubview(content)
    }
    required init?(coder: NSCoder) { fatalError() }
    override func layout() { super.layout(); effect.frame = bounds; subviews.last?.frame = bounds }
}

extension Theme {
    /// Applies the theme to the app and main window.
    func apply(to window: NSWindow?, container: ThemedContainer?) {
        NSApp.appearance = appearance
        guard let window else { return }
        container?.effect.isHidden = !glass
        container?.effect.material = .underWindowBackground
        window.isOpaque = !glass
        window.backgroundColor = glass ? .clear : (mode == .amoled ? .black : (mode == .colorful ? NSColor(red: 0.95, green: 0.97, blue: 1.0, alpha: 1) : .windowBackgroundColor))
        window.titlebarAppearsTransparent = glass
        window.hasShadow = true
    }
}

// MARK: - Colourful icons

/// Gives each kind of icon its own colour, so toolbars and sidebars read at a glance.
func symbolColor(_ name: String) -> Color {
    switch name {
    case "house", "house.fill": return .blue
    case "desktopcomputer": return .indigo
    case "arrow.down.circle": return .green
    case "doc.text", "doc.text.fill", "doc.on.doc", "doc": return .blue
    case "photo.on.rectangle": return .pink
    case "music.note": return .red
    case "film": return .orange
    case "pin", "pin.fill": return .red
    case "exclamationmark.triangle", "exclamationmark.triangle.fill": return .orange
    case "folder.badge.gearshape", "folder.badge.plus", "folder", "folder.fill": return Color(red: 0.98, green: 0.72, blue: 0.15)
    case "square.stack", "square.stack.3d.up": return .purple
    case "internaldrive": return .gray
    case "star", "star.fill": return .yellow
    case "scissors": return .orange
    case "doc.on.clipboard": return .teal
    case "pencil": return .orange
    case "trash": return .red
    case "trash.slash": return Color(red: 0.8, green: 0.1, blue: 0.2)
    case "arrow.uturn.backward": return .indigo
    case "wand.and.stars": return .purple
    case "rectangle.split.2x1", "rectangle.split.2x1.fill": return .cyan
    case "arrow.right.circle", "arrow.right.circle.fill": return .green
    case "arrow.triangle.2.circlepath": return .mint
    case "tray", "tray.full": return Color(red: 0.7, green: 0.5, blue: 0.3)
    case "sidebar.right": return .gray
    case "square.grid.2x2", "square.grid.2x2.fill": return .blue
    case "list.bullet": return .teal
    case "arrow.up.arrow.down": return .teal
    case "square.grid.3x1.below.line.grid.1x2": return .mint
    case "chevron.left", "chevron.right", "arrow.up": return .blue
    case "magnifyingglass": return .blue
    case "arrow.clockwise": return .green
    case "ellipsis": return .gray
    case "clock": return .orange
    case "externaldrive": return .brown
    case "chart.bar": return .indigo
    case "bookmark", "bookmark.fill": return .red
    case "note.text": return .yellow
    case "plus.square", "square.on.square": return .blue
    case "bolt.fill": return .yellow
    default: return .accentColor
    }
}

struct ColorSymbol: View {
    let name: String
    var size: CGFloat = 14
    var weight: Font.Weight = .regular
    var override: Color? = nil
    @ObservedObject private var theme = Theme.shared
    var body: some View {
        Image(systemName: name).font(.system(size: size, weight: weight)).symbolRenderingMode(.hierarchical)
            .foregroundStyle(theme.colorIcons ? (override ?? symbolColor(name)) : (override ?? Color.primary.opacity(0.75)))
    }
}
