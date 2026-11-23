# Feather View

A lightweight photo (and soon video) viewer for Windows. Offline, no ads, no telemetry, no account.
C# / .NET 10 / WPF, MVVM. Designed and developed by Akshaya Simha.

> The earlier native C++ viewer in `../FeatherViewer-Windows` is the **Safe Mode edition** (single small EXE, no .NET).
> Feather View (this project) is the full-featured modern app. See `CHECKLIST.md` for exactly what exists.

## Layout
```
src/FeatherView.Core   settings, logging, media scanning, LRU cache, update checker, MVVM base (no UI code, unit-tested)
src/FeatherView.App    WPF app: Views (XAML), ViewModels, Services (image/thumbnail/GIF/theme/dialogs), Controls (zoom surface)
tests/FeatherView.Tests
installer/FeatherView.iss   Inno Setup
tools/                      smoke test, latest.json generator
```

## Build
Requires the .NET 10 SDK on Windows.
```
dotnet test tests/FeatherView.Tests -c Release
dotnet publish src/FeatherView.App -c Release -r win-x64 --self-contained true -p:PublishReadyToRun=true -o publish/win-x64
iscc /DAppVersion=0.1.0 installer\FeatherView.iss     # needs Inno Setup 6
```
GitHub Actions (`.github/workflows/featherview.yml`) does all of this on every push and uploads the installer, portable ZIP and framework-dependent ZIP as a build artifact.

## Video
Two engines behind one interface (`IVideoEngine`): **Windows Media Foundation** (built in, no extra files, basic playback) and **libmpv** (optional download, exact seek, frame step, subtitles, audio tracks, A-B, chapters, HDR flag, hardware decoding).
Automatic mode uses libmpv when installed and falls back to the built-in player. Video keys: Space play/pause, Left/Right +-5 s, Ctrl+Left/Right +-30 s, Up/Down volume, `,` `.` frame step, `[` `]` speed, M mute, A set A/B/clear, Ctrl+S screenshot, Ctrl+J go to time, Ctrl+L loop, PageUp/PageDown previous/next file.
Videos never auto-play inside slideshows (slideshows show pictures only). In full screen the controls hide until you move the mouse; with the setting "Keep a video playing when I switch to another tab" a video keeps playing (sound) while you use another tab.

Interface: sidebar (Gallery / Folders / Favorites / Recent / Search + quick filters), rounded toolbar, details panel (Info / Histogram / EXIF), filmstrip and a status bar with wallpaper, print and open-folder actions. Tabs: Ctrl+T new, Ctrl+W close, Ctrl+Shift+T reopen, Ctrl+Tab next; tabs are restored at start-up (can be turned off). Photo view has a floating toolbar (zoom, fit, compare, shuffle) and the details panel is made of collapsible cards incl. offline text recognition. Extra keys: Ctrl+D favorite, Ctrl+B sidebar, T thumbnail strip, Ctrl+F search, Ctrl+P print.

## Keys
Left/Right previous/next | Home/End first/last | G gallery | S slideshow | I details | F or F11 full screen | R / L rotate | + / - zoom | 0 fit | 1 actual size |
Ctrl+O open file | Ctrl+Shift+O open folder | Ctrl+C copy | Del recycle | F5 reload | Esc back / exit full screen | Ctrl+, settings

## Automatic updates
Same model as FeatherPDF and FileGlade: at most once a day Feather View reads `latest.json` from the public releases repository, asks **Update now / Later / Skip this version**,
downloads over HTTPS from an allow-listed host, verifies size and SHA-256, then runs the installer silently and restarts. Switch it off in Settings. See `docs/UPDATES.md`.

## Privacy
No telemetry, no analytics, no uploads. The only network use is the optional update check. Logs stay on your PC and mask your user-profile path. Photo location (GPS) data is never read.
