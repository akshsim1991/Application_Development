import SwiftUI
import AppKit
import ImageCaptureCore
import UniformTypeIdentifiers

/// A photo, video or other file found on a connected iPhone or iPad.
struct DeviceFile: Identifiable, Hashable {
    let id: String
    let name: String
    let folder: String
    let size: Int64
    let date: Date?
    let isVideo: Bool
    let isImage: Bool
    fileprivate let item: ICCameraFile
    var kind: String { isVideo ? "Video" : isImage ? "Photo" : "File" }
    static func == (a: DeviceFile, b: DeviceFile) -> Bool { a.id == b.id }
    func hash(into hasher: inout Hasher) { hasher.combine(id) }
}

/// A connected iPhone or iPad, seen through macOS's image-capture service (the same one Image Capture and Photos use).
final class PhoneDevice: NSObject, ObservableObject, Identifiable, ICCameraDeviceDelegate {
    let camera: ICCameraDevice
    var id: String { camera.uuidString ?? camera.name ?? UUID().uuidString }
    @Published var name: String
    @Published var model: String?
    @Published var files: [DeviceFile] = []
    @Published var ready = false
    @Published var status = "Connecting…"
    @Published var thumbnails: [String: NSImage] = [:]
    private var pendingModel = true
    private var allFiles: [ICCameraFile] = []

    var kindName: String { camera.productKind ?? "iPhone" }
    var title: String { model ?? kindName }

    init(_ camera: ICCameraDevice) {
        self.camera = camera
        name = camera.name ?? "iPhone"
        super.init()
        camera.delegate = self
        camera.requestOpenSession()
    }
    deinit { camera.requestCloseSession() }

    // MARK: ICDeviceDelegate
    func didRemove(_ device: ICDevice) { DispatchQueue.main.async { self.ready = false; self.status = "Disconnected" } }
    func device(_ device: ICDevice, didOpenSessionWithError error: Error?) {
        DispatchQueue.main.async { self.status = error == nil ? "Reading files…" : "Unlock your iPhone, tap Trust, then try again. (\(error!.localizedDescription))" }
    }
    func device(_ device: ICDevice, didCloseSessionWithError error: Error?) {}
    func deviceDidBecomeReady(_ device: ICDevice) {}

    // MARK: ICCameraDeviceDelegate
    func cameraDevice(_ camera: ICCameraDevice, didAdd items: [ICCameraItem]) { collect(items) }
    func cameraDevice(_ camera: ICCameraDevice, didRemove items: [ICCameraItem]) {
        let gone = Set(items.compactMap { ($0 as? ICCameraFile).map(Self.identifier) })
        DispatchQueue.main.async { self.files.removeAll { gone.contains($0.id) } }
    }
    func cameraDevice(_ camera: ICCameraDevice, didRenameItems items: [ICCameraItem]) {}
    func cameraDeviceDidChangeCapability(_ camera: ICCameraDevice) {}
    func cameraDevice(_ camera: ICCameraDevice, didReceivePTPEvent eventData: Data) {}
    func cameraDeviceDidRemoveAccessRestriction(_ device: ICDevice) {}
    func cameraDeviceDidEnableAccessRestriction(_ device: ICDevice) { DispatchQueue.main.async { self.status = "Unlock your iPhone to see its files." } }
    func deviceDidBecomeReady(withCompleteContentCatalog device: ICCameraDevice) {
        DispatchQueue.main.async { self.ready = true; self.status = "\(self.files.count) items" }
        if pendingModel, let first = allFiles.first(where: { ($0.uti ?? "").contains("image") }) { first.requestMetadata() }
    }
    func cameraDevice(_ camera: ICCameraDevice, didReceiveThumbnail thumbnail: CGImage?, for item: ICCameraItem, error: Error?) {
        guard let thumbnail, let file = item as? ICCameraFile else { return }
        let key = Self.identifier(file)
        let image = NSImage(cgImage: thumbnail, size: NSSize(width: thumbnail.width, height: thumbnail.height))
        DispatchQueue.main.async { self.thumbnails[key] = image }
    }
    func cameraDevice(_ camera: ICCameraDevice, didReceiveMetadata metadata: [AnyHashable: Any]?, for item: ICCameraItem, error: Error?) {
        guard pendingModel else { return }
        let tiff = (metadata?["{TIFF}"] as? [String: Any]) ?? (metadata?["TIFF"] as? [String: Any])
        if let model = tiff?["Model"] as? String, !model.isEmpty { pendingModel = false; DispatchQueue.main.async { self.model = model } }
    }

    private static func identifier(_ file: ICCameraFile) -> String { "\(file.name ?? "")|\(file.fileSize)|\(Int(file.creationDate?.timeIntervalSince1970 ?? 0))" }
    private func collect(_ items: [ICCameraItem]) {
        var batch: [DeviceFile] = []
        func walk(_ item: ICCameraItem, folder: String) {
            if let dir = item as? ICCameraFolder { for child in dir.contents ?? [] { walk(child, folder: folder + "/" + (dir.name ?? "")) } }
            else if let file = item as? ICCameraFile {
                let uti = file.uti ?? ""
                let type = UTType(uti)
                batch.append(DeviceFile(id: Self.identifier(file), name: file.name ?? "Unnamed", folder: folder, size: Int64(file.fileSize), date: file.creationDate,
                                        isVideo: type?.conforms(to: .movie) ?? uti.contains("movie"), isImage: type?.conforms(to: .image) ?? uti.contains("image"), item: file))
                allFiles.append(file)
            }
        }
        for item in items { walk(item, folder: "") }
        DispatchQueue.main.async { self.files.append(contentsOf: batch); if !self.ready { self.status = "Reading files… \(self.files.count)" } }
    }

    func requestThumbnail(for file: DeviceFile) { if thumbnails[file.id] == nil { file.item.requestThumbnail() } }

    /// Copies files to a folder on the Mac. Existing files are never overwritten.
    func download(_ files: [DeviceFile], to folder: URL, progress: @escaping (Int, Int) -> Void, done: @escaping ([String]) -> Void) {
        var finished = 0
        var errors: [String] = []
        let lock = NSLock()
        guard !files.isEmpty else { done([]); return }
        let queue = DispatchQueue(label: "fileglade.device.download")
        queue.async {
            for file in files {
                let semaphore = DispatchSemaphore(value: 0)
                let target = TransferManager.uniqueURL(folder.appendingPathComponent(file.name))
                file.item.requestDownload(options: [.downloadsDirectoryURL: folder, .saveAsFilename: target.lastPathComponent, .overwrite: false]) { _, error in
                    lock.lock(); finished += 1; if let error { errors.append("\(file.name): \(error.localizedDescription)") }; lock.unlock()
                    progress(finished, files.count); semaphore.signal()
                }
                semaphore.wait()
            }
            done(errors)
        }
    }
}

final class DeviceStore: NSObject, ObservableObject, ICDeviceBrowserDelegate {
    static let shared = DeviceStore()
    @Published var phones: [PhoneDevice] = []
    private let browser = ICDeviceBrowser()
    private var started = false
    func start() {
        guard !started, !Prefs.selfTest else { return }
        started = true
        browser.delegate = self
        browser.browsedDeviceTypeMask = ICDeviceTypeMask(rawValue: ICDeviceTypeMask.camera.rawValue | ICDeviceLocationTypeMask.local.rawValue) ?? .camera
        browser.start()
    }
    func deviceBrowser(_ browser: ICDeviceBrowser, didAdd device: ICDevice, moreComing: Bool) {
        guard let camera = device as? ICCameraDevice else { return }
        let kind = (camera.productKind ?? "").lowercased()
        // Only phones and tablets; cameras and card readers are left to Image Capture.
        guard kind.contains("iphone") || kind.contains("ipad") || kind.contains("ipod") || (camera.name ?? "").lowercased().contains("iphone") || (camera.name ?? "").lowercased().contains("ipad") else { return }
        DispatchQueue.main.async { if !self.phones.contains(where: { $0.camera === camera }) { self.phones.append(PhoneDevice(camera)) } }
    }
    func deviceBrowser(_ browser: ICDeviceBrowser, didRemove device: ICDevice, moreGoing: Bool) {
        DispatchQueue.main.async { self.phones.removeAll { $0.camera === device } }
    }
}

enum DeviceFilter: String, CaseIterable, Identifiable { case all = "Everything", photos = "Photos", videos = "Videos", other = "Other files"; var id: String { rawValue } }

/// Browse everything a connected iPhone shares with the Mac, then copy it over.
struct DeviceBrowserView: View {
    @ObservedObject var phone: PhoneDevice
    let destination: () -> URL
    var embedded = false
    @Environment(\.dismiss) private var dismiss
    @State private var filter: DeviceFilter = .all
    @State private var selection = Set<String>()
    @State private var search = ""
    @State private var progress: (Int, Int)?
    @State private var message = ""
    private var shown: [DeviceFile] {
        phone.files.filter { f in
            (filter == .all || (filter == .photos && f.isImage) || (filter == .videos && f.isVideo) || (filter == .other && !f.isImage && !f.isVideo)) && (search.isEmpty || f.name.localizedCaseInsensitiveContains(search) || f.folder.localizedCaseInsensitiveContains(search))
        }.sorted { ($0.date ?? .distantPast) > ($1.date ?? .distantPast) }
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if !embedded {
                HStack(spacing: 10) {
                    Image(systemName: "iphone.gen3").font(.system(size: 28)).foregroundStyle(.blue)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(phone.name).font(.title3.bold())
                        Text(phone.title + " · " + phone.status).font(.caption).foregroundStyle(.secondary)
                    }
                    Spacer()
                    if !phone.ready { ProgressView().controlSize(.small) }
                }
            } else { Text(phone.status).font(.caption).foregroundStyle(.secondary) }
            HStack {
                Picker("", selection: $filter) { ForEach(DeviceFilter.allCases) { Text($0.rawValue).tag($0) } }.pickerStyle(.segmented).frame(width: 360)
                TextField("Search name or folder", text: $search).textFieldStyle(.roundedBorder).frame(maxWidth: 220)
                Spacer()
                Text("\(shown.count.formatted()) items").font(.caption).foregroundStyle(.secondary)
            }
            if phone.files.isEmpty && phone.ready { Text("No files are shared by this device. On the iPhone, unlock it and tap Trust when asked, then reopen.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity) }
            else {
                List(shown, selection: $selection) { file in
                    HStack(spacing: 10) {
                        Group {
                            if let image = phone.thumbnails[file.id] { Image(nsImage: image).resizable().aspectRatio(contentMode: .fill) }
                            else { Image(systemName: file.isVideo ? "film" : (file.isImage ? "photo" : "doc")).foregroundStyle(.secondary) }
                        }.frame(width: 44, height: 44).clipShape(RoundedRectangle(cornerRadius: 6)).background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 6))
                        VStack(alignment: .leading, spacing: 2) { Text(file.name).lineLimit(1); Text(file.folder.isEmpty ? "Camera roll" : file.folder).font(.caption2).foregroundStyle(.secondary) }
                        Spacer()
                        Text(file.kind).font(.caption).foregroundStyle(.secondary).frame(width: 52)
                        Text(file.date?.formatted(date: .abbreviated, time: .shortened) ?? "").font(.caption).foregroundStyle(.secondary).frame(width: 140, alignment: .leading)
                        Text(fileBytes(file.size)).font(.caption).foregroundStyle(.secondary).frame(width: 70, alignment: .trailing)
                    }.tag(file.id).onAppear { phone.requestThumbnail(for: file) }
                    .contextMenu { Button("Copy to This Folder") { copy([file]) } }
                }
            }
            HStack {
                if let progress { ProgressView(value: Double(progress.0), total: Double(max(progress.1, 1))).frame(width: 160); Text("\(progress.0) of \(progress.1)").font(.caption).foregroundStyle(.secondary) }
                else { Text(message.isEmpty ? "Select files, then copy them to the folder you are browsing." : message).font(.caption).foregroundStyle(.secondary) }
                Spacer()
                Button("Copy Selected to \(destination().lastPathComponent)") { copy(shown.filter { selection.contains($0.id) }) }.disabled(selection.isEmpty || progress != nil)
                Button("Copy All Shown") { copy(shown) }.disabled(shown.isEmpty || progress != nil)
                if !embedded { Button("Done") { dismiss() }.keyboardShortcut(.cancelAction) }
            }
        }.padding(18).frame(minWidth: embedded ? 0 : 820, minHeight: embedded ? 0 : 560)
    }
    private func copy(_ files: [DeviceFile]) {
        let folder = destination()
        progress = (0, files.count)
        phone.download(files, to: folder, progress: { done, total in DispatchQueue.main.async { progress = (done, total) } }, done: { errors in
            DispatchQueue.main.async {
                progress = nil
                message = errors.isEmpty ? "Copied \(files.count) item\(files.count == 1 ? "" : "s") to \(folder.lastPathComponent)." : "\(errors.count) failed: \(errors.first ?? "")"
                NotificationCenter.default.post(name: .init("FileGladeTransferDone"), object: nil)
            }
        })
    }
}
