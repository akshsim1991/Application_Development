# Feather View - project rules
- C# / .NET 10 / WPF / MVVM. No UI logic in code-behind except window chrome, drag-drop, wiring. Logic lives in ViewModels/Services; UI-free code in `FeatherView.Core` with unit tests.
- Never modify original files while viewing. Destructive actions confirm first and use the Recycle Bin. Never read GPS. No telemetry. Network only for the update check.
- Dev container cannot build WPF (no Windows SDK). Verified locally: `dotnet test tests/FeatherView.Tests -p:FV_TFM=net8.0`. WPF compiled in CI (windows-latest, .NET 10).
- Update scheme mirrors FileGlade/FeatherPDF (latest.json + SHA-256). Releases repo: akshsim1991/FeatherView-releases.
- Status of every feature is tracked in CHECKLIST.md; do not mark a feature done until it exists.
- Video: `IVideoEngine` with MediaFoundationEngine (built in) and MpvEngine (optional libmpv, never bundled). libmpv wrapper (`Core/Video/MpvPlayer`) is tested here against the system libmpv: `FV_TEST_MEDIA=/tmp/media dotnet test -p:FV_TFM=net8.0` (tests skip when libmpv/media are absent). WPF native-window embedding (HwndHost) means no WPF overlay may sit on top of the video (airspace).
- Local WPF type-check: compile App sources against NuGet Microsoft.WindowsDesktop.App.Ref with ImplicitUsings off (WPF SDK has no System.IO/System.Net.Http implicit usings; .NET 10 adds System.Windows.ThemeMode).
- Next: V2 plan in docs/V2-SPEC.md (architecture split of MainViewModel, filtering/search, persistent thumbnail cache, key remapping).
