import SwiftUI
import AppKit
import ImageIO
import UniformTypeIdentifiers

// MARK: - Photo metadata (EXIF) and removing it

enum ImageMetadata {
    static func summary(_ url: URL) -> [(label: String, value: String)] {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil), let props = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any] else { return [] }
        let tiff = props[kCGImagePropertyTIFFDictionary] as? [CFString: Any] ?? [:]
        let exif = props[kCGImagePropertyExifDictionary] as? [CFString: Any] ?? [:]
        let gps = props[kCGImagePropertyGPSDictionary] as? [CFString: Any]
        var rows: [(String, String)] = []
        if let w = props[kCGImagePropertyPixelWidth] as? Int, let h = props[kCGImagePropertyPixelHeight] as? Int { rows.append(("Size", "\(w) × \(h) pixels")) }
        let camera = [tiff[kCGImagePropertyTIFFMake] as? String, tiff[kCGImagePropertyTIFFModel] as? String].compactMap { $0 }.joined(separator: " ")
        if !camera.isEmpty { rows.append(("Camera", camera)) }
        if let lens = exif[kCGImagePropertyExifLensModel] as? String { rows.append(("Lens", lens)) }
        if let date = exif[kCGImagePropertyExifDateTimeOriginal] as? String { rows.append(("Taken", date)) }
        if let f = exif[kCGImagePropertyExifFNumber] as? Double { rows.append(("Aperture", "f/" + String(format: "%.1f", f))) }
        if let t = exif[kCGImagePropertyExifExposureTime] as? Double, t > 0 { rows.append(("Exposure", t < 1 ? "1/\(Int((1 / t).rounded())) s" : "\(t) s")) }
        if let iso = (exif[kCGImagePropertyExifISOSpeedRatings] as? [Int])?.first { rows.append(("ISO", String(iso))) }
        if let f = exif[kCGImagePropertyExifFocalLength] as? Double { rows.append(("Focal length", String(format: "%.0f mm", f))) }
        if let gps, let lat = gps[kCGImagePropertyGPSLatitude] as? Double, let lon = gps[kCGImagePropertyGPSLongitude] as? Double {
            rows.append(("Location", String(format: "%.4f, %.4f — this photo records where it was taken", lat, lon)))
        }
        return rows
    }
    static func hasLocation(_ url: URL) -> Bool { summary(url).contains { $0.label == "Location" } }

    /// Writes a copy without camera, date and location details. Its orientation is kept; JPEG copies are saved again at high quality.
    static func strippedCopy(of url: URL) throws -> URL {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil), let type = CGImageSourceGetType(source) else { throw NSError(domain: "FileGlade", code: 1, userInfo: [NSLocalizedDescriptionKey: "\(url.lastPathComponent) is not an image."]) }
        let props = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any] ?? [:]
        let orientation = props[kCGImagePropertyOrientation] ?? 1
        let target = TransferManager.uniqueURL(url.deletingLastPathComponent().appendingPathComponent(url.deletingPathExtension().lastPathComponent + " (no metadata)." + url.pathExtension))
        guard let destination = CGImageDestinationCreateWithURL(target as CFURL, type, 1, nil) else { throw NSError(domain: "FileGlade", code: 2, userInfo: [NSLocalizedDescriptionKey: "Cannot create \(target.lastPathComponent)."]) }
        // The picture is decoded and saved again with only its orientation, so no camera, date or location tags can carry over.
        guard let pixels = CGImageSourceCreateImageAtIndex(source, 0, nil) else { throw NSError(domain: "FileGlade", code: 4, userInfo: [NSLocalizedDescriptionKey: "Could not read \(url.lastPathComponent)."]) }
        CGImageDestinationAddImage(destination, pixels, [kCGImagePropertyOrientation: orientation, kCGImageDestinationLossyCompressionQuality: 0.95] as CFDictionary)
        guard CGImageDestinationFinalize(destination) else { throw NSError(domain: "FileGlade", code: 3, userInfo: [NSLocalizedDescriptionKey: "Could not save \(target.lastPathComponent)."]) }
        return target
    }
}

// MARK: - Similar file names

enum SimilarNames {
    static func key(_ name: String) -> String {
        var base = (name as NSString).deletingPathExtension.lowercased()
        let patterns = [#"\s*\(\d+\)$"#, #"[\s_-]*copy(\s*\d+)?$"#, #"[\s_-]*\d+$"#, #"[\s_-]*(final|new|old|backup|edited)$"#]
        var changed = true
        while changed { changed = false; for p in patterns { if let r = base.range(of: p, options: .regularExpression), r.lowerBound != base.startIndex { base.removeSubrange(r); changed = true } } }
        return base.filter { $0.isLetter || $0.isNumber }
    }
    static func distance(_ a: String, _ b: String) -> Int {
        let x = Array(a), y = Array(b)
        if x.isEmpty { return y.count }; if y.isEmpty { return x.count }
        var previous = Array(0...y.count)
        for i in 1...x.count {
            var row = [i]
            for j in 1...y.count { row.append(min(previous[j] + 1, row[j - 1] + 1, previous[j - 1] + (x[i - 1] == y[j - 1] ? 0 : 1))) }
            previous = row
        }
        return previous[y.count]
    }
    /// Groups of files in the same folder tree whose names differ only by copy numbers, "final", small typos and the like.
    static func groups(in root: URL, limit: Int = 5000) -> [[URL]] {
        var files: [URL] = []
        if let walk = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.isDirectoryKey], options: [.skipsHiddenFiles, .skipsPackageDescendants]) {
            for case let url as URL in walk { if (try? url.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) != true { files.append(url); if files.count >= limit { break } } }
        }
        var byKey: [String: [URL]] = [:]
        for file in files { let k = key(file.lastPathComponent) + "." + file.pathExtension.lowercased(); if k.count > 2 { byKey[k, default: []].append(file) } }
        var result = byKey.values.filter { $0.count > 1 }.map { $0.sorted { $0.path < $1.path } }
        // Near matches (one or two letters different) among the rest, for names long enough to be meaningful.
        let singles = byKey.filter { $0.value.count == 1 && $0.key.count >= 8 }.map { ($0.key, $0.value[0]) }.sorted { $0.0 < $1.0 }.prefix(1500)
        var used = Set<URL>()
        let list = Array(singles)
        for i in 0..<list.count {
            if used.contains(list[i].1) { continue }
            var group = [list[i].1]
            for j in (i + 1)..<max(i + 1, list.count) where abs(list[j].0.count - list[i].0.count) <= 2 && list[j].1.pathExtension.lowercased() == list[i].1.pathExtension.lowercased() && distance(list[i].0, list[j].0) <= 2 { group.append(list[j].1); used.insert(list[j].1) }
            if group.count > 1 { result.append(group) }
        }
        return result.sorted { $0[0].lastPathComponent.lowercased() < $1[0].lastPathComponent.lowercased() }
    }
}

// MARK: - Sensitive files (names and quick content checks; everything stays on the Mac)

enum SensitiveScanner {
    struct Finding: Identifiable { let id = UUID(); let url: URL; let reason: String }
    private static let nameRules: [(String, String)] = [
        (#"^id_(rsa|dsa|ecdsa|ed25519)$"#, "SSH private key"), (#"\.(pem|key|p12|pfx|jks|keystore|ppk)$"#, "Key or certificate file"), (#"^\.env(\..+)?$"#, "Environment file that often holds passwords"),
        (#"\.kdbx?$"#, "Password-manager database"), (#"\.ovpn$"#, "VPN profile"), (#"(^|[ _-])(passwords?|passwd|credentials?|secrets?|seed[ _-]?phrase|recovery[ _-]?codes?)([ _.-]|$)"#, "Name suggests passwords or secrets"),
        (#"^(wallet|keystore)(\.dat|\.json)?$"#, "Cryptocurrency wallet")
    ]
    private static let contentRules: [(String, String)] = [
        (#"-----BEGIN (RSA |EC |OPENSSH |DSA |ENCRYPTED )?PRIVATE KEY-----"#, "Contains a private key"), (#"AKIA[0-9A-Z]{16}"#, "Looks like an Amazon access key"),
        (#"xox[baprs]-[0-9A-Za-z-]{10,}"#, "Looks like a Slack token"), (#"ghp_[A-Za-z0-9]{36}"#, "Looks like a GitHub token"), (#"(?i)password\s*[:=]\s*\S{4,}"#, "Contains a password assignment")
    ]
    private static let textExtensions: Set<String> = ["txt", "md", "json", "yml", "yaml", "env", "conf", "config", "ini", "toml", "xml", "csv", "log", "sh", "py", "js", "ts", "plist", "properties", "pem", "key", ""]
    static func scan(_ root: URL, limit: Int = 30000, cancel: CancelToken? = nil) -> [Finding] {
        var out: [Finding] = []
        let nameRegex = nameRules.compactMap { (try? NSRegularExpression(pattern: $0.0, options: .caseInsensitive), $0.1) as? (NSRegularExpression, String) }
        let contentRegex = contentRules.compactMap { (try? NSRegularExpression(pattern: $0.0), $0.1) as? (NSRegularExpression, String) }
        guard let walk = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.isDirectoryKey, .fileSizeKey], options: [.skipsPackageDescendants]) else { return [] }
        var seen = 0
        for case let url as URL in walk {
            if cancel?.isCancelled == true || seen > limit { break }
            let v = try? url.resourceValues(forKeys: [.isDirectoryKey, .fileSizeKey])
            if v?.isDirectory == true { if url.lastPathComponent == "node_modules" || url.lastPathComponent == ".git" { walk.skipDescendants() }; continue }
            seen += 1
            let name = url.lastPathComponent
            if let hit = nameRegex.first(where: { $0.0.firstMatch(in: name, range: NSRange(name.startIndex..., in: name)) != nil }) { out.append(Finding(url: url, reason: hit.1)); continue }
            if let size = v?.fileSize, size > 0, size <= 256_000, textExtensions.contains(url.pathExtension.lowercased()), let data = try? Data(contentsOf: url, options: .mappedIfSafe), let text = String(data: data, encoding: .utf8) {
                if let hit = contentRegex.first(where: { $0.0.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)) != nil }) { out.append(Finding(url: url, reason: hit.1)) }
            }
        }
        return out.sorted { $0.url.path < $1.url.path }
    }
}

struct SensitiveFilesView: View {
    let root: URL
    let onShow: (URL) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var findings: [SensitiveScanner.Finding] = []
    @State private var scanning = true
    @State private var token = CancelToken()
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Sensitive files").font(.title3.bold())
            Text("Looks in \(root.path) for keys, passwords and secrets by name and by a quick look inside small text files. Nothing leaves your Mac and nothing is changed.").font(.caption).foregroundStyle(.secondary)
            if scanning { HStack { ProgressView().controlSize(.small); Text("Scanning…").font(.caption) } }
            else if findings.isEmpty { Text("Nothing suspicious found.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity) }
            else {
                List(findings) { finding in
                    HStack {
                        Image(systemName: "exclamationmark.shield.fill").foregroundStyle(.orange)
                        VStack(alignment: .leading, spacing: 2) { Text(finding.url.lastPathComponent); Text(finding.reason).font(.caption).foregroundStyle(.orange); Text(finding.url.deletingLastPathComponent().path).font(.caption2).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head) }
                        Spacer()
                        Button("Show") { dismiss(); onShow(finding.url) }.controlSize(.small)
                    }
                }
            }
            HStack { Text("\(findings.count) found").font(.caption).foregroundStyle(.secondary); Spacer(); Button("Done") { token.cancel(); dismiss() }.keyboardShortcut(.cancelAction) }
        }.padding(16).frame(width: 680, height: 500)
        .task { let r = root, t = token; findings = await Task.detached { SensitiveScanner.scan(r, cancel: t) }.value; scanning = false }
    }
}

struct SimilarNamesView: View {
    let root: URL
    let onShow: (URL) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var groups: [[URL]] = []
    @State private var scanning = true
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Similar file names").font(.title3.bold())
            Text("Files in \(root.lastPathComponent) and its folders whose names differ only by copy numbers, “final”, “backup” or small typos.").font(.caption).foregroundStyle(.secondary)
            if scanning { ProgressView() }
            else if groups.isEmpty { Text("No similar names found.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity) }
            else {
                List(Array(groups.enumerated()), id: \.offset) { _, group in
                    Section {
                        ForEach(group, id: \.self) { url in
                            HStack { Text(url.lastPathComponent); Spacer(); Text(fileBytes(Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0))).font(.caption).foregroundStyle(.secondary); Button("Show") { dismiss(); onShow(url) }.controlSize(.small) }
                            Text(url.deletingLastPathComponent().path).font(.caption2).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
                        }
                    }
                }
            }
            HStack { Text("\(groups.count) group\(groups.count == 1 ? "" : "s")").font(.caption).foregroundStyle(.secondary); Spacer(); Button("Done") { dismiss() }.keyboardShortcut(.cancelAction) }
        }.padding(16).frame(width: 680, height: 500)
        .task { let r = root; groups = await Task.detached { SimilarNames.groups(in: r) }.value; scanning = false }
    }
}

// MARK: - Set file dates

struct FileDatesView: View {
    let urls: [URL]
    let onDone: () -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var date = Date()
    @State private var setCreated = true
    @State private var message = ""
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Set file dates").font(.title3.bold())
            Text("\(urls.count) item\(urls.count == 1 ? "" : "s") selected").font(.caption).foregroundStyle(.secondary)
            DatePicker("Date modified", selection: $date)
            Toggle("Also set the creation date", isOn: $setCreated)
            Text("This changes the dates stored on the files. It cannot be undone, so note the old dates first if you need them.").font(.caption).foregroundStyle(.orange)
            if !message.isEmpty { Text(message).font(.caption) }
            HStack { Spacer(); Button("Cancel") { dismiss() }.keyboardShortcut(.cancelAction); Button("Apply") { apply() }.keyboardShortcut(.defaultAction) }
        }.padding(18).frame(width: 440)
    }
    private func apply() {
        var failed = 0
        for url in urls {
            var attributes: [FileAttributeKey: Any] = [.modificationDate: date]
            if setCreated { attributes[.creationDate] = date }
            do { try FileManager.default.setAttributes(attributes, ofItemAtPath: url.path) } catch { failed += 1 }
        }
        message = failed == 0 ? "Updated \(urls.count) item\(urls.count == 1 ? "" : "s")." : "\(failed) could not be changed."
        onDone()
        if failed == 0 { dismiss() }
    }
}

// MARK: - Photo metadata section for the details pane

struct PhotoMetadataSection: View {
    let url: URL
    @State private var rows: [(label: String, value: String)] = []
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            if !rows.isEmpty {
                Text("PHOTO DETAILS").font(.system(size: 9, weight: .medium)).tracking(0.8).foregroundStyle(.tertiary)
                ForEach(rows, id: \.label) { row in
                    HStack(alignment: .top) { Text(row.label).font(.system(size: 11)).foregroundStyle(.secondary).frame(width: 74, alignment: .leading); Text(row.value).font(.system(size: 11)).foregroundStyle(row.label == "Location" ? .orange : .primary).fixedSize(horizontal: false, vertical: true) }
                }
            }
        }.task(id: url) { rows = ImageMetadata.summary(url) }
    }
}
