# RideVoice 2.0 (iPhone) - hands-free for riders

Designed and developed by Akshaya Simha.

Made for a Bluetooth helmet headset (e.g. BluArmor) where touching the phone is not an option.

## Why not just "Hey Siri"?
"Hey Siri" hands-free only works through the headset mic for AirPods/Beats. Generic Bluetooth headsets need a
button press, and iOS gives apps no way to talk to Siri. So RideVoice listens itself.

## Ride Mode
1. Pair the helmet, tap **START RIDE MODE** once (or hold the helmet button and say "Start RideVoice").
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
Apple Maps is weak in India, so RideVoice can route with Google's **Routes API in two-wheeler mode** and find places
with the **Places API (New)**. One-time setup (needs a computer + a card for Google billing):
1. console.cloud.google.com > create a project > enable billing (set a small budget alert).
2. APIs & Services > Library > enable **Routes API** and **Places API (New)**.
3. Credentials > Create API key > restrict it: Application restriction = iOS apps, bundle ID
   `com.akshayasimha.voicemate`; API restriction = Routes API + Places API (New).
4. In RideVoice open "Navigation settings", paste the key. The label then shows "Google Maps".
Google has a monthly free allowance per API that personal use normally stays within, but check current pricing.
If the key is missing or a request fails, RideVoice falls back to Apple Maps and says so in that label.

## Using Siri without touching the phone
"Hey Siri" will not hear a generic headset mic. Use the headset's call/multifunction button instead (usually
press and hold ~2 s; check the BluArmor manual). To make it work with the phone locked in your pocket enable
Settings > Face ID & Passcode > Allow Access When Locked > Siri (and Settings > Siri > Allow Siri When Locked).
Siri can then run: "Navigate with RideVoice" (asks "Where do you want to go?"), "Start ride mode in RideVoice",
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


## Version 2.0 features
- **Navigation:** avoid tolls / highways, fastest or curvy route, multi-stop ("add a stop at ..."), nearest petrol pump /
  hospital / ATM / mechanic / puncture shop / food, import and export GPX.
- **Garage tab:** odometer (grows with rides), fuel log with mileage and cost per km, service reminders, insurance / PUC /
  licence expiry reminders. Anything due is announced when you start a ride.
- **Safety:** SOS and live location to your emergency contact, crash detection with a 30-second "I'm OK" countdown,
  speed alert, break reminder, speed camera and posted-limit warnings (OpenStreetMap data, patchy in India).
- **Trips tab:** every ride with a map, weekly distance chart, records, GPX export.
- **Ask anything:** unknown questions are answered by Claude if you add an Anthropic key.

## One-time setup for 2.0
### Emergency contact (free, uses a Telegram bot)
1. In Telegram, message **@BotFather**, send `/newbot`, follow the steps, copy the **bot token**.
2. Send any message to your new bot (or add the bot to a family group and write there).
3. RideVoice > Settings > Emergency contact: paste the token, tap **Find chat ID**, then **Send test message**.
4. Turn on **Crash detection** and/or set **Live tracking** (location every N minutes, sent to the same chat).
Use "Test crash alert" in Settings to rehearse the countdown. Crash detection is experimental and can give false
alarms (phone dropped or jolted) or miss a crash (phone not moving fast beforehand); it is an extra, not a guarantee.

### AI assistant (optional)
Create an API key at console.anthropic.com and paste it in Settings > AI assistant. Uses `claude-haiku-5-5`.

## Limits
Widgets / Dynamic Island, CarPlay and offline maps are not possible with a free-Apple-ID sideload. Music control works
for Apple Music only.
