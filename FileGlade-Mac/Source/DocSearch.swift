import SwiftUI
import AppKit
import PDFKit

struct DocHit: Identifiable {
    let url: URL
    let excerpt: String
    let count: Int
    var id: URL { url }
}

/// Finds text inside documents using Spotlight's own index (PDF, Word, Pages, text, code and more), then
/// shows the matching passage. Only folders that Spotlight indexes are covered.
struct DocSearchView: View {
    let folder: URL
    let onReveal: (URL) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var query = ""
    @State private var everywhere = true
    @State private var hits: [DocHit] = []
    @State private var searching = false
    @State private var note = "Type words to find inside your documents."
    @State private var generation = 0
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Search inside documents").font(.title3.bold())
            HStack {
                TextField("Words to find", text: $query).textFieldStyle(.roundedBorder).onSubmit { search() }
                Picker("Where", selection: $everywhere) { Text("Whole Mac").tag(true); Text("This folder").tag(false) }.pickerStyle(.segmented).frame(width: 180)
                Button("Search") { search() }.keyboardShortcut(.defaultAction).disabled(query.trimmingCharacters(in: .whitespaces).count < 2)
            }
            HStack(spacing: 8) { if searching { ProgressView().controlSize(.small) }; Text(note).font(.caption).foregroundStyle(.secondary) }
            List(hits) { hit in
                VStack(alignment: .leading, spacing: 3) {
                    HStack { FileIcon(url: hit.url, size: 18); Text(hit.url.lastPathComponent).font(.system(size: 12, weight: .semibold)).lineLimit(1); Spacer(); if hit.count > 0 { Text("\(hit.count) match\(hit.count == 1 ? "" : "es")").font(.caption2).foregroundStyle(.secondary) } }
                    if !hit.excerpt.isEmpty { Text(hit.excerpt).font(.caption).foregroundStyle(.secondary).lineLimit(3) }
                    Text(hit.url.deletingLastPathComponent().path).font(.caption2).foregroundStyle(.tertiary).lineLimit(1).truncationMode(.head)
                }.contentShape(Rectangle())
                .onTapGesture(count: 2) { NSWorkspace.shared.open(hit.url) }
                .contextMenu { Button("Open") { NSWorkspace.shared.open(hit.url) }; Button("Show in FileGlade") { dismiss(); onReveal(hit.url) }; Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([hit.url]) } }
            }
            HStack { Text("Double-click to open. Right-click for more.").font(.caption2).foregroundStyle(.secondary); Spacer(); Button("Done") { dismiss() }.keyboardShortcut(.cancelAction) }
        }.padding(18).frame(width: 700, height: 540)
    }
    private func search() {
        let text = query.trimmingCharacters(in: .whitespaces)
        guard text.count >= 2 else { return }
        generation += 1; let mine = generation
        hits = []; searching = true; note = "Searching…"
        let scope = everywhere ? NSHomeDirectory() : folder.path
        DispatchQueue.global(qos: .userInitiated).async {
            let paths = DocSearchEngine.spotlight(text, scope: scope, limit: 150)
            DispatchQueue.main.async { if mine == generation { note = paths.isEmpty ? "No documents contain “\(text)”." : "Found \(paths.count) document\(paths.count == 1 ? "" : "s"). Loading excerpts…" } }
            var found = 0
            for path in paths {
                if mine != self.currentGeneration() { return }
                let hit = DocSearchEngine.excerpt(for: URL(fileURLWithPath: path), query: text)
                found += 1
                DispatchQueue.main.async { if mine == generation { hits.append(hit); if found == paths.count { searching = false; note = "\(paths.count) document\(paths.count == 1 ? "" : "s") match\(paths.count >= 150 ? " (showing the first 150)" : "")." } } }
            }
            DispatchQueue.main.async { if mine == generation && paths.isEmpty { searching = false } }
        }
    }
    private func currentGeneration() -> Int { DispatchQueue.main.sync { generation } }
}

enum DocSearchEngine {
    static func spotlight(_ text: String, scope: String, limit: Int) -> [String] {
        let safe = text.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"").replacingOccurrences(of: "*", with: "")
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/mdfind")
        process.arguments = ["-onlyin", scope, "kMDItemTextContent == \"*\(safe)*\"cd"]
        let pipe = Pipe(); process.standardOutput = pipe; process.standardError = Pipe()
        guard (try? process.run()) != nil else { return [] }
        let data = pipe.fileHandleForReading.readDataToEndOfFile(); process.waitUntilExit()
        return Array(String(decoding: data, as: UTF8.self).split(separator: "\n").map(String.init).filter { !$0.contains("/Library/") }.prefix(limit))
    }
    private static func text(of url: URL) -> String {
        let ext = url.pathExtension.lowercased()
        if ext == "pdf" { return PDFDocument(url: url)?.string ?? "" }
        if let data = try? Data(contentsOf: url, options: .mappedIfSafe), data.count < 4_000_000, let string = String(data: data, encoding: .utf8), !string.contains("\0") { return string }
        // Word, Pages, slides and other formats: use the text Spotlight already extracted.
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/mdls")
        process.arguments = ["-raw", "-name", "kMDItemTextContent", url.path]
        let pipe = Pipe(); process.standardOutput = pipe; process.standardError = Pipe()
        guard (try? process.run()) != nil else { return "" }
        let data = pipe.fileHandleForReading.readDataToEndOfFile(); process.waitUntilExit()
        return String(decoding: data, as: UTF8.self)
    }
    static func excerpt(for url: URL, query: String) -> DocHit {
        let content = text(of: url)
        let options: String.CompareOptions = [.caseInsensitive, .diacriticInsensitive]
        guard let range = content.range(of: query, options: options) else { return DocHit(url: url, excerpt: "", count: 0) }
        var count = 0
        var cursor = content.startIndex
        while count < 999, let next = content.range(of: query, options: options, range: cursor..<content.endIndex) { count += 1; cursor = next.upperBound }
        let start = content.index(range.lowerBound, offsetBy: -70, limitedBy: content.startIndex) ?? content.startIndex
        let end = content.index(range.upperBound, offsetBy: 90, limitedBy: content.endIndex) ?? content.endIndex
        let snippet = content[start..<end].replacingOccurrences(of: "\n", with: " ").replacingOccurrences(of: "\r", with: " ")
        return DocHit(url: url, excerpt: (start > content.startIndex ? "…" : "") + snippet + (end < content.endIndex ? "…" : ""), count: count)
    }
}
