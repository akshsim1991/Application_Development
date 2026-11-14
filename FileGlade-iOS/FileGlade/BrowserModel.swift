import Foundation
import SwiftUI

struct SavedLocation: Identifiable, Codable {
    let id: UUID
    let name: String
    var bookmark: Data
}
enum FileSort: String, CaseIterable { case name = "Name", modified = "Date modified", size = "Size", kind = "Kind" }

@MainActor final class BrowserModel: ObservableObject {
    @Published var directory: URL
    @Published var root: URL
    @Published var entries: [FileItem] = []
    @Published var selection: Set<URL> = []
    @Published var selecting = false
    @Published var query = ""
    @Published var icons = true
    @Published var sort: FileSort = .name
    @Published var ascending = true
    @Published var grouping: FileGrouping = .none
    @Published var hidden = false
    @Published var busy = false
    @Published var problem: String?
    @Published var locations: [SavedLocation] = []
    @Published var deleted: [DeletedItem] = []
    @Published var clipboard: [URL] = []
    @Published var moving = false
    @Published var filter: PowerFilter = .all
    @Published var tabs: [WorkspaceTab] = []
    @Published var activeTab: UUID?
    let local: URL
    let trash: URL
    private var granted: [UUID: URL] = [:]
    private var accessed: [URL] = []
    private var history: [URL] = []
    private var historyPosition = -1
    private var generation = UUID()

    init() {
        let fm = FileManager.default
        local = stableFileURL(fm.urls(for: .documentDirectory, in: .userDomainMask)[0])
        trash = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("RecentlyDeleted", isDirectory: true)
        directory = local; root = local
        try? fm.createDirectory(at: local, withIntermediateDirectories: true)
        try? fm.createDirectory(at: trash, withIntermediateDirectories: true)
        if let data = UserDefaults.standard.data(forKey: "FileGlade.locations"), let saved = try? JSONDecoder().decode([SavedLocation].self, from: data) { locations = saved }
        for location in locations {
            var stale = false
            if let url = try? URL(resolvingBookmarkData: location.bookmark, options: .withoutUI, relativeTo: nil, bookmarkDataIsStale: &stale) {
                let access = url.startAccessingSecurityScopedResource()
                guard access || fm.isReadableFile(atPath: url.path) else { continue }
                granted[location.id] = url; if access { accessed.append(url) }
                if stale, let data = try? url.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil), let index = locations.firstIndex(where: { $0.id == location.id }) { locations[index].bookmark = data }
            }
        }
        saveLocations()
        history = [local]; historyPosition = 0
        let tab = WorkspaceTab(url: local, root: local, history: history, index: 0)
        tabs = [tab]; activeTab = tab.id
        refreshDeleted()
    }
    deinit { for url in accessed { url.stopAccessingSecurityScopedResource() } }
    var title: String { directory.path == local.path ? "FileGlade files" : directory.lastPathComponent }
    var dashboardRoots: [DashboardRoot] {
        [DashboardRoot(id: "local", name: "FileGlade files", url: local)] + locations.compactMap { location in
            guard let url = granted[location.id] else { return nil }
            return DashboardRoot(id: location.id.uuidString, name: location.name, url: url)
        }
    }
    var unavailableDashboardSources: [String] { locations.filter { granted[$0.id] == nil }.map(\.name) }
    var chosen: [FileItem] { entries.filter { selection.contains($0.url) } }
    var canBack: Bool { historyPosition > 0 }
    var canForward: Bool { historyPosition + 1 < history.count }
    var canUp: Bool { directory.path != root.path }
    var visible: [FileItem] {
        entries.filter { (query.isEmpty || $0.name.localizedStandardContains(query)) && filter.accepts(PowerFile($0.url), starred: FileLibrary.shared.isStarred($0.url)) }.sorted { a, b in
            if a.isDirectory != b.isDirectory { return a.isDirectory }
            var comparison: ComparisonResult
            switch sort {
            case .name: comparison = a.name.localizedStandardCompare(b.name)
            case .modified: comparison = a.modified.compare(b.modified)
            case .size: comparison = a.size == b.size ? .orderedSame : a.size < b.size ? .orderedAscending : .orderedDescending
            case .kind: comparison = a.kind.localizedStandardCompare(b.kind)
            }
            if comparison == .orderedSame { comparison = a.name.localizedStandardCompare(b.name) }
            return ascending ? comparison == .orderedAscending : comparison == .orderedDescending
        }
    }
    var crumbs: [URL] {
        var crumbs = [directory]; var parent = directory
        while parent.path != root.path && parent.path != "/" {
            parent.deleteLastPathComponent()
            crumbs.insert(parent, at: 0)
        }
        return crumbs
    }
    func reload() async {
        let ticket = UUID(); generation = ticket
        let url = directory; let showHidden = hidden
        busy = true
        do {
            let result = try await Task.detached(priority: .userInitiated) { try FileOperations.list(url, hidden: showHidden) }.value
            guard generation == ticket else { return }
            entries = result
            selection.formIntersection(Set(result.map(\.url)))
        } catch {
            guard generation == ticket else { return }
            entries = []; selection = []
            problem = "Could not open this folder.\n\(error.localizedDescription)\nIf access has expired, choose the folder again from Locations."
        }
        if generation == ticket { busy = false }
    }
    func navigate(_ url: URL, record: Bool = true) {
        let target = stableFileURL(url)
        directory = target; selection = []; query = ""
        if record { history = Array(history.prefix(historyPosition + 1)); history.append(target); historyPosition = history.count - 1 }
        updateTab(); FileLibrary.shared.opened(target)
        Task { await reload() }
    }
    private func updateTab() {
        if let i = tabs.firstIndex(where: { $0.id == activeTab }) { tabs[i].url = directory; tabs[i].root = root; tabs[i].history = history; tabs[i].index = historyPosition }
    }
    func addTab() {
        guard tabs.count < 8, !busy else { return }
        updateTab()
        let tab = WorkspaceTab(url: directory, root: root, history: [directory], index: 0)
        tabs.append(tab); activateTab(tab.id)
    }
    func activateTab(_ id: UUID) {
        guard !busy, let tab = tabs.first(where: { $0.id == id }) else { return }
        updateTab(); activeTab = id; root = tab.root; history = tab.history; historyPosition = tab.index
        navigate(tab.url, record: false)
    }
    func closeTab(_ id: UUID) {
        guard tabs.count > 1, !busy, let i = tabs.firstIndex(where: { $0.id == id }) else { return }
        let active = activeTab == id; tabs.remove(at: i)
        if active { activeTab = nil; activateTab(tabs[min(i, tabs.count - 1)].id) }
    }
    func revealItem(_ url: URL) {
        let url = stableFileURL(url)
        let roots = ([local] + Array(granted.values).map(stableFileURL)).filter { url.path == $0.path || url.path.hasPrefix($0.path + "/") }.sorted { $0.path.count > $1.path.count }
        root = roots.first ?? (PowerFile(url).directory ? url : url.deletingLastPathComponent())
        filter = .all
        navigate(PowerFile(url).directory ? url : url.deletingLastPathComponent())
        if !PowerFile(url).directory { Task { await reload(); selection = [url] } }
    }
    func toolsProduced(_ urls: [URL]) { Task { await reload(); selection = Set(urls) } }
    func back() { guard canBack else { return }; historyPosition -= 1; navigate(history[historyPosition], record: false) }
    func forward() { guard canForward else { return }; historyPosition += 1; navigate(history[historyPosition], record: false) }
    func up() { if canUp { navigate(directory.deletingLastPathComponent()) } }
    func openLocal() { root = local; history = []; historyPosition = -1; navigate(local) }
    func openLocation(_ location: SavedLocation) {
        guard let url = granted[location.id] else { problem = "Access to this folder has expired. Choose it again using Browse other folders."; return }
        root = stableFileURL(url); history = []; historyPosition = -1; navigate(root)
    }
    func addLocation(_ url: URL) {
        let access = url.startAccessingSecurityScopedResource()
        do {
            let bookmark = try url.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil)
            let location = SavedLocation(id: UUID(), name: url.lastPathComponent, bookmark: bookmark)
            if let i = locations.firstIndex(where: { location in
                if granted[location.id]?.path == url.path { return true }
                var stale = false
                return (try? URL(resolvingBookmarkData: location.bookmark, options: .withoutUI, relativeTo: nil, bookmarkDataIsStale: &stale)).map { stableFileURL($0).path == stableFileURL(url).path } ?? false
            }) {
                let old = locations[i]
                if granted[old.id] != nil { if access { url.stopAccessingSecurityScopedResource() } }
                else { granted[old.id] = url; if access { accessed.append(url) } }
                locations[i].bookmark = bookmark; saveLocations(); return
            }
            if access { accessed.append(url) }
            locations.append(location); granted[location.id] = url; saveLocations()
        } catch { if access { url.stopAccessingSecurityScopedResource() }; problem = error.localizedDescription }
    }
    func moveLocations(from source: IndexSet, to destination: Int) { locations.move(fromOffsets: source, toOffset: destination); saveLocations() }
    private func saveLocations() { if let data = try? JSONEncoder().encode(locations) { UserDefaults.standard.set(data, forKey: "FileGlade.locations") } }
    func removeDashboardLocation(_ location: SavedLocation) {
        locations.removeAll { $0.id == location.id }
        if let url = granted.removeValue(forKey: location.id), let i = accessed.firstIndex(of: url) { url.stopAccessingSecurityScopedResource(); accessed.remove(at: i) }
        saveLocations()
    }
    func toggleSelection(_ item: FileItem) { if selection.contains(item.url) { selection.remove(item.url) } else { selection.insert(item.url) } }
    func copy(_ items: [FileItem], cut: Bool) { clipboard = items.map(\.url); moving = cut; selecting = false; selection = [] }
    func newFolder(_ name: String) {
        let folder = directory
        Task { await run { [try FileOperations.newFolder(name, in: folder)] } }
    }
    func rename(_ item: FileItem, to name: String) {
        Task { await run({ [try FileOperations.rename(item.url, to: name)] }, completion: { urls in
            if let target = urls.first { FileLibrary.shared.applyRename([RenamePair(source: item.url, destination: target)]) }
        }) }
    }
    private func run(_ operation: @escaping @Sendable () throws -> [URL], completion: ([URL]) -> Void = { _ in }) async {
        guard !busy else { return }; busy = true
        do { let selected = try await Task.detached(priority: .userInitiated, operation: operation).value; await reload(); selection = Set(selected); completion(selected); FileLibrary.shared.log("Create or rename", detail: selected.map(\.lastPathComponent).joined(separator: ", ")) }
        catch { busy = false; problem = error.localizedDescription }
    }
    func paste() async {
        guard !busy, !clipboard.isEmpty else { return }
        busy = true
        let sources = clipboard; let destination = directory; let move = moving
        let result = await Task.detached(priority: .userInitiated) {
            var created: [URL] = []; var failed: [URL] = []; var messages: [String] = []; var pairs: [RenamePair] = []
            for url in sources {
                do { let target = try FileOperations.transfer(url, into: destination, moving: move); created.append(target); if move { pairs.append(RenamePair(source: url, destination: target)) } }
                catch { failed.append(url); messages.append("\(url.lastPathComponent): \(error.localizedDescription)") }
            }
            return (created, failed, messages, pairs)
        }.value
        if move { clipboard = result.1; moving = !result.1.isEmpty }
        FileLibrary.shared.applyRename(result.3)
        FileLibrary.shared.log(move ? "Move files" : "Copy files", detail: "\(result.0.count) completed · \(result.1.count) failed")
        if directory.path == destination.path { await reload(); selection = Set(result.0) } else { busy = false }
        if !result.2.isEmpty { problem = result.2.joined(separator: "\n\n") }
    }
    func importFiles(_ urls: [URL]) async {
        guard !busy else { return }; busy = true
        let destination = directory
        let result = await Task.detached(priority: .userInitiated) {
            var messages: [String] = []
            for url in urls {
                let access = url.startAccessingSecurityScopedResource()
                defer { if access { url.stopAccessingSecurityScopedResource() } }
                do { _ = try FileOperations.transfer(url, into: destination, moving: false) }
                catch { messages.append("\(url.lastPathComponent): \(error.localizedDescription)") }
            }
            return messages
        }.value
        await reload()
        FileLibrary.shared.log("Import files", detail: "\(urls.count - result.count) imported · \(result.count) failed")
        if !result.isEmpty { problem = result.joined(separator: "\n\n") }
    }
    func recycle(_ items: [FileItem]) async {
        guard !busy else { return }; busy = true
        let trash = trash
        let messages = await Task.detached(priority: .userInitiated) {
            var messages: [String] = []
            for item in items { do { try FileOperations.recycle(item.url, trash: trash) } catch { messages.append("\(item.name): \(error.localizedDescription)") } }
            return messages
        }.value
        selecting = false; selection = []; refreshDeleted(); await reload()
        FileLibrary.shared.log("Recently Deleted", detail: "\(items.count - messages.count) items moved")
        if !messages.isEmpty { problem = messages.joined(separator: "\n\n") }
    }
    func refreshDeleted() { deleted = FileOperations.deletedItems(in: trash) }
    func restore(_ item: DeletedItem) async {
        guard !busy else { return }; busy = true
        let trash = trash
        do { try await Task.detached(priority: .userInitiated) { try FileOperations.restore(item, trash: trash) }.value; FileLibrary.shared.log("Restore", detail: item.name) }
        catch { problem = error.localizedDescription }
        refreshDeleted(); await reload()
    }
}
