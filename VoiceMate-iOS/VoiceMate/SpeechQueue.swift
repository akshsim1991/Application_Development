import Foundation

/// One queue for everything the app says: no overlapping announcements, urgent warnings first, stale
/// navigation prompts dropped, and duplicate prompts suppressed.
@MainActor
final class SpeechQueue {
    private struct Item {
        let text: String
        let kind: SpeechKind
        let created = Date()
        let done: CheckedContinuation<Void, Never>
    }

    let engine: SpeechEngine
    /// Called with true when speech starts and false when the queue is empty again.
    var onBusy: ((Bool) -> Void)?

    private var items: [Item] = []
    private var current: Item?
    private var worker: Task<Void, Never>?
    private var lastSpoken: (text: String, at: Date)?

    init(engine: SpeechEngine) { self.engine = engine }

    /// Returns when this item has been spoken or dropped.
    func say(_ text: String, kind: SpeechKind = .reply) async {
        let t = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty else { return }
        if kind == .guidance, let last = lastSpoken, last.text == t, Date().timeIntervalSince(last.at) < 5 { return }
        await withCheckedContinuation { (c: CheckedContinuation<Void, Never>) in
            let item = Item(text: t, kind: kind, done: c)
            if kind == .urgent {
                items.removeAll { if $0.kind == .guidance { $0.done.resume(); return true }; return false }
                items.insert(item, at: 0)
                if let cur = current, cur.kind != .urgent { engine.stop() }
            } else {
                items.append(item)
            }
            startWorker()
        }
    }

    func stopAll() {
        let waiting = items
        items = []
        waiting.forEach { $0.done.resume() }
        engine.stop()
    }

    private func startWorker() {
        guard worker == nil else { return }
        worker = Task { [weak self] in
            guard let self else { return }
            onBusy?(true)
            while let item = nextItem() {
                current = item
                await engine.speak(item.text, kind: item.kind)
                lastSpoken = (item.text, Date())
                current = nil
                item.done.resume()
            }
            onBusy?(false)
            worker = nil
        }
    }

    private func nextItem() -> Item? {
        let now = Date()
        items.removeAll { item in
            if item.kind == .guidance, now.timeIntervalSince(item.created) > 12 { item.done.resume(); return true }
            return false
        }
        return items.isEmpty ? nil : items.removeFirst()
    }
}
