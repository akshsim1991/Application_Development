import AppIntents

/// "Hey Siri, ask VoiceMate" -> Siri asks "What would you like to ask?" -> answer is spoken by Siri
/// through your headphones. Runs without opening the app.
struct AskVoiceMateIntent: AppIntent {
    static var title: LocalizedStringResource = "Ask VoiceMate"
    static var description = IntentDescription("Ask VoiceMate a question and hear the answer.")
    static var openAppWhenRun = false

    @Parameter(title: "Question", requestValueDialog: "What would you like to ask?")
    var question: String

    @MainActor
    func perform() async throws -> some IntentResult & ReturnsValue<String> & ProvidesDialog {
        let answer = await Assistant.shared.reply(to: question)
        return .result(value: answer, dialog: IntentDialog(stringLiteral: answer))
    }
}

/// "Hey Siri, talk to VoiceMate" -> opens the app and starts the headphone-mic conversation loop.
struct StartTalkingIntent: AppIntent {
    static var title: LocalizedStringResource = "Talk to VoiceMate"
    static var description = IntentDescription("Open VoiceMate and start a hands-free conversation.")
    static var openAppWhenRun = true

    @MainActor
    func perform() async throws -> some IntentResult {
        ConversationController.shared.pendingListen = true
        ConversationController.shared.continuous = true
        return .result()
    }
}

struct VoiceMateShortcuts: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: AskVoiceMateIntent(),
            phrases: [
                "Ask \(.applicationName)",
                "Ask \(.applicationName) a question",
                "Question for \(.applicationName)"
            ],
            shortTitle: "Ask",
            systemImageName: "questionmark.bubble"
        )
        AppShortcut(
            intent: StartTalkingIntent(),
            phrases: [
                "Talk to \(.applicationName)",
                "Start \(.applicationName)",
                "Chat with \(.applicationName)"
            ],
            shortTitle: "Talk",
            systemImageName: "waveform"
        )
    }
}
