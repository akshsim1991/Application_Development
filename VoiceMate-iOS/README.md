# VoiceMate (iPhone) - hands-free for riders

Made for a Bluetooth helmet headset (e.g. BluArmor) where touching the phone is not an option.

## Why not just "Hey Siri"?
"Hey Siri" hands-free only works through the headset mic for AirPods/Beats. Generic Bluetooth headsets need a
button press, and iOS gives apps no way to talk to Siri. So VoiceMate listens itself.

## Ride Mode
1. Pair the helmet, tap **START RIDE MODE** once (or hold the helmet button and say "Start VoiceMate").
2. Lock the phone / put it in your pocket. The app keeps listening (background audio + location).
3. Say the wake phrase (default **"hey rider"**), then a command, e.g. "hey rider, what's my speed".
   Or say just the wake phrase, wait for "Yes?", then the command.

Commands: speed, where am I, heading, time, battery, play, pause, next song, previous song, repeat, help.
Calls, messages and navigation are not possible from the background: hold the helmet button for Siri.

## Limits to know about
- While the helmet mic is active, Bluetooth switches to call quality, so music sounds lower quality. This is
  how Bluetooth headsets work (HFP) and cannot be avoided by an app.
- Wind noise at speed will hurt recognition. Use the helmet's noise-reduced mic, say commands clearly, and
  consider a short wake phrase. Test at low speed first. Never look at or use the phone while riding.
- Music control works for the Apple Music / Music app only (not Spotify).
- iOS shows the orange mic indicator and a location indicator while Ride Mode is on; battery use is high.
  Keep the phone charging.

## Build
macOS + Xcode 15+ + XcodeGen (`brew install xcodegen`):

    cd VoiceMate-iOS && xcodegen generate && open VoiceMate.xcodeproj

Set your signing team and run on a real iPhone (not the Simulator).

## Extending
Add commands in `VoiceMate/Assistant.swift`.
