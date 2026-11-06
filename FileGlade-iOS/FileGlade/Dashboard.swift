import SwiftUI
import Foundation
import SQLite3
import UniformTypeIdentifiers


enum FileGrouping: String, CaseIterable, Identifiable {
    case none = "None", name = "Name", kind = "Kind", modified = "Date modified", size = "Size", location = "Location"
    var id: String { rawValue }
    func key(name: String, folder: Bool, size: Int64, modified: Date, category: String, location: String) -> String {
        switch self {
        case .none: return ""
        case .name:
            let first = String(name.prefix(1)).uppercased()
            if first >= "A" && first <= "H" { return "0|A–H" }
            if first >= "I" && first <= "P" { return "1|I–P" }
            if first >= "Q" && first <= "Z" { return "2|Q–Z" }
            return "3|Other names"
        case .kind: return folder ? "0|Folders" : "1|" + category.capitalized
        case .modified:
            let day = Calendar.current.startOfDay(for: Date())
            if modified >= day { return "0|Today" }
            if modified >= Calendar.current.date(byAdding: .day, value: -1, to: day)! { return "1|Yesterday" }
            if modified >= Calendar.current.date(byAdding: .day, value: -7, to: day)! { return "2|Previous 7 days" }
            if modified >= Calendar.current.date(byAdding: .day, value: -30, to: day)! { return "3|Previous 30 days" }
            return "4|Older"
        case .size:
            if folder { return "0|Folders" }
            if size == 0 { return "1|Empty files" }
            if size < 1_048_576 { return "2|Under 1 MB" }
            if size < 104_857_600 { return "3|1–100 MB" }
            if size < 1_073_741_824 { return "4|100 MB–1 GB" }
            return "5|1 GB and larger"
        case .location: return location
        }
    }
    var sqlOrder: String {
        switch self {
        case .none: return ""
        case .name: return "CASE WHEN upper(substr(items.name,1,1)) BETWEEN 'A' AND 'H' THEN 0 WHEN upper(substr(items.name,1,1)) BETWEEN 'I' AND 'P' THEN 1 WHEN upper(substr(items.name,1,1)) BETWEEN 'Q' AND 'Z' THEN 2 ELSE 3 END,"
        case .kind: return "CASE WHEN items.folder=1 THEN '0' ELSE '1' || items.category END,"
        case .modified:
            let day = Calendar.current.startOfDay(for: Date())
            let offsets = [0, -1, -7, -30]
            return "CASE " + offsets.enumerated().map { index, offset in "WHEN items.modified>=\(Calendar.current.date(byAdding: .day, value: offset, to: day)!.timeIntervalSince1970) THEN \(index)" }.joined(separator: " ") + " ELSE 4 END,"
        case .size: return "CASE WHEN items.folder=1 THEN 0 WHEN items.size=0 THEN 1 WHEN items.size<1048576 THEN 2 WHEN items.size<104857600 THEN 3 WHEN items.size<1073741824 THEN 4 ELSE 5 END,"
        case .location: return "roots.name COLLATE NOCASE,"
        }
    }
}
struct FileSection<Item>: Identifiable {
    let id: String
    let items: [Item]
    var title: String { id.components(separatedBy: "|").last ?? id }
}
func fileSections<Item>(_ items: [Item], key: (Item) -> String) -> [FileSection<Item>] {
    let groups = Dictionary(grouping: items, by: key)
    return groups.keys.sorted { $0.localizedStandardCompare($1) == .orderedAscending }.map { FileSection(id: $0, items: groups[$0]!) }
}
func fileCategory(_ url: URL) -> String {
    guard let type = UTType(filenameExtension: url.pathExtension) else { return "other" }
    if type.conforms(to: .image) { return "images" }
    if type.conforms(to: .movie) { return "video" }
    if type.conforms(to: .audio) { return "audio" }
    if type.conforms(to: .archive) { return "archives" }
    if PowerFilter.documents.accepts(PowerFile(url)) { return "documents" }
    return "other"
}
struct FileGladeAboutView: View {
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        VStack(spacing: 18) {
            Image(systemName: "folder.fill").font(.system(size: 60)).foregroundStyle(.blue)
            Text("FileGlade").font(.title.bold())
            Text("V2.0").font(.headline)
            Text("All your files. One clear view.").foregroundStyle(.secondary)
            Text("Designed by: Akshaya Simha")
            Text("ZIP support: ZIPFoundation · MIT License").font(.caption).foregroundStyle(.secondary)
            Button("Done") { dismiss() }.buttonStyle(.borderedProminent)
        }.padding(32).frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

struct DashboardRoot: Identifiable, Hashable, Sendable {
    let id: String
    let name: String
    let url: URL
}
struct DashboardItem: Identifiable, Sendable {
    let id: String
    let url: URL
    let name: String
    let folder: Bool
    let size: Int64
    let modified: Date
    let source: String
    let relativePath: String
    let category: String
}
struct DashboardPage: Sendable {
    var items: [DashboardItem] = []
    var matches = 0
    var files = 0
    var folders = 0
    var bytes: Int64 = 0
    var lastScan: Date?
    var issues: [String] = []
}
struct DashboardOverview: Sendable {
    var recent: [DashboardItem] = []
    var downloads: [DashboardItem] = []
    var large: [DashboardItem] = []
    var storage: [(category: String, count: Int, bytes: Int64)] = []
}
enum DashboardDate: String, CaseIterable, Identifiable {
    case any = "Any date", day = "Last 24 hours", week = "Last 7 days", month = "Last 30 days", year = "Last year"
    var id: String { rawValue }
    var days: Int? { switch self { case .any: return nil; case .day: return 1; case .week: return 7; case .month: return 30; case .year: return 365 } }
}
enum DashboardSize: String, CaseIterable, Identifiable {
    case any = "Any size", mb1 = "1 MB or more", mb10 = "10 MB or more", mb100 = "100 MB or more", gb1 = "1 GB or more"
    var id: String { rawValue }
    var bytes: Int64 { switch self { case .any: return 0; case .mb1: return 1_048_576; case .mb10: return 10_485_760; case .mb100: return 104_857_600; case .gb1: return 1_073_741_824 } }
}
struct DashboardFilters: Equatable, Sendable {
    var date: DashboardDate = .any
    var size: DashboardSize = .any
    var rootID: String? = nil
    var isActive: Bool { date != .any || size != .any || rootID != nil }
}
struct SavedSearch: Codable, Identifiable, Equatable {
    var id = UUID()
    var name: String
    var query: String
    var kind: String
    var date: String
    var size: String
    var rootID: String?
}
final class SavedSearchStore: ObservableObject {
    static let shared = SavedSearchStore()
    static let builtIns: [SavedSearch] = [
        SavedSearch(id: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!, name: "PDFs modified this week", query: ".pdf", kind: "Files", date: DashboardDate.week.rawValue, size: DashboardSize.any.rawValue, rootID: nil),
        SavedSearch(id: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!, name: "Large files (100 MB+)", query: "", kind: "Files", date: DashboardDate.any.rawValue, size: DashboardSize.mb100.rawValue, rootID: nil),
        SavedSearch(id: UUID(uuidString: "00000000-0000-0000-0000-000000000003")!, name: "Images this month", query: "", kind: "Images", date: DashboardDate.month.rawValue, size: DashboardSize.any.rawValue, rootID: nil),
        SavedSearch(id: UUID(uuidString: "00000000-0000-0000-0000-000000000004")!, name: "Recent downloads", query: "/downloads/", kind: "Files", date: DashboardDate.week.rawValue, size: DashboardSize.any.rawValue, rootID: nil)
    ]
    @Published var searches: [SavedSearch] = {
        guard let data = UserDefaults.standard.data(forKey: "savedSearches"), let list = try? JSONDecoder().decode([SavedSearch].self, from: data) else { return [] }
        return list
    }() { didSet { if let data = try? JSONEncoder().encode(searches) { UserDefaults.standard.set(data, forKey: "savedSearches") } } }
    /// A smart folder the user chose in the sidebar; the dashboard applies it, then clears it.
    @Published var pending: SavedSearch?
    var smartFolders: [SavedSearch] { Self.builtIns + searches }
    func isBuiltIn(_ search: SavedSearch) -> Bool { Self.builtIns.contains { $0.id == search.id } }
    func remove(_ search: SavedSearch) { searches.removeAll { $0.id == search.id } }
}
enum DashboardKind: String, CaseIterable, Identifiable {
    case all = "Everything", files = "Files", folders = "Folders", documents = "Documents", images = "Images", video = "Video", audio = "Audio", archives = "Archives"
    var id: String { rawValue }
}

// A disk-backed metadata index. Only the visible page is loaded into memory.
// File contents are never read by dashboard indexing.
final class DashboardIndex {
    private var db: OpaquePointer?
    private static let transient = unsafeBitCast(-1, to: sqlite3_destructor_type.self)
    init(at url: URL) throws {
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        guard sqlite3_open_v2(url.path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nil) == SQLITE_OK else { throw powerError("The file dashboard cache could not be opened.") }
        sqlite3_busy_timeout(db, 5_000)
        try execute("PRAGMA journal_mode=WAL")
        try execute("CREATE TABLE IF NOT EXISTS roots (id TEXT PRIMARY KEY, name TEXT NOT NULL, path TEXT NOT NULL, active INTEGER NOT NULL DEFAULT 1, last_scan REAL NOT NULL DEFAULT 0, issue TEXT NOT NULL DEFAULT '')")
        try execute("CREATE TABLE IF NOT EXISTS items (path TEXT PRIMARY KEY, root_id TEXT NOT NULL, relative_path TEXT NOT NULL, name TEXT NOT NULL, search TEXT NOT NULL, folder INTEGER NOT NULL, size INTEGER NOT NULL, modified REAL NOT NULL, category TEXT NOT NULL, version TEXT NOT NULL)")
        try execute("CREATE INDEX IF NOT EXISTS items_by_root ON items(root_id)")
        try execute("CREATE INDEX IF NOT EXISTS items_by_name ON items(folder DESC, name COLLATE NOCASE)")
    }
    deinit { sqlite3_close(db) }
    private func error() -> Error { powerError("Dashboard indexing: " + String(cString: sqlite3_errmsg(db))) }
    private func prepare(_ sql: String) throws -> OpaquePointer {
        var statement: OpaquePointer?
        guard sqlite3_prepare_v2(db, sql, -1, &statement, nil) == SQLITE_OK, let statement else { throw error() }
        return statement
    }
    private func bind(_ text: String, to index: Int32, in statement: OpaquePointer) { _ = sqlite3_bind_text(statement, index, text, -1, Self.transient) }
    private func text(_ statement: OpaquePointer, _ column: Int32) -> String { sqlite3_column_text(statement, column).map { String(cString: $0) } ?? "" }
    private func execute(_ sql: String, parameters: [String] = []) throws {
        let statement = try prepare(sql); defer { sqlite3_finalize(statement) }
        for (i, value) in parameters.enumerated() { bind(value, to: Int32(i + 1), in: statement) }
        let result = sqlite3_step(statement)
        guard result == SQLITE_DONE || result == SQLITE_ROW else { throw error() }
    }
    static func folded(_ string: String) -> String { string.folding(options: [.caseInsensitive, .diacriticInsensitive], locale: Locale(identifier: "en_US_POSIX")) }
    static func uniqueRoots(_ roots: [DashboardRoot]) -> [DashboardRoot] {
        var seen = Set<String>()
        return roots.filter { seen.insert(powerURL($0.url).path).inserted }
    }
    func synchronize(_ roots: [DashboardRoot]) throws {
        try execute("BEGIN IMMEDIATE")
        do {
            try execute("UPDATE roots SET active=0")
            for root in roots {
                let path = powerURL(root.url).path
                let lookup = try prepare("SELECT path FROM roots WHERE id=?")
                bind(root.id, to: 1, in: lookup)
                let changed = sqlite3_step(lookup) == SQLITE_ROW && text(lookup, 0) != path
                sqlite3_finalize(lookup)
                if changed { try execute("DELETE FROM items WHERE root_id=?", parameters: [root.id]) }
                try execute("INSERT INTO roots(id,name,path,active) VALUES(?,?,?,1) ON CONFLICT(id) DO UPDATE SET name=excluded.name,path=excluded.path,active=1", parameters: [root.id, root.name, path])
            }
            try execute("COMMIT")
        } catch { try? execute("ROLLBACK"); throw error }
    }
    private func category(_ url: URL, directory: Bool) -> String {
        if directory { return "folders" }
        guard let type = UTType(filenameExtension: url.pathExtension) else { return "other" }
        if type.conforms(to: .image) { return "images" }
        if type.conforms(to: .movie) { return "video" }
        if type.conforms(to: .audio) { return "audio" }
        if type.conforms(to: .archive) { return "archives" }
        if PowerFilter.documents.accepts(PowerFile(url)) { return "documents" }
        return "other"
    }
    func scan(_ root: DashboardRoot, hidden: Bool, progress: @Sendable (String) -> Void) throws {
        let fm = FileManager.default
        let base = powerURL(root.url)
        let version = UUID().uuidString
        // Test root access before treating an empty enumeration as an empty folder.
        _ = try fm.contentsOfDirectory(at: base, includingPropertiesForKeys: nil, options: [.skipsSubdirectoryDescendants])
        let keys: Set<URLResourceKey> = [.isDirectoryKey, .isSymbolicLinkKey, .fileSizeKey, .contentModificationDateKey]
        let options: FileManager.DirectoryEnumerationOptions = hidden ? [.skipsPackageDescendants] : [.skipsPackageDescendants, .skipsHiddenFiles]
        var skipped = 0
        var unreadable: [String] = []
        guard let walk = fm.enumerator(at: base, includingPropertiesForKeys: Array(keys), options: options, errorHandler: { url, _ in
            skipped += 1
            if url.path.hasPrefix(base.path + "/") { unreadable.append(String(url.path.dropFirst(base.path.count + 1))) }
            return true
        }) else { throw powerError("Could not read \(root.name).") }
        let insert = try prepare("INSERT INTO items(path,root_id,relative_path,name,search,folder,size,modified,category,version) VALUES(?,?,?,?,?,?,?,?,?,?) ON CONFLICT(path) DO UPDATE SET root_id=excluded.root_id,relative_path=excluded.relative_path,name=excluded.name,search=excluded.search,folder=excluded.folder,size=excluded.size,modified=excluded.modified,category=excluded.category,version=excluded.version")
        defer { sqlite3_finalize(insert) }
        var categoryCache: [String: String] = [:]
        var count = 0
        var inTransaction = false
        func add(_ url: URL, values: URLResourceValues, relative: String) throws {
            try Task.checkCancellation()
            if !inTransaction { try execute("BEGIN IMMEDIATE"); inTransaction = true }
            sqlite3_reset(insert); sqlite3_clear_bindings(insert)
            let directory = values.isDirectory ?? false
            let name = relative.isEmpty ? root.name : url.lastPathComponent
            let ext = url.pathExtension.lowercased()
            let kind: String
            if directory { kind = "folders" }
            else if let cached = categoryCache[ext] { kind = cached }
            else { kind = category(url, directory: false); categoryCache[ext] = kind }
            for (i, value) in [url.standardizedFileURL.path, root.id, relative, name, Self.folded(name + " " + relative + " " + root.name + " " + url.path)].enumerated() { bind(value, to: Int32(i + 1), in: insert) }
            sqlite3_bind_int(insert, 6, directory ? 1 : 0)
            sqlite3_bind_int64(insert, 7, directory ? 0 : Int64(values.fileSize ?? 0))
            sqlite3_bind_double(insert, 8, (values.contentModificationDate ?? .distantPast).timeIntervalSince1970)
            bind(kind, to: 9, in: insert); bind(version, to: 10, in: insert)
            guard sqlite3_step(insert) == SQLITE_DONE else { throw error() }
            count += 1
            if count % 500 == 0 { try execute("COMMIT"); inTransaction = false; progress("\(root.name) · \(count.formatted()) items") }
        }
        do {
            try add(base, values: base.resourceValues(forKeys: keys), relative: "")
            for case let url as URL in walk {
                try Task.checkCancellation()
                guard let values = try? url.resourceValues(forKeys: keys) else { skipped += 1; if url.path.hasPrefix(base.path + "/") { unreadable.append(String(url.path.dropFirst(base.path.count + 1))) }; continue }
                // FileManager enumerators do not descend into symlinks. Calling
                // skipDescendants on one can skip the following real directory.
                if values.isSymbolicLink == true { continue }
                guard url.path.hasPrefix(base.path + "/") else { skipped += 1; continue }
                let relative = String(url.path.dropFirst(base.path.count + 1))
                // Never walk app-private data inside the home folder: ~/Library (Mail, Messages, Safari…)
                // and the Apple Music library trigger extra macOS privacy prompts and are not user files.
                if root.id == "home" && (relative == "Library" || relative == "Music/Music") { walk.skipDescendants(); continue }
                #if os(macOS)
                // Without Full Disk Access, reading these folders makes macOS show a permission prompt for each.
                // Skip them in the background; they are included once access is granted.
                if root.id == "home" && !FullDiskAccess.granted && ["Desktop", "Documents", "Downloads"].contains(relative) { walk.skipDescendants(); continue }
                #endif
                try add(url, values: values, relative: relative)
            }
            if inTransaction { try execute("COMMIT"); inTransaction = false }
            // Retain older metadata only below unreadable paths. Deleted entries in
            // readable folders are still removed from the index, never from disk.
            for relative in unreadable {
                let escaped = relative.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "%", with: "\\%").replacingOccurrences(of: "_", with: "\\_")
                try execute("UPDATE items SET version=? WHERE root_id=? AND (relative_path=? OR relative_path LIKE ? ESCAPE '\\')", parameters: [version, root.id, relative, escaped + "/%"])
            }
            try execute("DELETE FROM items WHERE root_id=? AND version<>?", parameters: [root.id, version])
            try execute("UPDATE roots SET last_scan=?,issue=? WHERE id=?", parameters: [String(Date().timeIntervalSince1970), skipped == 0 ? "" : "\(root.name): \(skipped) folders or items could not be read; some cached entries may be older.", root.id])
            progress("\(root.name) · \(count.formatted()) items")
        } catch { if inTransaction { try? execute("ROLLBACK") }; throw error }
    }
    private func rows(_ extra: String, limit: Int) throws -> [DashboardItem] {
        let rows = try prepare("SELECT items.path,items.relative_path,items.name,items.folder,items.size,items.modified,roots.name,roots.path,items.category FROM items JOIN roots ON items.root_id=roots.id WHERE roots.active=1 AND items.folder=0 AND items.relative_path NOT LIKE 'Library/%' AND " + extra + " LIMIT ?")
        defer { sqlite3_finalize(rows) }
        sqlite3_bind_int64(rows, 1, Int64(limit))
        var result: [DashboardItem] = []
        while sqlite3_step(rows) == SQLITE_ROW {
            let relative = text(rows, 1)
            let base = URL(fileURLWithPath: text(rows, 7), isDirectory: true)
            result.append(DashboardItem(id: text(rows, 0), url: relative.isEmpty ? base : base.appendingPathComponent(relative), name: text(rows, 2), folder: false, size: sqlite3_column_int64(rows, 4), modified: Date(timeIntervalSince1970: sqlite3_column_double(rows, 5)), source: text(rows, 6), relativePath: relative, category: text(rows, 8)))
        }
        return result
    }
    func overview() throws -> DashboardOverview {
        var result = DashboardOverview()
        result.recent = try rows("items.modified<=\(Date().timeIntervalSince1970 + 60) ORDER BY items.modified DESC", limit: 6)
        result.downloads = try rows("(items.relative_path LIKE 'Downloads/%') ORDER BY items.modified DESC", limit: 6)
        result.large = try rows("items.size>=104857600 ORDER BY items.size DESC", limit: 6)
        let statement = try prepare("SELECT items.category,COUNT(*),COALESCE(SUM(items.size),0) FROM items JOIN roots ON items.root_id=roots.id WHERE roots.active=1 AND items.folder=0 AND items.relative_path NOT LIKE 'Library/%' GROUP BY items.category ORDER BY SUM(items.size) DESC")
        defer { sqlite3_finalize(statement) }
        while sqlite3_step(statement) == SQLITE_ROW { result.storage.append((text(statement, 0), Int(sqlite3_column_int64(statement, 1)), sqlite3_column_int64(statement, 2))) }
        return result
    }
    func recordIssue(_ root: DashboardRoot, _ message: String) { try? execute("UPDATE roots SET issue=? WHERE id=?", parameters: [root.name + ": " + message, root.id]) }
    func page(query: String, kind: DashboardKind, limit: Int, grouping: FileGrouping = .none, filters: DashboardFilters = DashboardFilters()) throws -> DashboardPage {
        var page = DashboardPage()
        let totals = try prepare("SELECT COALESCE(SUM(CASE WHEN folder=0 THEN 1 ELSE 0 END),0),COALESCE(SUM(CASE WHEN folder=1 THEN 1 ELSE 0 END),0),COALESCE(SUM(size),0) FROM items JOIN roots ON items.root_id=roots.id WHERE roots.active=1")
        if sqlite3_step(totals) == SQLITE_ROW { page.files = Int(sqlite3_column_int64(totals, 0)); page.folders = Int(sqlite3_column_int64(totals, 1)); page.bytes = sqlite3_column_int64(totals, 2) }
        sqlite3_finalize(totals)
        var condition = "roots.active=1 AND instr(items.search,?)>0"
        switch kind {
        case .all: break
        case .files: condition += " AND items.folder=0"
        case .folders: condition += " AND items.folder=1"
        default: condition += " AND items.category='\(kind.rawValue.lowercased())'"
        }
        if let days = filters.date.days { condition += " AND items.modified>=\(Date().addingTimeInterval(-Double(days) * 86_400).timeIntervalSince1970)" }
        if filters.size != .any { condition += " AND items.folder=0 AND items.size>=\(filters.size.bytes)" }
        if filters.rootID != nil { condition += " AND items.root_id=?" }
        let count = try prepare("SELECT COUNT(*) FROM items JOIN roots ON items.root_id=roots.id WHERE " + condition)
        bind(Self.folded(query), to: 1, in: count)
        if let root = filters.rootID { bind(root, to: 2, in: count) }
        if sqlite3_step(count) == SQLITE_ROW { page.matches = Int(sqlite3_column_int64(count, 0)) }
        sqlite3_finalize(count)
        let rows = try prepare("SELECT items.path,items.relative_path,items.name,items.folder,items.size,items.modified,roots.name,roots.path,items.category FROM items JOIN roots ON items.root_id=roots.id WHERE " + condition + " ORDER BY " + grouping.sqlOrder + " items.folder DESC,items.name COLLATE NOCASE,items.path LIMIT ?")
        bind(Self.folded(query), to: 1, in: rows)
        if let root = filters.rootID { bind(root, to: 2, in: rows); sqlite3_bind_int64(rows, 3, Int64(limit)) } else { sqlite3_bind_int64(rows, 2, Int64(limit)) }
        while sqlite3_step(rows) == SQLITE_ROW {
            let relative = text(rows, 1)
            let base = URL(fileURLWithPath: text(rows, 7), isDirectory: true)
            page.items.append(DashboardItem(id: text(rows, 0), url: relative.isEmpty ? base : base.appendingPathComponent(relative), name: text(rows, 2), folder: sqlite3_column_int(rows, 3) == 1, size: sqlite3_column_int64(rows, 4), modified: Date(timeIntervalSince1970: sqlite3_column_double(rows, 5)), source: text(rows, 6), relativePath: relative, category: text(rows, 8)))
        }
        sqlite3_finalize(rows)
        let roots = try prepare("SELECT last_scan,issue FROM roots WHERE active=1")
        var dates: [Double] = []
        while sqlite3_step(roots) == SQLITE_ROW { let date = sqlite3_column_double(roots, 0); if date > 0 { dates.append(date) }; let issue = text(roots, 1); if !issue.isEmpty { page.issues.append(issue) } }
        sqlite3_finalize(roots)
        page.lastScan = dates.min().map { Date(timeIntervalSince1970: $0) }
        return page
    }
}

@MainActor final class DashboardModel: ObservableObject {
    @Published var page = DashboardPage()
    @Published var overview = DashboardOverview()
    @Published var scanning = false
    @Published var status = "Preparing your files…"
    @Published var error: String?
    @Published var query = ""
    @Published var kind: DashboardKind = .all
    @Published var grouping: FileGrouping = .none
    @Published var hidden = false
    @Published var paused = false
    @Published var filters = DashboardFilters()
    func saveCurrent(rootName: String?) {
        var parts: [String] = []
        if !query.isEmpty { parts.append("“\(query)”") }
        if kind != .all { parts.append(kind.rawValue) }
        if filters.date != .any { parts.append(filters.date.rawValue) }
        if filters.size != .any { parts.append(filters.size.rawValue) }
        if let rootName { parts.append(rootName) }
        guard !parts.isEmpty else { return }
        let entry = SavedSearch(name: parts.joined(separator: " · "), query: query, kind: kind.rawValue, date: filters.date.rawValue, size: filters.size.rawValue, rootID: filters.rootID)
        let store = SavedSearchStore.shared
        store.searches.removeAll { $0.name == entry.name }; store.searches.insert(entry, at: 0); store.searches = Array(store.searches.prefix(20))
    }
    func apply(_ search: SavedSearch) {
        query = search.query; kind = DashboardKind(rawValue: search.kind) ?? .all
        filters = DashboardFilters(date: DashboardDate(rawValue: search.date) ?? .any, size: DashboardSize(rawValue: search.size) ?? .any, rootID: search.rootID)
    }
    func clearFilters() { query = ""; kind = .all; filters = DashboardFilters() }
    private var limit = 500
    private var worker: Task<Void, Never>?
    private var queryTask: Task<Void, Never>?
    private var queryTicket = UUID()
    private var roots: [DashboardRoot] = []
    private var pendingRoots: [DashboardRoot]?
    private let database: URL
    init(database: URL? = nil) {
        self.database = database ?? FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("FileGladeDashboard/index-v1.sqlite")
    }
    deinit { worker?.cancel(); queryTask?.cancel() }
    func refresh(_ sources: [DashboardRoot], force: Bool = false) {
        let sources = DashboardIndex.uniqueRoots(sources)
        if paused && !force && sources == roots { return }
        if scanning {
            if sources != roots || force { pendingRoots = sources; worker?.cancel() }
            return
        }
        roots = sources; scanning = true; paused = false; error = nil
        status = "Discovering your files…"
        let database = database; let hidden = hidden
        worker = Task.detached(priority: .utility) { [weak self] in
            do {
                let index = try DashboardIndex(at: database)
                try index.synchronize(sources)
                await self?.reloadResults()
                for source in sources {
                    try Task.checkCancellation()
                    await self?.setStatus("Indexing \(source.name)…")
                    do {
                        try index.scan(source, hidden: hidden) { [weak self] message in Task { @MainActor in self?.status = "Indexing " + message; self?.reloadResults() } }
                    } catch is CancellationError { throw CancellationError() }
                    catch { index.recordIssue(source, error.localizedDescription) }
                }
            } catch is CancellationError { }
            catch { await self?.setError(error.localizedDescription) }
            await self?.finished()
        }
    }
    private func setStatus(_ text: String) { status = text }
    private func setError(_ text: String) { error = text }
    private func finished() {
        scanning = false; worker = nil; status = paused ? "Indexing paused" : "Up to date"
        reloadResults()
        if let pendingRoots { self.pendingRoots = nil; refresh(pendingRoots) }
    }
    func stop() { pendingRoots = nil; paused = true; worker?.cancel(); status = "Pausing indexing…" }
    func searchChanged() { limit = 500; reloadResults(debounce: true) }
    func loadMore() { limit += 500; reloadResults() }
    func reloadResults(debounce: Bool = false) {
        queryTask?.cancel()
        let ticket = UUID(); queryTicket = ticket
        let database = database; let query = query; let kind = kind; let limit = limit; let grouping = grouping; let filters = filters
        queryTask = Task { [weak self] in
            if debounce { try? await Task.sleep(nanoseconds: 180_000_000) }
            guard !Task.isCancelled else { return }
            do {
                let (page, overview) = try await Task.detached(priority: .utility) { () -> (DashboardPage, DashboardOverview) in
                    let index = try DashboardIndex(at: database)
                    return (try index.page(query: query, kind: kind, limit: limit, grouping: grouping, filters: filters), query.isEmpty && !filters.isActive ? try index.overview() : DashboardOverview())
                }.value
                guard self?.queryTicket == ticket, !Task.isCancelled else { return }
                self?.page = page
                self?.overview = overview
            } catch { if self?.queryTicket == ticket { self?.error = error.localizedDescription } }
        }
    }
}

struct UnifiedDashboardView: View {
    let roots: [DashboardRoot]
    var unavailable: [String] = []
    let onOpen: (URL) -> Void
    let onReveal: (URL) -> Void
    var onConnect: (() -> Void)? = nil
    @StateObject private var model = DashboardModel()
    @ObservedObject private var library = FileLibrary.shared
    @ObservedObject private var store = SavedSearchStore.shared
    @Environment(\.scenePhase) private var scenePhase
    @State private var visible = false
    @State private var showStatus = false
    @State private var showAbout = false
    private let timer = Timer.publish(every: 30, on: .main, in: .common).autoconnect()
    private var signature: String { roots.map { $0.id + "|" + $0.url.path }.sorted().joined(separator: "\n") }
    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            HStack(spacing: 12) {
                Label("Search all files", systemImage: "magnifyingglass").labelStyle(.iconOnly).foregroundStyle(.secondary)
                TextField("Search all files and folders", text: $model.query).textFieldStyle(.plain)
                if !model.query.isEmpty { Button { model.query = "" } label: { Image(systemName: "xmark.circle.fill") }.buttonStyle(.plain).foregroundStyle(.secondary) }
                Picker("Show", selection: $model.kind) { ForEach(DashboardKind.allCases) { Text($0.rawValue).tag($0) } }.pickerStyle(.menu).fixedSize()
            }.padding(.horizontal, 14).padding(.vertical, 8)
            HStack(spacing: 10) {
                Picker("Date", selection: $model.filters.date) { ForEach(DashboardDate.allCases) { Text($0.rawValue).tag($0) } }.pickerStyle(.menu).fixedSize()
                Picker("Size", selection: $model.filters.size) { ForEach(DashboardSize.allCases) { Text($0.rawValue).tag($0) } }.pickerStyle(.menu).fixedSize()
                Picker("Location", selection: $model.filters.rootID) {
                    Text("All locations").tag(String?.none)
                    ForEach(DashboardIndex.uniqueRoots(roots)) { Text($0.name).tag(Optional($0.id)) }
                }.pickerStyle(.menu).fixedSize()
                if model.filters.isActive || model.kind != .all || !model.query.isEmpty { Button("Clear") { model.clearFilters() }.font(.caption) }
                Spacer()
                Button { model.saveCurrent(rootName: roots.first { $0.id == model.filters.rootID }?.name) } label: { Label("Save search", systemImage: "bookmark") }.font(.caption)
                    .disabled(model.query.isEmpty && model.kind == .all && !model.filters.isActive)
                Menu {
                    if store.searches.isEmpty { Text("No saved searches") }
                    ForEach(store.searches) { search in
                        Button(search.name) { model.apply(search) }
                    }
                    if !store.searches.isEmpty {
                        Divider()
                        Menu("Delete saved search") { ForEach(store.searches) { search in Button(search.name, role: .destructive) { store.remove(search) } } }
                    }
                } label: { Label("Saved searches", systemImage: "bookmark.fill") }.menuStyle(.borderlessButton).fixedSize()
            }.padding(.horizontal, 14).padding(.bottom, 6)
            Text("Searching \(model.filters.rootID == nil ? DashboardIndex.uniqueRoots(roots).map(\.name).joined(separator: ", ") : (roots.first { $0.id == model.filters.rootID }?.name ?? "selected location"))").font(.caption2).foregroundStyle(.secondary).lineLimit(1).padding(.horizontal, 16).padding(.bottom, 6)
            HStack {
                Menu { Picker("Group by", selection: $model.grouping) { ForEach(FileGrouping.allCases) { Text($0.rawValue).tag($0) } } } label: { Label("Group by", systemImage: "square.grid.3x1.below.line.grid.1x2") }
                Text(model.grouping.rawValue).font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("About") { showAbout = true }.font(.caption)
            }.padding(.horizontal, 16).padding(.vertical, 4)
            Divider()
            if model.query.isEmpty && model.kind == .all && !model.filters.isActive && model.grouping == .none && !model.overview.recent.isEmpty { overviewPanel }
            if model.page.items.isEmpty {
                VStack(spacing: 14) {
                    Image(systemName: model.scanning ? "folder.badge.gearshape" : "folder").font(.system(size: 45, weight: .light)).foregroundStyle(.blue)
                    Text(model.scanning ? "Finding your files…" : model.query.isEmpty ? "Your files, together" : "No matching items").font(.title3.weight(.semibold))
                    Text(model.scanning ? "Files appear here as folders are indexed." : model.query.isEmpty ? "Everything accessible to FileGlade appears on this dashboard." : "Try another name or file type.").font(.subheadline).foregroundStyle(.secondary).multilineTextAlignment(.center)
                }.padding(30).frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                List {
                    ForEach(fileSections(model.page.items) { item in model.grouping.key(name: item.name, folder: item.folder, size: item.size, modified: item.modified, category: item.category, location: item.source) }) { group in
                        Section { ForEach(group.items) { item in dashboardRow(item) } } header: { if model.grouping != .none { Text(group.title) } }
                    }
                    if model.page.items.count < model.page.matches {
                        Button("Show 500 more · \(model.page.matches.formatted()) matching items") { model.loadMore() }.padding(.vertical, 8)
                    }
                }.listStyle(.plain)
            }
            Divider()
            HStack(spacing: 8) {
                if model.scanning { ProgressView().controlSize(.small) }
                Text(model.scanning ? model.status : "\(model.page.matches.formatted()) items\(model.query.isEmpty && model.kind == .all ? "" : " matching")\(model.paused ? " · indexing paused" : "")").font(.caption).foregroundStyle(.secondary).lineLimit(1)
                Spacer()
                Button { showStatus.toggle() } label: { Image(systemName: "info.circle") }.buttonStyle(.plain).accessibilityLabel("Indexing and access status")
            }.padding(.horizontal, 16).frame(height: 35)
            if showStatus { statusPanel }
        }
        .task(id: signature) { visible = true; model.refresh(roots) }
        .onAppear { visible = true; applyPending() }
        .onChange(of: store.pending) { _ in applyPending() }
        .onDisappear { visible = false }
        .onChange(of: model.query) { _ in model.searchChanged() }
        .onChange(of: model.kind) { _ in model.searchChanged() }
        .onChange(of: model.grouping) { _ in model.searchChanged() }
        .onChange(of: model.filters) { _ in model.searchChanged() }
        .sheet(isPresented: $showAbout) { FileGladeAboutView() }
        .onReceive(timer) { _ in if visible && scenePhase == .active { model.refresh(roots) } }
        .onChange(of: scenePhase) { phase in if visible && phase == .active { model.refresh(roots) } }
    }
    private var header: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("All your files").font(.title2.bold())
                    Text("\(model.page.files.formatted()) files · \(model.page.folders.formatted()) folders").font(.subheadline).foregroundStyle(.secondary)
                }
                Spacer()
                Button { model.refresh(roots, force: true) } label: { Image(systemName: "arrow.clockwise") }.buttonStyle(.borderless).accessibilityLabel("Refresh all files")
                if let onConnect { Button(action: onConnect) { Image(systemName: "gearshape") }.buttonStyle(.borderless).accessibilityLabel("File access settings") }
            }
            #if os(iOS)
            if roots.count <= 1, let onConnect {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Include files from outside FileGlade").font(.subheadline.weight(.semibold))
                    Text("iOS requires one-time folder access. Allow a folder and all its subfolders will appear here automatically on future visits.").font(.caption).foregroundStyle(.secondary)
                    Button("Allow folder access", action: onConnect).font(.subheadline.weight(.semibold))
                }.padding(12).frame(maxWidth: .infinity, alignment: .leading).background(Color.blue.opacity(0.07), in: RoundedRectangle(cornerRadius: 12))
            }
            #endif
            if !unavailable.isEmpty || !model.page.issues.isEmpty || model.error != nil {
                Button { showStatus = true } label: { Label("Some folders need attention", systemImage: "exclamationmark.circle").font(.caption) }.buttonStyle(.plain).foregroundStyle(.orange)
            }
        }.padding(18)
    }
    private func applyPending() { if let search = store.pending { model.apply(search); store.pending = nil } }
    private var cardBackground: Color {
        #if os(macOS)
        Color(nsColor: .controlBackgroundColor)
        #else
        Color(.secondarySystemBackground)
        #endif
    }
    private var overviewPanel: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(alignment: .top, spacing: 12) {
                overviewCard("Recent files", "clock", model.overview.recent) { Text($0.modified.formatted(.relative(presentation: .named))) }
                if !model.overview.downloads.isEmpty { overviewCard("Downloads", "arrow.down.circle", model.overview.downloads) { Text($0.modified.formatted(.relative(presentation: .named))) } }
                if !model.overview.large.isEmpty { overviewCard("Large files", "externaldrive", model.overview.large) { Text(fileBytes($0.size)) } }
                storageCard
            }.padding(.horizontal, 16).padding(.vertical, 12)
        }.frame(height: 200).background(Color.primary.opacity(0.025))
    }
    private func overviewCard<Detail: View>(_ title: String, _ icon: String, _ items: [DashboardItem], @ViewBuilder detail: @escaping (DashboardItem) -> Detail) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Label(title, systemImage: icon).font(.subheadline.weight(.semibold))
            ForEach(items) { item in
                Button { library.opened(item.url); onOpen(item.url) } label: {
                    HStack(spacing: 8) {
                        Text(item.name).font(.caption).lineLimit(1).truncationMode(.middle)
                        Spacer(minLength: 6)
                        detail(item).font(.caption2).foregroundStyle(.secondary)
                    }.contentShape(Rectangle())
                }.buttonStyle(.plain).help(item.url.path)
                .contextMenu { Button("Show in folder") { onReveal(item.url) }; Button("Copy path") { copyFileText(item.url.path) } }
            }
            Spacer(minLength: 0)
        }.padding(12).frame(width: 290, height: 176, alignment: .topLeading).background(cardBackground, in: RoundedRectangle(cornerRadius: 10))
    }
    private var storageCard: some View {
        let total = max(model.overview.storage.reduce(0) { $0 + $1.bytes }, 1)
        return VStack(alignment: .leading, spacing: 6) {
            Label("Storage by type", systemImage: "chart.bar").font(.subheadline.weight(.semibold))
            ForEach(model.overview.storage.prefix(6), id: \.category) { row in
                VStack(alignment: .leading, spacing: 2) {
                    HStack { Text(row.category.capitalized).font(.caption); Spacer(); Text("\(fileBytes(row.bytes)) · \(row.count.formatted())").font(.caption2).foregroundStyle(.secondary) }
                    GeometryReader { proxy in Capsule().fill(Color.accentColor.opacity(0.7)).frame(width: max(3, proxy.size.width * CGFloat(Double(row.bytes) / Double(total)))) }.frame(height: 4)
                }
            }
            Spacer(minLength: 0)
        }.padding(12).frame(width: 290, height: 176, alignment: .topLeading).background(cardBackground, in: RoundedRectangle(cornerRadius: 10))
    }
    private func dashboardRow(_ item: DashboardItem) -> some View {
        HStack(spacing: 13) {
            Button { library.opened(item.url); onOpen(item.url) } label: {
                HStack(spacing: 13) {
                    Image(systemName: item.folder ? "folder.fill" : "doc.text.fill").font(.system(size: 27)).foregroundStyle(item.folder ? Color.yellow : .blue).frame(width: 34)
                    VStack(alignment: .leading, spacing: 5) {
                        HStack(spacing: 6) { Text(item.name).font(.subheadline).foregroundStyle(.primary).lineLimit(1); if let color = library.color(item.url) { Circle().fill(color).frame(width: 6, height: 6) }; if library.isStarred(item.url) { Image(systemName: "star.fill").font(.caption2).foregroundStyle(.yellow) } }
                        Text(item.relativePath.isEmpty ? item.source : item.source + " / " + item.relativePath).font(.caption2).foregroundStyle(.secondary).lineLimit(1).truncationMode(.middle)
                    }
                    Spacer(minLength: 0)
                    if !item.folder { Text(fileBytes(item.size)).font(.caption2).foregroundStyle(.secondary) }
                    if item.folder { Image(systemName: "chevron.right").font(.caption).foregroundStyle(.tertiary) }
                }.contentShape(Rectangle())
            }.buttonStyle(.plain)
        }.padding(.vertical, 3)
        .contextMenu {
            Button("Open") { library.opened(item.url); onOpen(item.url) }
            Button("Show in folder") { onReveal(item.url) }
            Button(library.isStarred(item.url) ? "Remove star" : "Star item") { library.toggleStar(item.url) }
            Button("Copy path") { copyFileText(item.url.path) }
        }
    }
    private var statusPanel: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 10) {
                Text("Automatic file dashboard").font(.headline)
                Text("Connected folders are indexed in the background and refreshed automatically while the dashboard is open. Search includes every indexed folder together. App bundles, symbolic links, and hidden items are excluded by default.").font(.caption).foregroundStyle(.secondary)
                ForEach(DashboardIndex.uniqueRoots(roots)) { root in Label(root.name, systemImage: "folder").font(.caption) }
                ForEach(unavailable, id: \.self) { Text($0 + ": access needs to be renewed.").font(.caption).foregroundStyle(.orange) }
                ForEach(model.page.issues, id: \.self) { Text($0).font(.caption).foregroundStyle(.orange) }
                if let error = model.error { Text(error).font(.caption).foregroundStyle(.red) }
                if let date = model.page.lastScan { Text("Last indexed \(date.formatted())").font(.caption2).foregroundStyle(.secondary) }
                #if os(macOS)
                if !FullDiskAccess.granted {
                    Text("Desktop, Documents, Downloads, iCloud Drive, cloud folders and external drives are left out of All files until FileGlade has Full Disk Access. This avoids repeated permission prompts. Grant it once, then reopen FileGlade.").font(.caption).foregroundStyle(.orange)
                    Button("Open Full Disk Access settings…") { FullDiskAccess.openSettings() }.font(.caption)
                } else { Text("Full Disk Access is on: no folder prompts.").font(.caption).foregroundStyle(.secondary) }
                #endif
                Toggle("Include hidden files", isOn: $model.hidden).font(.caption).onChange(of: model.hidden) { _ in model.refresh(roots, force: true) }
                if model.scanning { Button("Pause indexing") { model.stop() }.font(.caption) }
                if model.paused && !model.scanning { Button("Resume indexing") { model.refresh(roots, force: true) }.font(.caption) }
                #if os(iOS)
                Text("iOS does not give third-party apps silent access to other apps' private files. Only FileGlade's files and folders you allow can appear here.").font(.caption).foregroundStyle(.secondary)
                #endif
            }.padding(16).frame(maxWidth: .infinity, alignment: .leading)
        }.frame(maxHeight: 220)
    }
}

#if os(macOS)
enum FullDiskAccess {
    private static func readable(_ path: String) -> Bool? {
        guard let dir = opendir(path) else { return (errno == EPERM || errno == EACCES) ? false : nil }
        closedir(dir)
        return true
    }
    /// True when macOS has granted this app Full Disk Access. The probes are protected folders that
    /// fail silently, without showing a permission prompt.
    static var granted: Bool {
        readable(NSHomeDirectory() + "/Library/Safari") ?? readable("/Library/Application Support/com.apple.TCC") ?? false
    }
    static func openSettings() {
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_AllFiles") { NSWorkspace.shared.open(url) }
    }
}
func automaticMacDashboardRoots(volumes: [URL]) -> [DashboardRoot] {
    let fm = FileManager.default
    let home = fm.homeDirectoryForCurrentUser
    // Home covers ordinary personal folders; cloud providers live below Library,
    // which may be hidden, so they are discovered explicitly as well.
    var roots = [DashboardRoot(id: "home", name: "My Mac", url: home)]
    // iCloud Drive, cloud providers and external drives each trigger their own prompt without Full Disk Access.
    if !FullDiskAccess.granted { return roots }
    let cloudStorage = home.appendingPathComponent("Library/CloudStorage")
    if let providers = try? fm.contentsOfDirectory(at: cloudStorage, includingPropertiesForKeys: [.isDirectoryKey], options: .skipsHiddenFiles) {
        for provider in providers where (try? provider.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true { roots.append(DashboardRoot(id: "cloud:" + provider.lastPathComponent, name: provider.lastPathComponent, url: provider)) }
    }
    let iCloud = home.appendingPathComponent("Library/Mobile Documents/com~apple~CloudDocs")
    if fm.fileExists(atPath: iCloud.path) { roots.append(DashboardRoot(id: "icloud-drive", name: "iCloud Drive", url: iCloud)) }
    for volume in volumes where volume.path != "/" {
        let uuid = try? volume.resourceValues(forKeys: [.volumeUUIDStringKey]).volumeUUIDString
        roots.append(DashboardRoot(id: "volume:" + (uuid ?? volume.path), name: volume.lastPathComponent, url: volume))
    }
    return DashboardIndex.uniqueRoots(roots)
}
#endif
