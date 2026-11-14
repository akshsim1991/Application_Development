import SwiftUI
import AppKit

/// Sizes of the sidebar, details pane, second pane and file-list columns. They are remembered between launches.
final class PaneLayout: ObservableObject {
    static let shared = PaneLayout()
    @Published var sidebar: Double { didSet { save("sidebar", sidebar) } }
    @Published var inspector: Double { didSet { save("inspector", inspector) } }
    @Published var second: Double { didSet { save("second", second) } }
    @Published var dateColumn: Double { didSet { save("dateColumn", dateColumn) } }
    @Published var kindColumn: Double { didSet { save("kindColumn", kindColumn) } }
    @Published var sizeColumn: Double { didSet { save("sizeColumn", sizeColumn) } }
    init() {
        func value(_ key: String, _ fallback: Double) -> Double { let v = UserDefaults.standard.double(forKey: "layout." + key); return v > 0 ? v : fallback }
        sidebar = value("sidebar", 208); inspector = value("inspector", 280); second = value("second", 460)
        dateColumn = value("dateColumn", 160); kindColumn = value("kindColumn", 130); sizeColumn = value("sizeColumn", 80)
    }
    private func save(_ key: String, _ value: Double) { if !Prefs.selfTest { UserDefaults.standard.set(value, forKey: "layout." + key) } }
    func reset() { sidebar = 208; inspector = 280; second = 460; dateColumn = 160; kindColumn = 130; sizeColumn = 80 }
}

/// A thin draggable divider, like the one between panes in Windows Explorer. Double-click resets the size.
struct ResizeHandle: View {
    @Binding var width: Double
    var range: ClosedRange<Double>
    /// True when the panel being resized sits to the right of the handle (dragging left makes it wider).
    var growsLeft = false
    var resetTo: Double? = nil
    @State private var start: Double?
    @State private var hovering = false
    var body: some View {
        Rectangle().fill(hovering || start != nil ? Color.accentColor.opacity(0.55) : Color.primary.opacity(0.12))
            .frame(width: hovering || start != nil ? 3 : 1)
            .frame(width: 7).contentShape(Rectangle())
            .onHover { inside in hovering = inside; if inside { NSCursor.resizeLeftRight.push() } else { NSCursor.pop() } }
            .gesture(DragGesture(minimumDistance: 1, coordinateSpace: .global)
                .onChanged { drag in
                    if start == nil { start = width }
                    let delta = growsLeft ? -drag.translation.width : drag.translation.width
                    width = min(max((start ?? width) + Double(delta), range.lowerBound), range.upperBound)
                }
                .onEnded { _ in start = nil })
            .onTapGesture(count: 2) { if let resetTo { width = resetTo } }
            .help("Drag to resize. Double-click to reset.")
    }
}

/// Column headers for the details list: click to sort, drag the divider to resize.
struct DetailsHeader: View {
    @ObservedObject var model: Explorer
    var showDate = true
    var showKind = true
    @ObservedObject private var layout = PaneLayout.shared
    var body: some View {
        HStack(spacing: 0) {
            cell("Name", .name).frame(maxWidth: .infinity, alignment: .leading).padding(.leading, 40)
            if showDate { handle($layout.dateColumn, 90...400); cell("Date modified", .modified).frame(width: layout.dateColumn, alignment: .leading) }
            if showKind { handle($layout.kindColumn, 70...350); cell("Kind", .kind).frame(width: layout.kindColumn, alignment: .leading) }
            handle($layout.sizeColumn, 60...260); cell("Size", .size).frame(width: layout.sizeColumn, alignment: .trailing).padding(.trailing, 12)
        }
        .frame(height: 24).background(Theme.shared.panel)
        .contextMenu { Button("Reset Column Widths") { layout.reset() } }
    }
    private func cell(_ title: String, _ order: Order) -> some View {
        Button {
            if model.order == order { model.ascending.toggle() } else { model.order = order; model.ascending = true }
        } label: {
            HStack(spacing: 4) {
                Text(title).font(.system(size: 11, weight: .medium)).foregroundStyle(model.order == order ? Color.primary : Color.secondary)
                if model.order == order { Image(systemName: model.ascending ? "chevron.up" : "chevron.down").font(.system(size: 8, weight: .bold)).foregroundStyle(Color.accentColor) }
            }.frame(maxWidth: .infinity, alignment: title == "Size" ? .trailing : .leading).contentShape(Rectangle())
        }.buttonStyle(.plain)
    }
    /// Dragging a divider left makes the column to its right wider, like Windows.
    private func handle(_ width: Binding<Double>, _ range: ClosedRange<Double>) -> some View {
        ResizeHandle(width: width, range: range, growsLeft: true).frame(height: 24)
    }
}

/// The click layer behind a file row. The row's own text and icons ignore the mouse, so every click anywhere on the row
/// lands here: a double-click opens the item, a single click selects (⌘ toggles, ⇧ extends), a drag carries the selected
/// files to Finder or another pane, and a folder row accepts files dropped on it.
struct RowClickCatcher: NSViewRepresentable {
    let onOpen: () -> Void
    let dragURLs: () -> [URL]
    var dropInto: (([URL]) -> Void)? = nil
    func makeNSView(context: Context) -> CatcherView { let v = CatcherView(); update(v); return v }
    func updateNSView(_ view: CatcherView, context: Context) { update(view) }
    private func update(_ view: CatcherView) { view.onOpen = onOpen; view.dragURLs = dragURLs; view.dropInto = dropInto; view.refreshDropRegistration() }

    final class CatcherView: NSView, NSDraggingSource {
        var onOpen: () -> Void = {}
        var dragURLs: () -> [URL] = { [] }
        var dropInto: (([URL]) -> Void)?
        private var table: NSTableView? { var v: NSView? = self; while let current = v { if let t = current as? NSTableView { return t }; v = current.superview }; return nil }
        func refreshDropRegistration() { unregisterDraggedTypes(); if dropInto != nil { registerForDraggedTypes([.fileURL]) } }

        override func menu(for event: NSEvent) -> NSMenu? { table?.menu(for: event) ?? super.menu(for: event) }
        override func mouseDown(with event: NSEvent) {
            guard let table else { super.mouseDown(with: event); return }
            let row = table.row(at: table.convert(event.locationInWindow, from: nil))
            if event.clickCount >= 2 { onOpen(); return }
            guard row >= 0 else { return }
            let flags = event.modifierFlags
            let plain = !flags.contains(.command) && !flags.contains(.shift)
            if plain && !table.isRowSelected(row) { table.selectRowIndexes([row], byExtendingSelection: false) }
            let start = event.locationInWindow
            // Wait to see whether this is a click or the start of a drag.
            while let next = window?.nextEvent(matching: [.leftMouseDragged, .leftMouseUp]) {
                if next.type == .leftMouseUp { break }
                if hypot(next.locationInWindow.x - start.x, next.locationInWindow.y - start.y) > 4 {
                    if !table.isRowSelected(row) { table.selectRowIndexes([row], byExtendingSelection: flags.contains(.command)) }
                    beginDrag(with: next)
                    return
                }
            }
            if flags.contains(.command) { if table.isRowSelected(row) { table.deselectRow(row) } else { table.selectRowIndexes([row], byExtendingSelection: true) } }
            else if flags.contains(.shift), let anchor = table.selectedRowIndexes.filter({ $0 != row }).min(by: { abs($0 - row) < abs($1 - row) }) {
                table.selectRowIndexes(IndexSet(integersIn: min(anchor, row)...max(anchor, row)), byExtendingSelection: false)
            } else if plain { table.selectRowIndexes([row], byExtendingSelection: false) }
        }
        private func beginDrag(with event: NSEvent) {
            let urls = dragURLs()
            guard !urls.isEmpty else { return }
            let items = urls.enumerated().map { index, url -> NSDraggingItem in
                let item = NSDraggingItem(pasteboardWriter: url as NSURL)
                let icon = NSWorkspace.shared.icon(forFile: url.path); icon.size = NSSize(width: 32, height: 32)
                item.setDraggingFrame(NSRect(x: 8 + CGFloat(min(index, 4)) * 6, y: 0, width: 32, height: 32), contents: icon)
                return item
            }
            beginDraggingSession(with: items, event: event, source: self)
        }
        func draggingSession(_ session: NSDraggingSession, sourceOperationMaskFor context: NSDraggingContext) -> NSDragOperation { [.copy, .move, .generic] }

        override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation { dropInto == nil ? [] : (NSEvent.modifierFlags.contains(.option) ? .copy : .generic) }
        override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
            guard let dropInto, let urls = sender.draggingPasteboard.readObjects(forClasses: [NSURL.self], options: [.urlReadingFileURLsOnly: true]) as? [URL], !urls.isEmpty else { return false }
            dropInto(urls); return true
        }
    }
}
