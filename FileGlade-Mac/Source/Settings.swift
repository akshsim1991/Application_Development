import SwiftUI
import AppKit

struct SettingsView: View {
    @ObservedObject private var theme = Theme.shared
    @ObservedObject private var updater = Updater.shared
    @ObservedObject private var layout = PaneLayout.shared
    let openShortcuts: () -> Void
    var body: some View {
        TabView {
            appearance.tabItem { Label("Appearance", systemImage: "paintpalette") }
            general.tabItem { Label("General", systemImage: "gearshape") }
        }.frame(width: 560, height: 520).padding(16)
    }
    private var appearance: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("Theme").font(.headline)
                HStack(spacing: 12) { ForEach(ThemeMode.allCases) { mode in swatch(mode) } }
                Divider()
                Toggle(isOn: $theme.glass) { VStack(alignment: .leading) { Text("Glass").font(.headline); Text("A frosted, translucent window that softly blurs the desktop behind it.").font(.caption).foregroundStyle(.secondary) } }.toggleStyle(.switch)
                if theme.glass {
                    HStack(spacing: 10) {
                        ForEach(GlassTint.allCases) { tint in
                            Button { theme.tint = tint } label: {
                                Circle().fill(LinearGradient(colors: tint.colors, startPoint: .topLeading, endPoint: .bottomTrailing)).frame(width: 30, height: 30)
                                    .overlay(Circle().strokeBorder(theme.tint == tint ? Color.primary : Color.clear, lineWidth: 2)).help(tint.rawValue)
                            }.buttonStyle(.plain)
                        }
                    }
                    HStack { Text("Clarity").font(.caption); Slider(value: $theme.glassStrength, in: 0...1); Text("More solid = easier to read").font(.caption2).foregroundStyle(.secondary) }
                }
                Divider()
                Toggle(isOn: $theme.colorIcons) { VStack(alignment: .leading) { Text("Colourful icons").font(.headline); Text("Gives toolbar and sidebar icons their own colours.").font(.caption).foregroundStyle(.secondary) } }.toggleStyle(.switch)
                HStack(spacing: 14) { ForEach(["house", "arrow.down.circle", "doc.text", "photo.on.rectangle", "music.note", "film", "trash", "star.fill", "wand.and.stars"], id: \.self) { ColorSymbol(name: $0, size: 22) } }
            }.padding(8).frame(maxWidth: .infinity, alignment: .leading)
        }
    }
    private func swatch(_ mode: ThemeMode) -> some View {
        let selected = theme.mode == mode
        return Button { theme.mode = mode } label: {
            VStack(spacing: 6) {
                ZStack {
                    RoundedRectangle(cornerRadius: 8).fill(fill(mode))
                    VStack(alignment: .leading, spacing: 4) {
                        HStack(spacing: 3) { Circle().fill(.red).frame(width: 5, height: 5); Circle().fill(.yellow).frame(width: 5, height: 5); Circle().fill(.green).frame(width: 5, height: 5) }
                        HStack(spacing: 4) { RoundedRectangle(cornerRadius: 2).fill(accent(mode).opacity(0.7)).frame(width: 16, height: 30); VStack(spacing: 3) { ForEach(0..<3, id: \.self) { _ in RoundedRectangle(cornerRadius: 2).fill(line(mode)).frame(height: 5) } } }
                    }.padding(8)
                }.frame(width: 96, height: 66).overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(selected ? Color.accentColor : Color.primary.opacity(0.15), lineWidth: selected ? 3 : 1))
                Text(mode.rawValue).font(.caption).fontWeight(selected ? .semibold : .regular)
            }
        }.buttonStyle(.plain)
    }
    private func fill(_ m: ThemeMode) -> AnyShapeStyle {
        switch m {
        case .colorful: return AnyShapeStyle(LinearGradient(colors: [Color(red: 0.93, green: 0.96, blue: 1), .white], startPoint: .top, endPoint: .bottom))
        case .dark: return AnyShapeStyle(Color(white: 0.17))
        case .amoled: return AnyShapeStyle(Color.black)
        case .system: return AnyShapeStyle(LinearGradient(stops: [.init(color: .white, location: 0.5), .init(color: Color(white: 0.17), location: 0.5)], startPoint: .leading, endPoint: .trailing))
        }
    }
    private func accent(_ m: ThemeMode) -> Color { m == .colorful ? .blue : (m == .amoled ? .green : .orange) }
    private func line(_ m: ThemeMode) -> Color { m == .colorful ? Color.black.opacity(0.1) : Color.white.opacity(0.16) }
    private var general: some View {
        Form {
            Section("Layout") {
                Button("Reset Pane and Column Sizes") { layout.reset() }
                Text("Drag the dividers between the sidebar, file list and details pane, or between column headings, to resize them. Double-click a divider to reset it.").font(.caption).foregroundStyle(.secondary)
            }
            Section("File safety") {
                Toggle("Verify copies with a SHA-256 check", isOn: Binding(get: { UserDefaults.standard.bool(forKey: "verifyCopies") }, set: { UserDefaults.standard.set($0, forKey: "verifyCopies") }))
                Text("After each copy, FileGlade compares the copy with the original byte for byte. Slower, but catches damaged copies. A bad copy is removed and reported.").font(.caption).foregroundStyle(.secondary)
            }
            Section("Keyboard") { Button("Keyboard Shortcuts… (Mac or Windows layout)") { openShortcuts() } }
            Section("Updates") {
                Toggle("Check for updates automatically", isOn: Binding(get: { updater.automatic }, set: { updater.automatic = $0 }))
                Button("Check Now") { updater.check(userInitiated: true) }
                Text("Installed version \(updater.currentVersion)").font(.caption).foregroundStyle(.secondary)
            }
        }.formStyle(.grouped)
    }
}
