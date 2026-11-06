import SwiftUI
import AppKit

struct ArchiveEntry: Identifiable, Hashable {
    let path: String
    let size: Int64
    let isDirectory: Bool
    var id: String { path }
}

/// Reads and extracts many archive formats (ZIP, TAR, 7z, RAR, ISO and more) with the bsdtar tool that
/// ships with macOS, so no extra software is needed.
enum ArchiveTool {
    static let extensions: Set<String> = ["zip", "tar", "gz", "tgz", "bz2", "tbz", "xz", "txz", "7z", "rar", "cpio", "iso", "jar", "war", "apk", "xar", "lz", "z", "zst", "cab", "dmg"]
    static func isArchive(_ url: URL) -> Bool { extensions.contains(url.pathExtension.lowercased()) && url.pathExtension.lowercased() != "dmg" }

    private static func run(_ arguments: [String]) throws -> (status: Int32, output: Data, error: String) {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/bsdtar")
        process.arguments = arguments
        let out = Pipe(), err = Pipe()
        process.standardOutput = out; process.standardError = err
        try process.run()
        // Read stdout fully before waiting so a large listing cannot fill the pipe and stall.
        let data = out.fileHandleForReading.readDataToEndOfFile()
        let errorData = err.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        return (process.terminationStatus, data, String(decoding: errorData, as: UTF8.self))
    }

    static func list(_ archive: URL) throws -> [ArchiveEntry] {
        let result = try run(["-tvf", archive.path])
        guard result.status == 0 else { throw powerError("This archive could not be read. It may be damaged, password-protected or in an unsupported format.") }
        let expression = try NSRegularExpression(pattern: #"^(\S+)\s+\d+\s+\S+\s+\S+\s+(\d+)\s+\S+\s+\d+\s+\S+\s+(.*)$"#)
        var entries: [ArchiveEntry] = []
        for line in String(decoding: result.output, as: UTF8.self).split(separator: "\n") {
            let text = String(line)
            guard let match = expression.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)), match.numberOfRanges == 4,
                  let perms = Range(match.range(at: 1), in: text), let size = Range(match.range(at: 2), in: text), let name = Range(match.range(at: 3), in: text) else { continue }
            var path = String(text[name])
            if let arrow = path.range(of: " -> ") { path = String(path[..<arrow.lowerBound]) }
            let isDirectory = text[perms].hasPrefix("d") || path.hasSuffix("/")
            while path.hasSuffix("/") { path.removeLast() }
            if !path.isEmpty { entries.append(ArchiveEntry(path: path, size: Int64(text[size]) ?? 0, isDirectory: isDirectory)) }
        }
        return entries
    }

    /// Extracts selected paths (or everything) into a new folder beside the archive. Existing files are never overwritten.
    static func extract(_ archive: URL, paths: [String]?, into folder: URL) throws -> URL {
        let destination = PowerFS.unique(folder.appendingPathComponent(archive.deletingPathExtension().lastPathComponent + " extracted", isDirectory: true))
        try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
        var arguments = ["-xf", archive.path, "-C", destination.path, "--no-same-owner", "--no-same-permissions"]
        if let paths, !paths.isEmpty { arguments += ["--"] + paths }
        let result = try run(arguments)
        guard result.status == 0 else {
            try? FileManager.default.removeItem(at: destination)
            throw powerError("Extraction failed. " + result.error.split(separator: "\n").first.map(String.init).orEmpty)
        }
        return destination
    }
}
private extension Optional where Wrapped == String { var orEmpty: String { self ?? "" } }

struct ArchiveBrowserView: View {
    let archive: URL
    let onExtracted: (URL) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var entries: [ArchiveEntry] = []
    @State private var loading = true
    @State private var error: String?
    @State private var query = ""
    @State private var selection = Set<String>()
    @State private var working = false
    private var shown: [ArchiveEntry] {
        let q = query.lowercased()
        return Array(entries.filter { q.isEmpty || $0.path.lowercased().contains(q) }.prefix(5000))
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack { Label(archive.lastPathComponent, systemImage: "doc.zipper").font(.title3.bold()); Spacer(); if loading || working { ProgressView().controlSize(.small) } }
            TextField("Filter names", text: $query).textFieldStyle(.roundedBorder)
            if let error { Text(error).foregroundStyle(.red).font(.callout) }
            List(shown, selection: $selection) { entry in
                HStack(spacing: 8) {
                    Image(systemName: entry.isDirectory ? "folder" : "doc").foregroundStyle(.secondary).frame(width: 18)
                    Text(entry.path).lineLimit(1).truncationMode(.middle)
                    Spacer()
                    if !entry.isDirectory { Text(fileBytes(entry.size)).font(.caption).foregroundStyle(.secondary) }
                }.tag(entry.path)
            }
            HStack {
                Text("\(entries.count.formatted()) items\(entries.count > 5000 && query.isEmpty ? " · showing the first 5,000; use the filter to find others" : "")").font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("Extract Selected") { extract(selection.isEmpty ? nil : Array(selection)) }.disabled(selection.isEmpty || working)
                Button("Extract All") { extract(nil) }.disabled(entries.isEmpty || working)
                Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
            }
            Text("Items are extracted into a new folder next to the archive. Nothing is overwritten.").font(.caption2).foregroundStyle(.secondary)
        }.padding(18).frame(width: 640, height: 500)
        .task { await load() }
    }
    private func load() async {
        let url = archive
        let result = await Task.detached(priority: .userInitiated) { Result { try ArchiveTool.list(url) } }.value
        loading = false
        switch result { case .success(let list): entries = list; case .failure(let e): error = e.localizedDescription }
    }
    private func extract(_ paths: [String]?) {
        working = true; error = nil
        let url = archive, folder = archive.deletingLastPathComponent()
        Task {
            let result = await Task.detached(priority: .userInitiated) { Result { try ArchiveTool.extract(url, paths: paths, into: folder) } }.value
            working = false
            switch result { case .success(let out): onExtracted(out); dismiss(); case .failure(let e): error = e.localizedDescription }
        }
    }
}
