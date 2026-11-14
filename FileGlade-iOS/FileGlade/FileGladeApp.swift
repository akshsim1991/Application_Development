import SwiftUI
import QuickLook
import UIKit
import UniformTypeIdentifiers

@main struct FileGladeApp: App {
    @StateObject private var model = BrowserModel()
    @Environment(\.scenePhase) private var scenePhase
    @State private var activeScreen = 0
    @State private var dashboardSheet: DashboardSheet?
    var body: some Scene {
        WindowGroup {
            TabView(selection: $activeScreen) {
                NavigationStack {
                    UnifiedDashboardView(roots: model.dashboardRoots, unavailable: model.unavailableDashboardSources, onOpen: { url in
                        if PowerFile(url).directory { model.revealItem(url); activeScreen = 1 }
                        else { dashboardSheet = .preview(url) }
                    }, onReveal: { url in model.revealItem(url); activeScreen = 1 }, onConnect: { dashboardSheet = .access })
                    .navigationTitle("FileGlade").navigationBarTitleDisplayMode(.inline)
                }.tabItem { Label("All files", systemImage: "square.grid.2x2") }.tag(0)
                BrowserView(model: model, onDashboard: { activeScreen = 0 }).tabItem { Label("Folders", systemImage: "folder") }.tag(1)
                NavigationStack { LibraryToolsView { url in model.revealItem(url); activeScreen = 1 } }.tabItem { Label("Library", systemImage: "star") }.tag(2)
                DeletedView(model: model).tabItem { Label("Recently Deleted", systemImage: "trash") }.tag(3)
            }
            .tint(.blue)
            .alert("Something needs your attention", isPresented: Binding(get: { model.problem != nil }, set: { if !$0 { model.problem = nil } })) { Button("OK") { model.problem = nil } } message: { Text(model.problem ?? "") }
            .task { BridgeController.shared.rootsProvider = { model.dashboardRoots }; await model.reload() }
            .onChange(of: scenePhase) { _, phase in if phase == .active { if activeScreen == 1 { Task { await model.reload() } }; model.refreshDeleted() } }
            .sheet(item: $dashboardSheet) { sheet in
                switch sheet { case .access: FileAccessSetupView(model: model); case .preview(let url): PreviewView(url: url) }
            }
        }
    }
}

private enum DashboardSheet: Identifiable {
    case access, preview(URL)
    var id: String { switch self { case .access: return "access"; case .preview(let url): return url.path } }
}

struct FileAccessSetupView: View {
    @ObservedObject var model: BrowserModel
    @Environment(\.dismiss) private var dismiss
    @State private var picking = false
    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 12) {
                        Text("One dashboard. Saved access.").font(.title3.bold())
                        Text("Allow a folder once and FileGlade automatically includes that folder and all its subfolders in All files. Access is remembered for future launches.").font(.subheadline)
                        Text("Choose the broadest folder the Files picker allows, so you do not need to connect its subfolders individually. iOS requires this one-time step for files outside FileGlade and does not expose other apps' private files.").font(.caption).foregroundStyle(.secondary)
                    }.padding(.vertical, 8)
                    Button { picking = true } label: { Label("Allow a folder and its subfolders", systemImage: "folder.badge.plus") }
                }
                Section("Included automatically") {
                    Label("FileGlade files", systemImage: "checkmark.circle.fill")
                    ForEach(model.locations) { location in
                        Label(location.name, systemImage: model.unavailableDashboardSources.contains(location.name) ? "exclamationmark.circle" : "checkmark.circle.fill")
                            .swipeActions { Button("Remove from dashboard", role: .destructive) { model.removeDashboardLocation(location) } }
                    }
                }
                Section { Text("Removing a folder here only removes its dashboard connection. It does not delete files. Normal browsing uses All files; you do not need to select a location again each time.").font(.caption).foregroundStyle(.secondary) }
            }.navigationTitle("File access").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Done") { dismiss() } } }
            .fileImporter(isPresented: $picking, allowedContentTypes: [.folder]) { result in
                switch result { case .success(let url): model.addLocation(url); dismiss(); case .failure(let error): model.problem = error.localizedDescription }
            }
        }
    }
}

private enum BrowserSheet: Identifiable {
    case about, locations, preview(URL), share([URL]), details(FileItem), tools(URL, [URL]), computer
    var id: String {
        switch self { case .about: return "about"; case .locations: return "locations"; case .preview(let url): return "preview-\(url.path)"; case .share: return "share"; case .details(let item): return "info-\(item.url.path)"; case .tools: return "tools"; case .computer: return "computer" }
    }
}

struct BrowserView: View {
    @ObservedObject var model: BrowserModel
    var onDashboard: () -> Void = {}
    @ObservedObject private var library = FileLibrary.shared
    @State private var sheet: BrowserSheet?
    @State private var importing = false
    @State private var naming = false
    @State private var renameItem: FileItem?
    @State private var newName = ""
    @State private var trashItems: [FileItem] = []
    @State private var confirmTrash = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                workspaceBar
                Divider()
                commandBar
                Divider()
                pathBar
                Divider()
                files
            }
            .background(Color(uiColor: .systemGroupedBackground))
            .navigationTitle("FileGlade")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) { Button(action: onDashboard) { Image(systemName: "square.grid.2x2") }.accessibilityLabel("All files dashboard") }
                ToolbarItem(placement: .topBarTrailing) {
                    Button(model.selecting ? "Done" : "Select") { model.selecting.toggle(); model.selection = [] }.font(.subheadline.weight(.semibold))
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Menu {
                        Button("Power tools", systemImage: "wand.and.stars") { openTools() }
                        Button("New workspace tab", systemImage: "plus.square.on.square") { model.addTab() }
                        Button(library.isStarred(model.directory) ? "Unstar this folder" : "Star this folder", systemImage: "star") { library.toggleStar(model.directory) }
                        Divider()
                        Button("Import files", systemImage: "square.and.arrow.down") { importing = true }
                        Button("Add folders to dashboard", systemImage: "folder.badge.plus") { sheet = .locations }
                        Button("Select all", systemImage: "checkmark.circle") { model.selecting = true; model.selection = Set(model.visible.map(\.url)) }.disabled(model.entries.isEmpty)
                        Divider()
                        Toggle("Show hidden files", isOn: $model.hidden).onChange(of: model.hidden) { _, _ in Task { await model.reload() } }
                        Button("Refresh", systemImage: "arrow.clockwise") { Task { await model.reload() } }
                        Button("Computer access (USB)", systemImage: "cable.connector") { sheet = .computer }
                        Button("About FileGlade", systemImage: "info.circle") { sheet = .about }
                    } label: { Image(systemName: "ellipsis.circle") }.accessibilityLabel("More actions")
                }
            }
            .searchable(text: $model.query, placement: .navigationBarDrawer(displayMode: .always), prompt: "Search this folder")
            .safeAreaInset(edge: .bottom, spacing: 0) { bottomBar }
            .sheet(item: $sheet, onDismiss: { Task { await model.reload() } }) { value in
                switch value {
                case .about: FileGladeAboutView()
                case .locations: FileAccessSetupView(model: model)
                case .preview(let url): PreviewView(url: url)
                case .share(let urls): ShareView(urls: urls)
                case .details(let item): DetailsView(item: item)
                case .computer: ComputerAccessView()
                case .tools(let folder, let urls): PowerToolsView(folder: folder, selection: urls, onProduced: model.toolsProduced, onReveal: model.revealItem)
                }
            }
            .fileImporter(isPresented: $importing, allowedContentTypes: [.item], allowsMultipleSelection: true) { result in
                switch result { case .success(let urls): Task { await model.importFiles(urls) }; case .failure(let error): model.problem = error.localizedDescription }
            }
            .alert(renameItem == nil ? "New folder" : "Rename", isPresented: $naming) {
                TextField("Name", text: $newName).autocorrectionDisabled().textInputAutocapitalization(.never)
                Button("Cancel", role: .cancel) { renameItem = nil }
                Button(renameItem == nil ? "Create" : "Rename") { if let item = renameItem { model.rename(item, to: newName) } else { model.newFolder(newName) }; renameItem = nil }
            } message: { Text(renameItem == nil ? "Create a folder in \(model.title)." : "Include the extension when renaming a file.") }
            .confirmationDialog("Move \(trashItems.count == 1 ? "this item" : "these \(trashItems.count) items") to Recently Deleted?", isPresented: $confirmTrash, titleVisibility: .visible) {
                Button("Move to Recently Deleted", role: .destructive) { let items = trashItems; Task { await model.recycle(items) } }
            } message: { Text("You can restore items from the Recently Deleted tab in FileGlade.") }
        }
    }
    private func openTools() { sheet = .tools(model.directory, model.chosen.map(\.url).sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending }) }
    private var workspaceBar: some View {
        HStack(spacing: 8) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    ForEach(model.tabs) { tab in
                        HStack(spacing: 8) {
                            Button { model.activateTab(tab.id) } label: { Text(tab.url.path == model.local.path ? "FileGlade files" : tab.title).lineLimit(1).font(.caption.weight(.medium)).frame(maxWidth: 150) }
                            if model.tabs.count > 1 { Button { model.closeTab(tab.id) } label: { Image(systemName: "xmark").font(.system(size: 9)) }.accessibilityLabel("Close workspace") }
                        }.buttonStyle(.plain).padding(.horizontal, 11).frame(height: 30).background(model.activeTab == tab.id ? Color.blue.opacity(0.12) : Color.secondary.opacity(0.06), in: RoundedRectangle(cornerRadius: 8))
                    }
                }
            }
            Button { model.addTab() } label: { Image(systemName: "plus").frame(width: 30, height: 36) }.accessibilityLabel("New workspace tab").disabled(model.tabs.count >= 8)
        }.padding(.horizontal, 16).frame(height: 43).disabled(model.busy)
    }
    private var commandBar: some View {
        HStack(spacing: 14) {
            Button { renameItem = nil; newName = "New folder"; naming = true } label: { Label("New", systemImage: "folder.badge.plus") }
            Spacer()
            Menu {
                Picker("Sort by", selection: $model.sort) { ForEach(FileSort.allCases, id: \.self) { Text($0.rawValue).tag($0) } }
                Divider(); Toggle("Ascending", isOn: $model.ascending)
            } label: { Label("Sort", systemImage: "arrow.up.arrow.down") }
            Menu { Picker("Group by", selection: $model.grouping) { ForEach(FileGrouping.allCases) { Text($0.rawValue).tag($0) } } } label: { Image(systemName: "square.grid.3x1.below.line.grid.1x2") }.accessibilityLabel("Group by")
            Menu { Picker("Filter", selection: $model.filter) { ForEach(PowerFilter.allCases) { Text($0.rawValue).tag($0) } } } label: { Image(systemName: model.filter == .all ? "line.3.horizontal.decrease.circle" : "line.3.horizontal.decrease.circle.fill") }.accessibilityLabel("Filter files")
            Button { openTools() } label: { Image(systemName: "wand.and.stars") }.accessibilityLabel("Power tools")
            Button { model.icons.toggle() } label: { Image(systemName: model.icons ? "list.bullet" : "square.grid.2x2") }.accessibilityLabel(model.icons ? "Show list view" : "Show icon view")
        }.font(.subheadline).padding(.horizontal, 20).frame(height: 47).disabled(model.busy)
    }
    private var pathBar: some View {
        HStack(spacing: 10) {
            Button(action: model.back) { Image(systemName: "chevron.left").frame(width: 22, height: 40) }.disabled(!model.canBack).accessibilityLabel("Back")
            Button(action: model.forward) { Image(systemName: "chevron.right").frame(width: 22, height: 40) }.disabled(!model.canForward).accessibilityLabel("Forward")
            Button(action: model.up) { Image(systemName: "arrow.up").frame(width: 22, height: 40) }.disabled(!model.canUp).accessibilityLabel("Parent folder")
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 8) {
                    ForEach(model.crumbs, id: \.path) { url in
                        Button { model.navigate(url) } label: { Text(url.path == model.local.path ? "FileGlade files" : url.lastPathComponent).lineLimit(1).fontWeight(url.path == model.directory.path ? .semibold : .regular) }
                        if url.path != model.directory.path { Image(systemName: "chevron.right").font(.system(size: 8)).foregroundStyle(.tertiary) }
                    }
                }.font(.caption).padding(.trailing, 12)
            }
        }.padding(.leading, 15).frame(height: 48).disabled(model.busy)
    }
    private var groups: [FileSection<FileItem>] {
        fileSections(model.visible) { item in model.grouping.key(name: item.name, folder: item.isDirectory, size: item.size, modified: item.modified, category: fileCategory(item.url), location: item.url.deletingLastPathComponent().path) }
    }
    @ViewBuilder private var files: some View {
        if model.busy && model.entries.isEmpty {
            ProgressView("Opening folder…").frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if model.visible.isEmpty {
            ScrollView {
                VStack(spacing: 16) {
                    Image(systemName: model.query.isEmpty ? "folder" : "magnifyingglass").font(.system(size: 62, weight: .ultraLight)).foregroundStyle(.blue.opacity(0.7))
                    Text(model.query.isEmpty ? "Make room for your files" : "No matching files").font(.title3.weight(.semibold))
                    Text(model.query.isEmpty ? "Open a folder from Files or import documents\nto start exploring." : "Search looks for names in this folder only.").font(.subheadline).foregroundStyle(.secondary).multilineTextAlignment(.center)
                    if model.query.isEmpty { Button("Open a folder") { sheet = .locations }.buttonStyle(.borderedProminent); Button("Import files") { importing = true }.font(.subheadline) }
                }.frame(maxWidth: .infinity).padding(.top, 80).padding(.horizontal, 16)
            }.refreshable { await model.reload() }
        } else if model.icons {
            ScrollView {
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 95, maximum: 150), spacing: 10)], spacing: 16) {
                    ForEach(groups) { group in
                        Section {
                    ForEach(group.items) { item in
                        Button { tap(item) } label: {
                            VStack(spacing: 10) {
                                Image(systemName: item.icon).font(.system(size: 46, weight: .regular)).foregroundStyle(item.isDirectory ? Color.yellow : .blue).frame(height: 57)
                                Text(item.name).font(.system(size: 12)).foregroundStyle(.primary).lineLimit(2).multilineTextAlignment(.center).frame(height: 33, alignment: .top)
                            }
                            .padding(.vertical, 14).padding(.horizontal, 7).frame(maxWidth: .infinity)
                            .background(model.selection.contains(item.url) ? Color.blue.opacity(0.10) : Color.clear, in: RoundedRectangle(cornerRadius: 12))
                            .overlay(alignment: .topLeading) { HStack(spacing: 4) { if let color = library.color(item.url) { Circle().fill(color).frame(width: 7, height: 7) }; if library.isStarred(item.url) { Image(systemName: "star.fill").font(.system(size: 10)).foregroundStyle(.yellow) } }.padding(7) }
                            .overlay(alignment: .topTrailing) { if model.selecting { Image(systemName: model.selection.contains(item.url) ? "checkmark.circle.fill" : "circle").foregroundStyle(model.selection.contains(item.url) ? .blue : .secondary).padding(7) } }
                        }.buttonStyle(.plain).contextMenu { itemMenu(item) }.accessibilityLabel(item.name).accessibilityHint(item.isDirectory ? "Opens folder" : "Previews file")
                    }
                        } header: { if model.grouping != .none { Text(group.title).font(.headline).frame(maxWidth: .infinity, alignment: .leading).padding(.top, 10) } }
                    }
                }.padding(16)
            }.refreshable { await model.reload() }
        } else {
            List {
                ForEach(groups) { group in
                    Section {
                        ForEach(group.items) { item in
                Button { tap(item) } label: {
                    HStack(spacing: 14) {
                        if model.selecting { Image(systemName: model.selection.contains(item.url) ? "checkmark.circle.fill" : "circle").foregroundStyle(.blue) }
                        Image(systemName: item.icon).font(.system(size: 27)).foregroundStyle(item.isDirectory ? Color.yellow : .blue).frame(width: 37)
                        VStack(alignment: .leading, spacing: 5) { Text(item.name).foregroundStyle(.primary).font(.subheadline).lineLimit(2); Text(item.isDirectory ? "Folder" : ByteCountFormatter.string(fromByteCount: item.size, countStyle: .file)).font(.caption).foregroundStyle(.secondary) }
                        Spacer(minLength: 0)
                        if item.isDirectory && !model.selecting { Image(systemName: "chevron.right").font(.caption).foregroundStyle(.tertiary) }
                    }.padding(.vertical, 5)
                }.contextMenu { itemMenu(item) }.listRowBackground(model.selection.contains(item.url) ? Color.blue.opacity(0.1) : Color(uiColor: .secondarySystemGroupedBackground))
                        }
                    } header: { if model.grouping != .none { Text(group.title) } }
                }
            }.listStyle(.insetGrouped).refreshable { await model.reload() }
        }
    }
    private var bottomBar: some View {
        VStack(spacing: 0) {
            Divider()
            if model.selecting && !model.selection.isEmpty {
                HStack {
                    Text("\(model.selection.count) selected").font(.caption).foregroundStyle(.secondary)
                    Spacer()
                    Button { model.copy(model.chosen, cut: true) } label: { Image(systemName: "scissors").frame(width: 34, height: 44) }.accessibilityLabel("Cut selected items")
                    Button { model.copy(model.chosen, cut: false) } label: { Image(systemName: "doc.on.doc").frame(width: 34, height: 44) }.accessibilityLabel("Copy selected items")
                    Button { sheet = .share(model.chosen.map(\.url)) } label: { Image(systemName: "square.and.arrow.up").frame(width: 34, height: 44) }.accessibilityLabel("Share selected items")
                    Button { requestTrash(model.chosen) } label: { Image(systemName: "trash").frame(width: 34, height: 44) }.tint(.red).accessibilityLabel("Move selected items to Recently Deleted")
                }.padding(.horizontal, 16).disabled(model.busy)
            } else {
                HStack {
                    if model.busy { ProgressView().controlSize(.small) }
                    Text("\(model.visible.count) \(model.visible.count == 1 ? "item" : "items")\(model.query.isEmpty ? "" : " · filtered")").font(.caption).foregroundStyle(.secondary)
                    Spacer()
                    if !model.clipboard.isEmpty { Button { Task { await model.paste() } } label: { Label("Paste \(model.clipboard.count)", systemImage: "doc.on.clipboard") }.font(.caption.weight(.semibold)).disabled(model.busy) }
                }.padding(.horizontal, 20).frame(height: 38)
            }
        }.background(.bar)
    }
    private func tap(_ item: FileItem) {
        guard !model.busy else { return }
        if model.selecting { model.toggleSelection(item) }
        else if item.isDirectory { model.navigate(item.url) }
        else { library.opened(item.url); sheet = .preview(item.url) }
    }
    private func requestTrash(_ items: [FileItem]) { trashItems = items; confirmTrash = true }
    @ViewBuilder private func itemMenu(_ item: FileItem) -> some View {
        Button(library.isStarred(item.url) ? "Remove star" : "Star item", systemImage: "star") { library.toggleStar(item.url) }
        Menu("Color label", systemImage: "circle.fill") { ForEach(FileLibrary.labels, id: \.self) { label in Button(label) { library.setLabel(label, urls: [item.url]) } } }
        Button("Power tools", systemImage: "wand.and.stars") { sheet = .tools(model.directory, [item.url]) }
        Button("Copy path", systemImage: "link") { copyFileText(item.url.path) }
        Divider()
        Button(item.isDirectory ? "Open folder" : "Preview", systemImage: item.isDirectory ? "folder" : "eye") { if item.isDirectory { model.navigate(item.url) } else { sheet = .preview(item.url) } }
        Button("Details", systemImage: "info.circle") { sheet = .details(item) }
        Button("Share", systemImage: "square.and.arrow.up") { sheet = .share([item.url]) }
        Divider()
        Button("Copy", systemImage: "doc.on.doc") { model.copy([item], cut: false) }
        Button("Cut", systemImage: "scissors") { model.copy([item], cut: true) }
        Button("Rename", systemImage: "pencil") { renameItem = item; newName = item.name; naming = true }
        Divider()
        Button("Move to Recently Deleted", systemImage: "trash", role: .destructive) { requestTrash([item]) }
    }
}

struct LocationsView: View {
    @ObservedObject var model: BrowserModel
    @Environment(\.dismiss) private var dismiss
    @State private var picking = false
    var body: some View {
        NavigationStack {
            List {
                Section("This device") {
                    Button { model.openLocal(); dismiss() } label: { Label("FileGlade files", systemImage: "iphone") }
                }
                if !model.locations.isEmpty {
                    Section("Your folders") { ForEach(model.locations) { location in Button { model.openLocation(location); dismiss() } label: { Label(location.name, systemImage: "folder") } } }
                }
                Section {
                    Button { picking = true } label: { Label("Browse other folders", systemImage: "folder.badge.plus") }
                } footer: { Text("Choose a folder from iCloud Drive, On My iPhone, or a storage provider in Files. FileGlade can browse the folders you allow.") }
            }
            .navigationTitle("Locations").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Done") { dismiss() } } }
            .fileImporter(isPresented: $picking, allowedContentTypes: [.folder]) { result in
                switch result { case .success(let url): model.addLocation(url); dismiss(); case .failure(let error): model.problem = error.localizedDescription }
            }
        }
    }
}

struct DeletedView: View {
    @ObservedObject var model: BrowserModel
    var body: some View {
        NavigationStack {
            Group {
                if model.deleted.isEmpty { ContentUnavailableView("Nothing in Recently Deleted", systemImage: "trash", description: Text("Items you remove in FileGlade appear here, ready to restore.")) }
                else {
                    List {
                        Section {
                            ForEach(model.deleted) { item in
                                VStack(alignment: .leading, spacing: 9) {
                                    Label(item.name, systemImage: "doc").font(.subheadline.weight(.medium))
                                    Text("Removed \(item.deletedAt.formatted(date: .abbreviated, time: .shortened))").font(.caption).foregroundStyle(.secondary)
                                    Button("Restore to original folder", systemImage: "arrow.uturn.backward") { Task { await model.restore(item) } }.font(.caption).disabled(model.busy)
                                }.padding(.vertical, 6)
                            }
                        } footer: { Text("These items are stored inside FileGlade. Deleting the app also removes them. Restore important files before uninstalling.") }
                    }
                }
            }
            .navigationTitle("Recently Deleted").navigationBarTitleDisplayMode(.inline)
            .onAppear { model.refreshDeleted() }
        }
    }
}

struct DetailsView: View {
    let item: FileItem
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        NavigationStack {
            List {
                Section { VStack(spacing: 18) { Image(systemName: item.icon).font(.system(size: 65)).foregroundStyle(item.isDirectory ? Color.yellow : .blue); Text(item.name).font(.headline).textSelection(.enabled) }.frame(maxWidth: .infinity).padding(.vertical, 22) }
                Section("Details") {
                    LabeledContent("Kind", value: item.kind)
                    if !item.isDirectory { LabeledContent("Size", value: ByteCountFormatter.string(fromByteCount: item.size, countStyle: .file)) }
                    LabeledContent("Modified", value: item.modified.formatted(date: .abbreviated, time: .shortened))
                    VStack(alignment: .leading, spacing: 8) { Text("Location").font(.subheadline); Text(item.url.deletingLastPathComponent().path).font(.caption).foregroundStyle(.secondary).textSelection(.enabled) }
                }
            }.navigationTitle("Details").navigationBarTitleDisplayMode(.inline).toolbar { ToolbarItem(placement: .topBarTrailing) { Button("Done") { dismiss() } } }
        }
    }
}

struct PreviewView: UIViewControllerRepresentable {
    let url: URL
    func makeCoordinator() -> Coordinator { Coordinator(url: url) }
    func makeUIViewController(context: Context) -> QLPreviewController {
        let controller = QLPreviewController(); controller.dataSource = context.coordinator
        if (try? url.resourceValues(forKeys: [.isUbiquitousItemKey]).isUbiquitousItem) == true { try? FileManager.default.startDownloadingUbiquitousItem(at: url) }
        return controller
    }
    func updateUIViewController(_ uiViewController: QLPreviewController, context: Context) { }
    final class Coordinator: NSObject, QLPreviewControllerDataSource {
        let url: URL
        init(url: URL) { self.url = url }
        func numberOfPreviewItems(in controller: QLPreviewController) -> Int { 1 }
        func previewController(_ controller: QLPreviewController, previewItemAt index: Int) -> QLPreviewItem { url as NSURL }
    }
}
struct ShareView: UIViewControllerRepresentable {
    let urls: [URL]
    func makeUIViewController(context: Context) -> UIActivityViewController { UIActivityViewController(activityItems: urls, applicationActivities: nil) }
    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) { }
}
