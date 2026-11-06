import SwiftUI
import AppKit

enum CompareStatus: String, CaseIterable {
    case leftOnly = "Only on left", rightOnly = "Only on right", leftNewer = "Newer on left", rightNewer = "Newer on right", different = "Different", same = "Identical"
    var symbol: String { switch self { case .leftOnly: return "arrow.right.circle"; case .rightOnly: return "arrow.left.circle"; case .leftNewer: return "arrow.right.circle.fill"; case .rightNewer: return "arrow.left.circle.fill"; case .different: return "exclamationmark.triangle"; case .same: return "checkmark.circle" } }
}
struct CompareRow: Identifiable {
    let relative: String
    let status: CompareStatus
    let left: URL?
    let right: URL?
    let leftSize: Int64
    let rightSize: Int64
    var id: String { relative }
}

enum FolderCompare {
    private static func files(in root: URL) -> [String: (url: URL, size: Int64, modified: Date)] {
        var result: [String: (URL, Int64, Date)] = [:]
        let base = root.resolvingSymlinksInPath().standardizedFileURL.path
        guard let walk = FileManager.default.enumerator(at: root.resolvingSymlinksInPath(), includingPropertiesForKeys: [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey], options: [.skipsHiddenFiles]) else { return [:] }
        for case let url as URL in walk {
            guard let v = try? url.resourceValues(forKeys: [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey]), v.isDirectory != true else { continue }
            let path = url.standardizedFileURL.path
            guard path.hasPrefix(base + "/") else { continue }
            result[String(path.dropFirst(base.count + 1))] = (url, Int64(v.fileSize ?? 0), v.contentModificationDate ?? .distantPast)
        }
        return result
    }
    static func compare(left: URL, right: URL, deep: Bool) -> [CompareRow] {
        let l = files(in: left), r = files(in: right)
        var rows: [CompareRow] = []
        for key in Set(l.keys).union(r.keys) {
            switch (l[key], r[key]) {
            case let (a?, nil): rows.append(CompareRow(relative: key, status: .leftOnly, left: a.url, right: nil, leftSize: a.size, rightSize: 0))
            case let (nil, b?): rows.append(CompareRow(relative: key, status: .rightOnly, left: nil, right: b.url, leftSize: 0, rightSize: b.size))
            case let (a?, b?):
                let gap = a.modified.timeIntervalSince(b.modified)
                var status: CompareStatus
                if a.size == b.size && abs(gap) <= 2 { status = .same }
                else if a.size == b.size && deep, let x = try? PowerFS.digest(a.url), let y = try? PowerFS.digest(b.url), x == y { status = .same }
                else if abs(gap) <= 2 { status = .different }
                else { status = gap > 0 ? .leftNewer : .rightNewer }
                rows.append(CompareRow(relative: key, status: status, left: a.url, right: b.url, leftSize: a.size, rightSize: b.size))
            default: break
            }
        }
        return rows.sorted { ($0.status.rawValue, $0.relative.lowercased()) < ($1.status.rawValue, $1.relative.lowercased()) }
    }
}

/// Compares the left and right panes, then copies what is missing or newer. Nothing is deleted.
struct CompareView: View {
    let left: URL
    let right: URL
    let onDone: () -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var rows: [CompareRow] = []
    @State private var loading = true
    @State private var deep = false
    @State private var showSame = false
    @State private var excluded = Set<String>()
    private var shown: [CompareRow] { rows.filter { showSame || $0.status != .same } }
    private func chosen(_ statuses: [CompareStatus]) -> [CompareRow] { shown.filter { statuses.contains($0.status) && !excluded.contains($0.id) } }
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Compare folders").font(.title3.bold())
            Group { Label(left.path, systemImage: "rectangle.lefthalf.inset.filled"); Label(right.path, systemImage: "rectangle.righthalf.inset.filled") }.font(.caption).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
            HStack {
                Toggle("Compare contents when sizes match", isOn: $deep).onChange(of: deep) { _ in reload() }
                Toggle("Show identical files", isOn: $showSame)
                Spacer()
                if loading { ProgressView().controlSize(.small) }
            }.font(.caption)
            if !loading && shown.isEmpty { Text("The folders match. Nothing to copy.").frame(maxWidth: .infinity, maxHeight: .infinity) }
            else {
                List(shown) { row in
                    HStack(spacing: 8) {
                        if row.status != .same { Toggle("", isOn: Binding(get: { !excluded.contains(row.id) }, set: { if $0 { excluded.remove(row.id) } else { excluded.insert(row.id) } })).labelsHidden() }
                        Image(systemName: row.status.symbol).foregroundStyle(row.status == .different ? .orange : .secondary).frame(width: 18)
                        Text(row.relative).lineLimit(1).truncationMode(.middle)
                        Spacer(minLength: 6)
                        Text(row.status.rawValue).font(.caption).foregroundStyle(.secondary)
                        Text("\(row.left == nil ? "—" : fileBytes(row.leftSize)) | \(row.right == nil ? "—" : fileBytes(row.rightSize))").font(.caption2).foregroundStyle(.tertiary).frame(width: 130, alignment: .trailing)
                    }
                }
            }
            HStack {
                Text("\(chosen([.leftOnly, .leftNewer, .different]).count) can go right · \(chosen([.rightOnly, .rightNewer]).count) can go left").font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("Copy to Right →") { copy(toRight: true, toLeft: false) }.disabled(chosen([.leftOnly, .leftNewer, .different]).isEmpty)
                Button("← Copy to Left") { copy(toRight: false, toLeft: true) }.disabled(chosen([.rightOnly, .rightNewer]).isEmpty)
                Button("Sync Both Ways") { copy(toRight: true, toLeft: true) }.disabled(chosen([.leftOnly, .leftNewer, .rightOnly, .rightNewer]).isEmpty)
                Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
            }
            Text("Copying replaces older files by sending them to the Trash, so you can undo with ⌥⌘Z. Nothing is deleted.").font(.caption2).foregroundStyle(.secondary)
        }.padding(18).frame(width: 760, height: 520).onAppear { reload() }
    }
    private func reload() {
        loading = true
        let l = left, r = right, d = deep
        DispatchQueue.global(qos: .userInitiated).async {
            let result = FolderCompare.compare(left: l, right: r, deep: d)
            DispatchQueue.main.async { rows = result; excluded = []; loading = false }
        }
    }
    private func copy(toRight: Bool, toLeft: Bool) {
        var plan: [PlannedItem] = []
        let fm = FileManager.default
        if toRight { for row in chosen([.leftOnly, .leftNewer] + (toLeft ? [] : [.different])) { if let source = row.left { let target = right.appendingPathComponent(row.relative); plan.append(PlannedItem(source: source, target: target, replace: fm.fileExists(atPath: target.path))) } } }
        if toLeft { for row in chosen([.rightOnly, .rightNewer]) { if let source = row.right { let target = left.appendingPathComponent(row.relative); plan.append(PlannedItem(source: source, target: target, replace: fm.fileExists(atPath: target.path))) } } }
        TransferManager.shared.startPlan(kind: .copy, plan: plan, title: "Sync \(plan.count) files") { _ in onDone() }
        dismiss()
    }
}
