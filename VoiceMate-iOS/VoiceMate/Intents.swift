import AppIntents

/// "Hey Siri, ask VoiceMate" (when Siri is triggered by the helmet button). Runs without opening the app.
struct AskVoiceMateIntent: AppIntent {
    static var title: LocalizedStringResource = "Ask VoiceMate"
    static var description = IntentDescription("Ask VoiceMate for speed, location, time and more.")
    static var openAppWhenRun = false

    @Parameter(title: "Question", requestValueDialog: "What would you like to know?")
    var question: String

    @MainActor
    func perform() async throws -> some IntentResult & ReturnsValue<String> & ProvidesDialog {
        let answer = await Assistant.shared.reply(to: question)
        return .result(value: answer, dialog: IntentDialog(stringLiteral: answer))
    }
}

/// Hold the helmet button -> "Start ride mode": launches the app and begins always-on listening.
struct StartRideModeIntent: AppIntent {
    static var title: LocalizedStringResource = "Start Ride Mode"
    static var description = IntentDescription("Open VoiceMate and start hands-free listening.")
    static var openAppWhenRun = true

    @MainActor
    func perform() async throws -> some IntentResult {
        RideController.shared.pendingStart = true
        return .result()
    }
}

/// Hold the helmet button -> "Navigate with VoiceMate" -> "Where do you want to go?" -> answer.
struct NavigateToIntent: AppIntent {
    static var title: LocalizedStringResource = "Navigate with VoiceMate"
    static var description = IntentDescription("Start audio-only navigation to a place.")
    static var openAppWhenRun = true

    @Parameter(title: "Destination", requestValueDialog: "Where do you want to go?")
    var destination: String

    @MainActor
    func perform() async throws -> some IntentResult {
        RideController.shared.pendingDestination = destination
        RideController.shared.pendingStart = true
        return .result()
    }
}

struct VoiceMateShortcuts: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: StartRideModeIntent(),
            phrases: ["Start ride mode in \(.applicationName)", "Start \(.applicationName)", "Ride mode in \(.applicationName)"],
            shortTitle: "Ride Mode",
            systemImageName: "figure.outdoor.cycle"
        )
        AppShortcut(
            intent: NavigateToIntent(),
            phrases: ["Navigate with \(.applicationName)", "Take me somewhere with \(.applicationName)"],
            shortTitle: "Navigate",
            systemImageName: "location.north.line"
        )
        AppShortcut(
            intent: AskVoiceMateIntent(),
            phrases: ["Ask \(.applicationName)", "Ask \(.applicationName) a question"],
            shortTitle: "Ask",
            systemImageName: "questionmark.bubble"
        )
    }
}
