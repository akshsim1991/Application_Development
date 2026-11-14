import SwiftUI
import AppKit

/// One window for a connected iPhone: files and photos from the FileGlade app over USB, and the camera roll through macOS.
struct PhoneHubView: View {
    let ic: PhoneDevice?
    let bridge: BridgeDevice?
    let destination: () -> URL
    @Environment(\.dismiss) private var dismiss
    @State private var tab = 0
    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                Image(systemName: "iphone.gen3").font(.system(size: 30)).foregroundStyle(.blue)
                VStack(alignment: .leading, spacing: 2) {
                    Text(bridge?.info?.name ?? ic?.name ?? "iPhone").font(.title3.bold())
                    Text([bridge?.info?.model ?? ic?.model ?? ic?.kindName, bridge?.info.map { "iOS " + $0.systemVersion }, bridge?.info.map { "\(fileBytes($0.freeBytes)) free of \(fileBytes($0.totalBytes))" }].compactMap { $0 }.joined(separator: " · ")).font(.caption).foregroundStyle(.secondary)
                }
                Spacer()
                if bridge != nil && ic != nil { Picker("", selection: $tab) { Text("Files and Photos").tag(0); Text("Camera Roll").tag(1) }.pickerStyle(.segmented).frame(width: 260) }
                Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
            }.padding(16)
            Divider()
            Group {
                if let bridge, tab == 0 || ic == nil { BridgeBrowserView(device: bridge, destination: destination) }
                else if let ic { DeviceBrowserView(phone: ic, destination: destination, embedded: true) }
                else { Text("No device.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity) }
            }
        }.frame(minWidth: 860, minHeight: 600)
    }
}

struct BridgeBrowserView: View {
    @ObservedObject var device: BridgeDevice
    let destination: () -> URL
    @State private var code = ""
    @State private var rootID: String?
    @State private var folders: [String] = []
    @State private var entries: [BridgeEntry] = []
    @State private var total = 0
    @State private var loading = false
    @State private var error: String?
    @State private var selection = Set<String>()
    @State private var search = ""
    @State private var thumbs: [String: NSImage] = [:]
    @State private var progress: (done: Int, total: Int)?
    @State private var message = ""
    private var root: BridgeRoot? { device.roots.first { $0.id == rootID } }
    private var shown: [BridgeEntry] { search.isEmpty ? entries : entries.filter { $0.name.localizedCaseInsensitiveContains(search) } }
    var body: some View {
        switch device.state {
        case .checking: ProgressView("Connecting to the iPhone…").frame(maxWidth: .infinity, maxHeight: .infinity)
        case .appNotOpen:
            guide("Open FileGlade on your iPhone", "Unlock the iPhone, open the FileGlade app, then go to Browse ▸ Computer Access and turn on USB file access. Keep FileGlade open while you copy files.")
        case .needsCode:
            VStack(spacing: 14) {
                Image(systemName: "lock.shield").font(.system(size: 40)).foregroundStyle(.blue)
                Text("Enter the pairing code").font(.title3.bold())
                Text("The code is shown on the iPhone under Browse ▸ Computer Access.").font(.callout).foregroundStyle(.secondary)
                TextField("Pairing code", text: $code).textFieldStyle(.roundedBorder).frame(width: 200).onSubmit { pair() }
                Button("Connect") { pair() }.disabled(code.trimmingCharacters(in: .whitespaces).isEmpty)
            }.frame(maxWidth: .infinity, maxHeight: .infinity)
        case .failed(let message): guide("Cannot reach the iPhone", message + "\nUnlock the iPhone, check the cable and try again.")
        case .ready: browser
        }
    }
    private func guide(_ title: String, _ text: String) -> some View {
        VStack(spacing: 12) {
            Image(systemName: "iphone.slash").font(.system(size: 40)).foregroundStyle(.orange)
            Text(title).font(.title3.bold())
            Text(text).font(.callout).foregroundStyle(.secondary).multilineTextAlignment(.center).frame(maxWidth: 460)
            Button("Try Again") { device.refresh() }
        }.frame(maxWidth: .infinity, maxHeight: .infinity)
    }
    private func pair() { device.token = code.trimmingCharacters(in: .whitespaces); device.state = .checking; device.refresh() }

    private var browser: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                Button { goBack() } label: { Image(systemName: "chevron.left") }.disabled(rootID == nil)
                Button("This iPhone") { rootID = nil; folders = []; entries = [] }.buttonStyle(.link)
                if let root { Image(systemName: "chevron.right").font(.caption2); Button(root.name) { folders = []; load() }.buttonStyle(.link) }
                ForEach(Array(folders.enumerated()), id: \.offset) { index, folder in
                    Image(systemName: "chevron.right").font(.caption2)
                    Button(folder.split(separator: "/").last.map(String.init) ?? folder) { folders = Array(folders.prefix(index + 1)); load() }.buttonStyle(.link)
                }
                Spacer()
                if rootID != nil { TextField("Filter", text: $search).textFieldStyle(.roundedBorder).frame(width: 180) }
            }.padding(.horizontal, 16).padding(.top, 10)
            if rootID == nil {
                List(device.roots) { r in
                    Button { rootID = r.id; folders = []; load() } label: {
                        Label { VStack(alignment: .leading) { Text(r.name); Text(r.kind == "photos" ? "All photos and videos on the iPhone" : "Files and folders").font(.caption).foregroundStyle(.secondary) } } icon: { Image(systemName: r.kind == "photos" ? "photo.on.rectangle.angled" : "folder.fill").foregroundStyle(r.kind == "photos" ? .pink : .yellow) }
                    }.buttonStyle(.plain).padding(.vertical, 4)
                }
            } else {
                if let error { Text(error).font(.caption).foregroundStyle(.orange).padding(.horizontal, 16) }
                List(shown, selection: $selection) { entry in
                    HStack(spacing: 10) {
                        Group {
                            if let image = thumbs[entry.id] { Image(nsImage: image).resizable().aspectRatio(contentMode: .fill) }
                            else { Image(systemName: icon(entry)).font(.system(size: 20)).foregroundStyle(color(entry)) }
                        }.frame(width: 40, height: 40).clipShape(RoundedRectangle(cornerRadius: 6))
                        Text(entry.name).lineLimit(1)
                        Spacer()
                        Text(entry.kind.capitalized).font(.caption).foregroundStyle(.secondary).frame(width: 60)
                        Text(entry.modified.map { Date(timeIntervalSince1970: $0).formatted(date: .abbreviated, time: .shortened) } ?? "").font(.caption).foregroundStyle(.secondary).frame(width: 140, alignment: .leading)
                        Text(entry.isDirectory ? "—" : fileBytes(entry.size)).font(.caption).foregroundStyle(.secondary).frame(width: 70, alignment: .trailing)
                    }.tag(entry.id).contentShape(Rectangle())
                    .onTapGesture(count: 2) { open(entry) }
                    .onAppear { thumbnail(entry); if entry.id == entries.last?.id && entries.count < total && root?.kind == "photos" { loadMore() } }
                    .contextMenu { Button("Copy to This Folder") { copy([entry]) }.disabled(entry.isDirectory); Button("Open") { open(entry) } }
                }
                HStack {
                    if loading { ProgressView().controlSize(.small) }
                    if let progress { ProgressView(value: Double(progress.done), total: Double(max(progress.total, 1))).frame(width: 150); Text("\(progress.done) of \(progress.total)").font(.caption).foregroundStyle(.secondary) }
                    else { Text(message.isEmpty ? "\(entries.count.formatted()) of \(max(total, entries.count).formatted()) items · double-click to open" : message).font(.caption).foregroundStyle(.secondary) }
                    Spacer()
                    Button("Copy Selected to \(destination().lastPathComponent)") { copy(entries.filter { selection.contains($0.id) }) }.disabled(selection.isEmpty || progress != nil)
                }.padding(.horizontal, 16).padding(.bottom, 10)
            }
        }
    }
    private func icon(_ e: BridgeEntry) -> String { e.isDirectory ? "folder.fill" : (e.kind == "photo" ? "photo" : e.kind == "video" ? "film" : "doc") }
    private func color(_ e: BridgeEntry) -> Color { e.isDirectory ? .yellow : (e.kind == "photo" ? .pink : e.kind == "video" ? .orange : .blue) }

    private func goBack() { if !folders.isEmpty { folders.removeLast(); load() } else { rootID = nil; entries = [] } }
    private func load() {
        guard let root else { return }
        loading = true; error = nil; entries = []; selection = []; total = 0; thumbs = [:]
        let client = device.client, rootID = root.id, kind = root.kind, path = folders.last ?? ""
        Task.detached {
            do {
                let listing: BridgeListing = kind == "photos" ? try client.json("/photos", ["offset": "0", "limit": "300"], as: BridgeListing.self) : try client.json("/list", ["root": rootID, "path": path], as: BridgeListing.self)
                await MainActor.run { entries = listing.entries; total = listing.total; loading = false }
            } catch { await MainActor.run { self.error = error.localizedDescription; loading = false } }
        }
    }
    private func loadMore() {
        guard !loading else { return }
        loading = true
        let client = device.client, offset = entries.count
        Task.detached {
            let listing = try? client.json("/photos", ["offset": String(offset), "limit": "300"], as: BridgeListing.self)
            await MainActor.run { entries.append(contentsOf: listing?.entries ?? []); loading = false }
        }
    }
    private func open(_ entry: BridgeEntry) {
        if entry.isDirectory { folders.append(entry.id); load(); return }
        let temp = FileManager.default.temporaryDirectory.appendingPathComponent("FileGlade-phone", isDirectory: true)
        try? FileManager.default.createDirectory(at: temp, withIntermediateDirectories: true)
        fetch([entry], to: temp, open: true)
    }
    private func copy(_ items: [BridgeEntry]) { fetch(items.filter { !$0.isDirectory }, to: destination(), open: false) }
    private func fetch(_ items: [BridgeEntry], to folder: URL, open: Bool) {
        guard !items.isEmpty, let root else { return }
        let client = device.client, rootID = root.id, kind = root.kind
        progress = (0, items.count); message = ""
        Task.detached {
            var errors: [String] = []; var last: URL?
            for (index, entry) in items.enumerated() {
                do {
                    let target = folder.appendingPathComponent(entry.name)
                    last = kind == "photos" ? try client.download("/photo/original", ["id": entry.id], to: target) : try client.download("/file", ["root": rootID, "path": entry.id], to: target)
                } catch { errors.append("\(entry.name): \(error.localizedDescription)") }
                await MainActor.run { progress = (index + 1, items.count) }
            }
            await MainActor.run {
                progress = nil
                message = errors.isEmpty ? "Copied \(items.count) item\(items.count == 1 ? "" : "s") to \(folder.lastPathComponent)." : "\(errors.count) failed: \(errors.first ?? "")"
                if open, let last { NSWorkspace.shared.open(last) }
                NotificationCenter.default.post(name: .init("FileGladeTransferDone"), object: nil)
            }
        }
    }
    private func thumbnail(_ entry: BridgeEntry) {
        guard thumbs[entry.id] == nil, !entry.isDirectory, entry.kind == "photo" || entry.kind == "video", let root else { return }
        let client = device.client, rootID = root.id, kind = root.kind
        Task.detached {
            let data = kind == "photos" ? try? client.get("/photo/thumb", ["id": entry.id]) : try? client.get("/thumb", ["root": rootID, "path": entry.id])
            if let data, let image = NSImage(data: data) { await MainActor.run { thumbs[entry.id] = image } }
        }
    }
}
