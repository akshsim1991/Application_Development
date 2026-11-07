import SwiftUI

/// Ties together: headphone mic -> speech recognition -> Assistant -> spoken reply in headphones.
@MainActor
final class ConversationController: ObservableObject {
    static let shared = ConversationController()

    let route = AudioRoute()
    let listener = SpeechListener()
    private let speaker = Speaker()

    @Published var lastHeard = ""
    @Published var lastReply = ""
    @Published var errorMessage: String?
    @Published var isBusy = false
    /// When on, VoiceMate keeps listening again after each reply.
    @Published var continuous = false

    /// Set by the "Start Talking" Siri intent; the UI starts a turn when the app appears.
    var pendingListen = false

    func toggle() {
        if listener.isListening || isBusy { cancel() } else { Task { await beginTurn() } }
    }

    func cancel() {
        listener.stop()
        speaker.stop()
        isBusy = false
        continuous = false
        route.deactivate()
    }

    func beginTurn() async {
        errorMessage = nil
        guard await SpeechListener.requestPermissions() else {
            errorMessage = ListenError.notAuthorized.errorDescription
            return
        }
        do {
            try route.activate()
            isBusy = true
            try listener.start { [weak self] text in
                Task { @MainActor in await self?.handle(text) }
            }
        } catch {
            isBusy = false
            errorMessage = error.localizedDescription
        }
    }

    private func handle(_ text: String) async {
        lastHeard = text
        guard !text.isEmpty else {
            isBusy = false
            if continuous { continuous = false }
            return
        }
        let answer = await Assistant.shared.reply(to: text)
        lastReply = answer
        await speaker.speak(answer)
        isBusy = false
        if continuous { await beginTurn() } else { route.deactivate() }
    }
}
