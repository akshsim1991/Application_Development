# Feather View - project rules
- C# / .NET 10 / WPF / MVVM. No UI logic in code-behind except window chrome, drag-drop, wiring. Logic lives in ViewModels/Services; UI-free code in `FeatherView.Core` with unit tests.
- Never modify original files while viewing. Destructive actions confirm first and use the Recycle Bin. Never read GPS. No telemetry. Network only for the update check.
- Dev container cannot build WPF (no Windows SDK). Verified locally: `dotnet test tests/FeatherView.Tests -p:FV_TFM=net8.0`. WPF compiled in CI (windows-latest, .NET 10).
- Update scheme mirrors FileGlade/FeatherPDF (latest.json + SHA-256). Releases repo: akshsim1991/FeatherView-releases.
- Status of every feature is tracked in CHECKLIST.md; do not mark a feature done until it exists.
- Next: Phase 3 video (Media Foundation via MediaElement/MediaPlayer, libmpv optional), filtering, persistent thumbnail cache, key remapping.
