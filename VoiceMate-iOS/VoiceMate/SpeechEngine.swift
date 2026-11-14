import Foundation

enum SpeechKind {
    /// Answer to something you asked. Always spoken.
    case reply
    /// Navigation prompt. Dropped if it has waited too long (a stale turn instruction is worse than none).
    case guidance
    /// Safety warning. Jumps the queue and interrupts ordinary speech.
    case urgent
}

/// A replaceable text-to-speech backend. The app ships with `AppleSpeechEngine` (on-device system voices).
/// Another engine (a licensed on-device neural voice, or an optional online one) can be added by
/// implementing this protocol; nothing else in the app needs to change.
@MainActor
protocol SpeechEngine: AnyObject {
    var name: String { get }
    var isOnDevice: Bool { get }
    /// Locks the chosen voice for the whole ride so it can't change mid-ride.
    func prepareForRide()
    /// Returns when the text has been spoken (or cancelled).
    func speak(_ text: String, kind: SpeechKind) async
    func stop()
}
