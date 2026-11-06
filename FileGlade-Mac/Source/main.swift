import AppKit
import SwiftUI
import UniformTypeIdentifiers

func canonicalURL(_ url: URL) -> URL {
    if let resolved = realpath(url.path, nil) {
        defer { free(resolved) }
        return URL(fileURLWithPath: String(cString: resolved), isDirectory: false)
    }
    return URL(fileURLWithPath: url.path, isDirectory: false)
}

// FileGlade — a native, local-only file explorer for macOS.
struct FileEntry: Identifiable, Hashable {
    let url: URL
    let isDirectory: Bool
    let isPackage: Bool
    let size: Int64
    let modified: Date
    let kind: String
    var id: URL { url }
    var name: String { url.lastPathComponent }
    init(_ url: URL) {
        // Stable identity: directory URLs returned by FileManager carry a trailing slash.
        self.url = URL(fileURLWithPath: url.path, isDirectory: false)
        // One resource lookup (the directory listing prefetches these), not several file-system calls per item.
        let v = try? url.resourceValues(forKeys: [.isDirectoryKey, .isPackageKey, .fileSizeKey, .contentModificationDateKey, .localizedTypeDescriptionKey])
        isDirectory = v?.isDirectory ?? false
        isPackage = v?.isPackage ?? ["app", "bundle", "framework"].contains(url.pathExtension.lowercased())
        size = Int64(v?.fileSize ?? 0)
        modified = v?.contentModificationDate ?? .distantPast
        kind = v?.localizedTypeDescription ?? (isDirectory ? "Folder" : "File")
    }
}

enum Prefs {
    static let selfTest = CommandLine.arguments.contains("--self-test")
    static func set(_ value: Any, _ key: String) { if !selfTest { UserDefaults.standard.set(value, forKey: key) } }
}

enum Order: String, CaseIterable { case name = "Name", modified = "Date modified", kind = "Kind", size = "Size" }
enum Layout: String { case icons, details }

final class Explorer: ObservableObject {
    @Published var directory = FileManager.default.homeDirectoryForCurrentUser
    @Published var entries: [FileEntry] = [] { didSet { recomputeVisible() } }
    @Published private(set) var visible: [FileEntry] = []
    @Published var loading = false
    let isSecondary: Bool
    private var refreshGeneration = 0
    private(set) var dashboardRoots: [DashboardRoot] = []
    @Published var selection: Set<URL> = []
    @Published var query = "" { didSet { recomputeVisible() } }
    @Published var showHidden = UserDefaults.standard.bool(forKey: "showHidden")
    @Published var order: Order = Order(rawValue: UserDefaults.standard.string(forKey: "order") ?? "") ?? .name { didSet { Prefs.set(order.rawValue, "order"); recomputeVisible() } }
    @Published var ascending = UserDefaults.standard.object(forKey: "ascending") as? Bool ?? true { didSet { Prefs.set(ascending, "ascending"); recomputeVisible() } }
    @Published var grouping: FileGrouping = FileGrouping(rawValue: UserDefaults.standard.string(forKey: "grouping") ?? "") ?? .none { didSet { Prefs.set(grouping.rawValue, "grouping") } }
    @Published var layout: Layout = UserDefaults.standard.string(forKey: "layout") == "details" ? .details : .icons
    @Published var inspector = UserDefaults.standard.object(forKey: "inspector") as? Bool ?? true { didSet { Prefs.set(inspector, "inspector") } }
    @Published var failure: String?
    @Published var favorites: [URL] = []
    @Published var volumes: [URL] = []
    @Published var filter: PowerFilter = PowerFilter(rawValue: UserDefaults.standard.string(forKey: "filter") ?? "") ?? .all { didSet { Prefs.set(filter.rawValue, "filter"); recomputeVisible() } }
    @Published var tabs: [WorkspaceTab] = []
    @Published var activeTab: UUID?
    @Published var showDashboard = true { didSet { if !isSecondary { Prefs.set(showDashboard, "showDashboard") } } }
    private var history: [URL] = []
    private var historyIndex = -1
    private var watcher: DispatchSourceFileSystemObject?
    private var refreshWork: DispatchWorkItem?
    private var cutURLs: [URL] = []
    private var cutChangeCount: Int?
    private var workspaceObservers: [NSObjectProtocol] = []

    init(secondary: Bool = false) {
        isSecondary = secondary
        favorites = (UserDefaults.standard.stringArray(forKey: "favorites") ?? []).map { URL(fileURLWithPath: $0) }
        updateVolumes()
        for name in [NSWorkspace.didMountNotification, NSWorkspace.didUnmountNotification] {
            workspaceObservers.append(NSWorkspace.shared.notificationCenter.addObserver(forName: name, object: nil, queue: .main) { [weak self] _ in self?.updateVolumes() })
        }
        // Access to protected folders can change while the app runs, so re-check it when the app comes forward.
        workspaceObservers.append(NotificationCenter.default.addObserver(forName: NSApplication.didBecomeActiveNotification, object: nil, queue: .main) { [weak self] _ in self?.updateDashboardRoots() })
        if secondary {
            let start = (UserDefaults.standard.string(forKey: "secondPanePath")).map { URL(fileURLWithPath: $0) } ?? directory
            let tab = WorkspaceTab(url: start, root: URL(fileURLWithPath: "/"))
            tabs = [tab]; activeTab = tab.id
            navigate(FileManager.default.fileExists(atPath: start.path) ? start : directory)
            showDashboard = false
            return
        }
        let startOnDashboard = UserDefaults.standard.object(forKey: "showDashboard") as? Bool ?? true
        let saved = Prefs.selfTest ? [] : (UserDefaults.standard.stringArray(forKey: "openTabs") ?? []).map { URL(fileURLWithPath: $0) }.filter { FileManager.default.fileExists(atPath: $0.path) }
        if saved.isEmpty {
            let tab = WorkspaceTab(url: directory, root: URL(fileURLWithPath: "/"))
            tabs = [tab]; activeTab = tab.id
            navigate(directory)
            showDashboard = true
        } else {
            tabs = saved.prefix(8).map { WorkspaceTab(url: $0, root: URL(fileURLWithPath: "/"), history: [$0], index: 0) }
            let chosen = min(max(UserDefaults.standard.integer(forKey: "activeTabIndex"), 0), tabs.count - 1)
            activateTab(tabs[chosen].id)
            showDashboard = startOnDashboard
        }
    }
    deinit { watcher?.cancel(); workspaceObservers.forEach { NSWorkspace.shared.notificationCenter.removeObserver($0) } }
    var canBack: Bool { historyIndex > 0 }
    var canForward: Bool { historyIndex < history.count - 1 }
    var chosen: [FileEntry] { entries.filter { selection.contains($0.url) } }
    var focused: FileEntry? { chosen.count == 1 ? chosen.first : nil }
    /// Filtering and sorting happen once per change, not on every screen update.
    private func recomputeVisible() {
        let useFilter = filter != .all
        let needle = query
        let filtered = entries.filter { entry in
            (needle.isEmpty || entry.name.localizedStandardContains(needle)) && (!useFilter || filter.accepts(PowerFile(entry.url), starred: FileLibrary.shared.isStarred(entry.url)))
        }
        let order = self.order, ascending = self.ascending
        visible = filtered.sorted { a, b in
            if a.isDirectory != b.isDirectory { return a.isDirectory }
            var result: ComparisonResult
            switch order {
            case .name: result = a.name.localizedStandardCompare(b.name)
            case .modified: result = a.modified.compare(b.modified)
            case .kind: result = a.kind.localizedStandardCompare(b.kind)
            case .size: result = a.size == b.size ? .orderedSame : a.size < b.size ? .orderedAscending : .orderedDescending
            }
            if result == .orderedSame { result = a.name.localizedStandardCompare(b.name) }
            return ascending ? result == .orderedAscending : result == .orderedDescending
        }
    }
    var breadcrumbs: [URL] {
        var result = [directory]
        var current = directory
        while current.path != "/" {
            current.deleteLastPathComponent()
            result.insert(current, at: 0)
        }
        return result
    }
    func navigate(_ url: URL, record: Bool = true) {
        let target = canonicalURL(url)
        var isDir: ObjCBool = false
        guard FileManager.default.fileExists(atPath: target.path, isDirectory: &isDir), isDir.boolValue else { failure = "This folder is unavailable: \(target.path)"; return }
        directory = target
        showDashboard = false
        query = ""
        selection = []
        if isSecondary { UserDefaults.standard.set(target.path, forKey: "secondPanePath") }
        if record {
            history = Array(history.prefix(historyIndex + 1))
            history.append(target)
            historyIndex = history.count - 1
        }
        refresh()
        watch()
        updateTab()
        if !CommandLine.arguments.contains("--self-test") { FileLibrary.shared.opened(target) }
    }
    private func updateTab() {
        if let i = tabs.firstIndex(where: { $0.id == activeTab }) { tabs[i].url = directory; tabs[i].history = history; tabs[i].index = historyIndex }
        saveTabs()
    }
    private func saveTabs() {
        guard !Prefs.selfTest, !isSecondary else { return }
        UserDefaults.standard.set(tabs.map(\.url.path), forKey: "openTabs")
        UserDefaults.standard.set(tabs.firstIndex { $0.id == activeTab } ?? 0, forKey: "activeTabIndex")
    }
    func addTab() {
        guard tabs.count < 8 else { failure = "You can keep up to eight workspace tabs open."; return }
        updateTab()
        let tab = WorkspaceTab(url: directory, root: URL(fileURLWithPath: "/"), history: [directory], index: 0)
        tabs.append(tab); activateTab(tab.id)
    }
    func activateTab(_ id: UUID) {
        guard let tab = tabs.first(where: { $0.id == id }) else { return }
        updateTab(); activeTab = id; history = tab.history; historyIndex = tab.index
        navigate(tab.url, record: false)
    }
    func closeTab(_ id: UUID) {
        guard tabs.count > 1, let i = tabs.firstIndex(where: { $0.id == id }) else { return }
        let wasActive = activeTab == id
        tabs.remove(at: i)
        if wasActive { activeTab = nil; activateTab(tabs[min(i, tabs.count - 1)].id) } else { saveTabs() }
    }
    func back() { guard canBack else { return }; historyIndex -= 1; navigate(history[historyIndex], record: false) }
    func forward() { guard canForward else { return }; historyIndex += 1; navigate(history[historyIndex], record: false) }
    func up() { if directory.path != "/" { navigate(directory.deletingLastPathComponent()) } }
    /// Reads the folder off the main thread so large folders never freeze the window.
    func refresh() {
        let folder = directory, hidden = showHidden
        refreshGeneration += 1
        let generation = refreshGeneration
        let keys: [URLResourceKey] = [.isDirectoryKey, .isPackageKey, .fileSizeKey, .contentModificationDateKey, .localizedTypeDescriptionKey]
        let load: () -> Result<[FileEntry], Error> = {
            Result { try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: keys, options: hidden ? [] : [.skipsHiddenFiles]).map(FileEntry.init) }
        }
        let apply: (Result<[FileEntry], Error>) -> Void = { [weak self] result in
            guard let self, generation == self.refreshGeneration else { return }
            self.loading = false
            switch result {
            case .success(let list): self.entries = list; self.selection.formIntersection(Set(list.map(\.url)))
            case .failure(let error): self.entries = []; self.failure = "Could not read \(folder.path).\n\n\(error.localizedDescription)\n\nYou may need to allow access in System Settings → Privacy & Security → Files and Folders."
            }
        }
        if Prefs.selfTest { apply(load()); return }
        if entries.isEmpty { loading = true }
        DispatchQueue.global(qos: .userInitiated).async { let result = load(); DispatchQueue.main.async { apply(result) } }
    }
    private func watch() {
        watcher?.cancel()
        let fd = open(directory.path, O_EVTONLY)
        guard fd >= 0 else { return }
        let source = DispatchSource.makeFileSystemObjectSource(fileDescriptor: fd, eventMask: [.write, .rename, .delete, .extend, .attrib], queue: .main)
        source.setEventHandler { [weak self] in
            guard let self else { return }
            self.refreshWork?.cancel()
            let work = DispatchWorkItem { [weak self] in self?.refresh() }
            self.refreshWork = work
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.2, execute: work)
        }
        source.setCancelHandler { close(fd) }
        watcher = source
        source.resume()
    }
    func updateVolumes() { volumes = FileManager.default.mountedVolumeURLs(includingResourceValuesForKeys: nil, options: [.skipHiddenVolumes]) ?? []; updateDashboardRoots() }
    /// Cached so the screen does not touch the file system every time it redraws.
    func updateDashboardRoots() { dashboardRoots = automaticMacDashboardRoots(volumes: volumes); objectWillChange.send() }
    func select(_ entry: FileEntry) {
        if NSEvent.modifierFlags.contains(.command) {
            if selection.contains(entry.url) { selection.remove(entry.url) } else { selection.insert(entry.url) }
        } else if NSEvent.modifierFlags.contains(.shift), let first = visible.firstIndex(where: { selection.contains($0.url) }), let last = visible.firstIndex(of: entry) {
            selection.formUnion(visible[min(first,last)...max(first,last)].map(\.url))
        } else { selection = [entry.url] }
    }
    func openEntry(_ entry: FileEntry) {
        FileLibrary.shared.opened(entry.url)
        if entry.isDirectory && !entry.isPackage { navigate(entry.url) }
        else if !NSWorkspace.shared.open(entry.url) { failure = "No application could open \(entry.name)." }
    }
    func openSelection() { if let first = chosen.first { openEntry(first) } }
    func reveal() { NSWorkspace.shared.activateFileViewerSelecting(chosen.isEmpty ? [directory] : chosen.map(\.url)) }
    func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.allowsMultipleSelection = false
        panel.directoryURL = directory; panel.prompt = "Open folder"
        if panel.runModal() == .OK, let url = panel.url { navigate(url) }
    }
    func goToFolder() {
        if let path = prompt(title: "Go to folder", message: "Enter a folder path. You can use ~ for your home folder.", value: directory.path, action: "Go") {
            navigate(URL(fileURLWithPath: (path as NSString).expandingTildeInPath))
        }
    }
    func toggleFavorite() {
        if favorites.contains(directory) { favorites.removeAll { $0 == directory } } else { favorites.append(directory) }
        saveFavorites()
    }
    func replaceFavorite(_ old: URL, with new: URL) { if let i = favorites.firstIndex(of: old) { favorites[i] = new; saveFavorites() } }
    func removeFavorite(_ url: URL) { favorites.removeAll { $0 == url }; saveFavorites() }
    private func saveFavorites() { UserDefaults.standard.set(favorites.map(\.path), forKey: "favorites") }
    func toggleHidden() { showHidden.toggle(); UserDefaults.standard.set(showHidden, forKey: "showHidden"); refresh() }
    func setLayout(_ value: Layout) { layout = value; UserDefaults.standard.set(value.rawValue, forKey: "layout") }
    func newFolder() {
        guard let name = prompt(title: "New folder", message: "Create a folder in \(directory.lastPathComponent).", value: "New folder", action: "Create"), validName(name) else { return }
        do { let target = directory.appendingPathComponent(name); try FileManager.default.createDirectory(at: target, withIntermediateDirectories: false); refresh(); selection = [target]; FileLibrary.shared.log("New folder", detail: name) }
        catch { failure = error.localizedDescription }
    }
    func rename() {
        guard let entry = focused, let name = prompt(title: "Rename", message: "Enter a new name, including the extension for files.", value: entry.name, action: "Rename"), validName(name), name != entry.name else { return }
        let target = directory.appendingPathComponent(name)
        do { try FileManager.default.moveItem(at: entry.url, to: target); FileLibrary.shared.applyRename([RenamePair(source: entry.url, destination: target)]); TransferManager.shared.record(title: "Rename", steps: [.moved(from: entry.url, to: target)]); refresh(); selection = [target]; FileLibrary.shared.log("Rename", detail: "\(entry.name) → \(name)") }
        catch { failure = error.localizedDescription }
    }
    private func validName(_ name: String) -> Bool {
        guard !name.isEmpty, name != ".", name != "..", !name.contains("/"), !name.contains(":"), !name.contains("\0") else { failure = "Choose a name without / or : characters."; return false }
        return true
    }
    func copy(cut: Bool = false) {
        let urls = chosen.map(\.url)
        guard !urls.isEmpty else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.writeObjects(urls as [NSURL])
        cutURLs = cut ? urls : []
        cutChangeCount = cut ? NSPasteboard.general.changeCount : nil
        objectWillChange.send()
    }
    var canPaste: Bool { NSPasteboard.general.canReadObject(forClasses: [NSURL.self], options: [.urlReadingFileURLsOnly: true]) }
    func isCut(_ url: URL) -> Bool { cutChangeCount == NSPasteboard.general.changeCount && cutURLs.contains(url) }
    func paste() {
        let urls = (NSPasteboard.general.readObjects(forClasses: [NSURL.self], options: [.urlReadingFileURLsOnly: true]) as? [URL]) ?? []
        guard !urls.isEmpty else { return }
        let moving = cutChangeCount == NSPasteboard.general.changeCount && !cutURLs.isEmpty
        transfer(urls, into: directory, kind: moving ? .move : .copy, fromCut: moving)
    }
    /// Copies or moves items into a folder through the transfer manager (progress, conflicts, undo).
    func transfer(_ urls: [URL], into folder: URL, kind: TransferKind, fromCut: Bool = false) {
        TransferManager.shared.start(kind: kind, sources: urls, into: folder) { [weak self] job in
            guard let self else { return }
            if fromCut {
                let movedSources = Set(job.moved.map { $0.source.standardizedFileURL })
                let remaining = urls.filter { !movedSources.contains($0.standardizedFileURL) && $0.deletingLastPathComponent().standardizedFileURL != folder.standardizedFileURL }
                self.cutURLs = remaining
                NSPasteboard.general.clearContents()
                if !remaining.isEmpty { NSPasteboard.general.writeObjects(remaining as [NSURL]) }
                self.cutChangeCount = remaining.isEmpty ? nil : NSPasteboard.general.changeCount
            }
            if self.directory.standardizedFileURL == folder.standardizedFileURL { self.refresh(); self.selection = Set(job.completed) }
            FileLibrary.shared.applyRename(job.moved)
            if !Prefs.selfTest { FileLibrary.shared.log(kind == .move ? "Move files" : "Copy files", detail: "\(job.completed.count) completed · \(job.failures.count) failed") }
            if !job.earlyErrors.isEmpty { self.failure = job.earlyErrors.joined(separator: "\n\n") }
        }
    }
    /// Handles files dropped from Finder or inside the app. Moves on the same disk, copies otherwise (hold ⌥ to copy).
    func handleDrop(_ providers: [NSItemProvider], into folder: URL) -> Bool {
        let optionHeld = NSEvent.modifierFlags.contains(.option)
        let group = DispatchGroup()
        let lock = NSLock()
        var urls: [URL] = []
        for provider in providers where provider.hasItemConformingToTypeIdentifier(UTType.fileURL.identifier) {
            group.enter()
            provider.loadItem(forTypeIdentifier: UTType.fileURL.identifier, options: nil) { item, _ in
                var url: URL?
                if let data = item as? Data { url = URL(dataRepresentation: data, relativeTo: nil) } else if let value = item as? URL { url = value }
                if let url { lock.lock(); urls.append(url); lock.unlock() }
                group.leave()
            }
        }
        group.notify(queue: .main) { [weak self] in
            guard let self, !urls.isEmpty else { return }
            let volume = { (u: URL) in (try? u.resourceValues(forKeys: [.volumeURLKey]).volume)?.path }
            let sameDisk = urls.allSatisfy { volume($0) == volume(folder) }
            self.transfer(urls, into: folder, kind: (optionHeld || !sameDisk) ? .copy : .move)
        }
        return true
    }
    func undoFileOperation() {
        let problems = TransferManager.shared.undoLast()
        refresh()
        if !problems.isEmpty { failure = "Some items could not be undone:\n\n" + problems }
    }
    func trash() {
        let selected = chosen
        guard !selected.isEmpty else { return }
        let alert = NSAlert()
        alert.messageText = selected.count == 1 ? "Move “\(selected[0].name)” to Trash?" : "Move \(selected.count) items to Trash?"
        alert.informativeText = "You can undo this or restore the items from the Trash in Finder."
        alert.alertStyle = .warning; alert.addButton(withTitle: "Move to Trash"); alert.addButton(withTitle: "Cancel")
        guard alert.runModal() == .alertFirstButtonReturn else { return }
        var errors: [String] = []; var steps: [UndoRecord.Step] = []
        for entry in selected {
            do { var result: NSURL?; try FileManager.default.trashItem(at: entry.url, resultingItemURL: &result); if let result { steps.append(.trashed(original: entry.url, trash: result as URL)) } }
            catch { errors.append("\(entry.name): \(error.localizedDescription)") }
        }
        TransferManager.shared.record(title: "Move to Trash", steps: steps)
        refresh()
        FileLibrary.shared.log("Move to Trash", detail: "\(selected.count - errors.count) items moved · \(errors.count) failed")
        if !errors.isEmpty { failure = errors.joined(separator: "\n\n") }
    }
    /// Deletes items immediately, bypassing the Trash. This cannot be undone.
    func deletePermanently() {
        let selected = chosen
        guard !selected.isEmpty else { return }
        let alert = NSAlert()
        alert.messageText = selected.count == 1 ? "Permanently delete “\(selected[0].name)”?" : "Permanently delete \(selected.count) items?"
        alert.informativeText = "This skips the Trash and cannot be undone. The \(selected.count == 1 ? "item" : "items") will be erased from disk."
        alert.alertStyle = .critical
        let confirm = alert.addButton(withTitle: "Delete Permanently"); confirm.hasDestructiveAction = true
        alert.addButton(withTitle: "Cancel")
        guard alert.runModal() == .alertFirstButtonReturn else { return }
        var errors: [String] = []
        for entry in selected { do { try FileManager.default.removeItem(at: entry.url) } catch { errors.append("\(entry.name): \(error.localizedDescription)") } }
        refresh()
        FileLibrary.shared.log("Delete permanently", detail: "\(selected.count - errors.count) items deleted · \(errors.count) failed")
        if !errors.isEmpty { failure = errors.joined(separator: "\n\n") }
    }
    func prompt(title: String, message: String, value: String, action: String) -> String? {
        let alert = NSAlert(); alert.messageText = title; alert.informativeText = message
        let field = NSTextField(string: value); field.frame = NSRect(x: 0, y: 0, width: 360, height: 24)
        alert.accessoryView = field; alert.addButton(withTitle: action); alert.addButton(withTitle: "Cancel")
        alert.window.initialFirstResponder = field
        return alert.runModal() == .alertFirstButtonReturn ? field.stringValue : nil
    }
}

let accent = Color.accentColor

struct FileIcon: View {
    let url: URL
    var size: CGFloat = 48
    var thumbnail = false
    var directory: Bool? = nil
    var body: some View {
        if thumbnail && ["images", "video"].contains(fileCategory(url)) || thumbnail && url.pathExtension.lowercased() == "pdf" {
            ThumbnailIcon(url: url, size: size)
        } else {
            plain
        }
    }
    private var plain: some View {
        Image(nsImage: IconCache.icon(for: url, directory: directory)).resizable().interpolation(.high).aspectRatio(contentMode: .fit).frame(width: size, height: size)
    }
}

enum ExtraSheet: Identifiable {
    case compare, archive(URL), docs, collection(UUID)
    var id: String { switch self { case .compare: return "compare"; case .archive(let url): return "archive" + url.path; case .docs: return "docs"; case .collection(let id): return "collection" + id.uuidString } }
}

struct ExplorerView: View {
    @ObservedObject var model: Explorer
    var embedded = false
    @ObservedObject private var panes = PaneState.shared
    @ObservedObject private var basket = BasketStore.shared
    @ObservedObject private var collectionStore = CollectionStore.shared
    @State private var extra: ExtraSheet?
    @State private var showBasket = false
    @ObservedObject private var library = FileLibrary.shared
    @ObservedObject private var smartStore = SavedSearchStore.shared
    @ObservedObject private var transfers = TransferManager.shared
    @State private var toolContext: PowerContext?
    @State private var showSwitcher = false
    @State private var pathEditing = false
    @State private var path = ""
    @FocusState private var searchFocused: Bool
    @FocusState private var pathFocused: Bool
    private var home: URL { FileManager.default.homeDirectoryForCurrentUser }
    /// The pane that toolbar buttons and the sidebar act on.
    private var acting: Explorer { (panes.dual && !model.showDashboard && !embedded) ? (panes.active ?? model) : (embedded ? model : model) }
    private func bind<T>(_ keyPath: ReferenceWritableKeyPath<Explorer, T>) -> Binding<T> { Binding(get: { acting[keyPath: keyPath] }, set: { acting[keyPath: keyPath] = $0 }) }
    private var isActivePane: Bool { panes.dual && (panes.active ?? (embedded ? nil : model)) === model }
    @ViewBuilder var body: some View {
        if embedded {
            paneColumn
                .alert("Something needs your attention", isPresented: Binding(get: { model.failure != nil }, set: { if !$0 { model.failure = nil } })) { Button("OK") { model.failure = nil } } message: { Text(model.failure ?? "") }
                .onReceive(NotificationCenter.default.publisher(for: .init("FileGladeTransferDone"))) { _ in model.refresh() }
        } else { fullBody }
    }
    private var paneColumn: some View {
        VStack(spacing: 0) {
            locationbar
            Divider()
            content.onDrop(of: [.fileURL], isTargeted: nil) { model.handleDrop($0, into: model.directory) }
            if !embedded { TransferBar() }
            Divider()
            statusbar
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .overlay(alignment: .top) { if isActivePane { Rectangle().fill(accent).frame(height: 2) } }
        .simultaneousGesture(TapGesture().onEnded { panes.active = model })
    }
    private var fullBody: some View {
        VStack(spacing: 0) {
            UpdateBanner()
            if !model.showDashboard { if model.tabs.count > 1 { workspaceBar; Divider() }; commandbar; Divider() }
            HStack(spacing: 0) {
                sidebar.frame(width: 208)
                Divider()
                if model.showDashboard {
                    UnifiedDashboardView(roots: model.dashboardRoots, onOpen: { model.openEntry(FileEntry($0)) }, onReveal: { url in model.filter = .all; model.navigate(PowerFile(url).directory ? url : url.deletingLastPathComponent()); if !PowerFile(url).directory { model.selection = [powerURL(url)] } })
                } else {
                paneColumn
                if panes.dual { Divider(); ExplorerView(model: panes.second, embedded: true) }
                if model.inspector { Divider(); inspector.frame(width: 280) }
                }
            }
        }
        .frame(minWidth: 850, minHeight: 540)
        .background(Color(nsColor: .windowBackgroundColor))
        .tint(accent)
        .sheet(item: $toolContext, onDismiss: { model.refresh() }) { context in
            PowerToolsView(folder: context.folder, selection: context.selection, onProduced: { urls in model.refresh(); model.selection = Set(urls) }, onReveal: { url in model.filter = .all; model.navigate(PowerFile(url).directory ? url : url.deletingLastPathComponent()); if !PowerFile(url).directory { model.selection = [powerURL(url)] } })
        }
        .sheet(isPresented: $showSwitcher) { QuickSwitcher(model: acting, tools: { openTools() }) }
        .sheet(item: $extra) { item in
            switch item {
            case .compare: CompareView(left: model.directory, right: panes.second.directory) { model.refresh(); panes.second.refresh() }
            case .archive(let url): ArchiveBrowserView(archive: url) { out in acting.refresh(); acting.selection = [out] }
            case .collection(let id): CollectionView(id: id) { url in acting.filter = .all; acting.navigate(url.deletingLastPathComponent()); acting.selection = [powerURL(url)] }
            case .docs: DocSearchView(folder: acting.directory) { url in acting.filter = .all; acting.navigate(url.deletingLastPathComponent()); acting.selection = [powerURL(url)] }
            }
        }
        .onReceive(NotificationCenter.default.publisher(for: .init("FileGladeExtra"))) { note in
            switch note.object as? String {
            case "compare": if !panes.dual { panes.toggle() }; model.showDashboard = false; extra = .compare
            case "docs": extra = .docs
            case "archive": if let url = note.userInfo?["url"] as? URL { extra = .archive(url) }
            case "basket": showBasket.toggle()
            case "collection": if let id = note.userInfo?["id"] as? UUID { extra = .collection(id) }
            default: break
            }
        }
        .onReceive(NotificationCenter.default.publisher(for: .init("FileGladeSwitcher"))) { _ in showSwitcher = true }
        .alert("Something needs your attention", isPresented: Binding(get: { model.failure != nil }, set: { if !$0 { model.failure = nil } })) {
            Button("OK") { model.failure = nil }
        } message: { Text(model.failure ?? "") }
        .onReceive(NotificationCenter.default.publisher(for: .init("FileGladeTransferDone"))) { _ in model.refresh() }
        .onReceive(NotificationCenter.default.publisher(for: .init("FileGladeSearch"))) { _ in searchFocused = true }
        .onReceive(NotificationCenter.default.publisher(for: .init("FileGladePath"))) { _ in beginPath() }
        .onReceive(NotificationCenter.default.publisher(for: .init("FileGladeTools"))) { _ in openTools() }
    }
    private func openTools() { toolContext = PowerContext(folder: model.directory, selection: model.chosen.map(\.url).sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }) }
    private var workspaceBar: some View {
        HStack(spacing: 5) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 5) {
                    ForEach(model.tabs) { tab in
                        HStack(spacing: 10) {
                            Button { model.activateTab(tab.id) } label: { Label(tab.title, systemImage: "folder").font(.system(size: 11)).lineLimit(1).frame(maxWidth: 160) }.buttonStyle(.plain)
                                .onDrop(of: [.fileURL], isTargeted: nil) { model.handleDrop($0, into: tab.url) }
                            if model.tabs.count > 1 { Button { model.closeTab(tab.id) } label: { Image(systemName: "xmark").font(.system(size: 8)) }.buttonStyle(.plain).help("Close tab") }
                        }.padding(.horizontal, 12).frame(height: 24).background(model.activeTab == tab.id ? accent.opacity(0.12) : Color.primary.opacity(0.03), in: RoundedRectangle(cornerRadius: 7))
                    }
                }
            }
            Button(action: model.addTab) { Image(systemName: "plus").frame(width: 28, height: 30) }.buttonStyle(.plain).help("New workspace tab (⌘T)")
        }.padding(.horizontal, 12).frame(height: 32)
    }
    private var filterBar: some View {
        HStack(spacing: 12) {
            Picker("Filter", selection: $model.filter) { ForEach(PowerFilter.allCases) { Text($0.rawValue).tag($0) } }.pickerStyle(.menu).fixedSize()
            if model.filter != .all { Button("Clear filter") { model.filter = .all }.font(.system(size: 10)).buttonStyle(.plain).foregroundStyle(.secondary) }
            Spacer()
            Button { library.toggleStar(model.directory) } label: { Image(systemName: library.isStarred(model.directory) ? "star.fill" : "star") }.buttonStyle(.plain).help("Star current folder")
            Button { openTools() } label: { Label("Power tools", systemImage: "wand.and.stars").font(.system(size: 11, weight: .medium)) }.buttonStyle(.bordered)
        }.padding(.horizontal, 20).padding(.bottom, 10)
    }
    private var topbar: some View {
        HStack(spacing: 12) {
            Image(systemName: "rectangle.split.2x1.fill").font(.system(size: 18)).foregroundStyle(accent)
            Text("FileGlade").font(.system(size: 14, weight: .semibold))
            Text(model.showDashboard ? "/  All files" : "/  \(model.directory.lastPathComponent.isEmpty ? "Macintosh HD" : model.directory.lastPathComponent)").foregroundStyle(.secondary).lineLimit(1)
            Spacer()
            Text("FILE EXPLORER").font(.system(size: 9, weight: .semibold, design: .rounded)).tracking(1.8).foregroundStyle(.tertiary)
        }.padding(.horizontal, 21).frame(height: 48)
    }
    private var commandbar: some View {
        HStack(spacing: 5) {
            Button(action: acting.newFolder) { Label("New folder", systemImage: "folder.badge.plus").font(.system(size: 12, weight: .medium)) }.buttonStyle(.bordered).help("New folder (⇧⌘N)")
            Divider().frame(height: 20).padding(.horizontal, 9)
            tool("scissors", "Cut (⌘X)", disabled: acting.selection.isEmpty) { acting.copy(cut: true) }
            tool("doc.on.doc", "Copy (⌘C)", disabled: acting.selection.isEmpty) { acting.copy() }
            tool("doc.on.clipboard", "Paste (⌘V)") { acting.paste() }
            tool("pencil", "Rename (Return)", disabled: acting.selection.count != 1) { acting.rename() }
            tool("trash", "Move to Trash (⌘⌫)", disabled: acting.selection.isEmpty) { acting.trash() }
            tool("trash.slash", "Delete permanently (⌥⌘⌫) — skips the Trash", disabled: acting.selection.isEmpty) { acting.deletePermanently() }
            tool("arrow.uturn.backward", transfers.undoTitle + " (⌥⌘Z)", disabled: !transfers.canUndo) { acting.undoFileOperation() }
            Divider().frame(height: 20).padding(.horizontal, 9)
            Menu {
                Picker("Sort by", selection: bind(\.order)) { ForEach(Order.allCases, id: \.self) { Text($0.rawValue).tag($0) } }
                Divider()
                Toggle("Ascending", isOn: bind(\.ascending))
            } label: { Label("Sort", systemImage: "arrow.up.arrow.down").font(.system(size: 12)) }.menuStyle(.borderlessButton).fixedSize().padding(.horizontal, 7)
            Menu { Picker("Group by", selection: bind(\.grouping)) { ForEach(FileGrouping.allCases) { Text($0.rawValue).tag($0) } } } label: { Label("Group by", systemImage: "square.grid.3x1.below.line.grid.1x2").font(.system(size: 12)) }.menuStyle(.borderlessButton).fixedSize().padding(.horizontal, 7)
            Spacer()
            Picker("Filter", selection: bind(\.filter)) { ForEach(PowerFilter.allCases) { Text($0.rawValue).tag($0) } }.pickerStyle(.menu).labelsHidden().fixedSize().help("Filter files")
            tool(library.isStarred(acting.directory) ? "star.fill" : "star", "Star current folder") { library.toggleStar(acting.directory) }
            tool("wand.and.stars", "Power tools (⇧⌘P)") { openTools() }
            tool(panes.dual ? "rectangle.split.2x1.fill" : "rectangle.split.2x1", "Dual pane (⌘\\)") { panes.toggle() }
            if panes.dual {
                tool("arrow.right.circle", "Copy to other pane (⌥⌘C)", disabled: acting.selection.isEmpty) { panes.transferToOther(move: false) }
                tool("arrow.right.circle.fill", "Move to other pane (⌥⌘M)", disabled: acting.selection.isEmpty) { panes.transferToOther(move: true) }
                tool("arrow.triangle.2.circlepath", "Compare and sync panes") { extra = .compare }
            }
            Button { showBasket.toggle() } label: { Image(systemName: basket.items.isEmpty ? "tray" : "tray.full").font(.system(size: 14)).frame(width: 29, height: 28).contentShape(Rectangle()) }.buttonStyle(.plain).foregroundStyle(Color.primary.opacity(0.75)).help("Selection basket (⌘B adds the selection)")
                .popover(isPresented: $showBasket) { BasketView(target: acting) }
            HStack(spacing: 2) {
                viewButton("square.grid.2x2", layout: .icons)
                viewButton("list.bullet", layout: .details)
            }.padding(3).background(Color.primary.opacity(0.045), in: RoundedRectangle(cornerRadius: 7))
            tool("sidebar.right", "Toggle details pane") { model.inspector.toggle() }
            Menu {
                Button("Power tools…", action: openTools)
                Button("New workspace tab", action: model.addTab)
                Divider()
                Button("Open folder…", action: acting.chooseFolder)
                Button("Go to folder…", action: acting.goToFolder)
                Button("Show in Finder", action: acting.reveal)
                Divider()
                Button(acting.favorites.contains(acting.directory) ? "Unpin current folder" : "Pin current folder", action: acting.toggleFavorite)
                Button(acting.showHidden ? "Hide hidden files" : "Show hidden files", action: acting.toggleHidden)
                Button("Refresh", action: acting.refresh)
            } label: { Image(systemName: "ellipsis").frame(width: 26) }.menuStyle(.borderlessButton).fixedSize()
        }.padding(.horizontal, 12).frame(height: 38)
    }
    private func tool(_ icon: String, _ help: String, disabled: Bool = false, action: @escaping () -> Void) -> some View {
        Button(action: action) { Image(systemName: icon).font(.system(size: 14)).frame(width: 29, height: 28).contentShape(Rectangle()) }.buttonStyle(.plain).foregroundStyle(disabled ? Color.secondary.opacity(0.4) : Color.primary.opacity(0.75)).disabled(disabled).help(help).accessibilityLabel(help)
    }
    private func viewButton(_ icon: String, layout: Layout) -> some View {
        Button { acting.setLayout(layout) } label: { Image(systemName: icon).font(.system(size: 13)).frame(width: 28, height: 24).background(acting.layout == layout ? Color(nsColor: .controlBackgroundColor) : .clear, in: RoundedRectangle(cornerRadius: 5)) }.buttonStyle(.plain).help(layout == .icons ? "Icon view (⌘1)" : "Details view (⌘2)")
    }
    private var sidebar: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 5) {
                Button { model.showDashboard = true } label: {
                    Label("All files", systemImage: "square.grid.2x2.fill").font(.system(size: 13, weight: .semibold)).frame(maxWidth: .infinity, alignment: .leading).padding(12).background(model.showDashboard ? accent.opacity(0.12) : .clear, in: RoundedRectangle(cornerRadius: 7))
                }.buttonStyle(.plain).foregroundStyle(accent).padding(.bottom, 14)
                sectionTitle("Favorites")
                sideItem("Home", "house", home)
                sideItem("Desktop", "desktopcomputer", home.appendingPathComponent("Desktop"))
                sideItem("Downloads", "arrow.down.circle", home.appendingPathComponent("Downloads"))
                sideItem("Documents", "doc.text", home.appendingPathComponent("Documents"))
                sideItem("Pictures", "photo.on.rectangle", home.appendingPathComponent("Pictures"))
                sideItem("Music", "music.note", home.appendingPathComponent("Music"))
                sideItem("Movies", "film", home.appendingPathComponent("Movies"))
                sectionTitle("Smart folders").padding(.top, 20)
                ForEach(smartStore.smartFolders) { search in
                    Button { smartStore.pending = search; model.showDashboard = true } label: {
                        HStack(spacing: 11) { Image(systemName: "folder.badge.gearshape").font(.system(size: 14)).foregroundStyle(.secondary).frame(width: 18); Text(search.name).font(.system(size: 12)).lineLimit(1); Spacer(minLength: 0) }
                            .padding(.horizontal, 12).frame(height: 32).contentShape(Rectangle())
                    }.buttonStyle(.plain).help("Smart folder: a saved search. Nothing is moved.")
                    .contextMenu { if !smartStore.isBuiltIn(search) { Button("Remove smart folder") { smartStore.remove(search) } } }
                }
                sectionTitle("Collections").padding(.top, 20)
                if collectionStore.collections.isEmpty { Text("Group files from different folders with Tools ▸ New Collection.").font(.system(size: 11)).foregroundStyle(.tertiary).padding(.horizontal, 12).padding(.vertical, 4) }
                ForEach(collectionStore.collections) { collection in
                    Button { extra = .collection(collection.id) } label: {
                        HStack(spacing: 11) { Image(systemName: "square.stack").font(.system(size: 14)).foregroundStyle(.secondary).frame(width: 18); Text(collection.name).font(.system(size: 12)).lineLimit(1); Spacer(minLength: 0); Text("\(collection.items.count)").font(.system(size: 10)).foregroundStyle(.tertiary) }
                            .padding(.horizontal, 12).frame(height: 32).contentShape(Rectangle())
                    }.buttonStyle(.plain)
                }
                sectionTitle("Pinned").padding(.top, 20)
                if model.favorites.isEmpty { Text("Pin folders from the ★ button\nto keep them close.").font(.system(size: 11)).foregroundStyle(.tertiary).lineSpacing(4).padding(.horizontal, 12).padding(.vertical, 5) }
                ForEach(model.favorites, id: \.self) { url in
                    sideItem(url.lastPathComponent, FileManager.default.fileExists(atPath: url.path) ? "pin" : "exclamationmark.triangle", url).contextMenu {
                        if !FileManager.default.fileExists(atPath: url.path) { Button("Locate…") { let panel = NSOpenPanel(); panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.message = "Choose where “\(url.lastPathComponent)” is now."; if panel.runModal() == .OK, let new = panel.url { model.replaceFavorite(url, with: new) } } }
                        Button("Unpin") { model.removeFavorite(url) }
                    }
                }
                sectionTitle("Locations").padding(.top, 20)
                sideItem("Applications", "square.stack.3d.up", URL(fileURLWithPath: "/Applications"))
                ForEach(model.volumes, id: \.self) { url in sideItem(url.path == "/" ? "Macintosh HD" : url.lastPathComponent, "internaldrive", url) }
                Spacer(minLength: 30)
                Button(action: model.chooseFolder) { Label("Open a folder…", systemImage: "folder.badge.gearshape").font(.system(size: 11)) }.buttonStyle(.plain).foregroundStyle(.secondary).padding(12)
            }.padding(.horizontal, 12).padding(.top, 19)
        }.background(.regularMaterial)
    }
    private func sectionTitle(_ title: String) -> some View { Text(title).font(.system(size: 11, weight: .semibold)).foregroundStyle(.secondary).padding(.horizontal, 12).padding(.bottom, 6) }
    private func sideItem(_ title: String, _ icon: String, _ url: URL) -> some View {
        let selected = !model.showDashboard && acting.directory.path == url.path
        return Button { acting.navigate(url) } label: {
            HStack(spacing: 11) { Image(systemName: icon).font(.system(size: 14)).foregroundStyle(selected ? accent : .secondary).frame(width: 18); Text(title).font(.system(size: 12, weight: selected ? .semibold : .regular)).lineLimit(1); Spacer(minLength: 0) }
                .padding(.horizontal, 12).frame(height: 35).contentShape(Rectangle()).background(selected ? accent.opacity(0.1) : .clear, in: RoundedRectangle(cornerRadius: 6))
                .overlay(alignment: .leading) { if selected { Capsule().fill(accent).frame(width: 3, height: 17) } }
        }.buttonStyle(.plain).help(url.path)
        .onDrop(of: [.fileURL], isTargeted: nil) { acting.handleDrop($0, into: url) }
    }
    private var locationbar: some View {
        HStack(spacing: 7) {
            tool("chevron.left", "Back (⌘[)", disabled: !model.canBack, action: model.back)
            tool("chevron.right", "Forward (⌘])", disabled: !model.canForward, action: model.forward)
            tool("arrow.up", "Parent folder (⌘↑)", disabled: model.directory.path == "/", action: model.up)
            HStack(spacing: 7) {
                Image(systemName: "folder").foregroundStyle(accent)
                if pathEditing {
                    TextField("Folder path", text: $path).textFieldStyle(.plain).focused($pathFocused).onSubmit { model.navigate(URL(fileURLWithPath: (path as NSString).expandingTildeInPath)); pathEditing = false }.onExitCommand { pathEditing = false }
                } else {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 7) {
                            ForEach(model.breadcrumbs, id: \.self) { url in
                                Button(url.path == "/" ? "Mac" : url.lastPathComponent) { model.navigate(url) }.buttonStyle(.plain).font(.system(size: 11, weight: url == model.directory ? .semibold : .regular))
                                if url != model.directory { Image(systemName: "chevron.right").font(.system(size: 8)).foregroundStyle(.tertiary) }
                            }
                        }
                    }
                    Button { beginPath() } label: { Image(systemName: "pencil").font(.system(size: 10)).foregroundStyle(.secondary) }.buttonStyle(.plain).help("Edit path (⌘L)")
                }
                Button(action: model.refresh) { Image(systemName: "arrow.clockwise").font(.system(size: 11)) }.buttonStyle(.plain).help("Refresh (⌘R)")
            }.padding(.horizontal, 10).frame(height: 26).background(Color(nsColor: .controlBackgroundColor), in: RoundedRectangle(cornerRadius: 6)).overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(Color.primary.opacity(0.08)))
            HStack(spacing: 7) {
                Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                TextField("Search this folder", text: $model.query).textFieldStyle(.plain).font(.system(size: 11)).focused($searchFocused)
                if !model.query.isEmpty { Button { model.query = "" } label: { Image(systemName: "xmark.circle.fill").foregroundStyle(.secondary) }.buttonStyle(.plain) }
            }.padding(.horizontal, 10).frame(width: 170, height: 26).background(Color(nsColor: .controlBackgroundColor), in: RoundedRectangle(cornerRadius: 6)).overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(Color.primary.opacity(0.08)))
        }.padding(.horizontal, 12).frame(height: 40)
    }
    private func beginPath() { path = model.directory.path; pathEditing = true; pathFocused = true }
    private var groups: [FileSection<FileEntry>] {
        fileSections(model.visible) { entry in model.grouping.key(name: entry.name, folder: entry.isDirectory, size: entry.size, modified: entry.modified, category: fileCategory(entry.url), location: entry.url.deletingLastPathComponent().path) }
    }
    @ViewBuilder private var content: some View {
        if model.visible.isEmpty {
            VStack(spacing: 12) {
                Image(systemName: model.query.isEmpty ? "folder" : "magnifyingglass").font(.system(size: 42, weight: .ultraLight)).foregroundStyle(.tertiary)
                Text(model.query.isEmpty ? "This folder is empty" : "No matching items").font(.system(size: 17, weight: .medium))
                Text(model.query.isEmpty ? "Create a folder or paste files here." : "Try another name. Search looks in this folder only.").font(.system(size: 12)).foregroundStyle(.secondary)
            }.frame(maxWidth: .infinity, maxHeight: .infinity).background(Color(nsColor: .textBackgroundColor))
        } else if model.layout == .icons {
            ScrollView {
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 112, maximum: 144), spacing: 12)], alignment: .leading, spacing: 12) {
                    ForEach(groups) { group in
                        Section {
                    ForEach(group.items) { entry in
                        iconCard(entry)
                    }
                        } header: { if model.grouping != .none { Text(group.title).font(.headline).frame(maxWidth: .infinity, alignment: .leading).padding(.top, 12) } }
                    }
                }.padding(20)
            }.background(Color(nsColor: .textBackgroundColor))
        } else if model.grouping != .none {
            List {
                ForEach(groups) { group in
                    Section(group.title) {
                        ForEach(group.items) { entry in
                            HStack(spacing: 12) {
                                FileIcon(url: entry.url, size: 24, directory: entry.isDirectory)
                                Text(entry.name).frame(maxWidth: .infinity, alignment: .leading).lineLimit(1)
                                Text(entry.modified.formatted(date: .abbreviated, time: .shortened)).font(.caption).foregroundStyle(.secondary)
                                Text(entry.kind).font(.caption).foregroundStyle(.secondary).frame(width: 100)
                                Text(entry.isDirectory ? "—" : fileBytes(entry.size)).font(.caption).foregroundStyle(.secondary).frame(width: 75, alignment: .trailing)
                            }.padding(5).contentShape(Rectangle()).background(model.selection.contains(entry.url) ? accent.opacity(0.12) : .clear)
                            .onTapGesture(count: 2) { model.openEntry(entry) }.onTapGesture { model.select(entry) }.contextMenu { fileMenu(entry) }.onDrag { NSItemProvider(object: entry.url as NSURL) }
                            .onDrop(of: [.fileURL], isTargeted: nil) { providers in (entry.isDirectory && !entry.isPackage) ? model.handleDrop(providers, into: entry.url) : false }
                        }
                    }
                }
            }.listStyle(.plain)
        } else {
            Table(model.visible, selection: $model.selection) {
                TableColumn("Name") { entry in HStack(spacing: 9) { FileIcon(url: entry.url, size: 24, directory: entry.isDirectory); Text(entry.name).lineLimit(1) }.opacity(model.isCut(entry.url) ? 0.45 : 1).contextMenu { fileMenu(entry) }.onDrag { NSItemProvider(object: entry.url as NSURL) } }.width(min: 150, ideal: 230)
                TableColumn("Date modified") { entry in Text(entry.modified.formatted(date: .abbreviated, time: .shortened)).foregroundStyle(.secondary).font(.system(size: 11)) }.width(min: 125, ideal: 145)
                TableColumn("Kind") { entry in Text(entry.kind).foregroundStyle(.secondary).font(.system(size: 11)).lineLimit(1) }.width(min: 80, ideal: 110)
                TableColumn("Size") { entry in Text(entry.isDirectory ? "—" : ByteCountFormatter.string(fromByteCount: entry.size, countStyle: .file)).foregroundStyle(.secondary).font(.system(size: 11)) }.width(70)
            }.contextMenu(forSelectionType: URL.self) { urls in
                if !urls.isEmpty { Button("Open") { model.selection = urls; model.openSelection() }; Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting(Array(urls)) }; Divider(); Button("Copy") { model.selection = urls; model.copy() }; Button("Rename…") { model.selection = urls; model.rename() }.disabled(urls.count != 1); Button("Move to Trash…") { model.selection = urls; model.trash() }; Button("Delete Permanently…") { model.selection = urls; model.deletePermanently() } }
            } primaryAction: { urls in if let url = urls.first, let entry = model.entries.first(where: { $0.url == url }) { model.openEntry(entry) } }
        }
    }
    private func iconCard(_ entry: FileEntry) -> some View {
                        VStack(spacing: 9) { FileIcon(url: entry.url, size: 52, thumbnail: true, directory: entry.isDirectory); Text(entry.name).font(.system(size: 11)).lineLimit(2).multilineTextAlignment(.center).frame(height: 30, alignment: .top) }
                            .padding(.top, 14).padding(.horizontal, 6).padding(.bottom, 8).frame(maxWidth: .infinity)
                            .background(model.selection.contains(entry.url) ? accent.opacity(0.12) : Color.clear, in: RoundedRectangle(cornerRadius: 8))
                            .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(model.selection.contains(entry.url) ? accent.opacity(0.4) : .clear))
                            .opacity(model.isCut(entry.url) ? 0.45 : 1)
                            .overlay(alignment: .topTrailing) {
                                HStack(spacing: 4) { if let color = library.color(entry.url) { Circle().fill(color).frame(width: 7, height: 7) }; if library.isStarred(entry.url) { Image(systemName: "star.fill").font(.system(size: 9)).foregroundStyle(.yellow) } }.padding(7)
                            }
                            .contentShape(Rectangle())
                            .onTapGesture(count: 2) { model.openEntry(entry) }
                            .onTapGesture { model.select(entry) }
                            .contextMenu { fileMenu(entry) }
                            .onDrag { NSItemProvider(object: entry.url as NSURL) }
                            .onDrop(of: [.fileURL], isTargeted: nil) { providers in (entry.isDirectory && !entry.isPackage) ? model.handleDrop(providers, into: entry.url) : false }
                            .help(entry.name)
                            .accessibilityElement(children: .ignore)
                            .accessibilityLabel(entry.name)
                            .accessibilityAddTraits(.isButton)
                            .accessibilityAddTraits(model.selection.contains(entry.url) ? .isSelected : [])
                            .accessibilityAction { model.select(entry) }
                            .accessibilityAction(named: "Open") { model.openEntry(entry) }
    }
    @ViewBuilder private func fileMenu(_ entry: FileEntry) -> some View {
        Button("Open") { model.openEntry(entry) }
        Button(library.isStarred(entry.url) ? "Remove star" : "Star item") { library.toggleStar(entry.url) }
        Menu("Color label") { ForEach(FileLibrary.labels, id: \.self) { label in Button(label) { contextSelect(entry); library.setLabel(label, urls: model.chosen.map(\.url)) } } }
        Button("Power tools for selection…") { contextSelect(entry); openTools() }
        Button("Copy path") { copyFileText(entry.url.path) }
        Button("Add to Basket") { contextSelect(entry); basket.add(acting.chosen.map(\.url)) }
        Menu("Add to Collection") {
            ForEach(collectionStore.collections) { collection in Button(collection.name) { contextSelect(entry); collectionStore.add(acting.chosen.map(\.url), to: collection.id) } }
            if !collectionStore.collections.isEmpty { Divider() }
            Button("New Collection…") { contextSelect(entry); if let name = acting.prompt(title: "New collection", message: "Name the collection. Files stay where they are.", value: "New collection", action: "Create"), !name.isEmpty { _ = collectionStore.create(name, with: acting.chosen.map(\.url)) } }
        }
        if panes.dual { Button("Copy to Other Pane") { contextSelect(entry); panes.transferToOther(move: false) }; Button("Move to Other Pane") { contextSelect(entry); panes.transferToOther(move: true) } }
        if ArchiveTool.isArchive(entry.url) { Button("Browse Archive…") { extra = .archive(entry.url) } }
        Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([entry.url]) }
        Divider()
        Button("Cut") { contextSelect(entry); model.copy(cut: true) }
        Button("Copy") { contextSelect(entry); model.copy() }
        Button("Rename…") { model.selection = [entry.url]; model.rename() }
        Divider()
        Button("Move to Trash…") { contextSelect(entry); model.trash() }
        Button("Delete Permanently…") { contextSelect(entry); model.deletePermanently() }
    }
    private func contextSelect(_ entry: FileEntry) { if !model.selection.contains(entry.url) { model.selection = [entry.url] } }
    private var statusbar: some View {
        HStack(spacing: 12) {
            Text("\(model.visible.count) \(model.visible.count == 1 ? "item" : "items")")
            if !model.selection.isEmpty { Text("·"); Text("\(model.selection.count) selected") }
            if !model.query.isEmpty { Text("· Filtered") }
            Spacer()
            if let values = try? model.directory.resourceValues(forKeys: [.volumeAvailableCapacityKey]), let bytes = values.volumeAvailableCapacity { Text("\(ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)) available") }
        }.font(.system(size: 10)).foregroundStyle(.secondary).padding(.horizontal, 14).frame(height: 22)
    }
    private var inspector: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                HStack { Text("DETAILS").font(.system(size: 9, weight: .semibold)).tracking(1.4).foregroundStyle(.secondary); Spacer(); Button { model.inspector = false } label: { Image(systemName: "xmark").font(.system(size: 10)).foregroundStyle(.secondary) }.buttonStyle(.plain) }
                if let entry = acting.focused {
                    FilePreview(entry: entry).padding(.vertical, 6)
                    Text(entry.name).font(.system(size: 17, weight: .semibold)).textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
                    Text(entry.kind).font(.system(size: 11)).foregroundStyle(.secondary)
                    Divider()
                    detail("LOCATION", entry.url.deletingLastPathComponent().path)
                    if !entry.isDirectory { detail("SIZE", "\(ByteCountFormatter.string(fromByteCount: entry.size, countStyle: .file)) (\(entry.size.formatted()) bytes)") }
                    detail("MODIFIED", entry.modified.formatted(date: .abbreviated, time: .shortened))
                    detail("EXTENSION", entry.url.pathExtension.isEmpty ? "None" : entry.url.pathExtension.uppercased())
                    if entry.isDirectory { FolderSizeRow(url: entry.url) }
                    TagsAndNote(url: entry.url)
                    Button { acting.openEntry(entry) } label: { Label(entry.isDirectory ? "Open folder" : "Open file", systemImage: "arrow.up.forward.square").frame(maxWidth: .infinity) }.buttonStyle(.bordered)
                    Button("Show in Finder", action: acting.reveal).buttonStyle(.plain).font(.system(size: 11)).foregroundStyle(accent)
                } else {
                    Image(systemName: acting.selection.isEmpty ? "folder" : "doc.on.doc").font(.system(size: 55, weight: .ultraLight)).foregroundStyle(accent.opacity(0.65)).frame(maxWidth: .infinity).padding(.vertical, 25)
                    Text(acting.selection.isEmpty ? (acting.directory.lastPathComponent.isEmpty ? "Macintosh HD" : acting.directory.lastPathComponent) : "\(acting.selection.count) items selected").font(.system(size: 17, weight: .semibold)).fixedSize(horizontal: false, vertical: true)
                    Text(acting.selection.isEmpty ? "Select an item to see its details." : "Select a single item to see its details.").font(.system(size: 12)).foregroundStyle(.secondary).lineSpacing(4)
                    Divider()
                    detail("FOLDER", acting.directory.path)
                    detail("CONTENTS", "\(acting.entries.filter(\.isDirectory).count) folders, \(acting.entries.filter { !$0.isDirectory }.count) files\(acting.showHidden ? "" : " · hidden items excluded")")
                    Button(action: acting.toggleFavorite) { Label(acting.favorites.contains(acting.directory) ? "Unpin folder" : "Pin this folder", systemImage: acting.favorites.contains(acting.directory) ? "star.fill" : "star").frame(maxWidth: .infinity) }.buttonStyle(.bordered)
                }
            }.padding(14)
        }.background(Color.primary.opacity(0.015))
    }
    private func detail(_ label: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 6) { Text(label).font(.system(size: 9, weight: .medium)).tracking(0.8).foregroundStyle(.tertiary); Text(value).font(.system(size: 11)).foregroundStyle(.secondary).textSelection(.enabled).fixedSize(horizontal: false, vertical: true) }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuItemValidation {
    let mainModel = Explorer()
    /// The pane that menu commands act on: the active pane when two panes are showing.
    var model: Explorer { PaneState.shared.current ?? mainModel }
    var window: NSWindow!
    private var keyMonitor: Any?
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        buildMenu()
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1220, height: 780), styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "FileGlade"
        window.titlebarAppearsTransparent = false
        window.tabbingMode = .disallowed
        window.minSize = NSSize(width: 850, height: 570)
        PaneState.shared.main = mainModel
        window.contentView = NSHostingView(rootView: ExplorerView(model: mainModel))
        NotificationCenter.default.addObserver(forName: .init("FileGladeRebuildMenu"), object: nil, queue: .main) { [weak self] _ in self?.buildMenu() }
        window.setFrameAutosaveName("FileGladeMain")
        if !window.setFrameUsingName("FileGladeMain") { window.center() }
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        offerFullDiskAccess()
        Updater.shared.checkAutomatically()
        AutomationStore.shared.start()
        keyMonitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
            guard let self, NSApp.keyWindow === self.window, self.textResponder == nil else { return event }
            let modifiers = event.modifierFlags.intersection([.command, .option, .control, .shift])
            if event.keyCode == 53 { self.model.selection = []; self.model.query = ""; return nil }
            guard self.model.layout == .icons, modifiers.isEmpty || modifiers == .shift, [123, 124, 125, 126].contains(Int(event.keyCode)) else { return event }
            let items = self.model.visible
            guard !items.isEmpty else { return nil }
            let contentWidth = self.window.contentView!.bounds.width - 208 - (self.model.inspector ? 280 : 0) - 42
            let columns = max(1, Int((contentWidth + 12) / 124))
            let current = items.firstIndex { self.model.selection.contains($0.url) }
            let step = event.keyCode == 123 ? -1 : event.keyCode == 124 ? 1 : event.keyCode == 125 ? columns : -columns
            let index = current.map { min(max($0 + step, 0), items.count - 1) } ?? 0
            if modifiers == .shift { self.model.selection.insert(items[index].url) } else { self.model.selection = [items[index].url] }
            return nil
        }
        if CommandLine.arguments.count > 1 {
            let arg = CommandLine.arguments[1]
            if !arg.hasPrefix("-") { mainModel.navigate(URL(fileURLWithPath: (arg as NSString).expandingTildeInPath)) }
        }
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationWillTerminate(_ notification: Notification) { if let keyMonitor { NSEvent.removeMonitor(keyMonitor) } }
    func application(_ sender: NSApplication, openFiles filenames: [String]) { if let first = filenames.first { mainModel.navigate(URL(fileURLWithPath: first)) }; sender.reply(toOpenOrPrint: .success) }
    private func item(_ title: String, _ action: Selector, _ key: String = "", modifiers: NSEvent.ModifierFlags = .command) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: key); item.target = [#selector(cut(_:)), #selector(copyItems(_:)), #selector(paste(_:)), #selector(selectAll(_:))].contains(action) ? nil : self; item.keyEquivalentModifierMask = modifiers; return item
    }
    func buildMenu() {
        let main = NSMenu()
        let app = NSMenu(); app.addItem(item("About FileGlade", #selector(about))); app.addItem(item("Check for Updates…", #selector(checkUpdates))); app.addItem(item("Check for Updates Automatically", #selector(toggleAutoUpdate))); app.addItem(item("Full Disk Access Settings…", #selector(fullDiskAccess))); app.addItem(.separator()); app.addItem(item("Quit FileGlade", #selector(quit), "q"))
        let file = NSMenu(); file.addItem(item("Open Folder…", #selector(chooseFolder), "o")); file.addItem(item("Open Selected", #selector(openSelected), "o", modifiers: [.command, .shift])); file.addItem(item("New Folder…", #selector(newFolder), "n", modifiers: [.command, .shift])); file.addItem(item("Rename…", #selector(rename), "\r", modifiers: [])); file.addItem(item("Show in Finder", #selector(reveal))); file.addItem(.separator()); file.addItem(item("Move to Trash…", #selector(trash), "\u{8}")); file.addItem(item("Delete Permanently…", #selector(deletePermanently), "\u{8}", modifiers: [.command, .option])); file.addItem(.separator()); file.addItem(item("Close Window", #selector(closeWindow), "w"))
        file.insertItem(item("New Workspace Tab", #selector(newTab), "t"), at: 0)
        file.insertItem(item("Power Tools…", #selector(powerTools), "p", modifiers: [.command, .shift]), at: 1)
        let edit = NSMenu(); edit.addItem(NSMenuItem(title: "Undo", action: Selector(("undo:")), keyEquivalent: "z")); edit.addItem(NSMenuItem(title: "Redo", action: Selector(("redo:")), keyEquivalent: "Z")); edit.addItem(item("Undo File Operation", #selector(undoFile), "z", modifiers: [.command, .option])); edit.addItem(.separator()); edit.addItem(item("Cut", #selector(cut(_:)), "x")); edit.addItem(item("Copy", #selector(copyItems(_:)), "c")); edit.addItem(item("Paste", #selector(paste(_:)), "v")); edit.addItem(item("Select All", #selector(selectAll(_:)), "a")); edit.addItem(.separator()); edit.addItem(item("Search This Folder", #selector(search), "f"))
        let view = NSMenu(); view.addItem(item("Icons", #selector(icons), "1")); view.addItem(item("Details", #selector(details), "2")); view.addItem(item("Toggle Details Pane", #selector(inspector), "i", modifiers: [.command, .option])); view.addItem(.separator()); view.addItem(item("Show Hidden Files", #selector(hidden), ".", modifiers: [.command, .shift])); view.addItem(item("Refresh", #selector(refresh), "r"))
        let go = NSMenu(); go.addItem(item("Back", #selector(back), "[")); go.addItem(item("Forward", #selector(forward), "]")); go.addItem(item("Parent Folder", #selector(up), String(UnicodeScalar(NSUpArrowFunctionKey)!))); go.addItem(item("Edit Location", #selector(location), "l")); go.addItem(item("Go to Folder…", #selector(goTo), "g", modifiers: [.command, .shift])); go.addItem(item("Home", #selector(home), "h", modifiers: [.command, .shift])); go.addItem(.separator()); go.addItem(item("Quick Open…", #selector(quickOpen), "k"))
        let pane = NSMenu()
        pane.addItem(item("Dual Pane", #selector(toggleDual), "\\")); pane.addItem(item("Switch Pane", #selector(switchPane), "\t", modifiers: []))
        pane.addItem(item("Copy to Other Pane", #selector(copyToOther), "c", modifiers: [.command, .option])); pane.addItem(item("Move to Other Pane", #selector(moveToOther), "m", modifiers: [.command, .option]))
        pane.addItem(item("Compare and Sync Panes…", #selector(comparePanes))); pane.addItem(.separator())
        pane.addItem(item("Add Selection to Basket", #selector(addToBasket), "b")); pane.addItem(item("Show Basket", #selector(showBasket)))
        pane.addItem(.separator()); pane.addItem(item("Browse Archive…", #selector(browseArchive), "b", modifiers: [.command, .option])); pane.addItem(item("Search Inside Documents…", #selector(docSearch), "f", modifiers: [.command, .shift]))
        app.insertItem(item("Keyboard Shortcuts…", #selector(shortcutsWindow), ","), at: 1)
        let tools = NSMenu()
        tools.addItem(item("Automation Rules…", #selector(automationWindow))); tools.addItem(item("Storage Analyzer…", #selector(storageWindow))); tools.addItem(item("Find Similar Photos…", #selector(similarWindow)))
        tools.addItem(.separator()); tools.addItem(item("New Collection…", #selector(newCollection))); tools.addItem(item("Add Selection to Collection…", #selector(addToCollection)))
        tools.addItem(.separator()); tools.addItem(item("Back Up Settings…", #selector(backupSettings))); tools.addItem(item("Restore Settings…", #selector(restoreSettings)))
        for (title, menu) in [("FileGlade", app), ("File", file), ("Edit", edit), ("View", view), ("Go", go), ("Pane", pane), ("Tools", tools)] { let root = NSMenuItem(title: title, action: nil, keyEquivalent: ""); root.submenu = menu; main.addItem(root) }
        ShortcutStore.shared.apply(to: main)
        NSApp.mainMenu = main
    }
    private var textResponder: NSTextView? {
        let responder = (NSApp.modalWindow ?? NSApp.keyWindow)?.firstResponder
        if let text = responder as? NSTextView { return text }
        return (responder as? NSTextField)?.currentEditor() as? NSTextView
    }
    /// Explains once why macOS keeps asking for folder access and offers the one-time fix.
    private func offerFullDiskAccess() {
        guard !Prefs.selfTest, !FullDiskAccess.granted, !UserDefaults.standard.bool(forKey: "fullDiskAccessOffered") else { return }
        UserDefaults.standard.set(true, forKey: "fullDiskAccessOffered")
        DispatchQueue.main.async {
            let alert = NSAlert()
            alert.messageText = "Stop the repeated permission prompts"
            alert.informativeText = "macOS asks separately for Desktop, Documents, Downloads, iCloud and drives. Give FileGlade Full Disk Access once and it will not ask again.\n\nIn System Settings, turn on FileGlade under Privacy & Security ▸ Full Disk Access, then reopen FileGlade. Until then, FileGlade skips those folders in the background."
            alert.addButton(withTitle: "Open Settings")
            alert.addButton(withTitle: "Not Now")
            if alert.runModal() == .alertFirstButtonReturn { FullDiskAccess.openSettings() }
        }
    }
    @objc func quickOpen() { NotificationCenter.default.post(name: .init("FileGladeSwitcher"), object: nil) }
    private var toolWindows: [String: NSWindow] = [:]
    private func showToolWindow<Content: View>(_ id: String, _ title: String, _ size: NSSize, fresh: Bool = false, _ content: Content) {
        if let existing = toolWindows[id] { if fresh { existing.close() } else { existing.makeKeyAndOrderFront(nil); return } }
        let panel = NSWindow(contentViewController: NSHostingController(rootView: content))
        panel.title = title; panel.styleMask = [.titled, .closable, .resizable, .miniaturizable]; panel.isReleasedWhenClosed = false
        panel.setContentSize(size); panel.center()
        toolWindows[id] = panel
        NotificationCenter.default.addObserver(forName: NSWindow.willCloseNotification, object: panel, queue: .main) { [weak self] _ in if self?.toolWindows[id] === panel { self?.toolWindows[id] = nil } }
        panel.makeKeyAndOrderFront(nil)
    }
    @objc func automationWindow() { showToolWindow("automation", "Automation Rules", NSSize(width: 900, height: 640), AutomationView()) }
    @objc func storageWindow() { let m = model; showToolWindow("storage", "Storage Analyzer", NSSize(width: 800, height: 600), fresh: true, StorageAnalyzerView(folder: m.directory) { url in m.navigate(url.deletingLastPathComponent()); m.selection = [powerURL(url)] }) }
    @objc func similarWindow() { let m = model; showToolWindow("similar", "Similar Photos", NSSize(width: 820, height: 600), fresh: true, SimilarPhotosView(folder: m.directory) { m.refresh() }) }
    @objc func newCollection() {
        guard let name = model.prompt(title: "New collection", message: "Name the collection. Files stay where they are; the selected files are added.", value: "New collection", action: "Create"), !name.isEmpty else { return }
        let id = CollectionStore.shared.create(name, with: model.chosen.map(\.url))
        NotificationCenter.default.post(name: .init("FileGladeExtra"), object: "collection", userInfo: ["id": id])
    }
    @objc func addToCollection() {
        let store = CollectionStore.shared
        guard !store.collections.isEmpty else { newCollection(); return }
        guard !model.chosen.isEmpty else { model.failure = "Select files first."; return }
        let alert = NSAlert(); alert.messageText = "Add \(model.chosen.count) item\(model.chosen.count == 1 ? "" : "s") to a collection"
        let popup = NSPopUpButton(frame: NSRect(x: 0, y: 0, width: 260, height: 26)); popup.addItems(withTitles: store.collections.map(\.name)); alert.accessoryView = popup
        alert.addButton(withTitle: "Add"); alert.addButton(withTitle: "Cancel")
        if alert.runModal() == .alertFirstButtonReturn { store.add(model.chosen.map(\.url), to: store.collections[popup.indexOfSelectedItem].id) }
    }
    @objc func backupSettings() { Backup.saveWithPanel() }
    @objc func restoreSettings() { Backup.restoreWithPanel() }
    @objc func toggleDual() { PaneState.shared.toggle() }
    @objc func switchPane() { if textResponder == nil { PaneState.shared.switchPane() } }
    @objc func copyToOther() { PaneState.shared.transferToOther(move: false) }
    @objc func moveToOther() { PaneState.shared.transferToOther(move: true) }
    @objc func comparePanes() { NotificationCenter.default.post(name: .init("FileGladeExtra"), object: "compare") }
    @objc func addToBasket() { if textResponder == nil { BasketStore.shared.add(model.chosen.map(\.url)) } }
    @objc func showBasket() { NotificationCenter.default.post(name: .init("FileGladeExtra"), object: "basket") }
    @objc func docSearch() { NotificationCenter.default.post(name: .init("FileGladeExtra"), object: "docs") }
    @objc func browseArchive() {
        if let entry = model.focused, ArchiveTool.isArchive(entry.url) { NotificationCenter.default.post(name: .init("FileGladeExtra"), object: "archive", userInfo: ["url": entry.url]) }
        else { model.failure = "Select one archive (ZIP, TAR, 7z, RAR…) to browse." }
    }
    private var shortcutWindow: NSWindow?
    @objc func shortcutsWindow() {
        if let shortcutWindow { shortcutWindow.makeKeyAndOrderFront(nil); return }
        let panel = NSWindow(contentViewController: NSHostingController(rootView: ShortcutsView()))
        panel.title = "Keyboard Shortcuts"; panel.styleMask = [.titled, .closable]; panel.isReleasedWhenClosed = false; panel.center()
        shortcutWindow = panel
        NotificationCenter.default.addObserver(forName: NSWindow.willCloseNotification, object: panel, queue: .main) { [weak self] _ in self?.shortcutWindow = nil }
        panel.makeKeyAndOrderFront(nil)
    }
    @objc func checkUpdates() { Updater.shared.check(userInitiated: true) }
    @objc func toggleAutoUpdate() { Updater.shared.automatic.toggle() }
    @objc func fullDiskAccess() { FullDiskAccess.openSettings() }
    private var aboutWindow: NSWindow?
    @objc func about() {
        if let aboutWindow { aboutWindow.makeKeyAndOrderFront(nil); return }
        let host = NSHostingController(rootView: FileGladeAboutView())
        let panel = NSWindow(contentViewController: host)
        panel.title = "About FileGlade"; panel.styleMask = [.titled, .closable]; panel.isReleasedWhenClosed = false
        panel.setContentSize(NSSize(width: 480, height: 600)); panel.center()
        aboutWindow = panel
        NotificationCenter.default.addObserver(forName: NSWindow.willCloseNotification, object: panel, queue: .main) { [weak self] _ in self?.aboutWindow = nil }
        panel.makeKeyAndOrderFront(nil)
    }
    @objc func quit() { NSApp.terminate(nil) }
    @objc func closeWindow() { window.performClose(nil) }
    @objc func chooseFolder() { model.chooseFolder() }
    @objc func openSelected() { model.openSelection() }
    @objc func newFolder() { model.newFolder() }
    @objc func rename() { if textResponder == nil { model.rename() } }
    @objc func reveal() { model.reveal() }
    @objc func deletePermanently() { if textResponder == nil { model.deletePermanently() } }
    @objc func undoFile() { model.undoFileOperation() }
    @objc func trash() { if textResponder == nil { model.trash() } }
    @objc(cut:) func cut(_ sender: Any?) { if let text = textResponder { text.cut(nil) } else { model.copy(cut: true) } }
    @objc(copy:) func copyItems(_ sender: Any?) { if let text = textResponder { text.copy(nil) } else { model.copy() } }
    @objc(paste:) func paste(_ sender: Any?) { if let text = textResponder { text.paste(nil) } else { model.paste() } }
    @objc(selectAll:) func selectAll(_ sender: Any?) { if let text = textResponder { text.selectAll(nil) } else { model.selection = Set(model.visible.map(\.url)) } }
    @objc func search() { NotificationCenter.default.post(name: .init("FileGladeSearch"), object: nil) }
    @objc func icons() { model.setLayout(.icons) }
    @objc func details() { model.setLayout(.details) }
    @objc func inspector() { model.inspector.toggle() }
    @objc func hidden() { model.toggleHidden() }
    @objc func refresh() { model.refresh() }
    @objc func back() { model.back() }
    @objc func forward() { model.forward() }
    @objc func up() { model.up() }
    @objc func location() { NotificationCenter.default.post(name: .init("FileGladePath"), object: nil) }
    @objc func goTo() { model.goToFolder() }
    @objc func home() { model.navigate(FileManager.default.homeDirectoryForCurrentUser) }
    @objc func newTab() { model.addTab() }
    @objc func powerTools() { NotificationCenter.default.post(name: .init("FileGladeTools"), object: nil) }
    func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
        switch menuItem.action {
        case #selector(back): return model.canBack
        case #selector(forward): return model.canForward
        case #selector(up): return model.directory.path != "/"
        case #selector(rename): return textResponder == nil && model.selection.count == 1
        case #selector(undoFile): menuItem.title = TransferManager.shared.canUndo ? TransferManager.shared.undoTitle : "Undo File Operation"; return TransferManager.shared.canUndo
        case #selector(trash), #selector(deletePermanently), #selector(openSelected): return textResponder == nil && !model.selection.isEmpty
        case #selector(copyItems(_:)), #selector(cut(_:)): return textResponder != nil || !model.selection.isEmpty
        case #selector(paste(_:)): return textResponder != nil || model.canPaste
        case #selector(toggleDual): menuItem.state = PaneState.shared.dual ? .on : .off; return true
        case #selector(switchPane), #selector(comparePanes): return PaneState.shared.dual || menuItem.action == #selector(comparePanes)
        case #selector(copyToOther), #selector(moveToOther): return PaneState.shared.dual && !model.selection.isEmpty
        case #selector(addToBasket): return textResponder == nil && !model.selection.isEmpty
        case #selector(toggleAutoUpdate): menuItem.state = Updater.shared.automatic ? .on : .off; return true
        case #selector(hidden): menuItem.state = model.showHidden ? .on : .off; return true
        case #selector(icons): menuItem.state = model.layout == .icons ? .on : .off; return true
        case #selector(details): menuItem.state = model.layout == .details ? .on : .off; return true
        default: return true
        }
    }
}

if CommandLine.arguments.contains("--self-test") {
    // Exercise file operations only inside a disposable temporary directory.
    let fm = FileManager.default
    let savedClipboard = NSPasteboard.general.pasteboardItems?.map { item in
        item.types.compactMap { type -> (NSPasteboard.PasteboardType, Data)? in
            guard let data = item.data(forType: type) else { return nil }; return (type, data)
        }
    } ?? []
    defer {
        NSPasteboard.general.clearContents()
        let restored = savedClipboard.map { pairs -> NSPasteboardItem in
            let item = NSPasteboardItem(); for (type, data) in pairs { item.setData(data, forType: type) }; return item
        }
        NSPasteboard.general.writeObjects(restored)
    }
    let root = canonicalURL(fm.temporaryDirectory).appendingPathComponent("FileGlade-test-\(UUID().uuidString)")
    func root2(_ fm: FileManager, _ name: String) -> URL { let url = root.appendingPathComponent("extra-" + name); try? fm.createDirectory(at: url, withIntermediateDirectories: true); return url }
    do {
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let alpha = root.appendingPathComponent("Alpha.txt")
        try Data("hello".utf8).write(to: alpha)
        try Data("hidden".utf8).write(to: root.appendingPathComponent(".secret"))
        let folder = root.appendingPathComponent("Folder")
        try fm.createDirectory(at: folder, withIntermediateDirectories: false)
        let m = Explorer(); m.navigate(root)
        precondition(m.entries.count == (m.showHidden ? 3 : 2))
        precondition(m.visible.first?.isDirectory == true)
        m.query = "ALPHA"; precondition(m.visible.count == 1); m.query = ""
        m.selection = [alpha]; m.copy(); m.paste()
        precondition(fm.fileExists(atPath: root.appendingPathComponent("Alpha copy 2.txt").path), "Copy failed: selected=\(m.chosen.map(\.name)), entries=\(m.entries.map { $0.url.absoluteString }), source=\(alpha.absoluteString), error=\(m.failure ?? "none"), clipboard=\(NSPasteboard.general.types ?? [])")
        m.selection = [alpha]; m.copy(cut: true); m.navigate(folder); m.paste()
        precondition(fm.fileExists(atPath: folder.appendingPathComponent("Alpha.txt").path))
        precondition(!fm.fileExists(atPath: alpha.path))
        // Conflict handling (self-test keeps both) and undo of copies and moves.
        let beta = root.appendingPathComponent("Beta.txt"); try Data("beta".utf8).write(to: beta)
        m.navigate(root); m.selection = [beta]; m.copy(); m.navigate(folder); m.paste(); m.paste()
        precondition(fm.fileExists(atPath: folder.appendingPathComponent("Beta.txt").path) && fm.fileExists(atPath: folder.appendingPathComponent("Beta copy 2.txt").path), "Conflict keep-both failed")
        precondition(TransferManager.shared.undoLast().isEmpty && !fm.fileExists(atPath: folder.appendingPathComponent("Beta copy 2.txt").path), "Undo second copy failed")
        precondition(TransferManager.shared.undoLast().isEmpty && !fm.fileExists(atPath: folder.appendingPathComponent("Beta.txt").path), "Undo first copy failed")
        m.navigate(folder); m.selection = [folder.appendingPathComponent("Alpha.txt")]; m.copy(cut: true); m.navigate(root); m.paste()
        precondition(fm.fileExists(atPath: root.appendingPathComponent("Alpha.txt").path), "Move failed")
        precondition(TransferManager.shared.undoLast().isEmpty && fm.fileExists(atPath: folder.appendingPathComponent("Alpha.txt").path), "Undo move failed")
        m.navigate(folder)
        m.back(); precondition(m.directory.path == root.path)
        m.forward(); precondition(m.directory.path == folder.path)
        m.up(); precondition(m.directory.path == root.path)
        m.showHidden = true; m.refresh(); precondition(m.entries.contains { $0.name == ".secret" })
        m.order = .name; m.ascending = false; precondition(m.visible.first?.isDirectory == true)
        // Speed: opening, filtering and sorting a folder of 5,000 files must stay quick.
        let big = root2(fm, "big")
        for i in 0..<5000 { fm.createFile(atPath: big.appendingPathComponent("file \(i).txt").path, contents: Data()) }
        let started = Date()
        m.navigate(big); m.query = "99"; let filteredCount = m.visible.count; m.query = ""; m.order = .size; m.order = .name
        let elapsed = Date().timeIntervalSince(started)
        fputs("Timing: 5,000-file folder opened, filtered and re-sorted in \(String(format: "%.2f", elapsed)) s (\(filteredCount) matched)\n", stderr)
        precondition(elapsed < 3.0 && m.visible.count == 5000, "Large folder is too slow")
        m.navigate(root)
        // Automation: a rule moves matching old files and is reversible.
        let inbox = root2(fm, "inbox"), sorted = root2(fm, "sorted")
        let invoice = inbox.appendingPathComponent("invoice march.pdf"); try Data("pdf".utf8).write(to: invoice)
        try fm.setAttributes([.modificationDate: Date().addingTimeInterval(-3 * 86_400)], ofItemAtPath: invoice.path)
        var rule = AutomationRule(); rule.name = "Invoices"; rule.folder = inbox.path
        rule.conditions = [RuleCondition(kind: .nameContains, text: "invoice", number: 0), RuleCondition(kind: .extensionIs, text: "pdf", number: 0)]
        rule.actions = [RuleAction(kind: .moveTo, text: sorted.path), RuleAction(kind: .addTag, text: "Billing")]
        let plan = AutomationEngine.preview([rule]); precondition(plan.count == 1, "Rule preview failed")
        precondition(AutomationEngine.run(plan, title: "test").done == 1 && fm.fileExists(atPath: sorted.appendingPathComponent("invoice march.pdf").path), "Rule run failed")
        precondition(AutomationEngine.tags(of: sorted.appendingPathComponent("invoice march.pdf")).contains("Billing"), "Tagging failed")
        precondition(TransferManager.shared.undoLast().isEmpty && fm.fileExists(atPath: invoice.path), "Undo of rule failed")
        // Backup round trip.
        let suite = UserDefaults(suiteName: "fileglade.selftest")!; suite.removePersistentDomain(forName: "fileglade.selftest")
        suite.set("windows", forKey: "shortcutProfile")
        let restored = UserDefaults(suiteName: "fileglade.selftest2")!; restored.removePersistentDomain(forName: "fileglade.selftest2")
        let restoredCount = try Backup.restore(Backup.export(from: suite), into: restored)
        precondition(restoredCount >= 1 && restored.string(forKey: "shortcutProfile") == "windows", "Backup round trip failed")
        suite.removePersistentDomain(forName: "fileglade.selftest"); restored.removePersistentDomain(forName: "fileglade.selftest2")
        // Treemap rectangles stay inside the area and add up.
        let boxes = Treemap.layout([50, 30, 20], in: CGRect(x: 0, y: 0, width: 100, height: 100))
        precondition(abs(boxes.reduce(0) { $0 + $1.width * $1.height } - 10_000) < 1, "Treemap failed")
        // Keyboard layouts: Mac keeps Return = rename; Windows uses Enter = open and F2 = rename.
        let menu = NSMenu(), sub = NSMenu()
        let renameItem = NSMenuItem(title: "Rename…", action: Selector(("rename")), keyEquivalent: "\r"); renameItem.keyEquivalentModifierMask = []
        let openItem = NSMenuItem(title: "Open Selected", action: Selector(("openSelected")), keyEquivalent: "o"); openItem.keyEquivalentModifierMask = [.command, .shift]
        sub.addItem(renameItem); sub.addItem(openItem)
        let root = NSMenuItem(title: "File", action: nil, keyEquivalent: ""); root.submenu = sub; menu.addItem(root)
        ShortcutStore.shared.apply(to: menu)
        precondition(renameItem.keyEquivalent == "\r" && openItem.keyEquivalent == "o")
        ShortcutStore.shared.profile = .windows; ShortcutStore.shared.apply(to: menu)
        precondition(renameItem.keyEquivalent == String(UnicodeScalar(0xF705)!) && openItem.keyEquivalent == "\r", "Windows layout failed")
        ShortcutStore.shared.profile = .mac; ShortcutStore.shared.apply(to: menu)
        precondition(renameItem.keyEquivalent == "\r" && openItem.keyEquivalent == "o", "Mac layout restore failed")
        // Archives (bsdtar) and folder comparison.
        let archiveSource = root2(fm, "arc-src"); try Data("zip me".utf8).write(to: archiveSource.appendingPathComponent("note.txt"))
        let zipURL = archiveSource.deletingLastPathComponent().appendingPathComponent("sample.zip")
        let ditto = Process(); ditto.executableURL = URL(fileURLWithPath: "/usr/bin/ditto"); ditto.arguments = ["-c", "-k", archiveSource.path, zipURL.path]; try ditto.run(); ditto.waitUntilExit()
        let listing = try ArchiveTool.list(zipURL)
        precondition(listing.contains { $0.path.hasSuffix("note.txt") }, "Archive listing failed")
        let extracted = try ArchiveTool.extract(zipURL, paths: nil, into: zipURL.deletingLastPathComponent())
        precondition(fm.fileExists(atPath: extracted.appendingPathComponent("note.txt").path), "Archive extraction failed")
        let leftDir = root2(fm, "cmp-left"), rightDir = root2(fm, "cmp-right")
        try Data("a".utf8).write(to: leftDir.appendingPathComponent("only-left.txt")); try Data("b".utf8).write(to: rightDir.appendingPathComponent("only-right.txt"))
        try Data("same".utf8).write(to: leftDir.appendingPathComponent("both.txt")); try fm.copyItem(at: leftDir.appendingPathComponent("both.txt"), to: rightDir.appendingPathComponent("both.txt"))
        let rows = FolderCompare.compare(left: leftDir, right: rightDir, deep: false)
        precondition(rows.first { $0.relative == "only-left.txt" }?.status == .leftOnly && rows.first { $0.relative == "only-right.txt" }?.status == .rightOnly && rows.first { $0.relative == "both.txt" }?.status == .same, "Folder compare failed")
        // Advanced rename: regular expression, case and spaces.
        var rename = RenameOptions(); rename.useRegex = true; rename.find = "(\\d+)"; rename.replace = "#$1"; rename.caseMode = .upper; rename.spaces = .underscore
        precondition(rename.name(for: URL(fileURLWithPath: "/tmp/report 2024 final.txt"), index: 0) == "REPORT_#2024_FINAL.txt", "Advanced rename failed")
        precondition(Updater.isNewer("2.10", than: "2.9") && Updater.isNewer("3.0", than: "2.9.9") && !Updater.isNewer("2.0", than: "2.0.0") && !Updater.isNewer("1.9", than: "2.0"), "Version comparison failed")
        NSPasteboard.general.clearContents()
        print("PASS: enumeration, folders-first sorting, search, copy collision handling, cut/paste, history, parent navigation, hidden files")
    } catch { fputs("Self-test failed: \(error)\n", stderr); exit(1) }
}

// One-time import of settings, stars, labels and activity from the app's former name (Windowpane).
if !Prefs.selfTest, UserDefaults.standard.object(forKey: "migratedFromWindowpane") == nil {
    if let old = UserDefaults.standard.persistentDomain(forName: "local.windowpane.explorer") {
        for (key, value) in old where !key.hasPrefix("NSWindow Frame") {
            let newKey = key.replacingOccurrences(of: "Windowpane.", with: "FileGlade.")
            if UserDefaults.standard.object(forKey: newKey) == nil { UserDefaults.standard.set(value, forKey: newKey) }
        }
    }
    UserDefaults.standard.set(true, forKey: "migratedFromWindowpane")
}

if !CommandLine.arguments.contains("--self-test") {
let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.run()

}
