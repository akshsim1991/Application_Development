import Foundation
import PDFKit
import ImageIO
import AppKit

func expect(_ value: @autoclosure () -> Bool, _ message: String) throws { if !value() { throw powerError("TEST FAILED: " + message) } }
func mustFail(_ message: String, work: () throws -> Void) throws {
    do { try work() } catch { return }
    throw powerError("TEST FAILED: expected rejection: " + message)
}
func write(_ text: String, to url: URL) throws { try Data(text.utf8).write(to: url) }
actor TestGate {
    private var released = false
    private var waiter: CheckedContinuation<Void, Never>?
    func wait() async { if released { return }; await withCheckedContinuation { waiter = $0 } }
    func signal() { released = true; waiter?.resume(); waiter = nil }
}

@main struct PowerTests {
    static func main() async {
        do { try await run(); print("PASS: batch rename, swaps and rollback; ZIP roundtrip, traversal, symlink and CRC guards; streamed hashes and duplicates; text conflict protection; PDF merge; image export; scan cancellation.") }
        catch { fputs("\(error.localizedDescription)\n", stderr); exit(1) }
    }
    static func run() async throws {
        let fm = FileManager.default
        let root = powerURL(fm.temporaryDirectory).appendingPathComponent("FileGlade-power-tests-" + UUID().uuidString)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let a = root.appendingPathComponent("Report One.txt"), b = root.appendingPathComponent("Report Two.txt")
        try write("abc", to: a); try write("xyz", to: b)
        var rules = RenameOptions(); rules.find = "Report "; rules.replace = ""; rules.prefix = "Project-"; rules.numbered = true
        let plan = try PowerFS.planRename([a, b], options: rules)
        try expect(plan[0].destination.lastPathComponent == "Project-One_001.txt", "preserve extension and add padded counter")
        let undo = try PowerFS.rename(plan)
        try expect(!fm.fileExists(atPath: a.path) && fm.fileExists(atPath: plan[0].destination.path), "batch names applied")
        _ = try PowerFS.rename(undo)
        try expect(fm.fileExists(atPath: a.path), "undo restores original name")
        try mustFail("duplicate targets") { try PowerFS.validateRename([RenamePair(source: a, destination: b), RenamePair(source: b, destination: b)]) }
        let blocker = root.appendingPathComponent("Existing.txt"); try write("protected", to: blocker)
        try mustFail("existing destination") { _ = try PowerFS.rename([RenamePair(source: a, destination: blocker)]) }
        _ = try PowerFS.rename([RenamePair(source: a, destination: b), RenamePair(source: b, destination: a)])
        let swapped = try String(contentsOf: a, encoding: .utf8)
        try expect(swapped == "xyz", "two-phase rename supports swaps")
        _ = try PowerFS.rename([RenamePair(source: a, destination: b), RenamePair(source: b, destination: a)])
        let missing = root.appendingPathComponent("Missing.txt")
        try mustFail("partial batch rolls back") { _ = try PowerFS.rename([RenamePair(source: a, destination: root.appendingPathComponent("First.txt")), RenamePair(source: missing, destination: root.appendingPathComponent("Second.txt"))]) }
        try expect(fm.fileExists(atPath: a.path), "rollback kept source")

        let folder = root.appendingPathComponent("資料", isDirectory: true)
        try fm.createDirectory(at: folder.appendingPathComponent("Empty"), withIntermediateDirectories: true)
        let note = folder.appendingPathComponent("résumé.txt"); try write("Archive contents — exact bytes", to: note)
        let zip = try PowerFS.zip([folder, a], into: root, name: "Test.zip")
        let extracted = try PowerFS.unzip(zip, into: root)
        let unzippedText = try String(contentsOf: extracted.appendingPathComponent("資料/résumé.txt"), encoding: .utf8)
        try expect(unzippedText == "Archive contents — exact bytes", "Unicode archive roundtrip")
        try expect(fm.fileExists(atPath: extracted.appendingPathComponent("資料/Empty").path), "empty directory preserved")
        let extracted2 = try PowerFS.unzip(zip, into: root)
        try expect(extracted2.path != extracted.path, "extraction never overwrites an existing folder")
        let link = root.appendingPathComponent("Link")
        try fm.createSymbolicLink(at: link, withDestinationURL: folder)
        try mustFail("do not archive symlinks") { _ = try PowerFS.zip([link], into: root, name: "Link.zip") }

        let badZip = root.appendingPathComponent("Traversal.zip")
        try makeArchive(badZip, paths: ["../escaped.txt"], content: "escape")
        try mustFail("ZIP traversal") { _ = try PowerFS.unzip(badZip, into: root) }
        try expect(!fm.fileExists(atPath: root.appendingPathComponent("escaped.txt").path), "traversal wrote nothing outside staging")
        let conflict = root.appendingPathComponent("Conflicts.zip")
        try makeArchive(conflict, paths: ["A.txt", "a.txt"], content: "collision")
        try mustFail("case-folded archive collision") { _ = try PowerFS.unzip(conflict, into: root) }
        let corrupted = root.appendingPathComponent("Corrupt.zip")
        try makeArchive(corrupted, paths: ["payload.txt"], content: "unique-payload-to-corrupt")
        var bytes = try Data(contentsOf: corrupted)
        let payload = Data("unique-payload-to-corrupt".utf8)
        guard let range = bytes.range(of: payload) else { throw powerError("Missing ZIP test payload") }
        bytes[range.lowerBound] ^= 1
        try bytes.write(to: corrupted)
        try mustFail("ZIP integrity") { _ = try PowerFS.unzip(corrupted, into: root) }
        try expect(!fm.fileExists(atPath: root.appendingPathComponent("Corrupt extracted").path), "corrupt output was not published")

        let hash = try PowerFS.digest(a)
        try expect(hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "known SHA-256")
        let match = root.appendingPathComponent("Same contents.txt"); try fm.copyItem(at: a, to: match)
        let inventory = try PowerFS.inventory(root)
        let duplicateReport = try PowerFS.duplicates(inventory)
        try expect(duplicateReport.groups.contains { group in group.files.contains { $0.url.path == a.path } && group.files.contains { $0.url.path == match.path } }, "hash groups exact copies")
        try expect(!duplicateReport.groups.contains { group in group.files.contains { $0.url.path == a.path } && group.files.contains { $0.url.path == b.path } }, "same size alone does not mean duplicate")
        try expect(inventory.skipped > 0, "inventory skipped symlink")

        let snapshot = try PowerFS.readText(a)
        try write("Edited elsewhere", to: a)
        try mustFail("external text change") { _ = try PowerFS.saveText("Do not overwrite", snapshot: snapshot) }
        let newText = root.appendingPathComponent("New note.md")
        _ = try PowerFS.saveText("# A note", snapshot: TextSnapshot(url: newText, text: "", digest: nil))
        try mustFail("new text cannot overwrite") { _ = try PowerFS.saveText("Overwrite", snapshot: TextSnapshot(url: newText, text: "", digest: nil)) }
        let state = try PowerFS.readText(newText)
        _ = try PowerFS.saveText("# Updated", snapshot: state)
        try expect((try? String(contentsOf: newText, encoding: .utf8)) == "# Updated", "text saved with matching baseline")

        guard let context = CGContext(data: nil, width: 320, height: 200, bitsPerComponent: 8, bytesPerRow: 0, space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { throw powerError("Could not create test image") }
        context.setFillColor(NSColor.systemBlue.cgColor); context.fill(CGRect(x: 0, y: 0, width: 320, height: 200))
        guard let image = context.makeImage() else { throw powerError("Could not render test image") }
        let png = root.appendingPathComponent("Picture.png")
        guard let imageWriter = CGImageDestinationCreateWithURL(png as CFURL, "public.png" as CFString, 1, nil) else { throw powerError("Could not write test image") }
        CGImageDestinationAddImage(imageWriter, image, nil); try expect(CGImageDestinationFinalize(imageWriter), "image fixture written")
        let pdf = PDFDocument()
        let nsImage = NSImage(cgImage: image, size: NSSize(width: 320, height: 200))
        guard let page = PDFPage(image: nsImage) else { throw powerError("Could not make PDF fixture") }
        pdf.insert(page, at: 0)
        guard let secondPage = PDFPage(image: nsImage) else { throw powerError("Could not make second PDF page") }
        pdf.insert(secondPage, at: 1)
        let originalPDF = root.appendingPathComponent("Original.pdf"); try expect(pdf.write(to: originalPDF), "PDF fixture written")
        let combined = try PowerFS.combinePDFs([originalPDF, png, originalPDF], into: root, name: "Combined.pdf")
        try expect(PDFDocument(url: combined)?.pageCount == 5, "all pages of multi-page PDFs and images are merged")
        let resized = try PowerFS.resizeImages([png], into: root, maximum: 128, jpeg: true)
        guard let source = CGImageSourceCreateWithURL(resized[0] as CFURL, nil), let props = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any] else { throw powerError("No resized image properties") }
        try expect(props[kCGImagePropertyPixelWidth] as? Int == 128 && props[kCGImagePropertyPixelHeight] as? Int == 80, "image resized with aspect ratio")

        let gate = TestGate()
        let cancelled = Task.detached { await gate.wait(); return try PowerFS.inventory(root) }
        cancelled.cancel(); await gate.signal()
        do { _ = try await cancelled.value; throw powerError("TEST FAILED: scan ignored cancellation") } catch is CancellationError { }
    }
    static func makeArchive(_ url: URL, paths: [String], content: String) throws {
        let archive = try Archive(url: url, accessMode: .create)
        let data = Data(content.utf8)
        for path in paths { try archive.addEntry(with: path, type: .file, uncompressedSize: Int64(data.count), compressionMethod: .none) { offset, length in data.subdata(in: Int(offset)..<Int(offset) + length) } }
    }
}
