import Foundation
import UniformTypeIdentifiers
import Darwin

// Keep identity consistent across /var and /private/var and directory URL suffixes.
func stableFileURL(_ url: URL) -> URL {
    if let resolved = realpath(url.path, nil) {
        defer { free(resolved) }
        return URL(fileURLWithPath: String(cString: resolved), isDirectory: false)
    }
    return URL(fileURLWithPath: url.path, isDirectory: false)
}

struct FileItem: Identifiable, Hashable, Sendable {
    let url: URL
    let isDirectory: Bool
    let size: Int64
    let modified: Date
    let kind: String
    /// True for an iCloud file that is not downloaded to this iPhone yet.
    let isCloudOnly: Bool
    var id: URL { url }
    var name: String { url.lastPathComponent }
    var icon: String {
        if isDirectory { return "folder.fill" }
        guard let type = UTType(filenameExtension: url.pathExtension) else { return "doc.fill" }
        if type.conforms(to: .image) { return "photo.fill" }
        if type.conforms(to: .movie) { return "film.fill" }
        if type.conforms(to: .audio) { return "waveform" }
        if type.conforms(to: .pdf) { return "doc.richtext.fill" }
        if type.conforms(to: .archive) { return "archivebox.fill" }
        return "doc.text.fill"
    }
    init(_ url: URL) {
        self.url = stableFileURL(url)
        let values = try? url.resourceValues(forKeys: [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey, .ubiquitousItemDownloadingStatusKey])
        isCloudOnly = values?.ubiquitousItemDownloadingStatus == .notDownloaded
        let attrs = (try? FileManager.default.attributesOfItem(atPath: url.path)) ?? [:]
        isDirectory = values?.isDirectory ?? (attrs[.type] as? FileAttributeType == .typeDirectory)
        size = Int64(values?.fileSize ?? (attrs[.size] as? NSNumber)?.intValue ?? 0)
        modified = values?.contentModificationDate ?? attrs[.modificationDate] as? Date ?? .distantPast
        kind = isDirectory ? "Folder" : UTType(filenameExtension: url.pathExtension)?.localizedDescription ?? "File"
    }
}

enum FileIssue: LocalizedError {
    case invalidName, insideItself, missingFolder
    var errorDescription: String? {
        switch self {
        case .invalidName: return "Enter a name without / or : characters."
        case .insideItself: return "A folder cannot be copied or moved into itself."
        case .missingFolder: return "The original folder is unavailable. Open its location again before restoring this item."
        }
    }
}

struct DeletedItem: Identifiable, Codable, Sendable {
    let id: UUID
    let name: String
    let originalParent: URL
    let parentBookmark: Data?
    let deletedAt: Date
    func container(in trash: URL) -> URL { trash.appendingPathComponent(id.uuidString, isDirectory: true) }
    func payload(in trash: URL) -> URL { container(in: trash).appendingPathComponent("Payload", isDirectory: true).appendingPathComponent(name) }
}

enum FileOperations {
    static func list(_ folder: URL, hidden: Bool) throws -> [FileItem] {
        var result: [FileItem] = []
        try coordinateRead(folder) { url in
            result = try FileManager.default.contentsOfDirectory(at: url, includingPropertiesForKeys: [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey], options: hidden ? [] : [.skipsHiddenFiles]).map(FileItem.init)
        }
        return result
    }
    static func validateName(_ name: String) throws {
        if name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || [".", ".."].contains(name) || name.contains("/") || name.contains(":") || name.contains("\0") { throw FileIssue.invalidName }
    }
    static func newFolder(_ name: String, in folder: URL) throws -> URL {
        try validateName(name)
        let target = folder.appendingPathComponent(name)
        try coordinateWrite(target) { try FileManager.default.createDirectory(at: $0, withIntermediateDirectories: false) }
        return stableFileURL(target)
    }
    static func rename(_ item: URL, to name: String) throws -> URL {
        try validateName(name)
        let target = item.deletingLastPathComponent().appendingPathComponent(name)
        if item.path == target.path { return item }
        return try transfer(item, into: target.deletingLastPathComponent(), moving: true, exactName: name)
    }
    static func uniqueURL(_ requested: URL) -> URL {
        let fm = FileManager.default
        if !fm.fileExists(atPath: requested.path) { return requested }
        let ext = requested.pathExtension
        let base = requested.deletingPathExtension().lastPathComponent
        var n = 2
        while true {
            let name = "\(base) copy \(n)" + (ext.isEmpty ? "" : ".\(ext)")
            let url = requested.deletingLastPathComponent().appendingPathComponent(name)
            if !fm.fileExists(atPath: url.path) { return url }; n += 1
        }
    }
    static func transfer(_ source: URL, into folder: URL, moving: Bool, exactName: String? = nil) throws -> URL {
        let fm = FileManager.default
        let src = source.resolvingSymlinksInPath().standardizedFileURL.path
        let dst = folder.resolvingSymlinksInPath().standardizedFileURL.path
        if FileItem(source).isDirectory && (src == dst || dst.hasPrefix(src + "/")) { throw FileIssue.insideItself }
        if moving && source.deletingLastPathComponent().path == folder.path && exactName == nil { return source }
        let requested = folder.appendingPathComponent(exactName ?? source.lastPathComponent)
        let target = exactName == nil ? uniqueURL(requested) : requested
        var coordinationError: NSError?
        var operationError: Error?
        let coordinator = NSFileCoordinator(filePresenter: nil)
        if moving {
            coordinator.coordinate(writingItemAt: source, options: .forMoving, writingItemAt: target, options: [], error: &coordinationError) { src, dst in
                do { try fm.moveItem(at: src, to: dst); coordinator.item(at: src, didMoveTo: dst) } catch { operationError = error }
            }
        } else {
            coordinator.coordinate(readingItemAt: source, options: [], writingItemAt: target, options: [], error: &coordinationError) { src, dst in
                do { try fm.copyItem(at: src, to: dst) } catch { operationError = error }
            }
        }
        if let error = operationError ?? coordinationError { throw error }
        return stableFileURL(target)
    }
    static func recycle(_ source: URL, trash: URL) throws {
        let fm = FileManager.default
        let parent = source.deletingLastPathComponent()
        let bookmark = try? parent.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil)
        let record = DeletedItem(id: UUID(), name: source.lastPathComponent, originalParent: parent, parentBookmark: bookmark, deletedAt: Date())
        let container = record.container(in: trash)
        let payloadFolder = container.appendingPathComponent("Payload", isDirectory: true)
        try fm.createDirectory(at: payloadFolder, withIntermediateDirectories: true)
        try JSONEncoder().encode(record).write(to: container.appendingPathComponent("record.json"), options: .atomic)
        _ = try transfer(source, into: payloadFolder, moving: true, exactName: record.name)
    }
    static func deletedItems(in trash: URL) -> [DeletedItem] {
        let children = (try? FileManager.default.contentsOfDirectory(at: trash, includingPropertiesForKeys: nil)) ?? []
        return children.compactMap { folder in
            guard let data = try? Data(contentsOf: folder.appendingPathComponent("record.json")), let record = try? JSONDecoder().decode(DeletedItem.self, from: data), FileManager.default.fileExists(atPath: record.payload(in: trash).path) else { return nil }
            return record
        }.sorted { $0.deletedAt > $1.deletedAt }
    }
    static func restore(_ item: DeletedItem, trash: URL) throws {
        var parent = item.originalParent
        if let bookmark = item.parentBookmark {
            var stale = false
            if let resolved = try? URL(resolvingBookmarkData: bookmark, options: .withoutUI, relativeTo: nil, bookmarkDataIsStale: &stale) { parent = resolved }
        }
        let access = parent.startAccessingSecurityScopedResource()
        defer { if access { parent.stopAccessingSecurityScopedResource() } }
        guard FileManager.default.fileExists(atPath: parent.path) else { throw FileIssue.missingFolder }
        _ = try transfer(item.payload(in: trash), into: parent, moving: true)
        try? FileManager.default.removeItem(at: item.container(in: trash))
    }
    private static func coordinateRead(_ url: URL, body: (URL) throws -> Void) throws {
        var error: NSError?; var failure: Error?
        NSFileCoordinator(filePresenter: nil).coordinate(readingItemAt: url, options: [], error: &error) { coordinated in do { try body(coordinated) } catch { failure = error } }
        if let issue = failure ?? error { throw issue }
    }
    private static func coordinateWrite(_ url: URL, body: (URL) throws -> Void) throws {
        var error: NSError?; var failure: Error?
        NSFileCoordinator(filePresenter: nil).coordinate(writingItemAt: url, options: [], error: &error) { coordinated in do { try body(coordinated) } catch { failure = error } }
        if let issue = failure ?? error { throw issue }
    }
}
