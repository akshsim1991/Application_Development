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

## Audio-only navigation
Say: **"hey rider, navigate to <place>"** -> it replies with the place and distance -> say **"yes"**.
Directions are plain and relative, never "take the second exit":
"In 300 metres, turn left." / "At the roundabout, go straight." / "Take the exit on the right."
It announces earlier at higher speed, says "now" at the junction, reroutes if you leave the route, and says
"You have arrived". Also: "next turn", "how far", "stop navigation", "set home" then "navigate home".
Routing uses Apple Maps (MapKit): needs mobile data, and map quality/coverage varies by country - test your
usual roads first. It is car routing (no motorcycle profile, no avoid-highways option yet).

## Google Maps routing (recommended in India)
Apple Maps is weak in India, so VoiceMate can route with Google's **Routes API in two-wheeler mode** and find places
with the **Places API (New)**. One-time setup (needs a computer + a card for Google billing):
1. console.cloud.google.com > create a project > enable billing (set a small budget alert).
2. APIs & Services > Library > enable **Routes API** and **Places API (New)**.
3. Credentials > Create API key > restrict it: Application restriction = iOS apps, bundle ID
   `com.akshayasimha.voicemate`; API restriction = Routes API + Places API (New).
4. In VoiceMate open "Navigation settings", paste the key. The label then shows "Google Maps".
Google has a monthly free allowance per API that personal use normally stays within, but check current pricing.
If the key is missing or a request fails, VoiceMate falls back to Apple Maps and says so in that label.

## Using Siri without touching the phone
"Hey Siri" will not hear a generic headset mic. Use the headset's call/multifunction button instead (usually
press and hold ~2 s; check the BluArmor manual). To make it work with the phone locked in your pocket enable
Settings > Face ID & Passcode > Allow Access When Locked > Siri (and Settings > Siri > Allow Siri When Locked).
Siri can then run: "Navigate with VoiceMate" (asks "Where do you want to go?"), "Start ride mode in VoiceMate",
or ask Siri directly to call/message anyone.

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
