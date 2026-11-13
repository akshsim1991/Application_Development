# Feather System Utilities — project rules (Windows 10/11 x64 only)
One window, one offline installer: Startup + Services managers. Owner: Akshaya Simha. Spec: `docs/Development_Specification.md` (F001–F240, AT01–AT30).
Memory: `docs/PROJECT_STATE.md`, `DECISIONS.md`, `KNOWN_ISSUES.md`, `FEATURE_TRACKER.md`. Read CLAUDE.md + PROJECT_STATE first; don't re-read the spec wholesale (grep by F-ID).

## Stack / layout
- C++17, Win32 + common controls, no third-party deps, fully offline, no telemetry.
- `src/core/` portable logic (no windows.h): command parsing, CSV, filter, dep ordering, journal, atomic write. Unit tests run natively.
- `src/win/` Win32 shell (`main.cpp`), manifest (PerMonitorV2, asInvoker), version rc.
- `tests/core_tests.cpp`. `reference-binaries/` = original old EXEs (do not modify; not the integration path).
## Build/test
`./build.sh test` (native, 26 checks) · `./build.sh win` (MinGW cross → build/FeatherSystemUtilities.exe) · `./build.sh all`.
Env: Linux container, `g++-mingw-w64-x86-64` (apt). No Windows/wine: Win32 code is compile-checked only, NEVER run here.
## Hard rules
- No mutation of startup/services without preview→backup→apply→read-back→journal (spec §6.1). Current build is READ-ONLY.
- Never execute/load inspected targets. No secrets in logs/journal. Atomic writes for config/exports.
- Don't claim a feature works unless verified; Win32 paths are "compiles, untested on Windows".
- Old-app source (`akshsim1991/Project_1`) was not accessible; engines are reimplemented from the spec.
## Testing under Wine (no Windows here)
`apt-get install wine xvfb xdotool imagemagick`; `tests/smoke-wine.sh` seeds synthetic Run entries, runs the exe in Xvfb, saves screenshots. Wine != Windows: dark scrollbars/menus and Task Scheduler/shell-ext paths are not representative.
Files: `src/win/{main,startup,services,inspect,dialogs}.cpp`, `app.h` (shared decls).
