import AppKit
import SwiftUI
import CryptoKit

/// Self-updating, in the same way as FeatherPDF: the app reads a small `latest.json` manifest from a public
/// releases repository, asks whether to update now or later, downloads the new version, checks its size,
/// SHA-256 fingerprint and code signature, then swaps itself in and relaunches.
struct UpdateManifest: Decodable {
    struct Asset: Decodable { let url: URL; let size: Int64?; let sha256: String }
    let version: String
    let date: String?
    let notes: String?
    let mac: Asset
}

final class Updater: ObservableObject {
    static let shared = Updater()
    static let manifestURL = URL(string: "https://raw.githubusercontent.com/akshsim1991/FileGlade-releases/main/latest.json")!
    private static let allowedHosts: Set<String> = ["raw.githubusercontent.com", "github.com", "objects.githubusercontent.com", "release-assets.githubusercontent.com"]

    enum State: Equatable { case idle, checking, downloading(Double), installing, failed(String) }
    @Published var state: State = .idle

    var currentVersion: String { Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0" }
    var automatic: Bool {
        get { UserDefaults.standard.object(forKey: "autoUpdateCheck") as? Bool ?? true }
        set { UserDefaults.standard.set(newValue, forKey: "autoUpdateCheck"); objectWillChange.send() }
    }

    /// True when `a` is a higher version than `b` (numeric, dot-separated; missing parts count as 0).
    static func isNewer(_ a: String, than b: String) -> Bool {
        let x = a.split(separator: ".").map { Int($0.filter(\.isNumber)) ?? 0 }
        let y = b.split(separator: ".").map { Int($0.filter(\.isNumber)) ?? 0 }
        for i in 0..<max(x.count, y.count) {
            let l = i < x.count ? x[i] : 0, r = i < y.count ? y[i] : 0
            if l != r { return l > r }
        }
        return false
    }

    /// Checks at most once a day unless the user asked.
    func checkAutomatically() {
        guard automatic, !Prefs.selfTest else { return }
        let last = UserDefaults.standard.double(forKey: "lastUpdateCheck")
        guard Date().timeIntervalSince1970 - last > 86_400 else { return }
        check(userInitiated: false)
    }

    func check(userInitiated: Bool) {
        guard state == .idle || { if case .failed = state { return true }; return false }() else { return }
        state = .checking
        var request = URLRequest(url: Self.manifestURL, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 20)
        request.setValue("FileGlade/\(currentVersion)", forHTTPHeaderField: "User-Agent")
        URLSession.shared.dataTask(with: request) { [weak self] data, response, error in
            DispatchQueue.main.async {
                guard let self else { return }
                UserDefaults.standard.set(Date().timeIntervalSince1970, forKey: "lastUpdateCheck")
                guard error == nil, (response as? HTTPURLResponse)?.statusCode == 200, let data, let manifest = try? JSONDecoder().decode(UpdateManifest.self, from: data) else {
                    self.state = .idle
                    if userInitiated { self.inform("Could not check for updates", "FileGlade could not reach the update server. Check your internet connection and try again.") }
                    return
                }
                self.state = .idle
                guard Self.isNewer(manifest.version, than: self.currentVersion) else {
                    if userInitiated { self.inform("FileGlade is up to date", "You have the latest version (\(self.currentVersion)).") }
                    return
                }
                if !userInitiated, UserDefaults.standard.string(forKey: "skippedUpdateVersion") == manifest.version { return }
                self.offer(manifest)
            }
        }.resume()
    }

    private func inform(_ title: String, _ text: String) {
        let alert = NSAlert(); alert.messageText = title; alert.informativeText = text; alert.addButton(withTitle: "OK"); alert.runModal()
    }

    private func offer(_ manifest: UpdateManifest) {
        let alert = NSAlert()
        alert.messageText = "FileGlade \(manifest.version) is available"
        alert.informativeText = "You have \(currentVersion)." + (manifest.date.map { " Released \($0)." } ?? "") + ((manifest.notes?.isEmpty == false) ? "\n\n" + manifest.notes! : "")
        alert.addButton(withTitle: "Update Now")
        alert.addButton(withTitle: "Later")
        alert.addButton(withTitle: "Skip This Version")
        switch alert.runModal() {
        case .alertFirstButtonReturn: install(manifest)
        case .alertThirdButtonReturn: UserDefaults.standard.set(manifest.version, forKey: "skippedUpdateVersion")
        default: break
        }
    }

    private func fail(_ message: String) { DispatchQueue.main.async { self.state = .failed(message) } }

    private func install(_ manifest: UpdateManifest) {
        guard manifest.mac.url.scheme == "https", let host = manifest.mac.url.host, Self.allowedHosts.contains(host) else { fail("The update was not offered from a trusted address."); return }
        let destination = Bundle.main.bundleURL
        guard FileManager.default.isWritableFile(atPath: destination.deletingLastPathComponent().path) else {
            fail("FileGlade cannot replace itself in \(destination.deletingLastPathComponent().path). Move it to Applications or download the update manually.")
            return
        }
        state = .downloading(0)
        let delegate = DownloadDelegate { [weak self] fraction in DispatchQueue.main.async { self?.state = .downloading(fraction) } }
        let session = URLSession(configuration: .ephemeral, delegate: delegate, delegateQueue: nil)
        delegate.completion = { [weak self] location, error in
            guard let self else { return }
            guard let location, error == nil else { self.fail("The download failed. Try again later."); return }
            DispatchQueue.global(qos: .userInitiated).async { self.verifyAndSwap(zip: location, manifest: manifest, destination: destination) }
        }
        session.downloadTask(with: manifest.mac.url).resume()
    }

    private func run(_ tool: String, _ args: [String]) -> (Int32, String) {
        let process = Process(); process.executableURL = URL(fileURLWithPath: tool); process.arguments = args
        let pipe = Pipe(); process.standardOutput = pipe; process.standardError = pipe
        do { try process.run() } catch { return (-1, error.localizedDescription) }
        let data = pipe.fileHandleForReading.readDataToEndOfFile(); process.waitUntilExit()
        return (process.terminationStatus, String(decoding: data, as: UTF8.self))
    }
    private func teamID(_ app: URL) -> String? {
        let output = run("/usr/bin/codesign", ["-dv", "--verbose=2", app.path]).1
        return output.split(separator: "\n").first { $0.hasPrefix("TeamIdentifier=") }.map { String($0.dropFirst("TeamIdentifier=".count)) }
    }

    private func verifyAndSwap(zip: URL, manifest: UpdateManifest, destination: URL) {
        let fm = FileManager.default
        let work = fm.temporaryDirectory.appendingPathComponent("FileGlade-update-" + UUID().uuidString)
        do {
            try fm.createDirectory(at: work, withIntermediateDirectories: true)
            let archive = work.appendingPathComponent("update.zip")
            try fm.moveItem(at: zip, to: archive)
            DispatchQueue.main.async { self.state = .installing }
            // 1. Size and fingerprint must match the manifest.
            let size = (try fm.attributesOfItem(atPath: archive.path)[.size] as? NSNumber)?.int64Value ?? 0
            if let expected = manifest.mac.size, expected != size { throw NSError(domain: "Updater", code: 1, userInfo: [NSLocalizedDescriptionKey: "The downloaded file is the wrong size."]) }
            var hasher = SHA256()
            let handle = try FileHandle(forReadingFrom: archive)
            while let chunk = try handle.read(upToCount: 1 << 20), !chunk.isEmpty { hasher.update(data: chunk) }
            try? handle.close()
            let digest = hasher.finalize().map { String(format: "%02x", $0) }.joined()
            guard digest == manifest.mac.sha256.lowercased() else { throw NSError(domain: "Updater", code: 2, userInfo: [NSLocalizedDescriptionKey: "The downloaded file failed its security check, so it was not installed."]) }
            // 2. Unpack.
            let extracted = work.appendingPathComponent("extracted")
            guard run("/usr/bin/ditto", ["-x", "-k", archive.path, extracted.path]).0 == 0 else { throw NSError(domain: "Updater", code: 3, userInfo: [NSLocalizedDescriptionKey: "The update could not be unpacked."]) }
            guard let app = try fm.contentsOfDirectory(at: extracted, includingPropertiesForKeys: nil).first(where: { $0.pathExtension == "app" }) else { throw NSError(domain: "Updater", code: 4, userInfo: [NSLocalizedDescriptionKey: "The update did not contain an app."]) }
            // 3. It must be validly signed by the same developer as this copy.
            guard run("/usr/bin/codesign", ["--verify", "--deep", "--strict", app.path]).0 == 0 else { throw NSError(domain: "Updater", code: 5, userInfo: [NSLocalizedDescriptionKey: "The update's signature is not valid."]) }
            if let current = teamID(Bundle.main.bundleURL), let incoming = teamID(app), current != "not set", current != incoming { throw NSError(domain: "Updater", code: 6, userInfo: [NSLocalizedDescriptionKey: "The update was signed by a different developer, so it was not installed."]) }
            // 4. A helper waits for this app to quit, swaps the bundle (keeping the old one until the new copy is in place), and reopens it.
            let script = work.appendingPathComponent("swap.sh")
            let body = """
            #!/bin/zsh
            while kill -0 \(ProcessInfo.processInfo.processIdentifier) 2>/dev/null; do sleep 0.3; done
            DEST="\(destination.path)"
            NEW="\(app.path)"
            rm -rf "$DEST.old"
            if mv "$DEST" "$DEST.old" && ditto "$NEW" "$DEST"; then
              rm -rf "$DEST.old"
            else
              rm -rf "$DEST"; mv "$DEST.old" "$DEST"
            fi
            open "$DEST"
            rm -rf "\(work.path)"
            """
            try body.write(to: script, atomically: true, encoding: .utf8)
            let helper = Process(); helper.executableURL = URL(fileURLWithPath: "/bin/zsh"); helper.arguments = [script.path]
            try helper.run()
            DispatchQueue.main.async { NSApp.terminate(nil) }
        } catch {
            try? fm.removeItem(at: work)
            fail(error.localizedDescription)
        }
    }
}

private final class DownloadDelegate: NSObject, URLSessionDownloadDelegate {
    var completion: ((URL?, Error?) -> Void)?
    private let progress: (Double) -> Void
    private var finished: URL?
    init(progress: @escaping (Double) -> Void) { self.progress = progress }
    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didWriteData bytesWritten: Int64, totalBytesWritten: Int64, totalBytesExpectedToWrite: Int64) {
        if totalBytesExpectedToWrite > 0 { progress(Double(totalBytesWritten) / Double(totalBytesExpectedToWrite)) }
    }
    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didFinishDownloadingTo location: URL) {
        // The system deletes `location` after this returns, so keep our own copy.
        let keep = FileManager.default.temporaryDirectory.appendingPathComponent("FileGlade-download-" + UUID().uuidString + ".zip")
        if (downloadTask.response as? HTTPURLResponse)?.statusCode == 200, (try? FileManager.default.moveItem(at: location, to: keep)) != nil { finished = keep }
    }
    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) { completion?(finished, finished == nil ? (error ?? URLError(.badServerResponse)) : nil); session.finishTasksAndInvalidate() }
}

struct UpdateBanner: View {
    @ObservedObject private var updater = Updater.shared
    var body: some View {
        switch updater.state {
        case .downloading(let fraction):
            row { ProgressView(value: fraction).frame(width: 160); Text("Downloading update… \(Int(fraction * 100))%") }
        case .installing: row { ProgressView().controlSize(.small); Text("Installing update. FileGlade will restart.") }
        case .failed(let message): row { Image(systemName: "exclamationmark.triangle").foregroundStyle(.orange); Text(message).lineLimit(2); Spacer(); Button("Dismiss") { updater.state = .idle }.controlSize(.small) }
        default: EmptyView()
        }
    }
    private func row<Content: View>(@ViewBuilder _ content: () -> Content) -> some View {
        VStack(spacing: 0) { HStack(spacing: 10) { content() }.font(.system(size: 11)).padding(.horizontal, 14).frame(height: 28); Divider() }.background(Theme.shared.panel)
    }
}
