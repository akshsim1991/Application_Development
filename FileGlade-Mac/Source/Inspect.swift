import SwiftUI
import AppKit

// MARK: - File signatures (what a file really is, from its first bytes)

enum FileSignature {
    struct Match { let name: String; let extensions: Set<String>; let isProgram: Bool }
    private static let zipFamily: Set<String> = ["zip", "docx", "xlsx", "pptx", "jar", "apk", "epub", "odt", "ods", "odp", "pages", "numbers", "key", "war", "ipa", "xpi", "nupkg"]

    static func detect(_ url: URL) -> Match? {
        guard let handle = try? FileHandle(forReadingFrom: url), let head = try? handle.read(upToCount: 16), !head.isEmpty else { return nil }
        try? handle.close()
        return detect(bytes: [UInt8](head))
    }
    static func detect(bytes b: [UInt8]) -> Match? {
        func starts(_ p: [UInt8]) -> Bool { b.count >= p.count && Array(b.prefix(p.count)) == p }
        func ascii(_ s: String, at i: Int = 0) -> Bool { let p = Array(s.utf8); return b.count >= i + p.count && Array(b[i..<(i + p.count)]) == p }
        if starts([0x89, 0x50, 0x4E, 0x47]) { return Match(name: "PNG image", extensions: ["png"], isProgram: false) }
        if starts([0xFF, 0xD8, 0xFF]) { return Match(name: "JPEG image", extensions: ["jpg", "jpeg", "jpe"], isProgram: false) }
        if ascii("GIF8") { return Match(name: "GIF image", extensions: ["gif"], isProgram: false) }
        if ascii("%PDF") { return Match(name: "PDF document", extensions: ["pdf"], isProgram: false) }
        if starts([0x50, 0x4B, 0x03, 0x04]) { return Match(name: "ZIP archive (or Office/Pages document)", extensions: zipFamily, isProgram: false) }
        if starts([0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C]) { return Match(name: "7z archive", extensions: ["7z"], isProgram: false) }
        if ascii("Rar!") { return Match(name: "RAR archive", extensions: ["rar"], isProgram: false) }
        if starts([0x1F, 0x8B]) { return Match(name: "gzip compressed data", extensions: ["gz", "tgz", "svgz"], isProgram: false) }
        if ascii("BZh") { return Match(name: "bzip2 compressed data", extensions: ["bz2", "tbz", "tbz2"], isProgram: false) }
        if starts([0xCF, 0xFA, 0xED, 0xFE]) || starts([0xFE, 0xED, 0xFA, 0xCF]) || starts([0xCE, 0xFA, 0xED, 0xFE]) || starts([0xCA, 0xFE, 0xBA, 0xBE]) { return Match(name: "macOS program (Mach-O)", extensions: ["dylib", "o", "so", "bundle", ""], isProgram: true) }
        if starts([0x7F, 0x45, 0x4C, 0x46]) { return Match(name: "Linux program (ELF)", extensions: ["so", "o", ""], isProgram: true) }
        if ascii("MZ") { return Match(name: "Windows program (EXE/DLL)", extensions: ["exe", "dll", "sys", "scr", "com"], isProgram: true) }
        if ascii("SQLite format 3") { return Match(name: "SQLite database", extensions: ["sqlite", "sqlite3", "db", "db3", "store"], isProgram: false) }
        if ascii("ftyp", at: 4) {
            if ascii("heic", at: 8) || ascii("heix", at: 8) || ascii("mif1", at: 8) { return Match(name: "HEIC image", extensions: ["heic", "heif"], isProgram: false) }
            if ascii("qt  ", at: 8) { return Match(name: "QuickTime movie", extensions: ["mov", "qt"], isProgram: false) }
            if ascii("M4A ", at: 8) { return Match(name: "MPEG-4 audio", extensions: ["m4a"], isProgram: false) }
            return Match(name: "MPEG-4 video", extensions: ["mp4", "m4v", "mov"], isProgram: false)
        }
        if ascii("RIFF") && ascii("WAVE", at: 8) { return Match(name: "WAV audio", extensions: ["wav"], isProgram: false) }
        if ascii("ID3") || starts([0xFF, 0xFB]) { return Match(name: "MP3 audio", extensions: ["mp3"], isProgram: false) }
        if ascii("OggS") { return Match(name: "Ogg media", extensions: ["ogg", "oga", "ogv", "opus"], isProgram: false) }
        if ascii("#!") { return Match(name: "Script", extensions: ["sh", "command", "py", "rb", "pl", "js", ""], isProgram: true) }
        return nil
    }
    private static let plainExtensions: Set<String> = ["txt", "jpg", "jpeg", "png", "gif", "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "mp3", "mp4", "mov", "heic", "csv", "rtf", "pages", "numbers", "key"]
    /// A warning when the name and the contents disagree, especially a program disguised as a document or picture.
    static func warning(for url: URL) -> String? {
        guard let match = detect(url) else { return nil }
        let ext = url.pathExtension.lowercased()
        if match.isProgram && plainExtensions.contains(ext) { return "This looks like a program (\(match.name)) but is named “.\(ext)”. Do not open it unless you trust where it came from." }
        if !match.extensions.contains(ext) && plainExtensions.contains(ext) && !match.isProgram { return "The name says “.\(ext)” but the contents look like \(match.name)." }
        return nil
    }
}

// MARK: - Hex dump

enum HexDump {
    static func lines(_ data: Data, startOffset: Int) -> [String] {
        var out: [String] = []
        let bytes = [UInt8](data)
        for row in stride(from: 0, to: bytes.count, by: 16) {
            let chunk = bytes[row..<min(row + 16, bytes.count)]
            let hex = chunk.map { String(format: "%02X", $0) }.joined(separator: " ")
            let padded = hex + String(repeating: " ", count: max(0, 16 * 3 - 1 - hex.count))
            let text = String(chunk.map { $0 >= 32 && $0 < 127 ? Character(UnicodeScalar($0)) : "." })
            out.append(String(format: "%08X  ", startOffset + row) + padded + "  |" + text + "|")
        }
        return out
    }
}

struct HexViewerView: View {
    let url: URL
    @Environment(\.dismiss) private var dismiss
    @State private var offset = 0
    @State private var lines: [String] = []
    @State private var size: Int64 = 0
    @State private var jump = ""
    private let page = 4096
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(url.lastPathComponent).font(.title3.bold()).lineLimit(1)
                Spacer()
                if let match = FileSignature.detect(url) { Text(match.name).font(.caption).foregroundStyle(.secondary) }
            }
            if let warning = FileSignature.warning(for: url) { Label(warning, systemImage: "exclamationmark.triangle.fill").font(.caption).foregroundStyle(.orange) }
            ScrollView {
                VStack(alignment: .leading, spacing: 0) { ForEach(Array(lines.enumerated()), id: \.offset) { _, line in Text(line).font(.system(size: 11, design: .monospaced)).textSelection(.enabled) } }.frame(maxWidth: .infinity, alignment: .leading).padding(8)
            }.background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 6))
            HStack {
                Button("◀︎ Previous") { offset = max(0, offset - page); load() }.disabled(offset == 0)
                Button("Next ▶︎") { offset += page; load() }.disabled(Int64(offset + page) >= size)
                TextField("Go to offset (hex or decimal)", text: $jump).textFieldStyle(.roundedBorder).frame(width: 200).onSubmit { goTo() }
                Spacer()
                Text("\(fileBytes(size)) · showing bytes \(offset)–\(min(Int64(offset + page), size))").font(.caption).foregroundStyle(.secondary)
                Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
            }
        }.padding(16).frame(width: 760, height: 560).onAppear { load() }
    }
    private func goTo() {
        let text = jump.trimmingCharacters(in: .whitespaces).lowercased()
        let value = text.hasPrefix("0x") ? Int(text.dropFirst(2), radix: 16) : Int(text) ?? Int(text, radix: 16)
        if let value, value >= 0 { offset = value / 16 * 16; load() }
    }
    private func load() {
        size = Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
        guard let handle = try? FileHandle(forReadingFrom: url) else { lines = ["Cannot read this file."]; return }
        try? handle.seek(toOffset: UInt64(offset))
        let data = (try? handle.read(upToCount: page)) ?? Data()
        try? handle.close()
        lines = HexDump.lines(data, startOffset: offset)
    }
}

// MARK: - Compare two files

enum DiffKind { case same, added, removed }
struct DiffLine: Identifiable { let id = UUID(); let kind: DiffKind; let text: String; let leftNumber: Int?; let rightNumber: Int? }

enum TextDiff {
    static func compare(_ left: String, _ right: String) -> [DiffLine] {
        let a = left.components(separatedBy: "\n"), b = right.components(separatedBy: "\n")
        let difference = b.difference(from: a)
        var removed = Set<Int>(), inserted = Set<Int>()
        for change in difference { switch change { case .remove(let offset, _, _): removed.insert(offset); case .insert(let offset, _, _): inserted.insert(offset) } }
        var out: [DiffLine] = []
        var i = 0, j = 0
        while i < a.count || j < b.count {
            if i < a.count && removed.contains(i) { out.append(DiffLine(kind: .removed, text: a[i], leftNumber: i + 1, rightNumber: nil)); i += 1 }
            else if j < b.count && inserted.contains(j) { out.append(DiffLine(kind: .added, text: b[j], leftNumber: nil, rightNumber: j + 1)); j += 1 }
            else if i < a.count && j < b.count { out.append(DiffLine(kind: .same, text: a[i], leftNumber: i + 1, rightNumber: j + 1)); i += 1; j += 1 }
            else { break }
        }
        return out
    }
    /// For files that are not text: the first byte position where they differ, or nil if identical.
    static func firstDifference(_ a: URL, _ b: URL) -> Int? {
        guard let x = try? FileHandle(forReadingFrom: a), let y = try? FileHandle(forReadingFrom: b) else { return 0 }
        defer { try? x.close(); try? y.close() }
        var position = 0
        while true {
            let p = (try? x.read(upToCount: 65536)) ?? Data(), q = (try? y.read(upToCount: 65536)) ?? Data()
            if p.isEmpty && q.isEmpty { return nil }
            let common = min(p.count, q.count)
            for k in 0..<common where p[p.startIndex + k] != q[q.startIndex + k] { return position + k }
            if p.count != q.count { return position + common }
            position += common
        }
    }
}

struct FileCompareView: View {
    let left: URL
    let right: URL
    @Environment(\.dismiss) private var dismiss
    @State private var lines: [DiffLine] = []
    @State private var note = ""
    @State private var showSame = true
    private var changed: Int { lines.filter { $0.kind != .same }.count }
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Compare files").font(.title3.bold())
            HStack { Label(left.lastPathComponent, systemImage: "minus.circle").foregroundStyle(.red); Label(right.lastPathComponent, systemImage: "plus.circle").foregroundStyle(.green) }.font(.caption)
            if !note.isEmpty { Text(note).font(.callout).foregroundStyle(.secondary).padding(.vertical, 6) }
            else {
                Toggle("Show unchanged lines", isOn: $showSame).font(.caption)
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 0) {
                        ForEach(lines.filter { showSame || $0.kind != .same }) { line in
                            HStack(spacing: 8) {
                                Text(line.leftNumber.map(String.init) ?? "").frame(width: 36, alignment: .trailing).foregroundStyle(.tertiary)
                                Text(line.rightNumber.map(String.init) ?? "").frame(width: 36, alignment: .trailing).foregroundStyle(.tertiary)
                                Text((line.kind == .added ? "+ " : line.kind == .removed ? "− " : "  ") + line.text).frame(maxWidth: .infinity, alignment: .leading)
                            }.font(.system(size: 11, design: .monospaced)).padding(.vertical, 1).background(line.kind == .added ? Color.green.opacity(0.18) : line.kind == .removed ? Color.red.opacity(0.18) : .clear)
                        }
                    }
                }.background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 6))
            }
            HStack { Text(note.isEmpty ? "\(changed) changed line\(changed == 1 ? "" : "s")" : "").font(.caption).foregroundStyle(.secondary); Spacer(); Button("Done") { dismiss() }.keyboardShortcut(.cancelAction) }
        }.padding(16).frame(width: 820, height: 560).onAppear { load() }
    }
    private func load() {
        let limit = 2_000_000
        let sizeA = (try? left.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0, sizeB = (try? right.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0
        if sizeA <= limit, sizeB <= limit, let a = try? String(contentsOf: left, encoding: .utf8), let b = try? String(contentsOf: right, encoding: .utf8) { lines = TextDiff.compare(a, b); if changed == 0 { note = "The files have identical text." }; return }
        if let position = TextDiff.firstDifference(left, right) { note = "These are not plain text files (or are very large). They differ, starting at byte \(position)." }
        else { note = "The files are identical, byte for byte." }
    }
}

// MARK: - Empty folders

enum EmptyFolderFinder {
    private static let ignorable: Set<String> = [".DS_Store", ".localized", "Thumbs.db", "desktop.ini"]
    /// Folders that contain nothing but other empty folders (and system litter such as .DS_Store).
    static func find(in root: URL) -> [URL] {
        var result: [URL] = []
        let fm = FileManager.default
        func isEmpty(_ dir: URL) -> Bool {
            guard let items = try? fm.contentsOfDirectory(at: dir, includingPropertiesForKeys: [.isDirectoryKey, .isPackageKey], options: []) else { return false }
            var empty = true
            for item in items {
                if ignorable.contains(item.lastPathComponent) { continue }
                let values = try? item.resourceValues(forKeys: [.isDirectoryKey, .isPackageKey])
                if values?.isDirectory == true && values?.isPackage != true { if !isEmpty(item) { empty = false } } else { empty = false }
            }
            if empty { result.append(dir) }
            return empty
        }
        if let items = try? fm.contentsOfDirectory(at: root, includingPropertiesForKeys: [.isDirectoryKey, .isPackageKey], options: [.skipsHiddenFiles]) {
            for item in items { let v = try? item.resourceValues(forKeys: [.isDirectoryKey, .isPackageKey]); if v?.isDirectory == true && v?.isPackage != true { _ = isEmpty(item) } }
        }
        return result.sorted { $0.path < $1.path }
    }
}

struct EmptyFoldersView: View {
    let root: URL
    let onDone: () -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var folders: [URL] = []
    @State private var chosen = Set<URL>()
    @State private var scanning = true
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Empty folders").font(.title3.bold())
            Text(root.path).font(.caption).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
            if scanning { ProgressView("Looking…") }
            else if folders.isEmpty { Text("No empty folders found.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity) }
            else {
                List(folders, id: \.self) { folder in
                    Toggle(isOn: Binding(get: { chosen.contains(folder) }, set: { if $0 { chosen.insert(folder) } else { chosen.remove(folder) } })) {
                        VStack(alignment: .leading) { Text(folder.lastPathComponent); Text(folder.deletingLastPathComponent().path).font(.caption2).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head) }
                    }
                }
            }
            HStack {
                Text("Folders go to the Trash and can be restored with ⌥⌘Z.").font(.caption2).foregroundStyle(.secondary)
                Spacer()
                Button("Select All") { chosen = Set(folders) }.disabled(folders.isEmpty)
                Button("Move \(chosen.count) to Trash") { trash() }.disabled(chosen.isEmpty)
                Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
            }
        }.padding(16).frame(width: 600, height: 460)
        .task { let r = root; folders = await Task.detached { EmptyFolderFinder.find(in: r) }.value; scanning = false }
    }
    private func trash() {
        var steps: [UndoRecord.Step] = []
        // Deepest folders first, so a parent is only trashed after its empty children.
        for url in chosen.sorted(by: { $0.path.count > $1.path.count }) { var result: NSURL?; if (try? FileManager.default.trashItem(at: url, resultingItemURL: &result)) != nil, let result { steps.append(.trashed(original: url, trash: result as URL)) } }
        TransferManager.shared.record(title: "Remove empty folders", steps: steps)
        folders.removeAll { chosen.contains($0) || !FileManager.default.fileExists(atPath: $0.path) }; chosen = []; onDone()
    }
}
