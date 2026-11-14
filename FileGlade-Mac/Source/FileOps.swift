import AppKit
import SwiftUI
import Foundation

// MARK: - Model types

final class CancelToken {
    private let condition = NSCondition()
    private var cancelledFlag = false
    private var pausedFlag = false
    var isCancelled: Bool { condition.lock(); defer { condition.unlock() }; return cancelledFlag }
    var isPaused: Bool { condition.lock(); defer { condition.unlock() }; return pausedFlag }
    func cancel() { condition.lock(); cancelledFlag = true; pausedFlag = false; condition.broadcast(); condition.unlock() }
    func pause() { condition.lock(); pausedFlag = true; condition.unlock() }
    func resume() { condition.lock(); pausedFlag = false; condition.broadcast(); condition.unlock() }
    /// Blocks while paused, so a transfer stops between pieces and carries on exactly where it left off.
    func waitWhilePaused() { condition.lock(); while pausedFlag && !cancelledFlag { condition.wait() }; condition.unlock() }
}

enum TransferKind: String { case copy = "Copy", move = "Move" }
enum ConflictChoice { case keepBoth, replace, skip }

struct PlannedItem {
    let source: URL
    let target: URL
    let replace: Bool
}
struct TransferFailure: Identifiable {
    let id = UUID()
    let item: PlannedItem
    let message: String
}

struct UndoRecord {
    enum Step {
        case moved(from: URL, to: URL)
        case copied(URL, Date?)
        case replaced(trash: URL, original: URL)
        case trashed(original: URL, trash: URL)
    }
    let title: String
    let steps: [Step]
}

struct TransferJob: Identifiable {
    enum State { case running, finished, cancelled }
    let id = UUID()
    let kind: TransferKind
    var title: String
    var plan: [PlannedItem]
    var totalBytes: Int64 = 0
    var doneBytes: Int64 = 0
    var speed: Double = 0
    var eta: Double?
    var state: State = .running
    var failures: [TransferFailure] = []
    var completed: [URL] = []
    var moved: [RenamePair] = []
    var earlyErrors: [String] = []
    var paused = false
    let token = CancelToken()
    var fraction: Double { totalBytes > 0 ? min(1, Double(doneBytes) / Double(totalBytes)) : (state == .running ? 0 : 1) }
}

// MARK: - Journal: what was in flight, so an interrupted transfer can be finished or cleaned up

struct JournalItem: Codable { var source: String; var target: String; var replace: Bool; var state: String }   // pending, running, done
struct JournalJob: Codable, Identifiable { var id: UUID; var kind: String; var title: String; var items: [JournalItem] }

enum TransferJournal {
    /// Tests point this at a temporary folder.
    static var overrideDirectory: URL?
    private static let lock = NSLock()
    private static var file: URL {
        let dir = overrideDirectory ?? FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("FileGlade", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("transfer-journal.json")
    }
    static func load() -> [JournalJob] { lock.lock(); defer { lock.unlock() }; return (try? Data(contentsOf: file)).flatMap { try? JSONDecoder().decode([JournalJob].self, from: $0) } ?? [] }
    private static func save(_ jobs: [JournalJob]) { if let data = try? JSONEncoder().encode(jobs) { try? data.write(to: file, options: .atomic) } }
    static func begin(_ job: JournalJob) { lock.lock(); var jobs = (try? Data(contentsOf: file)).flatMap { try? JSONDecoder().decode([JournalJob].self, from: $0) } ?? []; jobs.append(job); save(jobs); lock.unlock() }
    static func mark(_ id: UUID, _ index: Int, _ state: String) {
        lock.lock(); defer { lock.unlock() }
        var jobs = (try? Data(contentsOf: file)).flatMap { try? JSONDecoder().decode([JournalJob].self, from: $0) } ?? []
        if let j = jobs.firstIndex(where: { $0.id == id }), index < jobs[j].items.count { jobs[j].items[index].state = state; save(jobs) }
    }
    static func finish(_ id: UUID) {
        lock.lock(); defer { lock.unlock() }
        let jobs = ((try? Data(contentsOf: file)).flatMap { try? JSONDecoder().decode([JournalJob].self, from: $0) } ?? []).filter { $0.id != id }
        save(jobs)
    }
    static func clear() { lock.lock(); save([]); lock.unlock() }

    /// Turns interrupted jobs into a plan that finishes only the unfinished items. Half-written targets are removed first.
    static func recoveryPlan() -> [(kind: TransferKind, title: String, plan: [PlannedItem])] {
        let fm = FileManager.default
        return load().compactMap { job in
            var plan: [PlannedItem] = []
            for item in job.items where item.state != "done" {
                let source = URL(fileURLWithPath: item.source), target = URL(fileURLWithPath: item.target)
                guard fm.fileExists(atPath: source.path) else { continue }
                if item.state == "running" && fm.fileExists(atPath: target.path) && source.path != target.path { try? fm.removeItem(at: target) }
                plan.append(PlannedItem(source: source, target: target, replace: item.replace && item.state == "pending" && fm.fileExists(atPath: target.path)))
            }
            return plan.isEmpty ? nil : (job.kind == "Move" ? .move : .copy, "Resume · " + job.title, plan)
        }
    }
}

enum CopyVerifier {
    /// True when the copy matches the source byte for byte (SHA-256), for a file or every file in a folder.
    static func matches(_ source: URL, _ copy: URL) -> Bool {
        var isDir: ObjCBool = false
        guard FileManager.default.fileExists(atPath: source.path, isDirectory: &isDir) else { return false }
        if !isDir.boolValue { return (try? PowerFS.digest(source)) != nil && (try? PowerFS.digest(source)) == (try? PowerFS.digest(copy)) }
        guard let walk = FileManager.default.enumerator(at: source, includingPropertiesForKeys: [.isDirectoryKey, .isSymbolicLinkKey]) else { return false }
        let base = source.standardizedFileURL.path
        for case let file as URL in walk {
            let values = try? file.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
            if values?.isDirectory == true || values?.isSymbolicLink == true { continue }
            let relative = String(file.standardizedFileURL.path.dropFirst(base.count + 1))
            if (try? PowerFS.digest(file)) != (try? PowerFS.digest(copy.appendingPathComponent(relative))) { return false }
        }
        return true
    }
}

// MARK: - Engine (runs off the main thread)

enum TransferEngine {
    static func size(of url: URL) -> Int64 {
        let fm = FileManager.default
        var isDir: ObjCBool = false
        guard fm.fileExists(atPath: url.path, isDirectory: &isDir) else { return 0 }
        if !isDir.boolValue { return Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0) }
        var total: Int64 = 0
        if let walk = fm.enumerator(at: url, includingPropertiesForKeys: [.fileSizeKey, .isDirectoryKey], options: []) {
            for case let item as URL in walk { total += Int64((try? item.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0) }
        }
        return total
    }

    /// Copies one item. Small files use the system copy (keeps metadata and clones);
    /// large files are copied in chunks so progress and cancellation work.
    static func copyTree(from source: URL, to destination: URL, token: CancelToken, progress: (Int64) -> Void) throws {
        let fm = FileManager.default
        if token.isCancelled { throw CancellationError() }
        let values = try source.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey, .fileSizeKey])
        if values.isSymbolicLink == true { try fm.copyItem(at: source, to: destination); return }
        if values.isDirectory == true {
            try fm.createDirectory(at: destination, withIntermediateDirectories: false)
            for child in try fm.contentsOfDirectory(at: source, includingPropertiesForKeys: nil, options: []) {
                try copyTree(from: child, to: destination.appendingPathComponent(child.lastPathComponent), token: token, progress: progress)
            }
            if let attrs = try? fm.attributesOfItem(atPath: source.path), let date = attrs[.modificationDate] as? Date {
                try? fm.setAttributes([.modificationDate: date], ofItemAtPath: destination.path)
            }
            return
        }
        let size = Int64(values.fileSize ?? 0)
        if size < 64 * 1_048_576 {
            try fm.copyItem(at: source, to: destination)
            progress(size)
            return
        }
        guard fm.createFile(atPath: destination.path, contents: nil) else { throw CocoaError(.fileWriteUnknown) }
        let input = try FileHandle(forReadingFrom: source)
        let output = try FileHandle(forWritingTo: destination)
        defer { try? input.close(); try? output.close() }
        while true {
            token.waitWhilePaused()
            if token.isCancelled { throw CancellationError() }
            let data = try input.read(upToCount: 4 * 1_048_576) ?? Data()
            if data.isEmpty { break }
            try output.write(contentsOf: data)
            progress(Int64(data.count))
        }
        if let attrs = try? fm.attributesOfItem(atPath: source.path) {
            var copy: [FileAttributeKey: Any] = [:]
            if let date = attrs[.modificationDate] { copy[.modificationDate] = date }
            if let mode = attrs[.posixPermissions] { copy[.posixPermissions] = mode }
            try? fm.setAttributes(copy, ofItemAtPath: destination.path)
        }
    }

    struct Outcome {
        var completed: [URL] = []
        var moved: [RenamePair] = []
        var failures: [TransferFailure] = []
        var steps: [UndoRecord.Step] = []
        var cancelled = false
    }

    static func execute(plan: [PlannedItem], kind: TransferKind, token: CancelToken, journalID: UUID? = nil, verify: Bool = UserDefaults.standard.bool(forKey: "verifyCopies"), progress: @escaping (Int64) -> Void) -> Outcome {
        let fm = FileManager.default
        var outcome = Outcome()
        for (index, item) in plan.enumerated() {
            token.waitWhilePaused()
            if token.isCancelled { outcome.cancelled = true; break }
            if let journalID { TransferJournal.mark(journalID, index, "running") }
            do {
                if item.replace && fm.fileExists(atPath: item.target.path) {
                    var trashed: NSURL?
                    try fm.trashItem(at: item.target, resultingItemURL: &trashed)
                    if let trashed { outcome.steps.append(.replaced(trash: trashed as URL, original: item.target)) }
                }
                switch kind {
                case .copy:
                    do { try copyTree(from: item.source, to: item.target, token: token, progress: progress) }
                    catch is CancellationError { try? fm.removeItem(at: item.target); outcome.cancelled = true; break }
                    if outcome.cancelled { break }
                    if verify && !CopyVerifier.matches(item.source, item.target) {
                        try? fm.removeItem(at: item.target)
                        throw NSError(domain: "FileGlade", code: 7, userInfo: [NSLocalizedDescriptionKey: "Verification failed: the copy did not match the original, so it was removed."])
                    }
                    let date = (try? fm.attributesOfItem(atPath: item.target.path))?[.modificationDate] as? Date
                    outcome.steps.append(.copied(item.target, date))
                case .move:
                    let bytes = size(of: item.source)
                    try fm.moveItem(at: item.source, to: item.target)
                    progress(bytes)
                    outcome.steps.append(.moved(from: item.source, to: item.target))
                    outcome.moved.append(RenamePair(source: item.source, destination: item.target))
                }
                if !outcome.cancelled { outcome.completed.append(item.target); if let journalID { TransferJournal.mark(journalID, index, "done") } }
            } catch {
                outcome.failures.append(TransferFailure(item: item, message: error.localizedDescription))
            }
        }
        return outcome
    }
}

// MARK: - Manager (main thread)

final class TransferManager: ObservableObject {
    static let shared = TransferManager()
    @Published var jobs: [TransferJob] = []
    @Published var undoStack: [UndoRecord] = []
    var canUndo: Bool { !undoStack.isEmpty }
    var undoTitle: String { undoStack.last.map { "Undo \($0.title)" } ?? "Undo" }

    static func uniqueURL(_ requested: URL, reserved: Set<String> = []) -> URL {
        let fm = FileManager.default
        if !fm.fileExists(atPath: requested.path) && !reserved.contains(requested.path) { return requested }
        let ext = requested.pathExtension
        let base = requested.deletingPathExtension().lastPathComponent
        var number = 2
        while true {
            let suffix = ext.isEmpty ? "" : ".\(ext)"
            let candidate = requested.deletingLastPathComponent().appendingPathComponent("\(base) copy \(number)\(suffix)")
            if !fm.fileExists(atPath: candidate.path) && !reserved.contains(candidate.path) { return candidate }
            number += 1
        }
    }

    private func describe(_ url: URL) -> String {
        let values = try? url.resourceValues(forKeys: [.fileSizeKey, .contentModificationDateKey, .isDirectoryKey])
        let date = (values?.contentModificationDate ?? .distantPast).formatted(date: .abbreviated, time: .shortened)
        if values?.isDirectory == true { return "folder, modified \(date)" }
        return "\(fileBytes(Int64(values?.fileSize ?? 0))), modified \(date)"
    }

    /// Asks what to do about one name conflict. Self-tests always keep both.
    private func ask(source: URL, existing: URL, remaining: Int) -> (ConflictChoice, Bool) {
        if Prefs.selfTest { return (.keepBoth, true) }
        let alert = NSAlert()
        alert.messageText = "“\(source.lastPathComponent)” already exists here"
        alert.informativeText = "Existing: \(describe(existing))\nIncoming: \(describe(source))"
        alert.addButton(withTitle: "Keep Both")
        alert.addButton(withTitle: "Replace")
        alert.addButton(withTitle: "Skip")
        alert.alertStyle = .warning
        if remaining > 0 { alert.showsSuppressionButton = true; alert.suppressionButton?.title = "Apply to all \(remaining) remaining conflicts" }
        let response = alert.runModal()
        let all = alert.suppressionButton?.state == .on
        switch response {
        case .alertFirstButtonReturn: return (.keepBoth, all)
        case .alertSecondButtonReturn: return (.replace, all)
        default: return (.skip, all)
        }
    }

    private func isDirectory(_ url: URL) -> Bool { (try? url.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) ?? false }

    func start(kind: TransferKind, sources: [URL], into directory: URL, completion: @escaping (TransferJob) -> Void) {
        let fm = FileManager.default
        let destination = directory.resolvingSymlinksInPath().standardizedFileURL
        var plan: [PlannedItem] = []
        var early: [String] = []
        var reserved = Set<String>()
        var sticky: ConflictChoice?
        let conflictCount = sources.filter { fm.fileExists(atPath: destination.appendingPathComponent($0.lastPathComponent).path) }.count
        var asked = 0
        for original in sources {
            let source = original.standardizedFileURL
            let parent = source.deletingLastPathComponent().resolvingSymlinksInPath().standardizedFileURL
            if kind == .move && parent == destination { continue }
            let resolved = source.resolvingSymlinksInPath()
            if isDirectory(source) && (destination.path == resolved.path || destination.path.hasPrefix(resolved.path + "/")) {
                early.append("\(source.lastPathComponent): A folder cannot be placed inside itself."); continue
            }
            let target = destination.appendingPathComponent(source.lastPathComponent)
            if fm.fileExists(atPath: target.path) || reserved.contains(target.path) {
                var choice: ConflictChoice
                if parent == destination { choice = .keepBoth }   // duplicating in place
                else if let sticky { choice = sticky }
                else {
                    asked += 1
                    let answer = ask(source: source, existing: target, remaining: max(0, conflictCount - asked))
                    choice = answer.0
                    if answer.1 { sticky = answer.0 }
                }
                switch choice {
                case .skip: continue
                case .replace: plan.append(PlannedItem(source: source, target: target, replace: true)); reserved.insert(target.path)
                case .keepBoth:
                    let unique = Self.uniqueURL(target, reserved: reserved)
                    plan.append(PlannedItem(source: source, target: unique, replace: false)); reserved.insert(unique.path)
                }
            } else {
                plan.append(PlannedItem(source: source, target: target, replace: false)); reserved.insert(target.path)
            }
        }
        var job = TransferJob(kind: kind, title: "\(kind.rawValue) \(plan.count == 1 ? plan[0].source.lastPathComponent : "\(plan.count) items")", plan: plan)
        job.earlyErrors = early
        if plan.isEmpty { completion(job); return }
        launch(job, completion: completion)
    }

    /// Runs an explicit list of copies or moves (for example from folder sync), creating missing folders.
    func startPlan(kind: TransferKind, plan: [PlannedItem], title: String, completion: @escaping (TransferJob) -> Void) {
        for item in plan { try? FileManager.default.createDirectory(at: item.target.deletingLastPathComponent(), withIntermediateDirectories: true) }
        let job = TransferJob(kind: kind, title: title, plan: plan)
        if plan.isEmpty { completion(job); return }
        launch(job, completion: completion)
    }

    func retry(_ job: TransferJob) {
        var next = TransferJob(kind: job.kind, title: "Retry · " + job.title, plan: job.failures.map(\.item))
        next.earlyErrors = []
        dismiss(job.id)
        launch(next) { _ in NotificationCenter.default.post(name: .init("FileGladeTransferDone"), object: nil) }
    }
    func cancel(_ id: UUID) { jobs.first { $0.id == id }?.token.cancel() }
    func pause(_ id: UUID) { guard let job = jobs.first(where: { $0.id == id }) else { return }; job.token.pause(); update(id) { $0.paused = true } }
    func resume(_ id: UUID) { guard let job = jobs.first(where: { $0.id == id }) else { return }; job.token.resume(); update(id) { $0.paused = false } }
    /// Offers to finish or discard transfers that were interrupted (for example by a crash or a power cut).
    func recoverInterrupted(autoResume: Bool = false) -> Int {
        let recovery = TransferJournal.recoveryPlan()
        let count = recovery.reduce(0) { $0 + $1.plan.count }
        guard count > 0 else { TransferJournal.clear(); return 0 }
        var resume = autoResume
        if !autoResume {
            let alert = NSAlert()
            alert.messageText = "FileGlade was interrupted while copying or moving files"
            alert.informativeText = "\(count) item\(count == 1 ? "" : "s") were not finished. Half-written copies are removed before finishing, and nothing is overwritten."
            alert.addButton(withTitle: "Finish Now"); alert.addButton(withTitle: "Discard")
            resume = alert.runModal() == .alertFirstButtonReturn
        }
        TransferJournal.clear()
        if resume { for job in recovery { startPlan(kind: job.kind, plan: job.plan, title: job.title) { _ in } } }
        return count
    }
    func dismiss(_ id: UUID) { jobs.removeAll { $0.id == id } }

    private func update(_ id: UUID, _ change: (inout TransferJob) -> Void) {
        if let i = jobs.firstIndex(where: { $0.id == id }) { change(&jobs[i]) }
    }

    private func launch(_ job: TransferJob, completion: @escaping (TransferJob) -> Void) {
        let id = job.id
        let kind = job.kind
        let plan = job.plan
        let token = job.token
        if !Prefs.selfTest || TransferJournal.overrideDirectory != nil {
            TransferJournal.begin(JournalJob(id: id, kind: kind.rawValue, title: job.title, items: plan.map { JournalItem(source: $0.source.path, target: $0.target.path, replace: $0.replace, state: "pending") }))
        }
        let finish: (TransferEngine.Outcome) -> Void = { [weak self] outcome in
            guard let self else { return }
            TransferJournal.finish(id)
            var done = job
            if let existing = self.jobs.first(where: { $0.id == id }) { done = existing }
            done.completed = outcome.completed; done.moved = outcome.moved; done.failures = outcome.failures
            done.state = outcome.cancelled ? .cancelled : .finished
            if !outcome.cancelled { done.doneBytes = max(done.doneBytes, done.totalBytes) }
            done.speed = 0; done.eta = nil
            self.update(id) { $0 = done }
            if !outcome.steps.isEmpty { self.record(title: done.title, steps: outcome.steps) }
            if done.failures.isEmpty && !Prefs.selfTest {
                DispatchQueue.main.asyncAfter(deadline: .now() + 4) { [weak self] in self?.dismiss(id) }
            }
            completion(done)
            NotificationCenter.default.post(name: .init("FileGladeTransferDone"), object: nil)
        }
        if Prefs.selfTest {
            finish(TransferEngine.execute(plan: plan, kind: kind, token: token, journalID: id) { _ in })
            return
        }
        jobs.append(job)
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            let total = kind == .copy ? plan.reduce(Int64(0)) { $0 + TransferEngine.size(of: $1.source) } : Int64(plan.count)
            DispatchQueue.main.async { self?.update(id) { $0.totalBytes = total } }
            let lock = NSLock()
            var done: Int64 = 0
            var lastPush = Date.distantPast
            let started = Date()
            let outcome = TransferEngine.execute(plan: plan, kind: kind, token: token, journalID: id) { bytes in
                lock.lock()
                done += kind == .copy ? bytes : 1
                let current = done
                let due = Date().timeIntervalSince(lastPush) > 0.15
                if due { lastPush = Date() }
                lock.unlock()
                guard due else { return }
                let elapsed = max(Date().timeIntervalSince(started), 0.05)
                let speed = Double(current) / elapsed
                DispatchQueue.main.async {
                    self?.update(id) { job in
                        job.doneBytes = current; job.speed = speed
                        job.eta = speed > 0 && job.totalBytes > current ? Double(job.totalBytes - current) / speed : nil
                    }
                }
            }
            DispatchQueue.main.async { finish(outcome) }
        }
    }

    // MARK: Undo

    func record(title: String, steps: [UndoRecord.Step]) {
        guard !steps.isEmpty else { return }
        undoStack.append(UndoRecord(title: title, steps: steps))
        if undoStack.count > 30 { undoStack.removeFirst() }
    }

    /// Undoes the most recent operation. Returns a message describing anything that could not be undone.
    @discardableResult func undoLast() -> String {
        guard let record = undoStack.popLast() else { return "" }
        let fm = FileManager.default
        var problems: [String] = []
        var restored: [RenamePair] = []
        for step in record.steps.reversed() {
            switch step {
            case .moved(let from, let to):
                if !fm.fileExists(atPath: to.path) { problems.append("\(to.lastPathComponent) is no longer there.") }
                else if fm.fileExists(atPath: from.path) { problems.append("\(from.lastPathComponent) cannot go back: something else now has that name.") }
                else if !fm.fileExists(atPath: from.deletingLastPathComponent().path) { problems.append("The original folder for \(from.lastPathComponent) is gone.") }
                else { do { try fm.moveItem(at: to, to: from); restored.append(RenamePair(source: to, destination: from)) } catch { problems.append("\(to.lastPathComponent): \(error.localizedDescription)") } }
            case .copied(let url, let date):
                guard fm.fileExists(atPath: url.path) else { problems.append("\(url.lastPathComponent) is no longer there."); continue }
                let now = (try? fm.attributesOfItem(atPath: url.path))?[.modificationDate] as? Date
                if let date, let now, abs(now.timeIntervalSince(date)) > 1 { problems.append("\(url.lastPathComponent) was changed after copying, so it was kept."); continue }
                do { try fm.trashItem(at: url, resultingItemURL: nil) } catch { problems.append("\(url.lastPathComponent): \(error.localizedDescription)") }
            case .replaced(let trash, let original):
                guard fm.fileExists(atPath: trash.path) else { problems.append("The replaced \(original.lastPathComponent) is no longer in the Trash."); continue }
                if fm.fileExists(atPath: original.path) { problems.append("\(original.lastPathComponent) could not be restored: the name is in use."); continue }
                do { try fm.moveItem(at: trash, to: original) } catch { problems.append("\(original.lastPathComponent): \(error.localizedDescription)") }
            case .trashed(let original, let trash):
                guard fm.fileExists(atPath: trash.path) else { problems.append("\(original.lastPathComponent) is no longer in the Trash."); continue }
                if fm.fileExists(atPath: original.path) { problems.append("\(original.lastPathComponent) could not be restored: the name is in use."); continue }
                do { try fm.moveItem(at: trash, to: original) } catch { problems.append("\(original.lastPathComponent): \(error.localizedDescription)") }
            }
        }
        FileLibrary.shared.applyRename(restored)
        if !Prefs.selfTest { FileLibrary.shared.log("Undo", detail: record.title + (problems.isEmpty ? "" : " · \(problems.count) not undone")) }
        objectWillChange.send()
        NotificationCenter.default.post(name: .init("FileGladeTransferDone"), object: nil)
        return problems.joined(separator: "\n")
    }
}

// MARK: - Progress bar view

struct TransferBar: View {
    @ObservedObject private var manager = TransferManager.shared
    private func eta(_ seconds: Double?) -> String {
        guard let seconds else { return "" }
        return seconds < 60 ? " · \(Int(seconds.rounded(.up))) s left" : " · \(Int((seconds / 60).rounded(.up))) min left"
    }
    var body: some View {
        if !manager.jobs.isEmpty {
            VStack(spacing: 0) {
                Divider()
                ForEach(manager.jobs) { job in
                    HStack(spacing: 10) {
                        Image(systemName: job.kind == .copy ? "doc.on.doc" : "folder").foregroundStyle(.secondary)
                        VStack(alignment: .leading, spacing: 3) {
                            Text(job.title).font(.system(size: 11, weight: .medium)).lineLimit(1)
                            if job.state == .running {
                                ProgressView(value: job.fraction).controlSize(.small).opacity(job.paused ? 0.4 : 1)
                                Text(job.kind == .copy ? "\(fileBytes(job.doneBytes)) of \(fileBytes(job.totalBytes)) · \(fileBytes(Int64(job.speed)))/s\(eta(job.eta))" : "Moving…").font(.system(size: 10)).foregroundStyle(.secondary)
                                if job.paused { Text("Paused").font(.system(size: 10, weight: .semibold)).foregroundStyle(.orange) }
                            } else if job.state == .cancelled {
                                Text("Cancelled · \(job.completed.count) completed").font(.system(size: 10)).foregroundStyle(.secondary)
                            } else if job.failures.isEmpty {
                                Text("Done").font(.system(size: 10)).foregroundStyle(.secondary)
                            } else {
                                Text("\(job.failures.count) failed: " + job.failures.prefix(2).map { "\($0.item.source.lastPathComponent) (\($0.message))" }.joined(separator: "; ")).font(.system(size: 10)).foregroundStyle(.orange).lineLimit(2)
                            }
                        }
                        Spacer()
                        if job.state == .running {
                            Button(job.paused ? "Resume" : "Pause") { if job.paused { manager.resume(job.id) } else { manager.pause(job.id) } }.controlSize(.small)
                            Button("Cancel") { manager.cancel(job.id) }.controlSize(.small)
                        }
                        else {
                            if !job.failures.isEmpty { Button("Retry") { manager.retry(job) }.controlSize(.small) }
                            Button { manager.dismiss(job.id) } label: { Image(systemName: "xmark") }.buttonStyle(.plain).foregroundStyle(.secondary)
                        }
                    }.padding(.horizontal, 20).padding(.vertical, 8)
                }
            }.background(Theme.shared.panel)
        }
    }
}
