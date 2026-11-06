import SwiftUI
import AppKit
import PDFKit
import AVKit
import QuickLookThumbnailing
import UniformTypeIdentifiers

private let thumbnailCache = NSCache<NSString, NSImage>()

/// Shows a Quick Look thumbnail (images, PDFs, video…) and falls back to the file icon.
struct ThumbnailIcon: View {
    let url: URL
    var size: CGFloat
    @State private var image: NSImage?
    var body: some View {
        Image(nsImage: image ?? NSWorkspace.shared.icon(forFile: url.path))
            .resizable().interpolation(.high).aspectRatio(contentMode: .fit)
            .frame(width: size, height: size)
            .task(id: url) { await load() }
    }
    private func load() async {
        let key = "\(url.path)@\(Int(size))" as NSString
        if let cached = thumbnailCache.object(forKey: key) { image = cached; return }
        let request = QLThumbnailGenerator.Request(fileAt: url, size: CGSize(width: size, height: size), scale: NSScreen.main?.backingScaleFactor ?? 2, representationTypes: .thumbnail)
        if let representation = try? await QLThumbnailGenerator.shared.generateBestRepresentation(for: request) {
            let result = representation.nsImage
            thumbnailCache.setObject(result, forKey: key)
            image = result
        }
    }
}

enum PreviewKind { case pdf, image, video, audio, text, none }

func previewKind(for entry: FileEntry) -> PreviewKind {
    if entry.isDirectory { return .none }
    guard let type = UTType(filenameExtension: entry.url.pathExtension) else { return .none }
    if type.conforms(to: .pdf) { return .pdf }
    if type.conforms(to: .image) { return .image }
    if type.conforms(to: .movie) { return .video }
    if type.conforms(to: .audio) { return .audio }
    if type.conforms(to: .text) || type.conforms(to: .sourceCode) || type.conforms(to: .json), entry.size <= 262_144 { return .text }
    return .none
}

struct PDFPreview: NSViewRepresentable {
    let url: URL
    func makeNSView(context: Context) -> PDFView {
        let view = PDFView()
        view.autoScales = true
        view.displayMode = .singlePageContinuous
        view.document = PDFDocument(url: url)
        return view
    }
    func updateNSView(_ view: PDFView, context: Context) {
        if view.document?.documentURL != url { view.document = PDFDocument(url: url) }
    }
}

struct MediaPreview: View {
    let url: URL
    let audio: Bool
    @State private var player: AVPlayer?
    var body: some View {
        Group {
            if let player { VideoPlayer(player: player) } else { Color.clear }
        }
        .frame(height: audio ? 56 : 160)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .task(id: url) { player?.pause(); player = AVPlayer(url: url) }
        .onDisappear { player?.pause() }
    }
}

struct TextPreview: View {
    let url: URL
    @State private var text = ""
    var body: some View {
        Text(text).font(.system(size: 10, design: .monospaced)).foregroundStyle(.secondary)
            .lineLimit(16).frame(maxWidth: .infinity, alignment: .leading)
            .padding(8).background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 6))
            .task(id: url) {
                if let data = try? Data(contentsOf: url, options: .mappedIfSafe).prefix(3000), let string = String(data: data, encoding: .utf8) { text = string } else { text = "" }
            }
    }
}

struct FilePreview: View {
    let entry: FileEntry
    var body: some View {
        Group {
            switch previewKind(for: entry) {
            case .pdf: PDFPreview(url: entry.url).frame(height: 280).clipShape(RoundedRectangle(cornerRadius: 6))
            case .image: ThumbnailIcon(url: entry.url, size: 230).frame(maxWidth: .infinity)
            case .video: MediaPreview(url: entry.url, audio: false)
            case .audio: MediaPreview(url: entry.url, audio: true)
            case .text: TextPreview(url: entry.url)
            case .none: ThumbnailIcon(url: entry.url, size: 82).frame(maxWidth: .infinity).padding(.vertical, 8)
            }
        }
        .id(entry.url)
    }
}

/// Remembers icons so lists do not ask the system for the same icon again and again.
enum IconCache {
    private static var images: [String: NSImage] = [:]
    private static let perItem: Set<String> = ["app", "bundle", "framework", "prefpane", "plugin", "photoslibrary", "musiclibrary", "dmg", "pkg", ""]
    static func icon(for url: URL, directory: Bool?) -> NSImage {
        let ext = url.pathExtension.lowercased()
        let key: String
        switch directory {
        case .some(true): key = perItem.contains(ext) && !ext.isEmpty ? "path:" + url.path : "folder"
        case .some(false): key = perItem.contains(ext) ? "path:" + url.path : "ext:" + ext
        case .none: key = "path:" + url.path
        }
        if let cached = images[key] { return cached }
        let image: NSImage
        if key == "folder" { image = NSWorkspace.shared.icon(for: .folder) }
        else if key.hasPrefix("ext:"), let type = UTType(filenameExtension: ext) { image = NSWorkspace.shared.icon(for: type) }
        else { image = NSWorkspace.shared.icon(forFile: url.path) }
        if images.count > 3000 { images.removeAll() }
        images[key] = image
        return image
    }
}
