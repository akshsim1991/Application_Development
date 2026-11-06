import SwiftUI
import AppKit

/// Two-pane browsing. The main window browser is the left pane; `second` is the right pane.
final class PaneState: ObservableObject {
    static let shared = PaneState()
    @Published var dual: Bool { didSet { UserDefaults.standard.set(dual, forKey: "dualPane") } }
    @Published var active: Explorer?
    weak var main: Explorer?
    lazy var second: Explorer = Explorer(secondary: true)
    init() { dual = UserDefaults.standard.bool(forKey: "dualPane") }

    /// The pane that menu commands and the toolbar act on.
    var current: Explorer? { dual ? (active ?? main) : main }
    func toggle() { dual.toggle(); if !dual { active = main } }
    func switchPane() { guard dual, let main else { return }; active = (active ?? main) === main ? second : main }
    func transferToOther(move: Bool) {
        guard dual, let main else { return }
        let source = active ?? main
        let target = source === main ? second : main
        let urls = source.chosen.map(\.url)
        guard !urls.isEmpty else { return }
        source.transfer(urls, into: target.directory, kind: move ? .move : .copy)
    }
}

/// Files gathered from several folders, ready to copy, move, compress or share together.
final class BasketStore: ObservableObject {
    static let shared = BasketStore()
    @Published var items: [URL] = []
    func add(_ urls: [URL]) { for url in urls where !items.contains(url) { items.append(url) } }
    func remove(_ url: URL) { items.removeAll { $0 == url } }
    func clear() { items = [] }
}

struct BasketView: View {
    @ObservedObject private var basket = BasketStore.shared
    let target: Explorer
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack { Label("Selection basket", systemImage: "tray.full").font(.headline); Spacer(); Text("\(basket.items.count) items").font(.caption).foregroundStyle(.secondary) }
            if basket.items.isEmpty {
                Text("Add files from any folder with ⌘B or the right-click menu, then copy, move, compress or share them together.").font(.caption).foregroundStyle(.secondary).frame(maxWidth: .infinity, alignment: .leading).padding(.vertical, 12)
            } else {
                List {
                    ForEach(basket.items, id: \.self) { url in
                        HStack(spacing: 8) {
                            FileIcon(url: url, size: 20)
                            VStack(alignment: .leading, spacing: 1) { Text(url.lastPathComponent).lineLimit(1); Text(url.deletingLastPathComponent().path).font(.caption2).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head) }
                            Spacer()
                            Button { basket.remove(url) } label: { Image(systemName: "xmark.circle.fill") }.buttonStyle(.plain).foregroundStyle(.secondary)
                        }
                    }
                }.frame(height: 190)
                Text("Destination: \(target.directory.path)").font(.caption2).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
                HStack {
                    Button("Copy Here") { target.transfer(basket.items, into: target.directory, kind: .copy) }
                    Button("Move Here") { target.transfer(basket.items, into: target.directory, kind: .move); basket.clear() }
                    Button("Compress") { compress() }
                    ShareLink(items: basket.items) { Label("Share", systemImage: "square.and.arrow.up") }
                    Spacer()
                    Button("Clear", role: .destructive) { basket.clear() }
                }.controlSize(.small)
            }
        }.padding(14).frame(width: 380)
    }
    private func compress() {
        let items = basket.items, folder = target.directory
        Task.detached {
            do { let url = try PowerFS.zip(items, into: folder, name: "Basket"); await MainActor.run { target.refresh(); target.selection = [url] } }
            catch { await MainActor.run { target.failure = error.localizedDescription } }
        }
    }
}
