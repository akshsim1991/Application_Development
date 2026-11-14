import SwiftUI
import Foundation
import UniformTypeIdentifiers
#if os(macOS)
import AppKit
#else
import UIKit
#endif

struct PowerContext: Identifiable {
    let id = UUID()
    let folder: URL
    let selection: [URL]
}

struct LibraryRecord: Identifiable, Codable {
    var path: String
    var bookmark: Data?
    var starred = false
    var label = "None"
    var opened = Date.distantPast
    /// Tags, folder colour and folder symbol are kept inside FileGlade (iOS does not let apps write Files' own tags).
    var tags: [String]?
    var folderColor: String?
    var folderSymbol: String?
    var id: String { path }
    var name: String { URL(fileURLWithPath: path).lastPathComponent }
}
struct FileActivity: Identifiable, Codable {
    var id = UUID()
    let date: Date
    let action: String
    let detail: String
}
final class FileLibrary: ObservableObject {
    static let shared = FileLibrary()
    @Published private(set) var records: [LibraryRecord] = [] { didSet { index = Dictionary(records.map { ($0.path, $0) }, uniquingKeysWith: { first, _ in first }) } }
    /// Path lookup, so checking a file's star or label is instant instead of a scan plus a disk call.
    private var index: [String: LibraryRecord] = [:]
    private var saveWork: DispatchWorkItem?
    @Published private(set) var activities: [FileActivity] = []
    @Published var lastRename: [RenamePair] = []
    static let labels = ["None", "Red", "Orange", "Green", "Blue", "Purple"]
    private var scopes: [URL] = []
    init() {
        if let data = UserDefaults.standard.data(forKey: "FileGlade.library.v2"), let value = try? JSONDecoder().decode([LibraryRecord].self, from: data) { records = value }
        if let data = UserDefaults.standard.data(forKey: "FileGlade.activity.v2"), let value = try? JSONDecoder().decode([FileActivity].self, from: data) { activities = value }
    }
    deinit { for url in scopes { url.stopAccessingSecurityScopedResource() } }
    func record(_ url: URL) -> LibraryRecord? { index[url.path] }
    func isStarred(_ url: URL) -> Bool { record(url)?.starred ?? false }
    func label(_ url: URL) -> String { record(url)?.label ?? "None" }
    func color(_ url: URL) -> Color? {
        switch label(url) { case "Red": return .red; case "Orange": return .orange; case "Green": return .green; case "Blue": return .blue; case "Purple": return .purple; default: return nil }
    }
    private func update(_ url: URL, edit: (inout LibraryRecord) -> Void) {
        let path = powerURL(url).path
        let position = records.firstIndex { $0.path == path }
        var entry = position.map { records[$0] } ?? LibraryRecord(path: path, bookmark: nil)
        edit(&entry)
        // Bookmarks are only needed for items you starred or labeled; skipping them keeps opening folders fast.
        if entry.bookmark == nil && (entry.starred || entry.label != "None" || entry.tags != nil || entry.folderColor != nil || entry.folderSymbol != nil) { entry.bookmark = try? url.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil) }
        if let position { records[position] = entry } else { records.append(entry) }
        save()
    }
    func tags(_ url: URL) -> [String] { record(url)?.tags ?? [] }
    func setTags(_ tags: [String], urls: [URL]) {
        for url in urls { update(url) { $0.tags = tags.isEmpty ? nil : tags } }
    }
    func addTag(_ tag: String, urls: [URL]) {
        for url in urls { update(url) { var t = $0.tags ?? []; if !t.contains(tag) { t.append(tag) }; $0.tags = t } }
    }
    func removeTag(_ tag: String, urls: [URL]) {
        for url in urls { update(url) { let t = ($0.tags ?? []).filter { $0 != tag }; $0.tags = t.isEmpty ? nil : t } }
    }
    /// Every tag in use, with how many items carry it.
    var allTags: [(name: String, count: Int)] {
        var counts: [String: Int] = [:]
        for record in records { for tag in record.tags ?? [] { counts[tag, default: 0] += 1 } }
        return counts.map { ($0.key, $0.value) }.sorted { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
    }
    func items(tagged tag: String) -> [LibraryRecord] { records.filter { ($0.tags ?? []).contains(tag) } }
    func folderStyle(_ url: URL) -> (color: String?, symbol: String?) { let r = record(url); return (r?.folderColor, r?.folderSymbol) }
    func setFolderStyle(_ url: URL, color: String?, symbol: String?) { update(url) { $0.folderColor = color; $0.folderSymbol = symbol } }
    func toggleStar(_ url: URL) { update(url) { $0.starred.toggle() } }
    func setLabel(_ label: String, urls: [URL]) { for url in urls { update(url) { $0.label = label } } }
    func opened(_ url: URL) { update(url) { $0.opened = Date() } }
    func applyRename(_ pairs: [RenamePair]) {
        // Rewrite all records from a snapshot so swaps do not relabel the wrong file.
        records = records.map { record in
            guard let pair = pairs.first(where: { record.path == $0.source.path || record.path.hasPrefix($0.source.path + "/") }) else { return record }
            var next = record
            next.path = pair.destination.path + String(record.path.dropFirst(pair.source.path.count))
            next.bookmark = try? URL(fileURLWithPath: next.path).bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil)
            return next
        }
        save()
        NotificationCenter.default.post(name: .init("FileGladePathsMoved"), object: nil, userInfo: ["pairs": pairs])
    }
    func resolve(_ record: LibraryRecord) -> URL {
        if let data = record.bookmark {
            var stale = false
            if let url = try? URL(resolvingBookmarkData: data, options: .withoutUI, relativeTo: nil, bookmarkDataIsStale: &stale) {
                if !scopes.contains(url), url.startAccessingSecurityScopedResource() { scopes.append(url) }
                return url
            }
        }
        return URL(fileURLWithPath: record.path)
    }
    func log(_ action: String, detail: String) {
        activities.insert(FileActivity(date: Date(), action: action, detail: detail), at: 0)
        activities = Array(activities.prefix(100))
        if let data = try? JSONEncoder().encode(activities) { UserDefaults.standard.set(data, forKey: "FileGlade.activity.v2") }
    }
    private func save() {
        func keep(_ r: LibraryRecord) -> Bool { r.starred || r.label != "None" || r.tags != nil || r.folderColor != nil || r.folderSymbol != nil }
        let pinned = records.filter(keep)
        let recent = records.filter { !keep($0) }.sorted { $0.opened > $1.opened }.prefix(100)
        records = pinned + recent
        // Writing to disk is batched, so rapid navigation does not stall on it.
        saveWork?.cancel()
        let snapshot = records
        let work = DispatchWorkItem { if let data = try? JSONEncoder().encode(snapshot) { UserDefaults.standard.set(data, forKey: "FileGlade.library.v2") } }
        saveWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.6, execute: work)
    }
}

enum PowerOutput {
    case files([URL]), renamed([RenamePair]), inventory(ScanInventory), duplicates(ScanInventory, [DuplicateSet], Int), hashes([(URL, String)]), text(TextSnapshot)
}
@MainActor final class PowerJob: ObservableObject {
    @Published var running = false
    @Published var status = ""
    @Published var error: String?
    @Published var output: PowerOutput?
    private var task: Task<PowerOutput, Error>?
    func run(_ title: String, work: @escaping @Sendable () throws -> PowerOutput, completion: @escaping (PowerOutput) -> Void = { _ in }) {
        guard !running else { return }
        running = true; status = title; error = nil; output = nil
        let task = Task.detached(priority: .userInitiated, operation: work); self.task = task
        Task {
            do { let result = try await task.value; output = result; status = "Completed"; running = false; completion(result); FileLibrary.shared.log(title, detail: "Completed") }
            catch { running = false; self.error = error is CancellationError ? "Cancelled. Original files were kept." : error.localizedDescription; status = ""; FileLibrary.shared.log(title, detail: self.error ?? "Failed") }
            self.task = nil
        }
    }
    func cancel() { task?.cancel(); status = "Stopping after the current file…" }
}
struct JobStatus: View {
    @ObservedObject var job: PowerJob
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            if job.running { HStack { ProgressView().controlSize(.small); Text(job.status).font(.caption); Spacer(); Button("Cancel") { job.cancel() }.font(.caption) } }
            else if !job.status.isEmpty { Label(job.status, systemImage: "checkmark.circle").font(.caption).foregroundStyle(.green) }
            if let error = job.error { Text(error).font(.caption).foregroundStyle(.red).textSelection(.enabled) }
        }.padding(.vertical, 5)
    }
}
extension View {
    @ViewBuilder func powerTitle(_ title: String) -> some View {
        #if os(macOS)
        self.navigationTitle(title)
        #else
        self.navigationTitle(title).navigationBarTitleDisplayMode(.inline)
        #endif
    }
}
func copyFileText(_ text: String) {
    #if os(macOS)
    NSPasteboard.general.clearContents(); NSPasteboard.general.setString(text, forType: .string)
    #else
    UIPasteboard.general.string = text
    #endif
}

struct PowerToolsView: View {
    let folder: URL
    let selection: [URL]
    let onProduced: ([URL]) -> Void
    let onReveal: (URL) -> Void
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 8) { Text("Power tools").font(.title2.bold()); Text("\(selection.count) selected · \(folder.lastPathComponent)").font(.caption).foregroundStyle(.secondary) }.padding(.vertical, 10)
                }
                Section("Organize") {
                    NavigationLink { BatchRenameView(urls: selection, onProduced: onProduced) } label: { ToolRow("Batch rename", "Preview names, replace text, add counters, and undo the last batch.", "character.cursor.ibeam") }
                    NavigationLink { ArchiveToolsView(folder: folder, urls: selection, onProduced: onProduced, reveal: reveal) } label: { ToolRow("ZIP archives", "Compress selections or extract ZIPs into a new folder.", "archivebox") }
                    NavigationLink { LibraryToolsView(reveal: reveal) } label: { ToolRow("Starred, recents & activity", "Return to important files and review recent operations.", "star") }
                }
                Section("Find & understand") {
                    NavigationLink { ScanToolsView(folder: folder, mode: .search, reveal: reveal) } label: { ToolRow("Search subfolders", "Find names by type, extension, size, and modified date.", "magnifyingglass") }
                    NavigationLink { ScanToolsView(folder: folder, mode: .duplicates, reveal: reveal) } label: { ToolRow("Find duplicates", "Compare file contents with SHA-256; review before removing.", "doc.on.doc") }
                    NavigationLink { ScanToolsView(folder: folder, mode: .storage, reveal: reveal) } label: { ToolRow("Storage analysis", "See the largest files and space used by file type.", "chart.bar") }
                    NavigationLink { ChecksumToolsView(urls: selection) } label: { ToolRow("Verify checksums", "Calculate and copy SHA-256 fingerprints for selected files.", "checkmark.shield") }
                }
                Section("Create & convert") {
                    NavigationLink { TextToolsView(folder: folder, url: selection.count == 1 && PowerFS.textExtensions.contains(selection[0].pathExtension.lowercased()) ? selection[0] : nil, onProduced: onProduced) } label: { ToolRow("Text editor", "Edit small UTF-8 files or create a note without leaving the app.", "square.and.pencil") }
                    NavigationLink { PDFToolsView(folder: folder, urls: selection, onProduced: onProduced, reveal: reveal) } label: { ToolRow("Combine PDFs & images", "Order documents and images, then save a new PDF.", "doc.richtext") }
                    NavigationLink { ImageToolsView(folder: folder, urls: selection, onProduced: onProduced, reveal: reveal) } label: { ToolRow("Resize & convert images", "Export smaller JPEG or PNG copies for sharing.", "photo.on.rectangle") }
                }
                Section { Text("Tools work on the current folder and the files you selected. Stars, color labels, recents, and activity are stored by this app on this device.").font(.caption).foregroundStyle(.secondary) }
            }.listStyle(.inset)
            .powerTitle("FileGlade")
            .toolbar { ToolbarItem(placement: .cancellationAction) { Button("Done") { dismiss() } } }
        }
        #if os(macOS)
        .frame(minWidth: 740, idealWidth: 800, minHeight: 580, idealHeight: 720)
        #endif
    }
    private func reveal(_ url: URL) { onReveal(url); dismiss() }
}
struct ToolRow: View {
    let title: String; let detail: String; let icon: String
    init(_ title: String, _ detail: String, _ icon: String) { self.title = title; self.detail = detail; self.icon = icon }
    var body: some View { HStack(spacing: 14) { Image(systemName: icon).font(.title3).foregroundStyle(.blue).frame(width: 30); VStack(alignment: .leading, spacing: 5) { Text(title).font(.headline); Text(detail).font(.caption).foregroundStyle(.secondary) } }.padding(.vertical, 8) }
}

struct BatchRenameView: View {
    let urls: [URL]
    let onProduced: ([URL]) -> Void
    @State private var options = RenameOptions()
    @State private var presets = RenamePresets.all()
    @State private var presetName = ""
    @StateObject private var job = PowerJob()
    @ObservedObject private var library = FileLibrary.shared
    private var plan: [RenamePair] { urls.enumerated().map { RenamePair(source: $0.element, destination: $0.element.deletingLastPathComponent().appendingPathComponent(options.name(for: $0.element, index: $0.offset))) } }
    private var validation: String? { if let regex = options.regexError { return regex }; do { try PowerFS.validateRename(plan); return nil } catch { return error.localizedDescription } }
    var body: some View {
        Form {
            Section("Presets") {
                HStack {
                    Menu("Load preset") {
                        if presets.isEmpty { Text("No saved presets") }
                        ForEach(presets.keys.sorted(), id: \.self) { name in Button(name) { options = presets[name] ?? options } }
                        if !presets.isEmpty { Divider(); Menu("Delete preset") { ForEach(presets.keys.sorted(), id: \.self) { name in Button(name, role: .destructive) { RenamePresets.delete(name); presets = RenamePresets.all() } } } }
                    }
                    Spacer()
                    TextField("Preset name", text: $presetName).frame(maxWidth: 160)
                    Button("Save preset") { let name = presetName.trimmingCharacters(in: .whitespaces); guard !name.isEmpty else { return }; RenamePresets.save(name, options); presets = RenamePresets.all(); presetName = "" }.disabled(presetName.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
            Section("Naming rules · extensions are preserved") {
                TextField(options.useRegex ? "Find (regular expression)" : "Find text", text: $options.find); TextField(options.useRegex ? "Replace with ($1 for groups)" : "Replace with", text: $options.replace)
                Toggle("Use regular expressions", isOn: $options.useRegex)
                if let error = options.regexError { Text(error).font(.caption).foregroundStyle(.red) }
                TextField("Prefix", text: $options.prefix); TextField("Suffix", text: $options.suffix)
                Picker("Letter case", selection: $options.caseMode) { ForEach(RenameCase.allCases) { Text($0.rawValue).tag($0) } }
                Picker("Spaces", selection: $options.spaces) { ForEach(RenameSpaces.allCases) { Text($0.rawValue).tag($0) } }
                Picker("Insert date", selection: $options.date) { ForEach(RenameDate.allCases) { Text($0.rawValue).tag($0) } }
                if options.date != .none { Picker("Date position", selection: $options.dateAtStart) { Text("Before name").tag(true); Text("After name").tag(false) } }
                Toggle("Append a number", isOn: $options.numbered)
                if options.numbered { Stepper("Start: \(options.start)", value: $options.start, in: 0...999999); Stepper("Digits: \(options.digits)", value: $options.digits, in: 1...8) }
            }.disabled(job.running)
            Section("Preview · \(plan.count) items") {
                ForEach(plan) { pair in VStack(alignment: .leading, spacing: 5) { Text(pair.source.lastPathComponent).font(.caption).foregroundStyle(.secondary); Label(pair.destination.lastPathComponent, systemImage: "arrow.turn.down.right").font(.body) } }
                if let validation { Text(validation).font(.caption).foregroundStyle(.red) }
            }
            Section {
                Button("Rename \(plan.count) items") {
                    let pairs = plan
                    job.run("Batch rename", work: { .renamed(try PowerFS.rename(pairs)) }) { output in
                        if case .renamed(let undo) = output { library.applyRename(pairs); library.lastRename = undo; onProduced(pairs.map(\.destination)) }
                    }
                }.disabled(validation != nil || job.running || plan.allSatisfy { $0.source.path == $0.destination.path })
                Button("Undo last batch rename") {
                    let pairs = library.lastRename
                    job.run("Undo batch rename", work: { _ = try PowerFS.rename(pairs); return .files(pairs.map(\.destination)) }) { _ in library.applyRename(pairs); library.lastRename = []; onProduced(pairs.map(\.destination)) }
                }.disabled(library.lastRename.isEmpty || job.running)
                JobStatus(job: job)
            }
        }.formStyle(.grouped).powerTitle("Batch rename")
    }
}

struct ArchiveToolsView: View {
    let folder: URL; let urls: [URL]; let onProduced: ([URL]) -> Void; let reveal: (URL) -> Void
    @State private var name = "Archive.zip"
    @StateObject private var job = PowerJob()
    @State private var created: [URL] = []
    var body: some View {
        Form {
            Section("Create a ZIP") {
                Text("\(urls.count) items selected").font(.caption).foregroundStyle(.secondary)
                TextField("Archive name", text: $name)
                Button("Compress selection") {
                    let name = name
                    job.run("Create ZIP", work: { .files([try PowerFS.zip(urls, into: folder, name: name)]) }, completion: finished)
                }.disabled(urls.isEmpty || job.running)
            }
            Section("Extract ZIPs") {
                Text("Extraction creates a separate folder and preserves existing files. Symbolic links and unsafe paths are rejected. Up to 50,000 entries and 4 GB expanded per ZIP.").font(.caption).foregroundStyle(.secondary)
                Button("Extract selected ZIPs") {
                    let archives = urls.filter { $0.pathExtension.lowercased() == "zip" }
                    job.run("Extract ZIP", work: {
                        var results: [URL] = []
                        for archive in archives { results.append(try PowerFS.unzip(archive, into: folder)) }
                        return .files(results)
                    }, completion: finished)
                }.disabled(!urls.contains { $0.pathExtension.lowercased() == "zip" } || job.running)
            }
            Section { JobStatus(job: job); ForEach(created, id: \.self) { url in Button("Show \(url.lastPathComponent)") { reveal(url) } } }
        }.formStyle(.grouped).powerTitle("ZIP archives")
    }
    private func finished(_ output: PowerOutput) { if case .files(let urls) = output { created = urls; onProduced(urls) } }
}

enum ScanMode: String, CaseIterable { case search = "Search subfolders", duplicates = "Find duplicates", storage = "Storage analysis" }
struct ScanToolsView: View {
    let folder: URL; let mode: ScanMode; let reveal: (URL) -> Void
    @StateObject private var job = PowerJob()
    @State private var inventory: ScanInventory?
    @State private var duplicates: [DuplicateSet] = []
    @State private var hashSkipped = 0
    @State private var query = ""
    @State private var ext = ""
    @State private var minimumMB = ""
    @State private var recent = false
    @State private var hidden = false
    @State private var filter: PowerFilter = .all
    private var matches: [PowerFile] {
        guard let inventory else { return [] }
        let extensions = Set(ext.lowercased().split(separator: ",").map { $0.trimmingCharacters(in: .whitespacesAndNewlines).replacingOccurrences(of: ".", with: "") })
        let minimum = (Double(minimumMB) ?? 0) * 1_000_000
        let candidates = mode == .search ? inventory.files + inventory.directories : inventory.files
        return candidates.filter { file in
            (query.isEmpty || file.name.localizedStandardContains(query)) && (extensions.isEmpty || extensions.contains(file.url.pathExtension.lowercased())) && Double(file.size) >= minimum && (!recent || file.modified > Date().addingTimeInterval(-7 * 86400)) && filter.accepts(file, starred: FileLibrary.shared.isStarred(file.url))
        }.sorted { mode == .storage ? $0.size > $1.size : $0.url.path.localizedStandardCompare($1.url.path) == .orderedAscending }
    }
    var body: some View {
        List {
            Section {
                Text(folder.path).font(.caption).foregroundStyle(.secondary).textSelection(.enabled)
                if mode == .search {
                    TextField("Name contains", text: $query)
                    TextField("Extensions, e.g. pdf, docx", text: $ext)
                    TextField("Minimum size in MB", text: $minimumMB)
                    Toggle("Modified in the last 7 days", isOn: $recent)
                    Picker("File type", selection: $filter) { ForEach(PowerFilter.allCases) { Text($0.rawValue).tag($0) } }
                }
                Toggle("Include hidden files", isOn: $hidden).disabled(job.running)
                Button(inventory == nil ? "Scan this folder and subfolders" : "Scan again") {
                    let hidden = hidden
                    job.run(mode.rawValue, work: {
                        let report = try PowerFS.inventory(folder, hidden: hidden)
                        if mode == .duplicates { let result = try PowerFS.duplicates(report); return .duplicates(report, result.groups, result.skipped) }
                        return .inventory(report)
                    }) { result in
                        switch result { case .inventory(let report): inventory = report; case .duplicates(let report, let groups, let skipped): inventory = report; duplicates = groups; hashSkipped = skipped; default: break }
                    }
                }.disabled(job.running)
                JobStatus(job: job)
            }
            if let report = inventory {
                Section("Summary") {
                    Text("\(report.files.count) files · \(report.folders) folders · \(fileBytes(report.total))")
                    Text("\(report.skipped + hashSkipped) skipped or unreadable\(report.truncated ? " · scan stopped at 100,000 entries" : "")").font(.caption).foregroundStyle(.secondary)
                    if mode == .duplicates { Text("\(duplicates.count) duplicate groups · \(fileBytes(duplicates.reduce(0) { $0 + $1.recoverableBytes })) in extra copies").font(.subheadline); Text("Content matches are verified with SHA-256. Nothing is removed automatically. Undownloaded iCloud files are skipped.").font(.caption).foregroundStyle(.secondary) }
                }
                if mode == .duplicates {
                    if duplicates.isEmpty { Text("No duplicate contents found among readable files.").foregroundStyle(.secondary) }
                    ForEach(duplicates) { group in
                        Section("\(group.files.count) copies · \(fileBytes(group.files.first?.size ?? 0)) each") { ForEach(group.files) { file in resultRow(file) } }
                    }
                } else {
                    if mode == .storage {
                        Section("Space by type") {
                            ForEach(categories(report), id: \.0) { category in HStack { Text(category.0); Spacer(); Text(fileBytes(category.1)).foregroundStyle(.secondary) } }
                        }
                    }
                    Section(mode == .storage ? "Largest files · first \(min(matches.count, 200))" : "\(matches.count) matching items · first \(min(matches.count, 500))") {
                        ForEach(Array(matches.prefix(mode == .storage ? 200 : 500))) { file in resultRow(file) }
                    }
                }
            }
        }.listStyle(.inset).powerTitle(mode.rawValue)
    }
    private func resultRow(_ file: PowerFile) -> some View {
        Button { reveal(file.url) } label: { VStack(alignment: .leading, spacing: 5) { Text(file.name).foregroundStyle(.primary); Text(file.url.deletingLastPathComponent().path).font(.caption2).foregroundStyle(.secondary).lineLimit(2); Text(file.directory ? "Folder" : fileBytes(file.size)).font(.caption).foregroundStyle(.secondary) } }.buttonStyle(.plain)
    }
    private func categories(_ report: ScanInventory) -> [(String, Int64)] {
        Dictionary(grouping: report.files, by: { $0.url.pathExtension.isEmpty ? "No extension" : $0.url.pathExtension.uppercased() }).map { ($0.key, $0.value.reduce(Int64(0)) { $0 + $1.size }) }.sorted { $0.1 > $1.1 }.prefix(12).map { $0 }
    }
}

struct LibraryToolsView: View {
    let reveal: (URL) -> Void
    @ObservedObject private var library = FileLibrary.shared
    @State private var section = "Starred"
    private var entries: [LibraryRecord] { section == "Starred" ? library.records.filter(\.starred) : Array(library.records.filter { $0.opened > .distantPast }.sorted { $0.opened > $1.opened }.prefix(50)) }
    var body: some View {
        List {
            Picker("Show", selection: $section) { Text("Starred").tag("Starred"); Text("Recents").tag("Recents"); Text("Activity").tag("Activity") }.pickerStyle(.segmented)
            if section == "Activity" {
                ForEach(library.activities) { activity in VStack(alignment: .leading, spacing: 5) { Text(activity.action).font(.headline); Text(activity.detail).font(.caption).foregroundStyle(.secondary); Text(activity.date.formatted()).font(.caption2).foregroundStyle(.tertiary) } }
                if library.activities.isEmpty { Text("No recorded operations yet.").foregroundStyle(.secondary) }
            } else {
                ForEach(entries) { record in
                    HStack {
                        Button { reveal(library.resolve(record)) } label: { VStack(alignment: .leading, spacing: 5) { Label(record.name, systemImage: record.starred ? "star.fill" : "clock"); Text(record.path).font(.caption2).foregroundStyle(.secondary).lineLimit(2) } }.buttonStyle(.plain)
                        Spacer()
                        if record.starred { Button { library.toggleStar(URL(fileURLWithPath: record.path)) } label: { Image(systemName: "star.slash") }.buttonStyle(.borderless).accessibilityLabel("Remove star") }
                    }
                }
                if entries.isEmpty { Text(section == "Starred" ? "Star files from their context menu to find them here." : "Opened files and folders appear here.").foregroundStyle(.secondary) }
            }
        }.listStyle(.inset).powerTitle("Your library")
    }
}

struct ChecksumToolsView: View {
    let urls: [URL]
    @StateObject private var job = PowerJob()
    @State private var hashes: [(URL, String)] = []
    @State private var expected = ""
    var body: some View {
        List {
            Section { Text("SHA-256 fingerprints let you verify a downloaded file or compare copies. Select files in the browser first.").font(.caption).foregroundStyle(.secondary)
                TextField("Expected SHA-256 (optional, for one file)", text: $expected)
                Button("Calculate checksums") {
                    let files = urls.filter { !PowerFile($0).directory }
                    job.run("Calculate checksums", work: { .hashes(try files.map { ($0, try PowerFS.digest($0)) }) }) { if case .hashes(let result) = $0 { hashes = result } }
                }.disabled(urls.filter { !PowerFile($0).directory }.isEmpty || job.running)
                JobStatus(job: job)
                if hashes.count == 1 && !expected.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    let value = expected.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
                    Label(value == hashes[0].1 ? "Checksum matches" : "Checksum does not match", systemImage: value == hashes[0].1 ? "checkmark.shield" : "exclamationmark.shield").foregroundStyle(value == hashes[0].1 ? Color.green : .red)
                }
            }
            ForEach(hashes.indices, id: \.self) { i in VStack(alignment: .leading, spacing: 8) { Text(hashes[i].0.lastPathComponent).font(.headline); Text(hashes[i].1).font(.system(.caption, design: .monospaced)).textSelection(.enabled); Button("Copy SHA-256") { copyFileText(hashes[i].1) } } }
        }.listStyle(.inset).powerTitle("Verify checksums")
    }
}

struct TextToolsView: View {
    let folder: URL; let url: URL?; let onProduced: ([URL]) -> Void
    @StateObject private var job = PowerJob()
    @State private var snapshot: TextSnapshot?
    @State private var text = ""
    @State private var filename = "Untitled.txt"
    @State private var loaded = false
    @State private var saved = false
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            if url == nil { TextField("Filename", text: $filename).textFieldStyle(.roundedBorder).disabled(snapshot != nil) }
            Text("UTF-8 text · up to 2 MB · saves check for external changes").font(.caption).foregroundStyle(.secondary)
            TextEditor(text: $text).font(.system(.body, design: .monospaced)).border(Color.secondary.opacity(0.2)).disabled(!loaded || job.running).onChange(of: text) { _ in saved = false }
            HStack { Text("\(text.count) characters").font(.caption).foregroundStyle(.secondary); Spacer(); if saved { Label("Saved", systemImage: "checkmark").font(.caption).foregroundStyle(.green) }
                Button("Save") {
                    let content = text; let name = filename
                    let existing = snapshot
                    job.run("Save text", work: {
                        try PowerFS.name(name)
                        let state = existing ?? TextSnapshot(url: folder.appendingPathComponent(name), text: "", digest: nil)
                        let target = try PowerFS.saveText(content, snapshot: state)
                        return .text(try PowerFS.readText(target))
                    }) { if case .text(let state) = $0 { snapshot = state; saved = true; onProduced([state.url]) } }
                }.buttonStyle(.borderedProminent).disabled(!loaded || job.running)
            }
            JobStatus(job: job)
        }.padding(20).powerTitle(url?.lastPathComponent ?? "New text file")
        .onAppear {
            guard !loaded else { return }
            if let url { job.run("Open text", work: { .text(try PowerFS.readText(url)) }) { if case .text(let state) = $0 { snapshot = state; text = state.text; filename = state.url.lastPathComponent; loaded = true } } }
            else { loaded = true }
        }
    }
}

struct PDFToolsView: View {
    let folder: URL; let urls: [URL]; let onProduced: ([URL]) -> Void; let reveal: (URL) -> Void
    @State private var ordered: [URL] = []
    @State private var name = "Combined.pdf"
    @State private var created: URL?
    @StateObject private var job = PowerJob()
    var body: some View {
        List {
            Section { Text("Select PDFs and images in the browser. Files are combined in the order below, with one page per image. The originals are kept.").font(.caption).foregroundStyle(.secondary); TextField("Output filename", text: $name) }
            Section("Page source order") {
                ForEach(ordered.indices, id: \.self) { i in HStack { Text("\(i + 1). \(ordered[i].lastPathComponent)").font(.subheadline); Spacer(); Button { ordered.swapAt(i, i - 1) } label: { Image(systemName: "arrow.up") }.disabled(i == 0 || job.running).buttonStyle(.borderless); Button { ordered.swapAt(i, i + 1) } label: { Image(systemName: "arrow.down") }.disabled(i + 1 == ordered.count || job.running).buttonStyle(.borderless) } }
                if ordered.isEmpty { Text("No supported PDFs or images selected.").foregroundStyle(.secondary) }
            }
            Section {
                Button("Create combined PDF") {
                    let files = ordered; let name = name
                    job.run("Combine PDF", work: { .files([try PowerFS.combinePDFs(files, into: folder, name: name)]) }) { if case .files(let result) = $0 { created = result.first; onProduced(result) } }
                }.disabled(ordered.isEmpty || job.running)
                JobStatus(job: job)
                if let created { Button("Show combined PDF") { reveal(created) } }
            }
        }.listStyle(.inset).powerTitle("Combine PDFs & images")
        .onAppear { if ordered.isEmpty { ordered = urls.filter { $0.pathExtension.lowercased() == "pdf" || UTType(filenameExtension: $0.pathExtension)?.conforms(to: .image) == true } } }
    }
}

struct ImageToolsView: View {
    let folder: URL; let urls: [URL]; let onProduced: ([URL]) -> Void; let reveal: (URL) -> Void
    @State private var maximum = 1920
    @State private var jpeg = true
    @State private var created: [URL] = []
    @StateObject private var job = PowerJob()
    private var images: [URL] { urls.filter { UTType(filenameExtension: $0.pathExtension)?.conforms(to: .image) == true } }
    var body: some View {
        Form {
            Section("Export copies · \(images.count) selected images") {
                Picker("Longest edge", selection: $maximum) { ForEach([640, 1280, 1920, 2560, 3840], id: \.self) { Text("\($0) px").tag($0) } }
                Picker("Format", selection: $jpeg) { Text("JPEG").tag(true); Text("PNG").tag(false) }
                Text("Exports the first frame of each image, applies its orientation, and keeps the original file. Transparent images are best exported as PNG.").font(.caption).foregroundStyle(.secondary)
                Button("Export resized images") {
                    let maximum = maximum; let jpeg = jpeg; let images = images
                    job.run("Resize images", work: { .files(try PowerFS.resizeImages(images, into: folder, maximum: maximum, jpeg: jpeg)) }) { if case .files(let urls) = $0 { created = urls; onProduced(urls) } }
                }.disabled(images.isEmpty || job.running)
                JobStatus(job: job)
            }
            if !created.isEmpty { Section("Created files") { ForEach(created, id: \.self) { url in Button(url.lastPathComponent) { reveal(url) } } } }
        }.formStyle(.grouped).powerTitle("Resize & convert images")
    }
}
