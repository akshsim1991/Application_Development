import SwiftUI
import UIKit
import QuickLook
import VisionKit
import Vision
import PDFKit
import ImageIO
import CoreImage
import UniformTypeIdentifiers

// MARK: - Image and PDF quick actions (like Files' Quick Actions menu)

enum QuickActionError: LocalizedError {
    case unreadable, noSubject, locked, failed(String)
    var errorDescription: String? {
        switch self {
        case .unreadable: return "This file could not be read as an image."
        case .noSubject: return "No clear subject was found in this photo."
        case .locked: return "The password was not accepted."
        case .failed(let m): return m
        }
    }
}

enum ImageQuickActions {
    static func upright(_ url: URL) throws -> CGImage {
        guard let image = UIImage(contentsOfFile: url.path) else { throw QuickActionError.unreadable }
        let renderer = UIGraphicsImageRenderer(size: image.size, format: { let f = UIGraphicsImageRendererFormat(); f.scale = image.scale; return f }())
        guard let cg = renderer.image(actions: { _ in image.draw(at: .zero) }).cgImage else { throw QuickActionError.unreadable }
        return cg
    }
    static func write(_ image: CGImage, type: UTType, to url: URL) throws {
        guard let destination = CGImageDestinationCreateWithURL(url as CFURL, type.identifier as CFString, 1, nil) else { throw QuickActionError.failed("This device cannot save that image format.") }
        CGImageDestinationAddImage(destination, image, [kCGImageDestinationLossyCompressionQuality: 0.92] as CFDictionary)
        guard CGImageDestinationFinalize(destination) else { throw QuickActionError.failed("The image could not be saved.") }
    }
    private static func output(_ source: URL, suffix: String, ext: String? = nil) -> URL {
        let name = source.deletingPathExtension().lastPathComponent + " " + suffix + "." + (ext ?? source.pathExtension)
        return FileOperations.uniqueURL(source.deletingLastPathComponent().appendingPathComponent(name))
    }
    static func rotate(_ url: URL, clockwise: Bool) throws -> URL {
        let cg = try upright(url)
        let size = CGSize(width: cg.height, height: cg.width)
        let rotated = UIGraphicsImageRenderer(size: size, format: { let f = UIGraphicsImageRendererFormat(); f.scale = 1; return f }()).image { context in
            context.cgContext.translateBy(x: size.width / 2, y: size.height / 2)
            context.cgContext.rotate(by: clockwise ? .pi / 2 : -.pi / 2)
            UIImage(cgImage: cg).draw(in: CGRect(x: -CGFloat(cg.width) / 2, y: -CGFloat(cg.height) / 2, width: CGFloat(cg.width), height: CGFloat(cg.height)))
        }
        guard let out = rotated.cgImage else { throw QuickActionError.unreadable }
        let target = output(url, suffix: clockwise ? "rotated right" : "rotated left")
        try write(out, type: UTType(filenameExtension: url.pathExtension) ?? .png, to: target)
        return target
    }
    static func convert(_ url: URL, to type: UTType) throws -> URL {
        let cg = try upright(url)
        let target = output(url, suffix: "converted", ext: type.preferredFilenameExtension ?? "jpg")
        try write(cg, type: type, to: target)
        return target
    }
    static func removeBackground(_ url: URL) throws -> URL {
        let cg = try upright(url)
        let request = VNGenerateForegroundInstanceMaskRequest()
        let handler = VNImageRequestHandler(cgImage: cg)
        try handler.perform([request])
        guard let result = request.results?.first, !result.allInstances.isEmpty else { throw QuickActionError.noSubject }
        let buffer = try result.generateMaskedImage(ofInstances: result.allInstances, from: handler, croppedToInstancesExtent: true)
        let image = CIImage(cvPixelBuffer: buffer)
        guard let out = CIContext().createCGImage(image, from: image.extent) else { throw QuickActionError.unreadable }
        let target = output(url, suffix: "no background", ext: "png")
        try write(out, type: .png, to: target)
        return target
    }
    /// Reads the words in an image or in every page of a PDF, on the device.
    static func extractText(_ url: URL) throws -> String {
        func recognize(_ image: CGImage) throws -> String {
            let request = VNRecognizeTextRequest(); request.recognitionLevel = .accurate; request.usesLanguageCorrection = true
            try VNImageRequestHandler(cgImage: image).perform([request])
            return (request.results ?? []).compactMap { $0.topCandidates(1).first?.string }.joined(separator: "\n")
        }
        if url.pathExtension.lowercased() == "pdf" {
            guard let document = PDFDocument(url: url) else { throw QuickActionError.unreadable }
            var pages: [String] = []
            for index in 0..<min(document.pageCount, 60) {
                guard let page = document.page(at: index) else { continue }
                let embedded = page.string?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
                if !embedded.isEmpty { pages.append(embedded); continue }
                let thumb = page.thumbnail(of: CGSize(width: 1600, height: 2200), for: .mediaBox)
                if let cg = thumb.cgImage { pages.append(try recognize(cg)) }
            }
            return pages.joined(separator: "\n\n")
        }
        return try recognize(try upright(url))
    }
    static func unlockPDF(_ url: URL, password: String) throws -> URL {
        guard let locked = PDFDocument(url: url) else { throw QuickActionError.unreadable }
        if locked.isLocked && !locked.unlock(withPassword: password) { throw QuickActionError.locked }
        let clean = PDFDocument()
        for index in 0..<locked.pageCount { if let page = locked.page(at: index)?.copy() as? PDFPage { clean.insert(page, at: index) } }
        let target = output(url, suffix: "unlocked")
        guard clean.write(to: target) else { throw QuickActionError.failed("The unlocked PDF could not be saved.") }
        return target
    }
}

extension BrowserModel {
    /// Runs a slow file action off the main thread, then selects the file it made.
    func quickAction(_ title: String, _ work: @escaping @Sendable () throws -> URL) {
        guard !busy else { return }
        busy = true
        Task {
            do {
                let url = try await Task.detached(priority: .userInitiated, operation: work).value
                await reload(); selection = [url]
                FileLibrary.shared.log(title, detail: url.lastPathComponent)
            } catch { busy = false; problem = error.localizedDescription }
        }
    }
}

// MARK: - Document scanner (camera) with searchable PDF output

struct DocumentScannerView: UIViewControllerRepresentable {
    let onFinish: ([UIImage]) -> Void
    let onCancel: () -> Void
    func makeCoordinator() -> Coordinator { Coordinator(self) }
    func makeUIViewController(context: Context) -> VNDocumentCameraViewController { let c = VNDocumentCameraViewController(); c.delegate = context.coordinator; return c }
    func updateUIViewController(_ controller: VNDocumentCameraViewController, context: Context) {}
    final class Coordinator: NSObject, VNDocumentCameraViewControllerDelegate {
        let parent: DocumentScannerView
        init(_ parent: DocumentScannerView) { self.parent = parent }
        func documentCameraViewController(_ controller: VNDocumentCameraViewController, didFinishWith scan: VNDocumentCameraScan) { parent.onFinish((0..<scan.pageCount).map { scan.imageOfPage(at: $0) }) }
        func documentCameraViewControllerDidCancel(_ controller: VNDocumentCameraViewController) { parent.onCancel() }
        func documentCameraViewController(_ controller: VNDocumentCameraViewController, didFailWithError error: Error) { parent.onCancel() }
    }
}

enum ScanPDF {
    /// Builds a PDF from scanned pages. With `searchable`, recognised words are laid over each page almost invisibly, so the PDF can be searched.
    static func make(_ pages: [UIImage], searchable: Bool, to url: URL) throws {
        let renderer = UIGraphicsPDFRenderer(bounds: CGRect(x: 0, y: 0, width: 612, height: 792))
        try renderer.writePDF(to: url) { context in
            for image in pages {
                let ratio = image.size.width / max(image.size.height, 1)
                let rect = ratio > 612.0 / 792.0 ? CGRect(x: 0, y: 0, width: 612, height: 612 / ratio) : CGRect(x: 0, y: 0, width: 792 * ratio, height: 792)
                context.beginPage(withBounds: rect, pageInfo: [:])
                image.draw(in: rect)
                guard searchable, let cg = image.cgImage else { continue }
                let request = VNRecognizeTextRequest(); request.recognitionLevel = .accurate
                try? VNImageRequestHandler(cgImage: cg).perform([request])
                for observation in request.results ?? [] {
                    guard let text = observation.topCandidates(1).first?.string else { continue }
                    let box = observation.boundingBox
                    let frame = CGRect(x: box.minX * rect.width, y: (1 - box.maxY) * rect.height, width: box.width * rect.width, height: box.height * rect.height)
                    let attributes: [NSAttributedString.Key: Any] = [.font: UIFont.systemFont(ofSize: max(frame.height * 0.8, 4)), .foregroundColor: UIColor(white: 0, alpha: 0.01)]
                    (text as NSString).draw(in: frame, withAttributes: attributes)
                }
            }
        }
    }
}

struct ScanSaveView: View {
    let pages: [UIImage]
    let folder: URL
    let onSaved: (URL) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var name = "Scan " + Date().formatted(date: .numeric, time: .omitted).replacingOccurrences(of: "/", with: "-")
    @State private var searchable = true
    @State private var saving = false
    @State private var error: String?
    var body: some View {
        NavigationStack {
            Form {
                Section { ScrollView(.horizontal) { HStack { ForEach(Array(pages.enumerated()), id: \.offset) { _, page in Image(uiImage: page).resizable().scaledToFit().frame(height: 120).clipShape(RoundedRectangle(cornerRadius: 6)) } } }; Text("\(pages.count) page\(pages.count == 1 ? "" : "s")").font(.caption).foregroundStyle(.secondary) }
                Section { TextField("File name", text: $name).autocorrectionDisabled(); Toggle("Make text searchable (OCR)", isOn: $searchable) } footer: { Text("Saved as a PDF in \(folder.lastPathComponent). Text is recognised on the iPhone; nothing is sent anywhere.") }
                if let error { Text(error).foregroundStyle(.red) }
            }.navigationTitle("Save scan").navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .topBarTrailing) { Button(saving ? "Saving…" : "Save") { save() }.disabled(saving || name.trimmingCharacters(in: .whitespaces).isEmpty) }
            }
        }
    }
    private func save() {
        saving = true
        let target = FileOperations.uniqueURL(folder.appendingPathComponent(name.trimmingCharacters(in: .whitespaces) + ".pdf"))
        let pages = pages, searchable = searchable
        Task.detached {
            do { try ScanPDF.make(pages, searchable: searchable, to: target); await MainActor.run { onSaved(target); dismiss() } }
            catch { await MainActor.run { self.error = error.localizedDescription; saving = false } }
        }
    }
}

// MARK: - Extracted text

struct ExtractedTextView: View {
    let url: URL
    @Environment(\.dismiss) private var dismiss
    @State private var text = ""
    @State private var working = true
    @State private var error: String?
    var body: some View {
        NavigationStack {
            Group {
                if working { ProgressView("Reading text…") }
                else if let error { Text(error).foregroundStyle(.secondary).padding() }
                else if text.isEmpty { ContentUnavailableView("No text found", systemImage: "text.viewfinder") }
                else { ScrollView { Text(text).textSelection(.enabled).frame(maxWidth: .infinity, alignment: .leading).padding() } }
            }.navigationTitle("Text in \(url.lastPathComponent)").navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) { Button("Done") { dismiss() } }
                ToolbarItem(placement: .topBarTrailing) { Button("Copy", systemImage: "doc.on.doc") { UIPasteboard.general.string = text }.disabled(text.isEmpty) }
            }
        }.task {
            let source = url
            do { text = try await Task.detached(priority: .userInitiated) { try ImageQuickActions.extractText(source) }.value } catch { self.error = error.localizedDescription }
            working = false
        }
    }
}

struct PasswordPromptView: View {
    let title: String
    let onSubmit: (String) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var password = ""
    var body: some View {
        NavigationStack {
            Form { SecureField("Password", text: $password) }.navigationTitle(title).navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarLeading) { Button("Cancel") { dismiss() } }; ToolbarItem(placement: .topBarTrailing) { Button("Unlock") { onSubmit(password); dismiss() } } }
        }.presentationDetents([.medium])
    }
}

// MARK: - Tags (kept inside FileGlade) and folder colour and symbol

func tagColor(_ name: String) -> Color {
    let palette: [Color] = [.red, .orange, .yellow, .green, .teal, .blue, .indigo, .purple, .pink]
    return palette[abs(name.unicodeScalars.reduce(0) { $0 &+ Int($1.value) }) % palette.count]
}
let folderColors: [(name: String, color: Color)] = [("Yellow", .yellow), ("Orange", .orange), ("Red", .red), ("Pink", .pink), ("Purple", .purple), ("Indigo", .indigo), ("Blue", .blue), ("Teal", .teal), ("Green", .green), ("Gray", .gray)]
func folderColor(_ name: String?) -> Color { folderColors.first { $0.name == name }?.color ?? .yellow }

struct TagEditorView: View {
    let urls: [URL]
    @ObservedObject private var library = FileLibrary.shared
    @Environment(\.dismiss) private var dismiss
    @State private var newTag = ""
    private func has(_ tag: String) -> Bool { urls.allSatisfy { library.tags($0).contains(tag) } }
    var body: some View {
        NavigationStack {
            List {
                Section("Add a tag") {
                    HStack { TextField("New tag", text: $newTag).textInputAutocapitalization(.never); Button("Add") { let t = newTag.trimmingCharacters(in: .whitespaces); if !t.isEmpty { library.addTag(t, urls: urls); newTag = "" } }.disabled(newTag.trimmingCharacters(in: .whitespaces).isEmpty) }
                }
                Section("Tags") {
                    let all = Set(library.allTags.map(\.name)).union(urls.flatMap { library.tags($0) }).sorted()
                    if all.isEmpty { Text("No tags yet.").foregroundStyle(.secondary) }
                    ForEach(all, id: \.self) { tag in
                        Button { if has(tag) { library.removeTag(tag, urls: urls) } else { library.addTag(tag, urls: urls) } } label: {
                            HStack { Circle().fill(tagColor(tag)).frame(width: 12, height: 12); Text(tag).foregroundStyle(.primary); Spacer(); if has(tag) { Image(systemName: "checkmark").foregroundStyle(.blue) } }
                        }
                    }
                }
            }.navigationTitle("Tags").navigationBarTitleDisplayMode(.inline).toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Done") { dismiss() } } }
        }.presentationDetents([.medium, .large])
    }
}

struct CustomizeFolderView: View {
    let url: URL
    let onChanged: () -> Void
    @ObservedObject private var library = FileLibrary.shared
    @Environment(\.dismiss) private var dismiss
    @State private var color: String?
    @State private var symbol = ""
    private let symbols = ["folder.fill", "star.fill", "heart.fill", "briefcase.fill", "graduationcap.fill", "house.fill", "camera.fill", "music.note", "doc.text.fill", "banknote.fill", "airplane", "cart.fill"]
    var body: some View {
        NavigationStack {
            Form {
                Section { ZStack { ItemGlyph(isDirectory: true, icon: "folder.fill", style: (color, symbol.isEmpty ? nil : symbol), size: 80) }.frame(maxWidth: .infinity) }
                Section("Colour") { LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 5), spacing: 14) { ForEach(folderColors, id: \.name) { item in Circle().fill(item.color).frame(width: 34, height: 34).overlay(Circle().strokeBorder(Color.primary, lineWidth: color == item.name ? 3 : 0)).onTapGesture { color = item.name } } } }
                Section("Symbol or emoji") {
                    TextField("Type an emoji", text: $symbol).onChange(of: symbol) { _, new in if new.count > 2 && !symbols.contains(new) { symbol = String(new.prefix(2)) } }
                    ScrollView(.horizontal) { HStack(spacing: 14) { ForEach(symbols, id: \.self) { name in Image(systemName: name).font(.title2).padding(8).background(symbol == name ? Color.accentColor.opacity(0.2) : .clear, in: RoundedRectangle(cornerRadius: 8)).onTapGesture { symbol = name } } } }
                }
                Section { Button("Reset to default", role: .destructive) { color = nil; symbol = "" } }
            }.navigationTitle("Customize folder").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarLeading) { Button("Cancel") { dismiss() } }; ToolbarItem(placement: .topBarTrailing) { Button("Save") { library.setFolderStyle(url, color: color, symbol: symbol.isEmpty ? nil : symbol); onChanged(); dismiss() } } }
            .onAppear { let style = library.folderStyle(url); color = style.color; symbol = style.symbol ?? "" }
        }
    }
}

/// A file or folder icon, using the folder's chosen colour and symbol.
struct ItemGlyph: View {
    let isDirectory: Bool
    let icon: String
    var style: (color: String?, symbol: String?) = (nil, nil)
    var size: CGFloat = 46
    var body: some View {
        if isDirectory {
            ZStack {
                Image(systemName: "folder.fill").font(.system(size: size)).foregroundStyle(folderColor(style.color))
                if let symbol = style.symbol {
                    if symbol.unicodeScalars.first.map({ $0.properties.isEmoji && $0.value > 255 }) ?? false { Text(symbol).font(.system(size: size * 0.4)).offset(y: size * 0.05) }
                    else { Image(systemName: symbol).font(.system(size: size * 0.3, weight: .semibold)).foregroundStyle(.white.opacity(0.9)).offset(y: size * 0.05) }
                }
            }
        } else { Image(systemName: icon).font(.system(size: size)).foregroundStyle(.blue) }
    }
}

// MARK: - Browse hub (Locations, Favorites, Tags, Recents, Deleted, Computer access)

struct BrowseHubView: View {
    @ObservedObject var model: BrowserModel
    @ObservedObject private var library = FileLibrary.shared
    let open: (URL) -> Void
    @State private var picking = false
    @State private var showComputer = false
    @State private var scanning = false
    @State private var scannedPages: [UIImage]?
    private var favorites: [LibraryRecord] { library.records.filter { $0.starred && FileManager.default.fileExists(atPath: $0.path) && PowerFile(URL(fileURLWithPath: $0.path)).directory } }
    var body: some View {
        NavigationStack {
            List {
                Section("Locations") {
                    Button { model.openLocal(); open(model.local) } label: { Label("FileGlade files", systemImage: "iphone").foregroundStyle(.primary) }
                    ForEach(model.locations) { location in Button { model.openLocation(location); open(model.directory) } label: { Label(location.name, systemImage: "folder.fill").foregroundStyle(.primary) } }
                        .onDelete { offsets in for i in offsets { model.removeDashboardLocation(model.locations[i]) } }
                        .onMove { model.moveLocations(from: $0, to: $1) }
                    Button { picking = true } label: { Label("Add a folder…", systemImage: "folder.badge.plus") }
                }
                if !favorites.isEmpty {
                    Section("Favorites") { ForEach(favorites) { record in Button { model.revealItem(library.resolve(record)); open(model.directory) } label: { Label(record.name, systemImage: "star.fill").foregroundStyle(.primary) } } }
                }
                Section("Tags") {
                    if library.allTags.isEmpty { Text("Tag files from their touch-and-hold menu.").font(.footnote).foregroundStyle(.secondary) }
                    ForEach(library.allTags, id: \.name) { tag in NavigationLink { TaggedItemsView(tag: tag.name, model: model, open: open) } label: { HStack { Circle().fill(tagColor(tag.name)).frame(width: 12, height: 12); Text(tag.name); Spacer(); Text("\(tag.count)").foregroundStyle(.secondary) } } }
                }
                Section {
                    NavigationLink { LibraryToolsView { url in model.revealItem(url); open(model.directory) } } label: { Label("Recents and starred", systemImage: "clock") }
                    NavigationLink { DeletedView(model: model) } label: { Label("Recently Deleted", systemImage: "trash") }
                    Button { scanning = true } label: { Label("Scan documents", systemImage: "doc.viewfinder") }
                    Button { showComputer = true } label: { Label("Computer access (USB)", systemImage: "cable.connector") }
                }
                Section { Button { if let url = URL(string: "shareddocuments://") { UIApplication.shared.open(url) } } label: { Label("Open Apple's Files app", systemImage: "arrow.up.forward.app") } } footer: { Text("Items shared with you through iCloud, and iCloud sharing and collaboration, are managed by Apple's Files app.") }
            }.navigationTitle("Browse").toolbar { EditButton() }
            .fileImporter(isPresented: $picking, allowedContentTypes: [.folder]) { result in if case .success(let url) = result { model.addLocation(url) } }
            .sheet(isPresented: $showComputer) { ComputerAccessView() }
            .fullScreenCover(isPresented: $scanning) { DocumentScannerView(onFinish: { pages in scanning = false; if !pages.isEmpty { scannedPages = pages } }, onCancel: { scanning = false }).ignoresSafeArea() }
            .sheet(isPresented: Binding(get: { scannedPages != nil }, set: { if !$0 { scannedPages = nil } })) {
                ScanSaveView(pages: scannedPages ?? [], folder: model.directory) { url in Task { await model.reload(); model.selection = [url] } }
            }
        }
    }
}

struct TaggedItemsView: View {
    let tag: String
    @ObservedObject var model: BrowserModel
    @ObservedObject private var library = FileLibrary.shared
    let open: (URL) -> Void
    var body: some View {
        List(library.items(tagged: tag)) { record in
            Button { model.revealItem(library.resolve(record)); open(model.directory) } label: { Label(record.name, systemImage: FileManager.default.fileExists(atPath: record.path) ? "doc" : "exclamationmark.triangle").foregroundStyle(.primary) }
        }.navigationTitle(tag)
    }
}
