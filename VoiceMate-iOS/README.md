# VoiceMate (iPhone)

Hands-free voice assistant that works through Bluetooth headphones (AirPods, etc.).

## What it does
- **Siri → VoiceMate:** with headphones on, say *"Hey Siri, ask VoiceMate"* (Siri then asks your question and
  speaks the answer) or *"Hey Siri, talk to VoiceMate"* (opens the app and starts a hands-free loop).
- **In-app loop:** tap the mic; your speech is captured from the headphone mic (Speech framework), answered by
  `Assistant`, and spoken back into the headphones (`AVSpeechSynthesizer`).

## Important limitation
iOS gives third-party apps no API to send audio to Siri or read Siri's replies. Integration with Siri is
one-way: Siri launches VoiceMate through App Intents. VoiceMate has its own speech-in / speech-out loop.

## Build
Requires macOS + Xcode 15+ and [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install xcodegen`).

    cd VoiceMate-iOS
    xcodegen generate
    open VoiceMate.xcodeproj

Set your signing team, run on a **real iPhone** (Siri, the Bluetooth mic and speech recognition don't work
properly in the Simulator). After first launch, the Siri phrases are available automatically; check
Settings > Siri to make sure "Hey Siri" / "Press Side Button for Siri" is enabled.

## Customising answers
Edit `VoiceMate/Assistant.swift`. It currently answers time, date and battery questions locally; swap in a call
to your own backend or an LLM API for open-ended conversation.

## Files
- `AudioRoute.swift` – audio session setup for Bluetooth mic + output, route display
- `SpeechListener.swift` – live speech recognition with silence detection
- `Speaker.swift` – text-to-speech
- `ConversationController.swift` – listen → answer → speak loop
- `Intents.swift` – Siri App Intents and phrases
- `ContentView.swift` – SwiftUI UI
