import SwiftUI
import AppKit

// MARK: - Notes (searchable, stored locally)

final class NotesStore: ObservableObject {
    static let shared = NotesStore()
    @Published private(set) var notes: [String: String]
    init() {
        notes = (UserDefaults.standard.data(forKey: "fileNotes").flatMap { try? JSONDecoder().decode([String: String].self, from: $0) }) ?? [:]
        NotificationCenter.default.addObserver(forName: .init("FileGladePathsMoved"), object: nil, queue: .main) { [weak self] note in
            guard let pairs = note.userInfo?["pairs"] as? [RenamePair] else { return }
            self?.moved(pairs)
        }
    }
    func note(for url: URL) -> String { notes[url.path] ?? "" }
    func set(_ text: String, for url: URL) {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty { notes[url.path] = nil } else { notes[url.path] = text }
        save()
    }
    func search(_ query: String) -> [URL] {
        let q = query.lowercased()
        return notes.filter { $0.value.lowercased().contains(q) && FileManager.default.fileExists(atPath: $0.key) }.keys.sorted().map { URL(fileURLWithPath: $0) }
    }
    private func moved(_ pairs: [RenamePair]) {
        var changed = false
        for pair in pairs { if let text = notes[pair.source.path] { notes[pair.source.path] = nil; notes[pair.destination.path] = text; changed = true } }
        if changed { save() }
    }
    private func save() { if !Prefs.selfTest, let data = try? JSONEncoder().encode(notes) { UserDefaults.standard.set(data, forKey: "fileNotes") } }
}

// MARK: - Collections (virtual folders that never move files)

struct CollectionItem: Codable, Hashable, Identifiable {
    var path: String
    var bookmark: Data?
    var id: String { path }
}
struct FileCollection: Codable, Identifiable {
    var id = UUID()
    var name: String
    var items: [CollectionItem] = []
}

final class CollectionStore: ObservableObject {
    static let shared = CollectionStore()
    @Published var collections: [FileCollection] { didSet { if !Prefs.selfTest, let data = try? JSONEncoder().encode(collections) { UserDefaults.standard.set(data, forKey: "fileCollections") } } }
    init() { collections = (UserDefaults.standard.data(forKey: "fileCollections").flatMap { try? JSONDecoder().decode([FileCollection].self, from: $0) }) ?? [] }

    func create(_ name: String, with urls: [URL] = []) -> UUID {
        var collection = FileCollection(name: name)
        collection.items = urls.map(Self.item)
        collections.append(collection)
        return collection.id
    }
    func add(_ urls: [URL], to id: UUID) {
        guard let i = collections.firstIndex(where: { $0.id == id }) else { return }
        for url in urls where !collections[i].items.contains(where: { $0.path == url.path }) { collections[i].items.append(Self.item(url)) }
    }
    func remove(_ item: CollectionItem, from id: UUID) { if let i = collections.firstIndex(where: { $0.id == id }) { collections[i].items.removeAll { $0.path == item.path } } }
    static func item(_ url: URL) -> CollectionItem { CollectionItem(path: url.path, bookmark: try? url.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil)) }

    /// Finds an item even if it was moved or renamed (bookmarks follow files on the same disk).
    /// Returns nil when it is truly missing.
    func resolve(_ item: CollectionItem, in id: UUID) -> URL? {
        if FileManager.default.fileExists(atPath: item.path) { return URL(fileURLWithPath: item.path) }
        var stale = false
        if let data = item.bookmark, let url = try? URL(resolvingBookmarkData: data, options: .withoutUI, relativeTo: nil, bookmarkDataIsStale: &stale), FileManager.default.fileExists(atPath: url.path) {
            relink(item, to: url, in: id)
            return url
        }
        return nil
    }
    func relink(_ item: CollectionItem, to url: URL, in id: UUID) {
        guard let c = collections.firstIndex(where: { $0.id == id }), let i = collections[c].items.firstIndex(where: { $0.path == item.path }) else { return }
        collections[c].items[i] = Self.item(url)
    }
}

/// Looks for a file by name in the dashboard index, to reconnect something that was moved.
enum Recovery {
    static func candidates(named name: String) -> [URL] {
        let db = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("FileGladeDashboard/index-v1.sqlite")
        guard let index = try? DashboardIndex(at: db), let page = try? index.page(query: name, kind: .all, limit: 40) else { return [] }
        return page.items.filter { $0.name == name }.map(\.url)
    }
}

struct CollectionView: View {
    let id: UUID
    let onShow: (URL) -> Void
    @ObservedObject private var store = CollectionStore.shared
    @Environment(\.dismiss) private var dismiss
    @State private var refresh = 0
    private var collection: FileCollection? { store.collections.first { $0.id == id } }
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let collection {
                HStack {
                    TextField("Collection name", text: Binding(get: { collection.name }, set: { name in if let i = store.collections.firstIndex(where: { $0.id == id }) { store.collections[i].name = name } })).font(.title3.bold()).textFieldStyle(.plain)
                    Spacer()
                    Button("Add Files…") { addFiles() }
                    Button("Delete Collection", role: .destructive) { store.collections.removeAll { $0.id == id }; dismiss() }
                }
                Text("A collection keeps related files from different folders together. Files stay where they are.").font(.caption).foregroundStyle(.secondary)
                if collection.items.isEmpty { Text("Empty. Add files here, or right-click files in the browser and choose Add to Collection.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity) }
                else {
                    List(collection.items) { item in
                        let url = store.resolve(item, in: id)
                        HStack(spacing: 10) {
                            if let url { FileIcon(url: url, size: 24) } else { Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.orange).frame(width: 24) }
                            VStack(alignment: .leading, spacing: 2) {
                                Text(URL(fileURLWithPath: item.path).lastPathComponent).lineLimit(1)
                                Text(url?.deletingLastPathComponent().path ?? "Missing — last seen in " + URL(fileURLWithPath: item.path).deletingLastPathComponent().path).font(.caption2).foregroundStyle(url == nil ? .orange : .secondary).lineLimit(1).truncationMode(.head)
                            }
                            Spacer()
                            if let url {
                                Button("Open") { NSWorkspace.shared.open(url) }.controlSize(.small)
                                Button("Show") { dismiss(); onShow(url) }.controlSize(.small)
                            } else {
                                Button("Find…") { locate(item) }.controlSize(.small)
                            }
                            Button { store.remove(item, from: id) } label: { Image(systemName: "xmark.circle.fill") }.buttonStyle(.plain).foregroundStyle(.secondary)
                        }
                    }.id(refresh)
                }
            } else { Text("This collection no longer exists.") }
            HStack { Spacer(); Button("Done") { dismiss() }.keyboardShortcut(.cancelAction) }
        }.padding(18).frame(width: 660, height: 500)
    }
    private func addFiles() {
        let panel = NSOpenPanel(); panel.canChooseFiles = true; panel.canChooseDirectories = true; panel.allowsMultipleSelection = true
        if panel.runModal() == .OK { store.add(panel.urls, to: id) }
    }
    /// Tries the index first, then asks you to point to the file.
    private func locate(_ item: CollectionItem) {
        let name = URL(fileURLWithPath: item.path).lastPathComponent
        if let found = Recovery.candidates(named: name).first(where: { FileManager.default.fileExists(atPath: $0.path) }) { store.relink(item, to: found, in: id); refresh += 1; return }
        let panel = NSOpenPanel(); panel.message = "“\(name)” could not be found automatically. Choose where it is now."; panel.canChooseFiles = true; panel.canChooseDirectories = true
        if panel.runModal() == .OK, let url = panel.url { store.relink(item, to: url, in: id); refresh += 1 }
    }
}

// MARK: - Tags and note editor for the details pane

struct TagsAndNote: View {
    let url: URL
    @State private var tags: [String] = []
    @State private var newTag = ""
    @State private var note = ""
    @ObservedObject private var notes = NotesStore.shared
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("TAGS").font(.system(size: 9, weight: .medium)).tracking(0.8).foregroundStyle(.tertiary)
            if !tags.isEmpty {
                HStack(spacing: 4) {
                    ForEach(tags, id: \.self) { tag in
                        HStack(spacing: 3) { Text(tag).font(.system(size: 10)); Button { remove(tag) } label: { Image(systemName: "xmark").font(.system(size: 7)) }.buttonStyle(.plain) }
                            .padding(.horizontal, 7).padding(.vertical, 3).background(Color.accentColor.opacity(0.15), in: Capsule())
                    }
                }
            }
            TextField("Add tag", text: $newTag).textFieldStyle(.roundedBorder).font(.system(size: 11)).onSubmit { add() }
            Text("NOTE").font(.system(size: 9, weight: .medium)).tracking(0.8).foregroundStyle(.tertiary).padding(.top, 4)
            TextEditor(text: $note).font(.system(size: 11)).frame(height: 54).scrollContentBackground(.hidden).background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 5))
                .onChange(of: note) { notes.set($0, for: url) }
        }
        .task(id: url) { tags = AutomationEngine.tags(of: url); note = notes.note(for: url); newTag = "" }
    }
    private func add() {
        let tag = newTag.trimmingCharacters(in: .whitespaces)
        guard !tag.isEmpty else { return }
        try? AutomationEngine.addTag(tag, to: url); tags = AutomationEngine.tags(of: url); newTag = ""
    }
    private func remove(_ tag: String) {
        try? AutomationEngine.setTags(tags.filter { $0 != tag }, on: url); tags = AutomationEngine.tags(of: url)
    }
}
