import SwiftUI
import AppKit

// MARK: - Rule model

enum ConditionKind: String, Codable, CaseIterable, Identifiable {
    case nameContains = "Name contains", nameMatches = "Name matches (regex)", extensionIs = "Extension is", kindIs = "Kind is"
    case sizeOver = "Size over (MB)", sizeUnder = "Size under (MB)", olderThan = "Modified more than (days) ago", newerThan = "Modified within (days)", hasTag = "Has tag"
    var id: String { rawValue }
    var usesNumber: Bool { [.sizeOver, .sizeUnder, .olderThan, .newerThan].contains(self) }
}
struct RuleCondition: Codable, Identifiable, Equatable {
    var id = UUID()
    var kind: ConditionKind = .nameContains
    var text = ""
    var number = 30.0
}
enum ActionKind: String, Codable, CaseIterable, Identifiable {
    case moveTo = "Move to folder", copyTo = "Copy to folder", sortByMonth = "Move into year/month folders in", sortByKind = "Move into kind folders in"
    case rename = "Rename to pattern", addTag = "Add tag", trash = "Move to Trash"
    var id: String { rawValue }
    var needsFolder: Bool { [.moveTo, .copyTo, .sortByMonth, .sortByKind].contains(self) }
}
struct RuleAction: Codable, Identifiable, Equatable {
    var id = UUID()
    var kind: ActionKind = .moveTo
    var text = ""
}
struct AutomationRule: Codable, Identifiable, Equatable {
    var id = UUID()
    var name = "New rule"
    var folder = ""
    var enabled = false
    var matchAll = true
    var includeSubfolders = false
    var conditions: [RuleCondition] = [RuleCondition()]
    var actions: [RuleAction] = [RuleAction()]
}

struct PlannedChange: Identifiable {
    let id = UUID()
    let rule: String
    let file: URL
    let summary: String
    let actions: [RuleAction]
}

// MARK: - Engine

enum AutomationEngine {
    static func tags(of url: URL) -> [String] { (try? url.resourceValues(forKeys: [.tagNamesKey]).tagNames) ?? [] }
    static func addTag(_ tag: String, to url: URL) throws {
        var names = tags(of: url)
        guard !names.contains(tag) else { return }
        names.append(tag)
        try setTags(names, on: url)
    }
    /// Finder tags are written through NSURL so this works on every supported macOS version.
    static func setTags(_ names: [String], on url: URL) throws { try (url as NSURL).setResourceValue(names as NSArray, forKey: .tagNamesKey) }
    static func expand(_ pattern: String, for url: URL, index: Int) -> String {
        let modified = (try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? Date()
        func format(_ f: String) -> String { let d = DateFormatter(); d.locale = Locale(identifier: "en_US_POSIX"); d.dateFormat = f; return d.string(from: modified) }
        return pattern.replacingOccurrences(of: "{name}", with: url.deletingPathExtension().lastPathComponent)
            .replacingOccurrences(of: "{ext}", with: url.pathExtension)
            .replacingOccurrences(of: "{date}", with: format("yyyy-MM-dd"))
            .replacingOccurrences(of: "{year}", with: format("yyyy"))
            .replacingOccurrences(of: "{month}", with: format("MM"))
            .replacingOccurrences(of: "{n}", with: String(index + 1))
    }
    static func matches(_ rule: AutomationRule, _ url: URL, now: Date = Date()) -> Bool {
        let values = try? url.resourceValues(forKeys: [.fileSizeKey, .contentModificationDateKey, .isDirectoryKey])
        let size = Double(values?.fileSize ?? 0) / 1_048_576
        let modified = values?.contentModificationDate ?? .distantPast
        let days = now.timeIntervalSince(modified) / 86_400
        let results: [Bool] = rule.conditions.map { c in
            let text = c.text.trimmingCharacters(in: .whitespaces)
            switch c.kind {
            case .nameContains: return text.isEmpty || url.lastPathComponent.localizedCaseInsensitiveContains(text)
            case .nameMatches: return (try? NSRegularExpression(pattern: text, options: .caseInsensitive))?.firstMatch(in: url.lastPathComponent, range: NSRange(url.lastPathComponent.startIndex..., in: url.lastPathComponent)) != nil
            case .extensionIs: return text.lowercased().split(whereSeparator: { ", ".contains($0) }).contains { $0.trimmingCharacters(in: CharacterSet(charactersIn: ".")) == url.pathExtension.lowercased() }
            case .kindIs: return (values?.isDirectory == true ? "folders" : fileCategory(url)) == text.lowercased()
            case .sizeOver: return size > c.number
            case .sizeUnder: return size < c.number
            case .olderThan: return days > c.number
            case .newerThan: return days <= c.number
            case .hasTag: return tags(of: url).contains { $0.caseInsensitiveCompare(text) == .orderedSame }
            }
        }
        return rule.matchAll ? results.allSatisfy { $0 } : results.contains(true)
    }
    /// Files in the rule's folder that match. Recently changed files and unfinished downloads are skipped.
    static func candidates(for rule: AutomationRule) -> [URL] {
        guard !rule.folder.isEmpty, !rule.conditions.isEmpty else { return [] }
        let root = URL(fileURLWithPath: (rule.folder as NSString).expandingTildeInPath)
        let fm = FileManager.default
        var urls: [URL] = []
        let options: FileManager.DirectoryEnumerationOptions = rule.includeSubfolders ? [.skipsHiddenFiles, .skipsPackageDescendants] : [.skipsHiddenFiles, .skipsSubdirectoryDescendants, .skipsPackageDescendants]
        guard let walk = fm.enumerator(at: root, includingPropertiesForKeys: [.isDirectoryKey, .contentModificationDateKey], options: options) else { return [] }
        for case let url as URL in walk {
            if ["download", "crdownload", "part", "partial", "tmp"].contains(url.pathExtension.lowercased()) { continue }
            let v = try? url.resourceValues(forKeys: [.isDirectoryKey, .contentModificationDateKey])
            if let m = v?.contentModificationDate, Date().timeIntervalSince(m) < 60 { continue }
            if v?.isDirectory == true && !rule.conditions.contains(where: { $0.kind == .kindIs && $0.text.lowercased() == "folders" }) { continue }
            if matches(rule, url) { urls.append(url) }
            if urls.count >= 500 { break }
        }
        return urls
    }
    static func describe(_ action: RuleAction, file: URL, index: Int) -> String {
        switch action.kind {
        case .moveTo: return "Move to \(action.text)"
        case .copyTo: return "Copy to \(action.text)"
        case .sortByMonth: return "Move into \(action.text)/year/month"
        case .sortByKind: return "Move into \(action.text)/\(fileCategory(file))"
        case .rename: return "Rename to “\(expand(action.text, for: file, index: index))”"
        case .addTag: return "Tag “\(action.text)”"
        case .trash: return "Move to Trash"
        }
    }
    static func preview(_ rules: [AutomationRule]) -> [PlannedChange] {
        var out: [PlannedChange] = []
        for rule in rules {
            for (i, file) in candidates(for: rule).enumerated() {
                out.append(PlannedChange(rule: rule.name, file: file, summary: rule.actions.map { describe($0, file: file, index: i) }.joined(separator: " → "), actions: rule.actions))
            }
        }
        return out
    }
    /// Applies the planned changes. Nothing is overwritten (clashing names get "copy 2"), every move can be undone, and
    /// deleting only ever means "Move to Trash".
    @discardableResult static func run(_ changes: [PlannedChange], title: String) -> (done: Int, failed: [String]) {
        let fm = FileManager.default
        var steps: [UndoRecord.Step] = []
        var done = 0
        var failed: [String] = []
        for (index, change) in changes.enumerated() {
            var current = change.file
            do {
                for action in change.actions {
                    let folder = URL(fileURLWithPath: (action.text as NSString).expandingTildeInPath, isDirectory: true)
                    switch action.kind {
                    case .moveTo, .sortByMonth, .sortByKind, .copyTo:
                        var destinationFolder = folder
                        if action.kind == .sortByMonth { destinationFolder = folder.appendingPathComponent(expand("{year}", for: current, index: index)).appendingPathComponent(expand("{month}", for: current, index: index)) }
                        if action.kind == .sortByKind { destinationFolder = folder.appendingPathComponent(fileCategory(current)) }
                        try fm.createDirectory(at: destinationFolder, withIntermediateDirectories: true)
                        let target = TransferManager.uniqueURL(destinationFolder.appendingPathComponent(current.lastPathComponent))
                        if action.kind == .copyTo { try fm.copyItem(at: current, to: target); steps.append(.copied(target, (try? fm.attributesOfItem(atPath: target.path))?[.modificationDate] as? Date)) }
                        else { try fm.moveItem(at: current, to: target); steps.append(.moved(from: current, to: target)); current = target }
                    case .rename:
                        let name = expand(action.text, for: current, index: index)
                        try PowerFS.name(name)
                        let target = TransferManager.uniqueURL(current.deletingLastPathComponent().appendingPathComponent(name))
                        if target != current { try fm.moveItem(at: current, to: target); steps.append(.moved(from: current, to: target)); current = target }
                    case .addTag: try addTag(action.text, to: current)
                    case .trash:
                        var result: NSURL?; try fm.trashItem(at: current, resultingItemURL: &result)
                        if let result { steps.append(.trashed(original: current, trash: result as URL)) }
                    }
                }
                done += 1
            } catch { failed.append("\(change.file.lastPathComponent): \(error.localizedDescription)") }
        }
        TransferManager.shared.record(title: title, steps: steps)
        FileLibrary.shared.log("Automation", detail: "\(title): \(done) done, \(failed.count) failed")
        NotificationCenter.default.post(name: .init("FileGladeTransferDone"), object: nil)
        return (done, failed)
    }
}

// MARK: - Store and background runner

final class AutomationStore: ObservableObject {
    static let shared = AutomationStore()
    @Published var rules: [AutomationRule] { didSet { if !Prefs.selfTest, let data = try? JSONEncoder().encode(rules) { UserDefaults.standard.set(data, forKey: "automationRules") } } }
    @Published var autoRun: Bool { didSet { UserDefaults.standard.set(autoRun, forKey: "automationAutoRun") } }
    @Published var trashDays: Int { didSet { UserDefaults.standard.set(trashDays, forKey: "automationTrashDays") } }
    @Published var lastRun: String = ""
    private var timer: Timer?
    init() {
        rules = (UserDefaults.standard.data(forKey: "automationRules").flatMap { try? JSONDecoder().decode([AutomationRule].self, from: $0) }) ?? []
        autoRun = UserDefaults.standard.object(forKey: "automationAutoRun") as? Bool ?? true
        trashDays = UserDefaults.standard.integer(forKey: "automationTrashDays")
    }
    func start() {
        guard !Prefs.selfTest, timer == nil else { return }
        timer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in self?.tick() }
        DispatchQueue.main.asyncAfter(deadline: .now() + 20) { [weak self] in self?.tick() }
    }
    func tick() {
        guard autoRun else { return }
        let enabled = rules.filter(\.enabled)
        let days = trashDays
        guard !enabled.isEmpty || days > 0 else { return }
        DispatchQueue.global(qos: .utility).async { [weak self] in
            let changes = AutomationEngine.preview(enabled)
            var summary = ""
            DispatchQueue.main.sync { if !changes.isEmpty { let r = AutomationEngine.run(changes, title: "Automation"); summary = "\(r.done) file\(r.done == 1 ? "" : "s") organized" } }
            if days > 0 { let n = Self.cleanTrash(olderThanDays: days); if n > 0 { summary += (summary.isEmpty ? "" : ", ") + "\(n) old Trash item\(n == 1 ? "" : "s") deleted" } }
            if !summary.isEmpty { DispatchQueue.main.async { self?.lastRun = summary + " at " + Date().formatted(date: .omitted, time: .shortened) } }
        }
    }
    /// Permanently deletes items that have been in the Trash longer than the chosen number of days.
    static func cleanTrash(olderThanDays days: Int) -> Int {
        let trash = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".Trash")
        guard let items = try? FileManager.default.contentsOfDirectory(at: trash, includingPropertiesForKeys: [.addedToDirectoryDateKey], options: []) else { return 0 }
        let cutoff = Date().addingTimeInterval(-Double(days) * 86_400)
        var removed = 0
        for item in items {
            guard let added = try? item.resourceValues(forKeys: [.addedToDirectoryDateKey]).addedToDirectoryDate, added < cutoff else { continue }
            if (try? FileManager.default.removeItem(at: item)) != nil { removed += 1 }
        }
        return removed
    }
}

// MARK: - Window

struct AutomationView: View {
    @ObservedObject private var store = AutomationStore.shared
    @State private var selected: UUID?
    @State private var preview: [PlannedChange] = []
    @State private var message = ""
    private var index: Int? { store.rules.firstIndex { $0.id == selected } }
    var body: some View {
        HStack(spacing: 0) {
            VStack(spacing: 0) {
                List(selection: $selected) {
                    ForEach(store.rules) { rule in
                        HStack { Image(systemName: rule.enabled ? "bolt.fill" : "bolt.slash").foregroundStyle(rule.enabled ? .yellow : .secondary); Text(rule.name).lineLimit(1) }.tag(rule.id)
                    }
                }
                HStack { Button { let r = AutomationRule(); store.rules.append(r); selected = r.id } label: { Image(systemName: "plus") }; Button { if let i = index { store.rules.remove(at: i); selected = nil } } label: { Image(systemName: "minus") }.disabled(index == nil); Spacer() }.padding(8)
            }.frame(width: 190)
            Divider()
            if let i = index { editor(i) } else { overview }
        }.frame(minWidth: 860, minHeight: 600)
    }
    private var overview: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Automation").font(.title2.bold())
            Text("Rules watch a folder and organize files for you: move invoices into folders, sort downloads by type, tag or rename files, or clear old items. Add a rule with +, then use Preview to see exactly what would happen before turning it on.").foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Toggle("Run enabled rules automatically every minute while FileGlade is open", isOn: $store.autoRun)
            Divider()
            Text("Trash cleanup").font(.headline)
            Stepper(store.trashDays == 0 ? "Do not delete old Trash items" : "Permanently delete Trash items older than \(store.trashDays) days", value: $store.trashDays, in: 0...365, step: 5)
            Text("This erases items for good, without undo. It is off unless you choose a number of days. It needs Full Disk Access to see the Trash.").font(.caption).foregroundStyle(.orange)
            if !store.lastRun.isEmpty { Label(store.lastRun, systemImage: "checkmark.circle").font(.caption).foregroundStyle(.secondary) }
            Spacer()
        }.padding(20).frame(maxWidth: .infinity, alignment: .leading)
    }
    private func editor(_ i: Int) -> some View {
        Form {
            Section {
                TextField("Name", text: $store.rules[i].name)
                HStack { TextField("Folder to watch", text: $store.rules[i].folder); Button("Choose…") { if let path = chooseFolder() { store.rules[i].folder = path } } }
                Toggle("Include subfolders", isOn: $store.rules[i].includeSubfolders)
                Toggle("Rule is on", isOn: $store.rules[i].enabled)
            }
            Section("If \(store.rules[i].matchAll ? "all" : "any") of these match") {
                Picker("Match", selection: $store.rules[i].matchAll) { Text("All conditions").tag(true); Text("Any condition").tag(false) }.pickerStyle(.segmented)
                ForEach($store.rules[i].conditions) { $c in
                    HStack {
                        Picker("", selection: $c.kind) { ForEach(ConditionKind.allCases) { Text($0.rawValue).tag($0) } }.labelsHidden().frame(width: 230)
                        if c.kind.usesNumber { TextField("", value: $c.number, format: .number).frame(width: 80) } else { TextField(c.kind == .kindIs ? "documents, images, video…" : "text", text: $c.text) }
                        Button { store.rules[i].conditions.removeAll { $0.id == c.id } } label: { Image(systemName: "minus.circle") }.buttonStyle(.plain)
                    }
                }
                Button("Add condition") { store.rules[i].conditions.append(RuleCondition()) }
            }
            Section("Then do this, in order") {
                ForEach($store.rules[i].actions) { $a in
                    HStack {
                        Picker("", selection: $a.kind) { ForEach(ActionKind.allCases) { Text($0.rawValue).tag($0) } }.labelsHidden().frame(width: 230)
                        if a.kind != .trash {
                            TextField(a.kind == .rename ? "{name} {date} {year} {month} {n} {ext}" : (a.kind == .addTag ? "Tag name" : "Folder path"), text: $a.text)
                            if a.kind.needsFolder { Button("Choose…") { if let path = chooseFolder() { a.text = path } } }
                        } else { Spacer() }
                        Button { store.rules[i].actions.removeAll { $0.id == a.id } } label: { Image(systemName: "minus.circle") }.buttonStyle(.plain)
                    }
                }
                Button("Add action") { store.rules[i].actions.append(RuleAction()) }
                Text("Files changed in the last minute and unfinished downloads are skipped. Name clashes keep both files, and every change can be undone with ⌥⌘Z.").font(.caption).foregroundStyle(.secondary)
            }
            Section {
                HStack {
                    Button("Preview") { preview = AutomationEngine.preview([store.rules[i]]); message = preview.isEmpty ? "No files match right now." : "\(preview.count) file\(preview.count == 1 ? "" : "s") would change." }
                    Button("Run Now") { let r = AutomationEngine.run(preview.isEmpty ? AutomationEngine.preview([store.rules[i]]) : preview, title: "Rule “\(store.rules[i].name)”"); message = "\(r.done) done" + (r.failed.isEmpty ? "" : ", \(r.failed.count) failed: " + (r.failed.first ?? "")); preview = [] }
                    Text(message).font(.caption).foregroundStyle(.secondary)
                }
                ForEach(preview.prefix(60)) { change in VStack(alignment: .leading, spacing: 2) { Text(change.file.lastPathComponent).font(.caption.weight(.semibold)); Text(change.summary).font(.caption2).foregroundStyle(.secondary) } }
            }
        }.formStyle(.grouped)
    }
    private func chooseFolder() -> String? {
        let panel = NSOpenPanel(); panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.allowsMultipleSelection = false
        return panel.runModal() == .OK ? panel.url?.path : nil
    }
}
