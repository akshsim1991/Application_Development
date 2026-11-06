import AppKit
import Foundation

/// Exports and restores your FileGlade setup (rules, collections, notes, shortcuts, saved searches, stars and more).
enum Backup {
    static let keys = ["favorites", "savedSearches", "renamePresets", "shortcutProfile", "shortcutOverrides", "FileGlade.library.v2", "automationRules", "automationAutoRun",
                       "automationTrashDays", "fileCollections", "fileNotes", "layout", "order", "ascending", "grouping", "inspector", "filter", "showHidden", "dualPane", "autoUpdateCheck"]

    static func export(from defaults: UserDefaults = .standard) throws -> Data {
        var payload: [String: Any] = [:]
        for key in keys { if let value = defaults.object(forKey: key) { payload[key] = value } }
        payload["_fileglade"] = ["format": 1, "date": Date()] as [String: Any]
        return try PropertyListSerialization.data(fromPropertyList: payload, format: .xml, options: 0)
    }
    /// Returns how many settings were restored. Only known keys are accepted.
    @discardableResult static func restore(_ data: Data, into defaults: UserDefaults = .standard) throws -> Int {
        guard let payload = try PropertyListSerialization.propertyList(from: data, options: [], format: nil) as? [String: Any], payload["_fileglade"] != nil else {
            throw NSError(domain: "FileGlade", code: 1, userInfo: [NSLocalizedDescriptionKey: "This is not a FileGlade settings backup."])
        }
        var count = 0
        for key in keys { if let value = payload[key] { defaults.set(value, forKey: key); count += 1 } }
        return count
    }

    static func saveWithPanel() {
        let panel = NSSavePanel()
        panel.nameFieldStringValue = "FileGlade Settings.plist"
        panel.message = "Save a backup of your FileGlade rules, collections, notes, shortcuts and preferences."
        guard panel.runModal() == .OK, let url = panel.url else { return }
        do { try export().write(to: url, options: .atomic) } catch { alert("Backup failed", error.localizedDescription) }
    }
    static func restoreWithPanel() {
        let panel = NSOpenPanel(); panel.canChooseFiles = true; panel.canChooseDirectories = false
        panel.message = "Choose a FileGlade settings backup to restore."
        guard panel.runModal() == .OK, let url = panel.url else { return }
        let confirm = NSAlert(); confirm.messageText = "Restore these settings?"
        confirm.informativeText = "Your current rules, collections, notes, shortcuts and preferences will be replaced by the backup."
        confirm.addButton(withTitle: "Restore"); confirm.addButton(withTitle: "Cancel")
        guard confirm.runModal() == .alertFirstButtonReturn else { return }
        do {
            let count = try restore(Data(contentsOf: url))
            alert("Settings restored", "\(count) settings were restored. Quit and reopen FileGlade to apply them everywhere.")
        } catch { alert("Restore failed", error.localizedDescription) }
    }
    private static func alert(_ title: String, _ text: String) { let a = NSAlert(); a.messageText = title; a.informativeText = text; a.addButton(withTitle: "OK"); a.runModal() }
}
