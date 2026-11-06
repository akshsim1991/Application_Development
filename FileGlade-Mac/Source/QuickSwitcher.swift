import SwiftUI
import AppKit

/// Cached on-demand folder sizes, so a folder is only measured when asked.
enum FolderSizeCache {
    static var values: [String: Int64] = [:]
}

struct FolderSizeRow: View {
    let url: URL
    @State private var size: Int64?
    @State private var working = false
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("FOLDER SIZE").font(.system(size: 9, weight: .medium)).tracking(0.8).foregroundStyle(.tertiary)
            if let size { Text(fileBytes(size)).font(.system(size: 11)).foregroundStyle(.secondary) }
            else if working { HStack(spacing: 6) { ProgressView().controlSize(.small); Text("Calculating…").font(.system(size: 11)).foregroundStyle(.secondary) } }
            else { Button("Calculate size") { calculate() }.font(.system(size: 11)).buttonStyle(.link) }
        }
        .task(id: url) { size = FolderSizeCache.values[url.path]; working = false }
    }
    private func calculate() {
        working = true
        let target = url
        DispatchQueue.global(qos: .userInitiated).async {
            let total = TransferEngine.size(of: target)
            DispatchQueue.main.async {
                FolderSizeCache.values[target.path] = total
                if target == url { size = total; working = false }
            }
        }
    }
}

struct SwitcherResult: Identifiable {
    let id = UUID()
    let title: String
    let detail: String
    let icon: String
    let action: () -> Void
}

/// ⌘K: jump to any indexed file or folder, an open tab, or a command.
struct QuickSwitcher: View {
    @ObservedObject var model: Explorer
    var tools: () -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var query = ""
    @State private var indexed: [DashboardItem] = []
    @State private var selected = 0
    @FocusState private var focused: Bool
    private var database: URL { FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("FileGladeDashboard/index-v1.sqlite") }

    private var results: [SwitcherResult] {
        var list: [SwitcherResult] = []
        let q = query.lowercased()
        let commands: [(String, String, () -> Void)] = [
            ("New folder", "folder.badge.plus", { model.newFolder() }),
            ("Power tools", "wand.and.stars", tools),
            ("New workspace tab", "plus.square", { model.addTab() }),
            ("Go to All files", "square.grid.2x2", { model.showDashboard = true }),
            ("Toggle details pane", "sidebar.right", { model.inspector.toggle() }),
            ("Undo last file operation", "arrow.uturn.backward", { model.undoFileOperation() })
        ]
        for command in commands where q.isEmpty || command.0.lowercased().contains(q) { list.append(SwitcherResult(title: command.0, detail: "Command", icon: command.1, action: command.2)) }
        for tab in model.tabs where !q.isEmpty && tab.title.lowercased().contains(q) { list.append(SwitcherResult(title: tab.title, detail: "Open tab", icon: "square.on.square", action: { model.activateTab(tab.id) })) }
        if q.count >= 2 { for url in NotesStore.shared.search(q).prefix(5) { list.append(SwitcherResult(title: url.lastPathComponent, detail: "Note: " + NotesStore.shared.note(for: url).prefix(60), icon: "note.text", action: { model.navigate(url.deletingLastPathComponent()); model.selection = [url] })) } }
        for item in indexed {
            list.append(SwitcherResult(title: item.name, detail: item.url.deletingLastPathComponent().path, icon: item.folder ? "folder" : "doc", action: {
                if item.folder { model.navigate(item.url) } else { model.navigate(item.url.deletingLastPathComponent()); model.selection = [item.url]; NSWorkspace.shared.open(item.url) }
            }))
        }
        return list
    }
    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                TextField("Find a file, folder, tab or command", text: $query).textFieldStyle(.plain).font(.system(size: 15)).focused($focused)
                    .onSubmit { run(selected) }
                    .onChange(of: query) { _ in selected = 0; search() }
            }.padding(12)
            Divider()
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(spacing: 0) {
                        ForEach(Array(results.prefix(40).enumerated()), id: \.offset) { index, result in
                            HStack(spacing: 10) {
                                Image(systemName: result.icon).frame(width: 20).foregroundStyle(.secondary)
                                Text(result.title).lineLimit(1)
                                Spacer(minLength: 8)
                                Text(result.detail).font(.caption).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
                            }.padding(.horizontal, 12).frame(height: 30).contentShape(Rectangle())
                            .background(index == selected ? Color.accentColor.opacity(0.18) : .clear)
                            .onTapGesture { run(index) }.id(index)
                        }
                    }
                }.onChange(of: selected) { proxy.scrollTo($0) }
            }
        }
        .frame(width: 560, height: 380)
        .onAppear { focused = true; search() }
        .onExitCommand { dismiss() }
        .background(KeyCatcher { key in
            switch key {
            case 125: selected = min(selected + 1, max(0, min(results.count, 40) - 1)); return true
            case 126: selected = max(selected - 1, 0); return true
            default: return false
            }
        })
    }
    private func run(_ index: Int) {
        let list = results
        guard index < list.count else { return }
        dismiss()
        DispatchQueue.main.async { list[index].action() }
    }
    private func search() {
        let text = query, db = database
        guard !text.isEmpty else { indexed = []; return }
        DispatchQueue.global(qos: .userInitiated).async {
            let page = try? DashboardIndex(at: db).page(query: text, kind: .all, limit: 30)
            DispatchQueue.main.async { if text == query { indexed = page?.items ?? [] } }
        }
    }
}

/// Captures up/down arrow keys while the switcher is open.
struct KeyCatcher: NSViewRepresentable {
    let handler: (Int) -> Bool
    func makeNSView(context: Context) -> NSView {
        let view = NSView()
        context.coordinator.monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { event in
            handler(Int(event.keyCode)) ? nil : event
        }
        return view
    }
    func updateNSView(_ view: NSView, context: Context) {}
    func makeCoordinator() -> Coordinator { Coordinator() }
    final class Coordinator { var monitor: Any?; deinit { if let monitor { NSEvent.removeMonitor(monitor) } } }
}
