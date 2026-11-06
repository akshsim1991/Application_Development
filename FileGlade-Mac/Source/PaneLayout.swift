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
    @ObservedObject private var layout = PaneLayout.shared
    var body: some View {
        HStack(spacing: 0) {
            cell("Name", .name).frame(maxWidth: .infinity, alignment: .leading).padding(.leading, 40)
            handle($layout.dateColumn, 90...400); cell("Date modified", .modified).frame(width: layout.dateColumn, alignment: .leading)
            handle($layout.kindColumn, 70...350); cell("Kind", .kind).frame(width: layout.kindColumn, alignment: .leading)
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
