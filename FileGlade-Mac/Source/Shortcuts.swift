import SwiftUI
import AppKit

/// A keyboard shortcut: one key plus modifier keys.
struct KeyCombo: Codable, Equatable {
    var key: String
    var mods: UInt
    var flags: NSEvent.ModifierFlags { NSEvent.ModifierFlags(rawValue: mods) }
    var isEmpty: Bool { key.isEmpty }
    var text: String {
        if key.isEmpty { return "None" }
        var out = ""
        if flags.contains(.control) { out += "⌃" }
        if flags.contains(.option) { out += "⌥" }
        if flags.contains(.shift) { out += "⇧" }
        if flags.contains(.command) { out += "⌘" }
        return out + KeyCombo.glyph(key)
    }
    static func glyph(_ key: String) -> String {
        let table: [String: String] = ["\r": "Return", "\u{3}": "Enter", "\u{8}": "Delete", "\u{7F}": "Delete", "\u{1B}": "Esc", "\t": "Tab", " ": "Space",
            "\u{F728}": "Fn Delete", "\u{F700}": "↑", "\u{F701}": "↓", "\u{F702}": "←", "\u{F703}": "→", "\u{F704}": "F1", "\u{F705}": "F2", "\u{F706}": "F3", "\u{F707}": "F4", "\u{F708}": "F5", "\u{F709}": "F6", "\u{F70A}": "F7", "\u{F70B}": "F8", "\u{F70C}": "F9", "\u{F70D}": "F10", "\u{F70E}": "F11", "\u{F70F}": "F12"]
        return table[key] ?? key.uppercased()
    }
    static let relevant: NSEvent.ModifierFlags = [.command, .shift, .option, .control]
    static func make(_ key: String, _ flags: NSEvent.ModifierFlags = []) -> KeyCombo { KeyCombo(key: key, mods: flags.intersection(relevant).rawValue) }
}

enum ShortcutProfile: String, CaseIterable, Identifiable { case mac = "Mac", windows = "Windows"; var id: String { rawValue } }

/// Keyboard layouts. "Mac" is the default; "Windows" follows Explorer habits (⌘ stands in for Ctrl, ⌥ for Alt).
/// Individual shortcuts can then be changed on top of either layout.
final class ShortcutStore: ObservableObject {
    static let shared = ShortcutStore()
    @Published var profile: ShortcutProfile { didSet { if !Prefs.selfTest { UserDefaults.standard.set(profile.rawValue, forKey: "shortcutProfile") } } }
    @Published var overrides: [String: KeyCombo] { didSet { if !Prefs.selfTest, let data = try? JSONEncoder().encode(overrides) { UserDefaults.standard.set(data, forKey: "shortcutOverrides") } } }
    private(set) var defaults: [String: KeyCombo] = [:]
    private(set) var titles: [String: String] = [:]
    private(set) var order: [String] = []

    static let fKey: (Int) -> String = { String(UnicodeScalar(0xF703 + $0)!) }
    static let windows: [String: KeyCombo] = [
        "openSelected": .make("\r"),
        "rename": .make(fKey(2)),
        "trash": .make("\u{F728}"),
        "deletePermanently": .make("\u{F728}", .shift),
        "refresh": .make(fKey(5)),
        "back": .make("\u{F702}", .option),
        "forward": .make("\u{F703}", .option),
        "up": .make("\u{F700}", .option),
        "inspector": .make("p", .option),
        "search": .make(fKey(3)),
        "location": .make(fKey(4)),
        "quickOpen": .make("e", .command)
    ]

    init() {
        profile = ShortcutProfile(rawValue: UserDefaults.standard.string(forKey: "shortcutProfile") ?? "") ?? .mac
        overrides = (UserDefaults.standard.data(forKey: "shortcutOverrides").flatMap { try? JSONDecoder().decode([String: KeyCombo].self, from: $0) }) ?? [:]
    }

    func combo(for id: String) -> KeyCombo? {
        if let custom = overrides[id] { return custom }
        if profile == .windows, let win = Self.windows[id] { return win }
        return defaults[id]
    }
    func conflict(for combo: KeyCombo, except id: String) -> String? {
        guard !combo.isEmpty else { return nil }
        for other in order where other != id { if let c = self.combo(for: other), c == combo { return titles[other] } }
        return nil
    }
    func setCustom(_ id: String, _ combo: KeyCombo) { overrides[id] = combo; NotificationCenter.default.post(name: .init("FileGladeRebuildMenu"), object: nil) }
    func resetCustom(_ id: String) { overrides[id] = nil; NotificationCenter.default.post(name: .init("FileGladeRebuildMenu"), object: nil) }
    func resetAll() { overrides = [:]; NotificationCenter.default.post(name: .init("FileGladeRebuildMenu"), object: nil) }
    func choose(_ profile: ShortcutProfile) { self.profile = profile; NotificationCenter.default.post(name: .init("FileGladeRebuildMenu"), object: nil) }

    /// Reads each menu item's built-in shortcut, then applies the chosen layout and the user's changes.
    func apply(to menu: NSMenu, path: String = "") {
        for item in menu.items {
            if let sub = item.submenu { apply(to: sub, path: item.title); continue }
            guard let action = item.action, !item.isSeparatorItem else { continue }
            let id = NSStringFromSelector(action).replacingOccurrences(of: ":", with: "")
            if ["undo", "redo", "about", "quit"].contains(id) { continue }
            if defaults[id] == nil && titles[id] == nil { order.append(id) }
            titles[id] = (path.isEmpty ? "" : path + " ▸ ") + item.title
            if defaults[id] == nil, !item.keyEquivalent.isEmpty { defaults[id] = KeyCombo(key: item.keyEquivalent, mods: item.keyEquivalentModifierMask.intersection(KeyCombo.relevant).rawValue) }
            if let combo = combo(for: id) { item.keyEquivalent = combo.key; item.keyEquivalentModifierMask = combo.flags }
            else { item.keyEquivalent = "" }
        }
    }
}

struct ShortcutsView: View {
    @ObservedObject private var store = ShortcutStore.shared
    @State private var recording: String?
    @State private var message: String?
    @State private var monitor: Any?
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Keyboard shortcuts").font(.title3.bold())
            HStack {
                Picker("Layout", selection: Binding(get: { store.profile }, set: { store.choose($0) })) { ForEach(ShortcutProfile.allCases) { Text($0.rawValue).tag($0) } }.pickerStyle(.segmented).frame(width: 220)
                Text(store.profile == .windows ? "Windows habits: Enter opens, F2 renames, Delete trashes, F5 refreshes. ⌘ stands in for Ctrl and ⌥ for Alt." : "Mac habits: Return renames, ⌘O opens, ⌘⌫ trashes.").font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
            Text("On a MacBook, function keys such as F2 may need the Fn key unless “Use F1, F2, etc. keys as standard function keys” is on in System Settings ▸ Keyboard.").font(.caption2).foregroundStyle(.secondary)
            if let message { Text(message).font(.caption).foregroundStyle(.orange) }
            List {
                ForEach(store.order, id: \.self) { id in
                    HStack {
                        Text(store.titles[id] ?? id).lineLimit(1)
                        Spacer()
                        Button(recording == id ? "Press keys… (Esc cancels)" : (store.combo(for: id)?.text ?? "None")) { startRecording(id) }
                            .buttonStyle(.bordered).controlSize(.small).frame(minWidth: 130)
                        Button("Clear") { store.setCustom(id, KeyCombo(key: "", mods: 0)) }.controlSize(.small).disabled(store.combo(for: id) == nil)
                        Button("Reset") { store.resetCustom(id) }.controlSize(.small).disabled(store.overrides[id] == nil)
                    }
                }
            }
            HStack { Spacer(); Button("Reset All My Changes") { store.resetAll() }.disabled(store.overrides.isEmpty) }
        }.padding(18).frame(width: 680, height: 560)
        .onDisappear { stopRecording() }
    }
    private func stopRecording() { if let monitor { NSEvent.removeMonitor(monitor) }; monitor = nil; recording = nil }
    private func startRecording(_ id: String) {
        stopRecording(); message = nil; recording = id
        monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { event in
            if event.keyCode == 53 { stopRecording(); return nil }
            guard let key = event.charactersIgnoringModifiers, !key.isEmpty else { return nil }
            let flags = event.modifierFlags.intersection(KeyCombo.relevant)
            let special = key.unicodeScalars.contains { $0.value >= 0xF700 && $0.value <= 0xF8FF } || ["\r", "\u{3}", "\u{8}", "\u{7F}", "\t"].contains(key)
            if !special && flags.isEmpty { message = "Add ⌘, ⌥ or ⌃ so normal typing still works."; return nil }
            let combo = KeyCombo(key: key.count == 1 ? key.lowercased() : key, mods: flags.rawValue)
            if let other = store.conflict(for: combo, except: id) { message = "\(combo.text) is already used by “\(other)”. Change or clear that one first."; stopRecording(); return nil }
            store.setCustom(id, combo); stopRecording(); return nil
        }
    }
}
