import SwiftUI
import AppKit
import Vision

// MARK: - Disk usage

struct StorageItem: Identifiable {
    let url: URL
    let size: Int64
    let isDirectory: Bool
    var growth: Int64?
    var id: String { url.path }
}

enum StorageScan {
    static func allocatedSize(_ url: URL, token: CancelToken) -> Int64 {
        let keys: [URLResourceKey] = [.totalFileAllocatedSizeKey, .fileSizeKey, .isSymbolicLinkKey, .isDirectoryKey]
        let fm = FileManager.default
        guard let v = try? url.resourceValues(forKeys: Set(keys)) else { return 0 }
        if v.isSymbolicLink == true { return 0 }
        if v.isDirectory != true { return Int64(v.totalFileAllocatedSize ?? v.fileSize ?? 0) }
        var total: Int64 = 0
        guard let walk = fm.enumerator(at: url, includingPropertiesForKeys: keys, options: []) else { return 0 }
        for case let child as URL in walk {
            if token.isCancelled { return total }
            if let cv = try? child.resourceValues(forKeys: Set(keys)), cv.isSymbolicLink != true, cv.isDirectory != true { total += Int64(cv.totalFileAllocatedSize ?? cv.fileSize ?? 0) }
        }
        return total
    }
    static func scan(_ folder: URL, token: CancelToken) -> [StorageItem] {
        let children = (try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.isDirectoryKey], options: [])) ?? []
        var results = [StorageItem?](repeating: nil, count: children.count)
        let lock = NSLock()
        DispatchQueue.concurrentPerform(iterations: children.count) { i in
            let url = children[i]
            let isDir = (try? url.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) ?? false
            let item = StorageItem(url: url, size: allocatedSize(url, token: token), isDirectory: isDir)
            lock.lock(); results[i] = item; lock.unlock()
        }
        return results.compactMap { $0 }.sorted { $0.size > $1.size }
    }
}

/// Remembers folder sizes between scans so growth can be shown ("+3 GB since last time").
enum StorageSnapshots {
    struct Snapshot: Codable { var date: Double; var sizes: [String: Int64] }
    private static var file: URL {
        let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("FileGlade", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("storage-snapshots.json")
    }
    private static func load() -> [String: [Snapshot]] { (try? Data(contentsOf: file)).flatMap { try? JSONDecoder().decode([String: [Snapshot]].self, from: $0) } ?? [:] }
    /// The newest snapshot taken more than an hour ago, for comparison.
    static func baseline(for folder: URL) -> Snapshot? { load()[folder.path]?.last { Date().timeIntervalSince1970 - $0.date > 3600 } }
    static func record(_ items: [StorageItem], for folder: URL) {
        var all = load()
        var list = all[folder.path] ?? []
        if let last = list.last, Date().timeIntervalSince1970 - last.date < 3600 { return }
        list.append(Snapshot(date: Date().timeIntervalSince1970, sizes: Dictionary(items.map { ($0.url.lastPathComponent, $0.size) }, uniquingKeysWith: { a, _ in a })))
        all[folder.path] = Array(list.suffix(10))
        if let data = try? JSONEncoder().encode(all) { try? data.write(to: file, options: .atomic) }
    }
}

enum Treemap {
    /// Squarified treemap: rectangles sized by value, as close to square as possible.
    static func layout(_ values: [Double], in rect: CGRect) -> [CGRect] {
        let total = values.reduce(0, +)
        guard total > 0, rect.width > 0, rect.height > 0 else { return values.map { _ in .zero } }
        let areas = values.map { $0 / total * Double(rect.width * rect.height) }
        var rects = [CGRect](repeating: .zero, count: values.count)
        var free = rect
        var start = 0
        func worst(_ row: ArraySlice<Double>, _ side: Double) -> Double {
            let s = row.reduce(0, +); guard let mx = row.max(), let mn = row.min(), s > 0, mn > 0 else { return .infinity }
            return max(side * side * mx / (s * s), s * s / (side * side * mn))
        }
        while start < areas.count {
            let side = Double(min(free.width, free.height))
            var end = start + 1
            while end < areas.count && worst(areas[start...end], side) <= worst(areas[start..<end], side) { end += 1 }
            let row = areas[start..<end], sum = row.reduce(0, +)
            let thickness = side > 0 ? sum / side : 0
            var offset = 0.0
            for i in start..<end {
                let length = thickness > 0 ? areas[i] / thickness : 0
                if free.width >= free.height { rects[i] = CGRect(x: free.minX, y: free.minY + CGFloat(offset), width: CGFloat(thickness), height: CGFloat(length)) }
                else { rects[i] = CGRect(x: free.minX + CGFloat(offset), y: free.minY, width: CGFloat(length), height: CGFloat(thickness)) }
                offset += length
            }
            if free.width >= free.height { free = CGRect(x: free.minX + CGFloat(thickness), y: free.minY, width: free.width - CGFloat(thickness), height: free.height) }
            else { free = CGRect(x: free.minX, y: free.minY + CGFloat(thickness), width: free.width, height: free.height - CGFloat(thickness)) }
            start = end
        }
        return rects
    }
}

struct StorageAnalyzerView: View {
    @State private var stack: [URL]
    @State private var items: [StorageItem] = []
    @State private var scanning = false
    @State private var treemap = true
    @State private var token = CancelToken()
    @State private var baselineDate: Date?
    let onShow: (URL) -> Void
    init(folder: URL, onShow: @escaping (URL) -> Void) { _stack = State(initialValue: [folder]); self.onShow = onShow }
    private var folder: URL { stack.last! }
    private var total: Int64 { items.reduce(0) { $0 + $1.size } }
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Button { token.cancel(); stack.removeLast(); scan() } label: { Image(systemName: "chevron.left") }.disabled(stack.count < 2)
                Text(folder.path).font(.system(size: 12)).lineLimit(1).truncationMode(.head)
                Spacer()
                Picker("", selection: $treemap) { Image(systemName: "square.grid.2x2").tag(true); Image(systemName: "list.bullet").tag(false) }.pickerStyle(.segmented).frame(width: 90)
                Button { scan() } label: { Image(systemName: "arrow.clockwise") }
            }
            HStack(spacing: 8) {
                if scanning { ProgressView().controlSize(.small); Text("Measuring…").font(.caption).foregroundStyle(.secondary) }
                else { Text("\(fileBytes(total)) in \(items.count) items").font(.caption).foregroundStyle(.secondary) }
                if let baselineDate { Text("· growth since \(baselineDate.formatted(date: .abbreviated, time: .shortened))").font(.caption).foregroundStyle(.secondary) }
            }
            if treemap { treemapView.frame(maxHeight: .infinity) } else { listView }
            Text("Sizes are space used on disk. Click a folder to look inside it. Growth is shown once an earlier scan is at least an hour old.").font(.caption2).foregroundStyle(.secondary)
        }.padding(16).frame(minWidth: 760, minHeight: 560).onAppear { scan() }.onDisappear { token.cancel() }
    }
    private var treemapView: some View {
        GeometryReader { proxy in
            let shown = Array(items.filter { $0.size > 0 }.prefix(60))
            let rects = Treemap.layout(shown.map { Double($0.size) }, in: CGRect(origin: .zero, size: proxy.size))
            ZStack(alignment: .topLeading) {
                ForEach(Array(shown.enumerated()), id: \.element.id) { index, item in
                    let r = rects[index].insetBy(dx: 1, dy: 1)
                    RoundedRectangle(cornerRadius: 4).fill(Color(hue: Double(index * 37 % 100) / 100, saturation: item.isDirectory ? 0.5 : 0.28, brightness: 0.72))
                        .overlay(alignment: .topLeading) {
                            if r.width > 70 && r.height > 36 { VStack(alignment: .leading, spacing: 1) { Text(item.url.lastPathComponent).font(.system(size: 11, weight: .semibold)).lineLimit(1); Text(fileBytes(item.size) + growthText(item)).font(.system(size: 10)) }.foregroundStyle(.black.opacity(0.8)).padding(5) }
                        }
                        .frame(width: max(r.width, 0), height: max(r.height, 0)).offset(x: r.minX, y: r.minY)
                        .onTapGesture { open(item) }.help("\(item.url.lastPathComponent) — \(fileBytes(item.size))")
                }
            }
        }
    }
    private var listView: some View {
        List(items) { item in
            HStack {
                FileIcon(url: item.url, size: 22, directory: item.isDirectory)
                Text(item.url.lastPathComponent).lineLimit(1)
                Spacer()
                if let g = item.growth, g != 0 { Text((g > 0 ? "+" : "−") + fileBytes(abs(g))).font(.caption).foregroundStyle(g > 0 ? .orange : .green) }
                Text(fileBytes(item.size)).font(.caption).foregroundStyle(.secondary).frame(width: 80, alignment: .trailing)
                GeometryReader { p in Capsule().fill(Color.accentColor.opacity(0.6)).frame(width: p.size.width * CGFloat(Double(item.size) / Double(max(items.first?.size ?? 1, 1)))) }.frame(width: 90, height: 6)
            }.contentShape(Rectangle()).onTapGesture(count: 2) { open(item) }
            .contextMenu { Button("Show in FileGlade") { onShow(item.url) }; Button("Show in Finder") { NSWorkspace.shared.activateFileViewerSelecting([item.url]) } }
        }
    }
    private func growthText(_ item: StorageItem) -> String { if let g = item.growth, g != 0 { return "  " + (g > 0 ? "+" : "−") + fileBytes(abs(g)) }; return "" }
    private func open(_ item: StorageItem) { if item.isDirectory { token.cancel(); stack.append(item.url); scan() } else { onShow(item.url) } }
    private func scan() {
        token.cancel(); let mine = CancelToken(); token = mine
        scanning = true; items = []
        let target = folder
        DispatchQueue.global(qos: .userInitiated).async {
            var result = StorageScan.scan(target, token: mine)
            let base = StorageSnapshots.baseline(for: target)
            if let base { result = result.map { var i = $0; i.growth = i.size - (base.sizes[i.url.lastPathComponent] ?? 0); return i } }
            if !mine.isCancelled { StorageSnapshots.record(result, for: target) }
            DispatchQueue.main.async { guard !mine.isCancelled else { return }; items = result; baselineDate = base.map { Date(timeIntervalSince1970: $0.date) }; scanning = false }
        }
    }
}

// MARK: - Similar photos

struct SimilarGroup: Identifiable { let id = UUID(); var urls: [URL]; var sizes: [URL: Int64] }

/// Finds photos that look alike (burst shots, edits, near-duplicates) using Apple's on-device image analysis.
struct SimilarPhotosView: View {
    let folder: URL
    let onDone: () -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var prints: [(url: URL, print: VNFeaturePrintObservation, size: Int64)] = []
    @State private var groups: [SimilarGroup] = []
    @State private var threshold = 0.2
    @State private var progress = 0.0
    @State private var scanning = false
    @State private var status = "Choose Scan to look for similar photos in this folder and its subfolders."
    @State private var chosen = Set<URL>()
    @State private var token = CancelToken()
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Similar photos").font(.title3.bold())
            Text(folder.path).font(.caption).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
            HStack {
                Button(scanning ? "Scanning…" : "Scan") { scan() }.disabled(scanning)
                if scanning { Button("Stop") { token.cancel() } ; ProgressView(value: progress).frame(width: 140) }
                Spacer()
                Text("Strictness").font(.caption)
                Slider(value: $threshold, in: 0.05...0.6).frame(width: 140).onChange(of: threshold) { _ in cluster() }
                Text(threshold < 0.15 ? "Near-identical" : threshold < 0.35 ? "Similar" : "Loosely similar").font(.caption).foregroundStyle(.secondary).frame(width: 100, alignment: .leading)
            }
            Text(status).font(.caption).foregroundStyle(.secondary)
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 14) {
                    ForEach(groups) { group in
                        VStack(alignment: .leading, spacing: 6) {
                            Text("\(group.urls.count) similar photos").font(.caption.weight(.semibold))
                            ScrollView(.horizontal) {
                                HStack(spacing: 10) {
                                    ForEach(group.urls, id: \.self) { url in
                                        VStack(spacing: 4) {
                                            ThumbnailIcon(url: url, size: 110)
                                            Text(url.lastPathComponent).font(.system(size: 9)).lineLimit(1).frame(width: 110)
                                            Text(fileBytes(group.sizes[url] ?? 0)).font(.system(size: 9)).foregroundStyle(.secondary)
                                            Toggle("Remove", isOn: Binding(get: { chosen.contains(url) }, set: { if $0 { chosen.insert(url) } else { chosen.remove(url) } })).font(.system(size: 10)).toggleStyle(.checkbox)
                                        }
                                    }
                                }
                            }
                        }.padding(8).background(Color.primary.opacity(0.04), in: RoundedRectangle(cornerRadius: 8))
                    }
                }
            }
            HStack {
                Text("The largest photo in each group is kept ticked-off. Removing sends photos to the Trash; ⌥⌘Z undoes it.").font(.caption2).foregroundStyle(.secondary)
                Spacer()
                Button("Move \(chosen.count) to Trash") { trashChosen() }.disabled(chosen.isEmpty)
                Button("Done") { token.cancel(); dismiss() }.keyboardShortcut(.cancelAction)
            }
        }.padding(16).frame(width: 820, height: 600).onDisappear { token.cancel() }
    }
    private func scan() {
        token.cancel(); let mine = CancelToken(); token = mine
        scanning = true; groups = []; chosen = []; prints = []; progress = 0; status = "Finding photos…"
        let root = folder
        DispatchQueue.global(qos: .userInitiated).async {
            let exts: Set<String> = ["jpg", "jpeg", "png", "heic", "tiff", "tif", "bmp", "gif", "webp"]
            var urls: [URL] = []
            if let walk = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.fileSizeKey], options: [.skipsHiddenFiles, .skipsPackageDescendants]) {
                for case let url as URL in walk where exts.contains(url.pathExtension.lowercased()) { urls.append(url); if urls.count >= 1500 { break } }
            }
            DispatchQueue.main.async { status = "Analyzing \(urls.count) photos…" }
            let lock = NSLock(); var done = 0
            var found: [(URL, VNFeaturePrintObservation, Int64)] = []
            let queue = OperationQueue(); queue.maxConcurrentOperationCount = 4
            for url in urls {
                queue.addOperation {
                    if mine.isCancelled { return }
                    let request = VNGenerateImageFeaturePrintRequest()
                    if (try? VNImageRequestHandler(url: url).perform([request])) != nil, let observation = request.results?.first {
                        let size = Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
                        lock.lock(); found.append((url, observation, size)); lock.unlock()
                    }
                    lock.lock(); done += 1; let fraction = Double(done) / Double(max(urls.count, 1)); lock.unlock()
                    if done % 10 == 0 { DispatchQueue.main.async { progress = fraction } }
                }
            }
            queue.waitUntilAllOperationsAreFinished()
            DispatchQueue.main.async {
                scanning = false; progress = 1
                if mine.isCancelled { status = "Stopped."; return }
                prints = found.map { (url: $0.0, print: $0.1, size: $0.2) }
                cluster()
            }
        }
    }
    private func cluster() {
        var result: [(rep: VNFeaturePrintObservation, members: [(URL, Int64)])] = []
        for item in prints {
            var placed = false
            for i in result.indices {
                var distance: Float = 0
                if (try? result[i].rep.computeDistance(&distance, to: item.print)) != nil, Double(distance) < threshold { result[i].members.append((item.url, item.size)); placed = true; break }
            }
            if !placed { result.append((item.print, [(item.url, item.size)])) }
        }
        groups = result.filter { $0.members.count > 1 }.map { g in
            let sorted = g.members.sorted { $0.1 > $1.1 }
            return SimilarGroup(urls: sorted.map(\.0), sizes: Dictionary(sorted.map { ($0.0, $0.1) }, uniquingKeysWith: { a, _ in a }))
        }
        chosen = Set(groups.flatMap { $0.urls.dropFirst() })
        status = groups.isEmpty ? (prints.isEmpty ? status : "No similar photos found among \(prints.count) photos.") : "\(groups.count) group\(groups.count == 1 ? "" : "s") of similar photos among \(prints.count) analyzed."
    }
    private func trashChosen() {
        var steps: [UndoRecord.Step] = []
        for url in chosen { var result: NSURL?; if (try? FileManager.default.trashItem(at: url, resultingItemURL: &result)) != nil, let result { steps.append(.trashed(original: url, trash: result as URL)) } }
        TransferManager.shared.record(title: "Remove similar photos", steps: steps)
        let removed = chosen; prints.removeAll { removed.contains($0.url) }; chosen = []
        cluster(); onDone()
    }
}
