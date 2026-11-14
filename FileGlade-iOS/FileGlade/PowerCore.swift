import Foundation
import CryptoKit
import PDFKit
import ImageIO
import UniformTypeIdentifiers
import Darwin

func powerURL(_ url: URL) -> URL {
    if let path = realpath(url.path, nil) { defer { free(path) }; return URL(fileURLWithPath: String(cString: path), isDirectory: false) }
    return URL(fileURLWithPath: url.path, isDirectory: false)
}
func powerError(_ message: String) -> NSError { NSError(domain: "FileGlade", code: 1, userInfo: [NSLocalizedDescriptionKey: message]) }
func fileBytes(_ count: Int64) -> String { ByteCountFormatter.string(fromByteCount: count, countStyle: .file) }

struct PowerFile: Identifiable, Hashable, Sendable {
    let url: URL
    let directory: Bool
    let size: Int64
    let modified: Date
    var id: URL { url }
    var name: String { url.lastPathComponent }
    init(_ url: URL) {
        self.url = powerURL(url)
        let v = try? url.resourceValues(forKeys: [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey])
        let attrs = (try? FileManager.default.attributesOfItem(atPath: url.path)) ?? [:]
        directory = v?.isDirectory ?? (attrs[.type] as? FileAttributeType == .typeDirectory)
        size = Int64(v?.fileSize ?? (attrs[.size] as? NSNumber)?.intValue ?? 0)
        modified = v?.contentModificationDate ?? attrs[.modificationDate] as? Date ?? .distantPast
    }
}
enum PowerFilter: String, CaseIterable, Identifiable {
    case all = "All files", documents = "Documents", images = "Images", video = "Video", audio = "Audio", archives = "Archives", folders = "Folders", large = "Large files", recent = "Last 7 days", starred = "Starred"
    var id: String { rawValue }
    func accepts(_ file: PowerFile, starred: Bool = false) -> Bool {
        accepts(directory: file.directory, size: file.size, modified: file.modified, ext: file.url.pathExtension, starred: starred)
    }
    /// The same test without building a PowerFile (which touches the disk), so big folders filter instantly.
    func accepts(directory: Bool, size: Int64, modified: Date, ext: String, starred: Bool = false) -> Bool {
        if self == .all { return true }
        if self == .folders { return directory }
        if self == .starred { return starred }
        if self == .recent { return modified > Date().addingTimeInterval(-7 * 86400) }
        if self == .large { return !directory && size >= 100_000_000 }
        guard !directory, let type = ExtensionTypes.type(ext) else { return false }
        let lower = ext.lowercased()
        switch self {
        case .documents: return type.conforms(to: .text) || type.conforms(to: .pdf) || type.conforms(to: .spreadsheet) || type.conforms(to: .presentation) || ["doc", "docx", "pages", "rtf"].contains(lower)
        case .images: return type.conforms(to: .image)
        case .video: return type.conforms(to: .movie)
        case .audio: return type.conforms(to: .audio)
        case .archives: return type.conforms(to: .archive) || ["zip", "rar", "7z", "gz", "tar"].contains(lower)
        default: return false
        }
    }
}

/// Looking up a file type is slow, and a folder has few distinct extensions, so results are remembered.
enum ExtensionTypes {
    private static let lock = NSLock()
    private static var types: [String: UTType?] = [:]
    private static var categories: [String: String] = [:]
    static func type(_ ext: String) -> UTType? {
        let key = ext.lowercased()
        lock.lock(); defer { lock.unlock() }
        if let known = types[key] { return known }
        let value = UTType(filenameExtension: key)
        types[key] = .some(value)
        return value
    }
    static func category(_ ext: String) -> String {
        let key = ext.lowercased()
        lock.lock(); if let known = categories[key] { lock.unlock(); return known }; lock.unlock()
        var result = "other"
        if let type = type(key) {
            if type.conforms(to: .image) { result = "images" }
            else if type.conforms(to: .movie) { result = "video" }
            else if type.conforms(to: .audio) { result = "audio" }
            else if type.conforms(to: .archive) { result = "archives" }
            else if PowerFilter.documents.accepts(directory: false, size: 0, modified: .distantPast, ext: key) { result = "documents" }
        }
        lock.lock(); categories[key] = result; lock.unlock()
        return result
    }
}

struct WorkspaceTab: Identifiable {
    let id = UUID()
    var url: URL
    var root: URL
    var history: [URL] = []
    var index = -1
    var title: String { url.lastPathComponent.isEmpty ? "Macintosh HD" : url.lastPathComponent }
}

enum RenameCase: String, CaseIterable, Identifiable, Codable, Sendable { case keep = "Keep case", lower = "lowercase", upper = "UPPERCASE", title = "Title Case", sentence = "Sentence case"; var id: String { rawValue } }
enum RenameSpaces: String, CaseIterable, Identifiable, Codable, Sendable { case keep = "Keep spaces", underscore = "Spaces to _", dash = "Spaces to -", remove = "Remove spaces"; var id: String { rawValue } }
enum RenameDate: String, CaseIterable, Identifiable, Codable, Sendable { case none = "No date", modified = "Date modified", created = "Date created", today = "Today's date"; var id: String { rawValue } }

struct RenameOptions: Equatable, Sendable, Codable {
    var find = ""
    var replace = ""
    var useRegex = false
    var prefix = ""
    var suffix = ""
    var caseMode: RenameCase = .keep
    var spaces: RenameSpaces = .keep
    var date: RenameDate = .none
    var dateAtStart = true
    var numbered = false
    var start = 1
    var digits = 3
    /// A readable message when the regular expression is invalid.
    var regexError: String? {
        guard useRegex, !find.isEmpty else { return nil }
        do { _ = try NSRegularExpression(pattern: find) ; return nil } catch { return "Invalid regular expression." }
    }
    private func dateText(for url: URL) -> String? {
        guard date != .none else { return nil }
        let value: Date?
        switch date {
        case .modified: value = try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate
        case .created: value = try? url.resourceValues(forKeys: [.creationDateKey]).creationDate
        default: value = Date()
        }
        guard let value else { return nil }
        let formatter = DateFormatter(); formatter.locale = Locale(identifier: "en_US_POSIX"); formatter.dateFormat = "yyyy-MM-dd"
        return formatter.string(from: value)
    }
    func name(for url: URL, index: Int) -> String {
        let isFolder = PowerFile(url).directory
        let ext = isFolder ? "" : url.pathExtension
        var stem = ext.isEmpty ? url.lastPathComponent : url.deletingPathExtension().lastPathComponent
        if !find.isEmpty {
            if useRegex {
                if let expression = try? NSRegularExpression(pattern: find) { stem = expression.stringByReplacingMatches(in: stem, range: NSRange(stem.startIndex..., in: stem), withTemplate: replace) }
            } else { stem = stem.replacingOccurrences(of: find, with: replace) }
        }
        switch spaces {
        case .keep: break
        case .underscore: stem = stem.replacingOccurrences(of: " ", with: "_")
        case .dash: stem = stem.replacingOccurrences(of: " ", with: "-")
        case .remove: stem = stem.replacingOccurrences(of: " ", with: "")
        }
        switch caseMode {
        case .keep: break
        case .lower: stem = stem.lowercased()
        case .upper: stem = stem.uppercased()
        case .title: stem = stem.capitalized
        case .sentence: stem = stem.prefix(1).uppercased() + stem.dropFirst().lowercased()
        }
        var result = prefix + stem + suffix
        if let day = dateText(for: url) { result = dateAtStart ? day + "_" + result : result + "_" + day }
        let counter = numbered ? "_" + String(format: "%0*d", max(1, min(digits, 8)), start + index) : ""
        return result + counter + (ext.isEmpty ? "" : "." + ext)
    }
}
/// Reusable naming presets, stored locally.
enum RenamePresets {
    static func all() -> [String: RenameOptions] {
        guard let data = UserDefaults.standard.data(forKey: "renamePresets"), let value = try? JSONDecoder().decode([String: RenameOptions].self, from: data) else { return [:] }
        return value
    }
    static func save(_ name: String, _ options: RenameOptions) {
        var current = all(); current[name] = options
        if let data = try? JSONEncoder().encode(current) { UserDefaults.standard.set(data, forKey: "renamePresets") }
    }
    static func delete(_ name: String) {
        var current = all(); current[name] = nil
        if let data = try? JSONEncoder().encode(current) { UserDefaults.standard.set(data, forKey: "renamePresets") }
    }
}
struct RenamePair: Identifiable, Hashable, Sendable {
    let source: URL
    let destination: URL
    var id: URL { source }
}
struct ScanInventory: Sendable {
    var files: [PowerFile] = []
    var directories: [PowerFile] = []
    var folders = 0
    var skipped = 0
    var truncated = false
    var total: Int64 { files.reduce(0) { $0 + $1.size } }
}
struct DuplicateSet: Identifiable, Sendable {
    let digest: String
    let files: [PowerFile]
    var id: String { digest }
    var recoverableBytes: Int64 { Int64(max(0, files.count - 1)) * (files.first?.size ?? 0) }
}
struct TextSnapshot: Sendable { let url: URL; let text: String; let digest: String? }

enum PowerFS {
    static func name(_ name: String) throws {
        guard !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, ![".", ".."].contains(name), !name.contains("/"), !name.contains(":"), !name.contains("\0") else { throw powerError("Choose a name without / or : characters.") }
    }
    static func unique(_ desired: URL) -> URL {
        if !FileManager.default.fileExists(atPath: desired.path) { return desired }
        let ext = desired.pathExtension
        let stem = desired.deletingPathExtension().lastPathComponent
        var n = 2
        while true {
            let next = desired.deletingLastPathComponent().appendingPathComponent(stem + " \(n)" + (ext.isEmpty ? "" : "." + ext))
            if !FileManager.default.fileExists(atPath: next.path) { return next }; n += 1
        }
    }
    static func read(_ url: URL, body: (URL) throws -> Void) throws {
        var coordination: NSError?; var operation: Error?
        NSFileCoordinator(filePresenter: nil).coordinate(readingItemAt: url, options: [], error: &coordination) { resolved in do { try body(resolved) } catch { operation = error } }
        if let error = operation ?? coordination { throw error }
    }
    static func write(_ url: URL, body: (URL) throws -> Void) throws {
        var coordination: NSError?; var operation: Error?
        NSFileCoordinator(filePresenter: nil).coordinate(writingItemAt: url, options: [], error: &coordination) { resolved in do { try body(resolved) } catch { operation = error } }
        if let error = operation ?? coordination { throw error }
    }
    static func move(_ source: URL, to destination: URL) throws {
        var coordination: NSError?; var operation: Error?
        let coordinator = NSFileCoordinator(filePresenter: nil)
        coordinator.coordinate(writingItemAt: source, options: .forMoving, writingItemAt: destination, options: [], error: &coordination) { a, b in
            do { try FileManager.default.moveItem(at: a, to: b); coordinator.item(at: a, didMoveTo: b) } catch { operation = error }
        }
        if let error = operation ?? coordination { throw error }
    }
    static func publish(_ source: URL, to target: URL) throws {
        let wrapper = target.deletingLastPathComponent().appendingPathComponent(".fileglade-output-" + UUID().uuidString, isDirectory: true)
        try write(wrapper) { try FileManager.default.createDirectory(at: $0, withIntermediateDirectories: false) }
        defer { try? FileManager.default.removeItem(at: wrapper) }
        let staged = wrapper.appendingPathComponent("Payload")
        try read(source) { try FileManager.default.copyItem(at: $0, to: staged) }
        try move(staged, to: target)
    }
    static func publishData(_ data: Data, to target: URL) throws {
        let temporary = FileManager.default.temporaryDirectory.appendingPathComponent("FileGlade-data-" + UUID().uuidString)
        try data.write(to: temporary, options: .withoutOverwriting)
        defer { try? FileManager.default.removeItem(at: temporary) }
        try publish(temporary, to: target)
    }
    static func planRename(_ urls: [URL], options: RenameOptions) throws -> [RenamePair] {
        let pairs = urls.enumerated().map { RenamePair(source: powerURL($0.element), destination: $0.element.deletingLastPathComponent().appendingPathComponent(options.name(for: $0.element, index: $0.offset))) }
        try validateRename(pairs)
        return pairs
    }
    static func validateRename(_ pairs: [RenamePair]) throws {
        guard !pairs.isEmpty else { throw powerError("Select files or folders before opening Batch Rename.") }
        let sources = Set(pairs.map { $0.source.path })
        var targets = Set<String>()
        for pair in pairs {
            try name(pair.destination.lastPathComponent)
            guard sources.filter({ $0 != pair.source.path }).allSatisfy({ !$0.hasPrefix(pair.source.path + "/") }) else { throw powerError("Rename a folder and its contents in separate batches.") }
            let folded = pair.destination.path.folding(options: [.caseInsensitive], locale: Locale(identifier: "en_US_POSIX"))
            guard targets.insert(folded).inserted else { throw powerError("Two items would get the same name: \(pair.destination.lastPathComponent).") }
            if pair.source.path != pair.destination.path && FileManager.default.fileExists(atPath: pair.destination.path) && !sources.contains(pair.destination.path) { throw powerError("A file already uses the name \(pair.destination.lastPathComponent).") }
        }
    }
    static func rename(_ pairs: [RenamePair]) throws -> [RenamePair] {
        try validateRename(pairs); try Task.checkCancellation()
        let changes = pairs.filter { $0.source.path != $0.destination.path }
        let staged = changes.map { RenamePair(source: $0.source, destination: $0.source.deletingLastPathComponent().appendingPathComponent(".fileglade-rename-" + UUID().uuidString)) }
        var stageCount = 0; var finishCount = 0
        do {
            for pair in staged { try move(pair.source, to: pair.destination); stageCount += 1 }
            for (i, pair) in changes.enumerated() { try move(staged[i].destination, to: pair.destination); finishCount += 1 }
        } catch {
            // Return completed destinations to their private stage before restoring originals.
            var recovery: [String] = []
            if finishCount > 0 { for i in (0..<finishCount).reversed() { do { try move(changes[i].destination, to: staged[i].destination) } catch { recovery.append(changes[i].destination.path) } } }
            if stageCount > 0 { for i in (0..<stageCount).reversed() { do { try move(staged[i].destination, to: changes[i].source) } catch { recovery.append(staged[i].destination.path) } } }
            if !recovery.isEmpty { throw powerError("Rename could not finish. Some items need manual recovery at:\n" + recovery.joined(separator: "\n")) }
            throw error
        }
        return changes.map { RenamePair(source: powerURL($0.destination), destination: $0.source) }
    }
    static func inventory(_ root: URL, hidden: Bool = false) throws -> ScanInventory {
        var report = ScanInventory()
        let keys: [URLResourceKey] = [.isDirectoryKey, .isRegularFileKey, .isSymbolicLinkKey, .isPackageKey, .fileSizeKey, .contentModificationDateKey, .isUbiquitousItemKey, .ubiquitousItemDownloadingStatusKey]
        let options: FileManager.DirectoryEnumerationOptions = hidden ? [.skipsPackageDescendants] : [.skipsPackageDescendants, .skipsHiddenFiles]
        guard let walk = FileManager.default.enumerator(at: root, includingPropertiesForKeys: keys, options: options, errorHandler: { _, _ in report.skipped += 1; return true }) else { throw powerError("This folder could not be scanned.") }
        var seen = 0
        for case let url as URL in walk {
            try Task.checkCancellation(); seen += 1
            if seen > 100_000 { report.truncated = true; break }
            guard let v = try? url.resourceValues(forKeys: Set(keys)) else { report.skipped += 1; continue }
            if v.isSymbolicLink == true { walk.skipDescendants(); report.skipped += 1; continue }
            if v.isDirectory == true { report.folders += 1; report.directories.append(PowerFile(url)); continue }
            if v.isRegularFile == true { report.files.append(PowerFile(url)) }
        }
        return report
    }
    static func digest(_ url: URL) throws -> String {
        var digest = ""
        try read(url) { coordinated in
            let before = PowerFile(coordinated)
            let handle = try FileHandle(forReadingFrom: coordinated); defer { try? handle.close() }
            var hasher = SHA256()
            while let chunk = try handle.read(upToCount: 1_048_576), !chunk.isEmpty { try Task.checkCancellation(); hasher.update(data: chunk) }
            let after = PowerFile(coordinated)
            guard before.size == after.size, before.modified == after.modified else { throw powerError("\(url.lastPathComponent) changed while being read. Please try again.") }
            digest = hasher.finalize().map { String(format: "%02x", $0) }.joined()
        }
        return digest
    }
    static func duplicates(_ inventory: ScanInventory) throws -> (groups: [DuplicateSet], skipped: Int) {
        let sizes = Dictionary(grouping: inventory.files, by: \.size).filter { $0.value.count > 1 }
        var groups: [String: [PowerFile]] = [:]; var skipped = 0
        for files in sizes.values {
            for file in files {
                try Task.checkCancellation()
                let v = try? file.url.resourceValues(forKeys: [.isUbiquitousItemKey, .ubiquitousItemDownloadingStatusKey])
                if v?.isUbiquitousItem == true, let status = v?.ubiquitousItemDownloadingStatus, status != .current && status != .downloaded { skipped += 1; continue }
                do { groups[try digest(file.url), default: []].append(file) }
                catch is CancellationError { throw CancellationError() }
                catch { skipped += 1 }
            }
        }
        return (groups.filter { $0.value.count > 1 }.map { DuplicateSet(digest: $0.key, files: $0.value.sorted { $0.url.path < $1.url.path }) }.sorted { $0.recoverableBytes > $1.recoverableBytes }, skipped)
    }
    static func zip(_ urls: [URL], into folder: URL, name archiveName: String) throws -> URL {
        try name(archiveName)
        guard !urls.isEmpty else { throw powerError("Select the files or folders to archive.") }
        let finalName = archiveName.lowercased().hasSuffix(".zip") ? archiveName : archiveName + ".zip"
        let target = unique(folder.appendingPathComponent(finalName))
        let staging = FileManager.default.temporaryDirectory.appendingPathComponent("FileGlade-zip-" + UUID().uuidString)
        try FileManager.default.createDirectory(at: staging, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: staging) }
        let archiveURL = staging.appendingPathComponent("Archive.zip")
        do {
          let archive = try Archive(url: archiveURL, accessMode: .create)
          var names = Set<String>(); var count = 0
          for source in urls {
            guard names.insert(source.lastPathComponent.lowercased()).inserted else { throw powerError("Selected items have matching names. Archive them in separate batches.") }
            try addToArchive(source, path: source.lastPathComponent, archive: archive, count: &count)
            if PowerFile(source).directory {
                guard let walk = FileManager.default.enumerator(at: source, includingPropertiesForKeys: [.isSymbolicLinkKey], options: []) else { throw powerError("Cannot read \(source.lastPathComponent).") }
                for case let child as URL in walk {
                    let relative = String(child.path.dropFirst(source.path.count + 1))
                    try addToArchive(child, path: source.lastPathComponent + "/" + relative, archive: archive, count: &count)
                }
            }
          }
        }
        try Task.checkCancellation()
        try publish(archiveURL, to: target)
        return powerURL(target)
    }
    private static func addToArchive(_ url: URL, path: String, archive: Archive, count: inout Int) throws {
        try Task.checkCancellation(); count += 1
        guard count <= 50_000 else { throw powerError("This archive exceeds the 50,000 item limit. Archive smaller folders separately.") }
        let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
        guard attributes[.type] as? FileAttributeType != .typeSymbolicLink else { throw powerError("Symbolic links are not supported in ZIP tools: \(url.lastPathComponent).") }
        try read(url) { try archive.addEntry(with: path, fileURL: $0, compressionMethod: .deflate) }
    }
    static func unzip(_ source: URL, into folder: URL) throws -> URL {
        let stage = FileManager.default.temporaryDirectory.appendingPathComponent("FileGlade-unzip-" + UUID().uuidString)
        try FileManager.default.createDirectory(at: stage, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: stage) }
        let target = unique(folder.appendingPathComponent(source.deletingPathExtension().lastPathComponent + " extracted", isDirectory: true))
        try read(source) { url in
            let archive = try Archive(url: url, accessMode: .read)
            let entries = Array(archive)
            guard entries.count <= 50_000 else { throw powerError("This ZIP contains more than 50,000 entries.") }
            var total: UInt64 = 0; var paths = Set<String>()
            for entry in entries {
                try Task.checkCancellation()
                let path = entry.path
                let components = path.split(separator: "/", omittingEmptySubsequences: false)
                guard !path.isEmpty, !path.hasPrefix("/"), !path.contains("\\"), !path.contains(":"), !path.contains("\0"), !components.contains(".."), entry.type != .symlink else { throw powerError("This ZIP contains an unsafe path or a symbolic link.") }
                let resolved = stage.appendingPathComponent(path).standardizedFileURL
                guard resolved.path.hasPrefix(stage.path + "/") else { throw powerError("This ZIP attempts to write outside its destination.") }
                let folded = resolved.path.lowercased()
                guard paths.insert(folded).inserted else { throw powerError("This ZIP has conflicting entry names.") }
                guard entry.uncompressedSize <= 4_000_000_000, total <= 4_000_000_000 - entry.uncompressedSize else { throw powerError("Expanded ZIPs are limited to 4 GB per operation.") }
                total += entry.uncompressedSize
            }
            var expanded: UInt64 = 0
            for entry in entries {
                try Task.checkCancellation()
                let output = stage.appendingPathComponent(entry.path)
                try FileManager.default.createDirectory(at: output.deletingLastPathComponent(), withIntermediateDirectories: true)
                if entry.type == .directory { try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true); continue }
                guard !FileManager.default.fileExists(atPath: output.path), FileManager.default.createFile(atPath: output.path, contents: nil) else { throw powerError("The ZIP contains conflicting paths.") }
                let file = try FileHandle(forWritingTo: output)
                defer { try? file.close() }
                var written: UInt64 = 0
                let checksum = try archive.extract(entry, bufferSize: 1_048_576) { data in
                    try Task.checkCancellation()
                    guard UInt64(data.count) <= entry.uncompressedSize - min(written, entry.uncompressedSize), expanded <= 4_000_000_000 - UInt64(data.count) else { throw powerError("ZIP content exceeds the declared size or extraction limit.") }
                    written += UInt64(data.count); expanded += UInt64(data.count)
                    try file.write(contentsOf: data)
                }
                guard written == entry.uncompressedSize else { throw powerError("The ZIP has a truncated entry.") }
                guard checksum == entry.checksum else { throw powerError("The ZIP failed its integrity check. No extracted folder was published.") }
            }
        }
        try Task.checkCancellation()
        try publish(stage, to: target)
        return powerURL(target)
    }
    static let textExtensions = ["txt", "md", "markdown", "json", "csv", "tsv", "log", "yaml", "yml", "xml", "html", "css", "js", "swift", "py", "sh"]
    static func readText(_ url: URL) throws -> TextSnapshot {
        var result: TextSnapshot?
        try read(url) { source in
            guard PowerFile(source).size <= 2_000_000 else { throw powerError("The text editor supports files up to 2 MB.") }
            let data = try Data(contentsOf: source)
            guard data.count <= 2_000_000, let text = String(data: data, encoding: .utf8) else { throw powerError("This file is not UTF-8 text or is larger than 2 MB.") }
            result = TextSnapshot(url: powerURL(url), text: text, digest: SHA256.hash(data: data).description)
        }
        return result!
    }
    static func saveText(_ text: String, snapshot: TextSnapshot) throws -> URL {
        guard text.utf8.count <= 2_000_000 else { throw powerError("The text editor supports files up to 2 MB.") }
        try write(snapshot.url) { target in
            if FileManager.default.fileExists(atPath: target.path) {
                guard let expected = snapshot.digest, PowerFile(target).size <= 2_000_000, SHA256.hash(data: try Data(contentsOf: target)).description == expected else { throw powerError("This file changed outside the editor. Reopen it before saving; your typed text has been kept.") }
            } else if snapshot.digest != nil { throw powerError("The original file was moved or removed. Create a new text file to keep these edits.") }
            try Data(text.utf8).write(to: target, options: .atomic)
        }
        return powerURL(snapshot.url)
    }
    static func combinePDFs(_ urls: [URL], into folder: URL, name filename: String) throws -> URL {
        try name(filename); guard !urls.isEmpty else { throw powerError("Select PDFs or images first.") }
        let output = PDFDocument(); var pageIndex = 0
        for url in urls {
            try Task.checkCancellation()
            guard pageIndex < 2_000 else { throw powerError("Combine up to 2,000 pages at a time.") }
            try read(url) { source in
                if source.pathExtension.lowercased() == "pdf" {
                    guard let pdf = PDFDocument(url: source), !pdf.isLocked else { throw powerError("Cannot read \(url.lastPathComponent). Unlock encrypted PDFs in another app first.") }
                    guard pageIndex + pdf.pageCount <= 2_000 else { throw powerError("Combine up to 2,000 pages at a time.") }
                    for index in 0..<pdf.pageCount {
                        guard let page = pdf.page(at: index)?.copy() as? PDFPage else { throw powerError("A page in \(url.lastPathComponent) could not be read.") }
                        output.insert(page, at: pageIndex); pageIndex += 1
                    }
                } else {
                    #if os(macOS)
                    guard let image = NSImage(contentsOf: source), let page = PDFPage(image: image) else { throw powerError("Cannot convert \(url.lastPathComponent) to a PDF page.") }
                    #else
                    guard let image = UIImage(contentsOfFile: source.path), let page = PDFPage(image: image) else { throw powerError("Cannot convert \(url.lastPathComponent) to a PDF page.") }
                    #endif
                    output.insert(page, at: pageIndex); pageIndex += 1
                }
            }
        }
        guard pageIndex > 0, let data = output.dataRepresentation() else { throw powerError("No PDF pages could be created.") }
        let target = unique(folder.appendingPathComponent(filename.lowercased().hasSuffix(".pdf") ? filename : filename + ".pdf"))
        try publishData(data, to: target)
        return powerURL(target)
    }
    static func resizeImages(_ urls: [URL], into folder: URL, maximum: Int, jpeg: Bool) throws -> [URL] {
        guard !urls.isEmpty else { throw powerError("Select images first.") }
        var result: [URL] = []
        for url in urls {
            try Task.checkCancellation()
            var image: CGImage?
            try read(url) { source in
                guard let input = CGImageSourceCreateWithURL(source as CFURL, nil) else { throw powerError("Cannot read \(url.lastPathComponent).") }
                image = CGImageSourceCreateThumbnailAtIndex(input, 0, [kCGImageSourceCreateThumbnailFromImageAlways: true, kCGImageSourceThumbnailMaxPixelSize: max(64, min(maximum, 8192)), kCGImageSourceCreateThumbnailWithTransform: true] as CFDictionary)
            }
            guard let image else { throw powerError("Cannot resize \(url.lastPathComponent).") }
            let target = unique(folder.appendingPathComponent(url.deletingPathExtension().lastPathComponent + "-\(maximum)." + (jpeg ? "jpg" : "png")))
            do {
                let data = NSMutableData()
                guard let writer = CGImageDestinationCreateWithData(data, (jpeg ? UTType.jpeg.identifier : UTType.png.identifier) as CFString, 1, nil) else { throw powerError("Cannot create the image.") }
                CGImageDestinationAddImage(writer, image, [kCGImageDestinationLossyCompressionQuality: 0.85] as CFDictionary)
                guard CGImageDestinationFinalize(writer) else { throw powerError("Image conversion failed.") }
                try publishData(data as Data, to: target)
            }
            result.append(powerURL(target))
        }
        return result
    }
}
