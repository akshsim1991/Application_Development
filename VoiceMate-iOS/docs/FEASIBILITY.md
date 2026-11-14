# VoiceMate / RideVoice: feasibility, licensing and architecture

Status date: 2026-11-14. Written before further coding, as the brief requires. **Nothing in this repository has yet been
run on a physical iPhone with a BluArmor helmet.** Where a claim could not be verified it says so.

Classification used below:
**A** = public iOS API, **B** = supported with limitations, **C** = needs a third-party service or licence,
**D** = experimental, **E** = not feasible or not permitted.

## 1. Feature matrix

| Requirement | Class | Reality / limitation | State in the app |
|---|---|---|---|
| Audio-first turn-by-turn | B, C | iOS has no built-in turn-by-turn API for third parties. Needs a routing provider (section 3). | Works with Google Routes (two-wheeler) or Apple MapKit; **never compiled-and-tested on a device** |
| BluArmor mic and speaker | B | Standard Bluetooth profiles through AVAudioSession (A2DP stereo output, HFP mic). BluArmor-specific behaviour, firmware quirks and intercom features are **unverified**. No access to proprietary buttons. | Implemented generically, untested with BluArmor |
| Siri via App Intents / App Shortcuts | A, B | Max 10 App Shortcuts; every phrase must contain the app name. Hands-free "Hey Siri" works through the headset mic only for Apple/Beats headsets; other headsets need a button press, which depends on the headset. Whether an intent runs on a locked phone depends on the intent's authentication policy and the user's Siri settings: **to be verified on device**. | 10 shortcuts implemented |
| In-app voice commands | B, D | Needs the microphone session open. Continuous wake-phrase listening in the background is technically possible while a ride is active (audio + location background modes) but shows the orange mic indicator, costs battery, can be ended by iOS, and may draw App Store review questions. It is a convenience, not a supported "always listening" API. | Implemented, default on, can be switched off in Settings for best audio quality |
| Loudspeaker, not earpiece, when no Bluetooth | A | `.defaultToSpeaker` plus an `overrideOutputAudioPort(.speaker)` safeguard on route changes. | Implemented, **to be verified on device** |
| Locked screen, screen off | B | Works only inside iOS background limits: Location background mode keeps the app running; Audio mode allows speech. iOS may still terminate it. | Implemented; needs long-ride testing |
| Google Maps integration | B | Can hand a destination to the Google Maps app via a Maps URL. **Cannot** intercept or restyle Google Maps' own voice. Using Google's Routes API for your own guidance is subject to Google's terms (section 3). | Handoff button added; Routes API in use |
| Mappls integration | C | iOS SDK and REST need Mappls licence keys; no public price found. | Not implemented |
| Indian road features (flyovers, underpasses, service roads) | B | Only as good as the routing provider's data. The app never invents them: it speaks the provider's maneuver, with roundabout exit numbers parsed from the instruction. | Partial |
| Indian English; other Indian languages | B | TTS: whichever voices are installed. Speech recognition follows the phone's language. Command grammar is English only. | English only |
| Natural voices | A, B | AVSpeechSynthesizer exposes installed system voices, including Enhanced/Premium **only if the user downloaded them**. Apps cannot download voices or list ones not yet downloaded. Siri's own voices are generally not exposed. No emotion control. | Voice gallery, presets, previews done |
| Pronunciation overrides | A | Respelling dictionary done. (IPA via `AVSpeechSynthesisIPANotationAttribute` is possible later.) | Done |
| Optional third-party on-device neural TTS | C | Possible behind the `SpeechEngine` protocol. Model licences vary and must be checked per model. Adds app size, CPU and battery. | Interface only |
| Optional cloud TTS | C | Internet, cost and privacy trade-offs; must be opt-in and labelled. | Not implemented |
| Ride recording, GPX export | A | Done. Elevation (CLLocation altitude or CMAltimeter) and CSV export not yet. | Partial |
| Multiple stops, saved places | A | Done. Reverse route, saved routes: not yet. | Partial |
| Offline maps and routing | C, E | MapKit offers no offline routing to third parties. Offline needs licensed map data plus an on-device router: large work and licence cost. | Not implemented |
| Overspeed alert, break reminder, GPS-loss warning | A | Done. Low-battery warning not yet. | Mostly done |
| SOS / location sharing | B, C | Uses a Telegram bot (free, third-party, needs internet). Not an emergency service. | Done |
| Crash detection | D | Heuristic only (hard jolt at speed, then stopped, then 30 s countdown). Will give false alarms and misses. **Not certified emergency response.** | Done, experimental |
| Widgets, Live Activities | B | Need extensions; data sharing normally needs App Groups, which free Apple ID sideloading may not allow. | Not implemented |
| Day/night mode | A | App is dark only. | Not implemented |
| No account, ads or telemetry | - | True. Network calls the app makes are listed in section 6. | Done |

## 2. iOS constraints that shape the design

1. **Bluetooth profile switching.** With the microphone open, Bluetooth headsets use the hands-free (HFP) profile: mono, narrow-band, noticeably flatter than stereo (A2DP). No app can avoid this while it records. The app therefore has two modes (`AudioRoute`): *hands-free* (mic open, wake phrase works, flatter sound) and *guidance only* (mic closed, A2DP stereo, music ducked only while speaking, commands via Siri).
2. **Background execution.** Navigation relies on the Location background mode. Speech needs the Audio background mode. Speech recognition cannot run unless an audio session with input is active.
3. **Siri.** Third-party apps cannot replace Siri or read a Siri conversation. App Shortcuts are the supported route.
4. **Voices.** See matrix. The app lists installed voices honestly and tells you how to install better ones.
5. **Permissions.** Location (When In Use + background indicator), Microphone, Speech Recognition, Motion, Personal Voice (optional). Least privilege: nothing else is requested.

## 3. Routing providers and licensing (verify before any release)

| Provider | India / motorcycle | Licensing and cost notes | Offline |
|---|---|---|---|
| **Google Routes API** (in use) | Good coverage; has a `TWO_WHEELER` mode | Paid after free allowance (check current pricing). Terms: the EEA terms say Routes steps may not be shown with a map, and real-time navigation must meet Google's Safety Requirements; older Directions API terms forbid using results with a non-Google map. I could not confirm the current global wording. **Treat as a licensing risk for anything beyond personal use; get a legal read.** | No |
| **Google Navigation SDK for iOS** | Full guidance from Google | Separate product with its own billing and eligibility (not verified for India or for custom voice). | No |
| **Mappls (MapmyIndia)** | India-focused map data (claimed; evaluate) | iOS SDK and REST need licence keys under their API terms; no public price sheet found; contact Mappls. | Unverified |
| **Apple MapKit (MKDirections)** | Weaker India coverage in practice; no two-wheeler profile | Free with an Apple developer account (check terms). | No |
| **Open-source routers (Valhalla, GraphHopper, OSRM) on OSM data** | Motorcycle profiles exist (Valhalla, GraphHopper); OSM India data quality varies | Software licences are permissive or copyleft depending on the project; OSM data needs ODbL attribution; public demo servers are not for production; self-hosting costs money and effort. | Possible with significant work |
| HERE, TomTom | Commercial, India coverage exists | Free tiers and paid plans; check terms for navigation use. | Varies |

**Recommendation.** Keep Google Routes for personal use now. Before wider use, decide between Mappls (India data, needs a licence conversation) and a licensed or self-hosted OSM router. The `RoutingProvider` protocol already isolates this choice.

## 4. Voice engines

| Engine | Naturalness | Offline | Cost / licence | Notes |
|---|---|---|---|---|
| Apple system voices (in use) | Decent; Enhanced/Premium noticeably better; not human-indistinguishable | Yes | Free | No emotion control; user must download better voices |
| Personal Voice (iOS 17) | User's own voice | Yes | Free | Needs user setup and authorization |
| On-device neural TTS (Piper, Sherpa-ONNX, Kokoro and similar) | Potentially more natural | Yes | Model licences vary: **check each** | Adds 50-300 MB, CPU and battery; Indian voices unverified |
| Cloud TTS (Google, Azure, others) | Very natural, many Indian languages | No | Per-character cost | Latency, privacy; must be optional and labelled |

No listening tests have been done. A real-device log table is in section 7 (blank on purpose).

## 5. Architecture (current files -> requested modules)

| Module | Files | Notes |
|---|---|---|
| NavigationEngine | `Navigator.swift` | Route following, junction timing, rerouting, stops |
| RoutingProvider | protocol in `Navigator.swift`, `GoogleRouting.swift`, `AppleProvider` | Swappable |
| LocationService | `LocationProvider.swift` | GPS strength, speed fallback, weak-signal callback |
| BluetoothAudioManager | `AudioRoute.swift` | Modes, speaker safeguard, interruptions, route report |
| SpeechEngine | `SpeechEngine.swift`, `AppleSpeechEngine.swift`, `SpeechQueue.swift`, `PhraseFormatter.swift`, `VoiceStore.swift` | Protocol + queue + phrasing |
| VoiceCommandService | `RideListener.swift`, `Assistant.swift`, `Intents.swift` | In-app commands and Siri |
| RideTracker | `TripRecorder.swift`, `GPX.swift` | |
| SafetyService | `Emergency.swift`, `RoadInfo.swift` | |
| PersistenceService | UserDefaults + JSON files | SwiftData not used; simple data. Revisit if history grows. |
| SettingsStore | `Prefs.swift`, `VoiceStore.swift` | |

Data flow: location fix -> `Navigator` decides an announcement -> `RideController.say(kind)` -> `SpeechQueue` (priority, de-duplication, stale-drop) -> `SpeechEngine` -> `AVSpeechSynthesizer` -> route chosen by `AudioRoute`.

**Known gap against the brief:** instruction wording (`plainPhrase`) still lives inside `Navigator.swift`. The brief wants navigation data and phrasing fully separate; the next refactor is an `InstructionPhraser` that takes a structured maneuver.

## 6. Network calls (privacy table)

| Feature | Service | Data sent | Needed? |
|---|---|---|---|
| Routing and place search | Google Routes / Places (if key set), else Apple Maps | Start, destination, search text | Yes for navigation |
| Weather | Open-Meteo | Rounded position | Only when asked |
| Speed cameras and limits | Overpass (OpenStreetMap) | Position | Optional |
| SOS, location sharing | Telegram Bot API | Message with map link | Optional |
| AI answers | Anthropic API | The question text | Optional, off without a key |

No account, no ads, no analytics. Ride history and settings stay on the phone. Before an App Store release add a privacy manifest and nutrition label covering the data above.

## 7. Device test plan (all results pending)

| Test | Expected | Result |
|---|---|---|
| Helmet connected: spoken prompt | Plays in helmet | |
| Helmet disconnects mid-ride | Next prompt from **loudspeaker**, not earpiece | |
| Helmet reconnects | Prompts return to helmet within a few seconds | |
| Locked screen, 30 min ride | Prompts keep coming, GPS continues | |
| Incoming call during ride | Guidance pauses, resumes after | |
| Music playing (Apple Music) | Music ducks during prompts, recovers (guidance-only mode) | |
| Siri shortcut on locked phone | Document actual behaviour | |
| Poor GPS (tunnel, flyover) | "GPS signal is weak" after 15 s, no wrong turns announced | |
| No internet | Existing route continues; rerouting and search fail with a clear message | |
| Low battery | Document drain per hour in both audio modes | |
| Voice comparison on Indian place names | Note best voice and respellings | |
| Wake phrase in wind at speed | Document recognition rate | |
| Duplicate announcements | None | |

Unit tests (`VoiceMateTests`): number words, distance phrasing, abbreviation expansion, pronunciation overrides, chunking, roundabout exit parsing. Run in CI (non-blocking) or with Cmd+U in Xcode.

## 8. MVP definition

Navigation with spoken prompts through a Bluetooth helmet or the loudspeaker, a voice gallery with presets and previews, location and ride tracking, 10 Siri shortcuts, in-app commands, and safety basics. Everything beyond that (offline maps, Mappls, widgets, other Indian-language commands) is later.

## Sources

- [Apple developer forum: no API to list or download voices not yet downloaded](https://developer.apple.com/forums/thread/758460)
- [Apple developer forum: request to download Premium and Enhanced voices programmatically](https://developer.apple.com/forums/thread/787779)
- [Using Personal Voice in an iOS app](https://bendodson.com/weblog/2024/04/03/using-your-personal-voice-in-an-ios-app/)
- [Google Maps Platform EEA Service Specific Terms](https://cloud.google.com/terms/maps-platform/eea/maps-service-terms)
- [Routes API adjustments for EEA customers](https://developers.google.com/maps/comms/eea/routes)
- [Google Navigation SDK for iOS usage and billing](https://developers.google.com/maps/documentation/navigation/ios-sdk/usage-and-billing)
- [Mappls (MapmyIndia) vector SDK for iOS](https://github.com/mappls-api/mapmyindia-maps-vectorSDK-iOS)
