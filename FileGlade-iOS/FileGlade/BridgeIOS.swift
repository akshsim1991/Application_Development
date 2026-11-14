import SwiftUI
import UIKit
import Photos
import ImageIO
import UniformTypeIdentifiers

/// Lets a Mac running FileGlade browse this iPhone over the USB cable. Read-only, off by default, and protected by a
/// pairing code. The server listens only on the phone itself, so nothing is reachable from Wi-Fi.
@MainActor final class BridgeController: ObservableObject {
    static let shared = BridgeController()
    @Published var enabled: Bool { didSet { UserDefaults.standard.set(enabled, forKey: "bridgeEnabled"); apply() } }
    @Published var code: String
    @Published var status = "Off"
    @Published var photosAllowed = false
    var rootsProvider: () -> [DashboardRoot] = { [] }
    private let server = BridgeHTTPServer()
    private let service: FileBridgeService
    private let cache = FileManager.default.temporaryDirectory.appendingPathComponent("FileGladeBridge", isDirectory: true)

    init() {
        enabled = UserDefaults.standard.bool(forKey: "bridgeEnabled")
        let saved = UserDefaults.standard.string(forKey: "bridgeCode") ?? ""
        let initial = saved.isEmpty ? Self.newCode() : saved
        code = initial
        UserDefaults.standard.set(initial, forKey: "bridgeCode")
        service = FileBridgeService(token: initial, roots: { [] }, info: { BridgeController.deviceInfo() })
        service.roots = { [weak self] in self?.currentRoots() ?? [] }
        service.extra = { [weak self] request in await self?.photosAndThumbs(request) }
        server.handler = { [service] request in await service.handle(request) }
        try? FileManager.default.createDirectory(at: cache, withIntermediateDirectories: true)
        apply()
    }
    static func newCode() -> String { String(format: "%06d", Int.random(in: 0...999_999)) }
    func regenerateCode() { code = Self.newCode(); UserDefaults.standard.set(code, forKey: "bridgeCode"); service.token = code }

    private func apply() {
        if enabled {
            do { try server.start(); status = "On — keep FileGlade open and the iPhone unlocked"; UIApplication.shared.isIdleTimerDisabled = true; requestPhotos() }
            catch { status = "Could not start: \(error.localizedDescription)"; enabled = false }
        } else { server.stop(); status = "Off"; UIApplication.shared.isIdleTimerDisabled = false }
    }
    private func requestPhotos() {
        PHPhotoLibrary.requestAuthorization(for: .readWrite) { [weak self] state in
            Task { @MainActor in self?.photosAllowed = (state == .authorized || state == .limited) }
        }
    }
    private func currentRoots() -> [(root: BridgeRoot, url: URL?)] {
        var list: [(root: BridgeRoot, url: URL?)] = rootsProvider().map { (BridgeRoot(id: $0.id, name: $0.name, kind: "files"), $0.url) }
        list.append((BridgeRoot(id: "photos", name: "Photos & Videos", kind: "photos"), nil))
        return list
    }

    // MARK: Device info
    static let models: [String: String] = [
        "iPhone10,1": "iPhone 8", "iPhone10,4": "iPhone 8", "iPhone10,2": "iPhone 8 Plus", "iPhone10,5": "iPhone 8 Plus", "iPhone10,3": "iPhone X", "iPhone10,6": "iPhone X",
        "iPhone11,2": "iPhone XS", "iPhone11,4": "iPhone XS Max", "iPhone11,6": "iPhone XS Max", "iPhone11,8": "iPhone XR", "iPhone12,1": "iPhone 11", "iPhone12,3": "iPhone 11 Pro",
        "iPhone12,5": "iPhone 11 Pro Max", "iPhone12,8": "iPhone SE (2nd generation)", "iPhone13,1": "iPhone 12 mini", "iPhone13,2": "iPhone 12", "iPhone13,3": "iPhone 12 Pro",
        "iPhone13,4": "iPhone 12 Pro Max", "iPhone14,4": "iPhone 13 mini", "iPhone14,5": "iPhone 13", "iPhone14,2": "iPhone 13 Pro", "iPhone14,3": "iPhone 13 Pro Max",
        "iPhone14,6": "iPhone SE (3rd generation)", "iPhone14,7": "iPhone 14", "iPhone14,8": "iPhone 14 Plus", "iPhone15,2": "iPhone 14 Pro", "iPhone15,3": "iPhone 14 Pro Max",
        "iPhone15,4": "iPhone 15", "iPhone15,5": "iPhone 15 Plus", "iPhone16,1": "iPhone 15 Pro", "iPhone16,2": "iPhone 15 Pro Max", "iPhone17,3": "iPhone 16", "iPhone17,4": "iPhone 16 Plus",
        "iPhone17,1": "iPhone 16 Pro", "iPhone17,2": "iPhone 16 Pro Max", "iPhone17,5": "iPhone 16e"
    ]
    nonisolated static func deviceInfo() -> BridgeInfo {
        var system = utsname(); uname(&system)
        let machine = withUnsafePointer(to: &system.machine) { $0.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) } }
        let home = URL(fileURLWithPath: NSHomeDirectory())
        let values = try? home.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey, .volumeTotalCapacityKey])
        return BridgeInfo(name: UIDevice.current.name, model: models[machine] ?? (machine.hasPrefix("iPad") ? "iPad" : machine), machine: machine, systemName: UIDevice.current.systemName,
                          systemVersion: UIDevice.current.systemVersion, appVersion: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "",
                          freeBytes: values?.volumeAvailableCapacityForImportantUsage ?? 0, totalBytes: Int64(values?.volumeTotalCapacity ?? 0))
    }

    // MARK: Photos and thumbnails
    private func photosAndThumbs(_ request: HTTPRequest) async -> HTTPResponse? {
        switch request.path {
        case "/photos": return await photoList(request)
        case "/photo/thumb": return await photoThumb(request)
        case "/photo/original": return await photoOriginal(request)
        case "/thumb": return fileThumb(request)
        default: return nil
        }
    }
    private func photoList(_ request: HTTPRequest) async -> HTTPResponse {
        guard PHPhotoLibrary.authorizationStatus(for: .readWrite) == .authorized || PHPhotoLibrary.authorizationStatus(for: .readWrite) == .limited else { return .text(403, "Allow Photos access for FileGlade on the iPhone.") }
        let offset = Int(request.query["offset"] ?? "0") ?? 0, limit = min(Int(request.query["limit"] ?? "300") ?? 300, 1000)
        let options = PHFetchOptions(); options.sortDescriptors = [NSSortDescriptor(key: "creationDate", ascending: false)]
        let assets = PHAsset.fetchAssets(with: options)
        var entries: [BridgeEntry] = []
        if offset < assets.count {
            for index in offset..<min(offset + limit, assets.count) {
                let asset = assets.object(at: index)
                let resource = PHAssetResource.assetResources(for: asset).first { $0.type == .photo || $0.type == .video } ?? PHAssetResource.assetResources(for: asset).first
                let size = (resource?.value(forKey: "fileSize") as? NSNumber)?.int64Value ?? 0
                entries.append(BridgeEntry(id: asset.localIdentifier, name: resource?.originalFilename ?? "Photo \(index)", isDirectory: false, size: size, modified: asset.creationDate?.timeIntervalSince1970, kind: asset.mediaType == .video ? "video" : "photo"))
            }
        }
        return .json(BridgeListing(entries: entries, total: assets.count))
    }
    private func asset(_ request: HTTPRequest) -> PHAsset? {
        guard let id = request.query["id"] else { return nil }
        return PHAsset.fetchAssets(withLocalIdentifiers: [id], options: nil).firstObject
    }
    private func photoThumb(_ request: HTTPRequest) async -> HTTPResponse {
        guard let asset = asset(request) else { return .text(404, "Unknown photo") }
        return await withCheckedContinuation { continuation in
            let options = PHImageRequestOptions(); options.deliveryMode = .fastFormat; options.resizeMode = .fast; options.isSynchronous = false; options.isNetworkAccessAllowed = false
            PHImageManager.default().requestImage(for: asset, targetSize: CGSize(width: 160, height: 160), contentMode: .aspectFill, options: options) { image, _ in
                if let data = image?.jpegData(compressionQuality: 0.6) { continuation.resume(returning: HTTPResponse(status: 200, headers: ["Content-Type": "image/jpeg"], body: .data(data))) }
                else { continuation.resume(returning: .text(404, "No thumbnail")) }
            }
        }
    }
    private func photoOriginal(_ request: HTTPRequest) async -> HTTPResponse {
        guard let asset = asset(request), let resource = PHAssetResource.assetResources(for: asset).first(where: { $0.type == .photo || $0.type == .video }) ?? PHAssetResource.assetResources(for: asset).first else { return .text(404, "Unknown photo") }
        // Keep only a few exported files around.
        pruneCache()
        let target = cache.appendingPathComponent(UUID().uuidString + "-" + resource.originalFilename)
        let options = PHAssetResourceRequestOptions(); options.isNetworkAccessAllowed = true
        let failed: Bool = await withCheckedContinuation { continuation in
            PHAssetResourceManager.default().writeData(for: resource, toFile: target, options: options) { error in continuation.resume(returning: error != nil) }
        }
        guard !failed, let size = (try? FileManager.default.attributesOfItem(atPath: target.path))?[.size] as? NSNumber else { return .text(500, "Could not read the photo") }
        return FileBridgeService.fileResponse(target, size: size.int64Value, rangeHeader: request.headers["range"])
    }
    private func pruneCache() {
        let files = (try? FileManager.default.contentsOfDirectory(at: cache, includingPropertiesForKeys: [.contentModificationDateKey])) ?? []
        func date(_ url: URL) -> Date { (try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast }
        let sorted = files.sorted { date($0) < date($1) }
        if sorted.count > 4 { for url in sorted.prefix(sorted.count - 4) { try? FileManager.default.removeItem(at: url) } }
    }
    private func fileThumb(_ request: HTTPRequest) -> HTTPResponse {
        guard let rootID = request.query["root"], let base = currentRoots().first(where: { $0.root.id == rootID })?.url else { return .text(404, "Unknown folder") }
        let comps = (request.query["path"] ?? "").split(separator: "/").map(String.init)
        guard !comps.contains("..") else { return .text(403, "Forbidden") }
        let url = comps.reduce(base) { $0.appendingPathComponent($1) }
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else { return .text(404, "Not an image") }
        let options: [CFString: Any] = [kCGImageSourceCreateThumbnailFromImageAlways: true, kCGImageSourceThumbnailMaxPixelSize: 160, kCGImageSourceCreateThumbnailWithTransform: true]
        guard let image = CGImageSourceCreateThumbnailAtIndex(source, 0, options as CFDictionary), let data = UIImage(cgImage: image).jpegData(compressionQuality: 0.6) else { return .text(404, "No thumbnail") }
        return HTTPResponse(status: 200, headers: ["Content-Type": "image/jpeg"], body: .data(data))
    }
}

struct ComputerAccessView: View {
    @ObservedObject private var bridge = BridgeController.shared
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        NavigationStack {
            List {
                Section {
                    Toggle("USB file access", isOn: $bridge.enabled)
                    Text(bridge.status).font(.footnote).foregroundStyle(.secondary)
                } footer: { Text("Lets a Mac running FileGlade browse and copy this iPhone's files and photos over the USB cable. It is read-only, protected by a pairing code, and not available over Wi-Fi.") }
                if bridge.enabled {
                    Section("Pairing code") {
                        Text(bridge.code).font(.system(size: 34, weight: .semibold, design: .monospaced)).frame(maxWidth: .infinity)
                        Button("Make a new code", systemImage: "arrow.clockwise") { bridge.regenerateCode() }
                        Text("On the Mac: connect the cable, tap Trust here if asked, open FileGlade, choose this iPhone under Devices and enter the code.").font(.footnote).foregroundStyle(.secondary)
                    }
                    Section("Shared") {
                        Label("FileGlade files and folders you allowed", systemImage: "folder")
                        Label(bridge.photosAllowed ? "Photos and videos" : "Photos and videos (allow Photos access when asked)", systemImage: "photo.on.rectangle")
                    }
                }
            }.navigationTitle("Computer access").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Done") { dismiss() } } }
        }
    }
}
