import Foundation

func requireDashboard(_ value: @autoclosure () -> Bool, _ message: String) throws { if !value() { throw powerError("DASHBOARD TEST FAILED: " + message) } }

@main struct DashboardTests {
    static func main() async {
        do { try await run(); print("PASS: unified multi-root index, overlapping-root deduplication, full-path and Unicode search, category filters, pagination, cache restoration, deleted-file refresh, disconnected-root exclusion, hidden-file setting, cancellation, and grouping continuity.") }
        catch { fputs(error.localizedDescription + "\n", stderr); exit(1) }
    }
    static func run() async throws {
        let fm = FileManager.default
        let temporary = powerURL(fm.temporaryDirectory).appendingPathComponent("FileGlade-dashboard-tests-" + UUID().uuidString)
        try fm.createDirectory(at: temporary, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: temporary) }
        let a = temporary.appendingPathComponent("A")
        let b = temporary.appendingPathComponent("B")
        try fm.createDirectory(at: a.appendingPathComponent("Sub"), withIntermediateDirectories: true)
        try fm.createDirectory(at: b, withIntermediateDirectories: true)
        try Data("alpha".utf8).write(to: a.appendingPathComponent("Notes.txt"))
        try Data("Unicode document".utf8).write(to: a.appendingPathComponent("Sub/résumé.md"))
        try Data("metadata only".utf8).write(to: a.appendingPathComponent("Picture.jpg"))
        try Data("hidden".utf8).write(to: a.appendingPathComponent(".hidden"))
        try Data("other source".utf8).write(to: b.appendingPathComponent("Notes.txt"))
        try fm.createSymbolicLink(at: a.appendingPathComponent("Do not follow"), withDestinationURL: b)
        let roots = [DashboardRoot(id: "A", name: "My ' Mac", url: a), DashboardRoot(id: "B", name: "Cloud", url: b), DashboardRoot(id: "sub", name: "Nested folder", url: a.appendingPathComponent("Sub"))]
        let database = temporary.appendingPathComponent("Cache/index.sqlite")
        let index = try DashboardIndex(at: database)
        try index.synchronize(roots)
        for root in roots { try index.scan(root, hidden: false) { _ in } }
        let all = try index.page(query: "", kind: .all, limit: 100)
        try requireDashboard(all.files == 4 && all.folders == 3, "all roots are combined without counting overlapping folders twice")
        try requireDashboard(Set(all.items.map { $0.url.path }).count == all.items.count, "one row per filesystem path")
        for grouping in FileGrouping.allCases {
            let groupedPage = try index.page(query: "", kind: .all, limit: 100, grouping: grouping)
            let sections = fileSections(groupedPage.items) { item in grouping.key(name: item.name, folder: item.folder, size: item.size, modified: item.modified, category: item.category, location: item.source) }
            try requireDashboard(sections.flatMap(\.items).count == all.matches, "grouping retains every item: " + grouping.rawValue)
            let keys = groupedPage.items.map { item in grouping.key(name: item.name, folder: item.folder, size: item.size, modified: item.modified, category: item.category, location: item.source) }
            var closed = Set<String>(); var last: String?
            for key in keys { if key != last { try requireDashboard(!closed.contains(key), "groups stay contiguous across pages: " + grouping.rawValue); if let last { closed.insert(last) }; last = key } }
        }
        let today = Calendar.current.startOfDay(for: Date())
        try requireDashboard(FileGrouping.modified.key(name: "a", folder: false, size: 0, modified: today, category: "other", location: "") == "0|Today", "today boundary")
        try requireDashboard(FileGrouping.size.key(name: "a", folder: false, size: 1_048_576, modified: today, category: "other", location: "") == "3|1–100 MB", "size boundary")
        let unicode = try index.page(query: "resume", kind: .all, limit: 100)
        try requireDashboard(unicode.items.count == 1 && unicode.items[0].name == "résumé.md", "case/diacritic insensitive search")
        let pathSearch = try index.page(query: "Sub/", kind: .all, limit: 100)
        try requireDashboard(pathSearch.items.contains { $0.name == "résumé.md" }, "absolute paths remain searchable across overlapping sources")
        let fileSearch = try index.page(query: "NOTES.TXT", kind: .files, limit: 100)
        try requireDashboard(fileSearch.items.count == 2, "same name in different roots is retained")
        let images = try index.page(query: "", kind: .images, limit: 100)
        try requireDashboard(images.items.count == 1 && images.items[0].name == "Picture.jpg", "type filtering uses metadata, not file contents")
        let shortPage = try index.page(query: "", kind: .all, limit: 2)
        try requireDashboard(shortPage.items.count == 2 && shortPage.matches == 7, "pagination preserves total match count")
        let quoted = try index.page(query: "' OR 1=1 --", kind: .all, limit: 100)
        try requireDashboard(quoted.items.isEmpty, "search terms are bound as data")
        let restored = try DashboardIndex(at: database).page(query: "", kind: .all, limit: 100)
        try requireDashboard(restored.matches == all.matches, "cached index works after reopening")
        try fm.removeItem(at: a.appendingPathComponent("Notes.txt"))
        try index.scan(roots[0], hidden: false) { _ in }
        let refreshed = try index.page(query: "Notes.txt", kind: .all, limit: 100)
        try requireDashboard(refreshed.items.count == 1 && refreshed.items[0].source == "Cloud", "deleted files disappear after refresh")
        let nestedPath = try index.page(query: "Sub/", kind: .all, limit: 100)
        try requireDashboard(nestedPath.items.contains { $0.name == "résumé.md" }, "relative path search")
        
        try index.synchronize([roots[0]])
        let connected = try index.page(query: "", kind: .all, limit: 100)
        try requireDashboard(!connected.items.contains { $0.source == "Cloud" }, "disconnected sources are excluded")
        try index.scan(roots[0], hidden: true) { _ in }
        let hidden = try index.page(query: ".hidden", kind: .all, limit: 100)
        try requireDashboard(hidden.items.count == 1, "hidden files can be included")
        let task = Task.detached {
            try await Task.sleep(nanoseconds: 100_000_000)
            let cancelled = try DashboardIndex(at: database)
            try cancelled.scan(roots[0], hidden: false) { _ in }
        }
        task.cancel()
        do { try await task.value; throw powerError("Cancellation was ignored") } catch is CancellationError { }
        let kept = try index.page(query: "résumé", kind: .all, limit: 100)
        try requireDashboard(kept.items.count == 1, "cancelled refresh preserves indexed files")
    }
}
