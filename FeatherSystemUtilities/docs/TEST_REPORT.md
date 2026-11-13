# Test report - 1.5.0 (2026-11-13)
Environment: Linux container, MinGW-w64 GCC 13 cross-build; Wine 9 + Xvfb for UI smoke tests. NOT real Windows.
| Area | Method | Result |
|---|---|---|
| Core logic (command parsing, CSV formula-safety, filter, dependency order, journal, atomic write, UTF-8) | native unit tests | 26/26 pass |
| Build | cross-compile exe + NSIS installer | success, no errors |
| Dashboard, Startup, Services, Drivers, Settings pages render; themes Dark/Light/Warm; settings persist | Wine screenshots | pass |
| Run-key Disable (+journal, read-back), multi-select confirm, Details/Properties dialogs, Add dialog, Profiles menu and plan review | Wine screenshots / journal check | pass |
| Service Start/Stop/Kill/start-type/recovery edits | not exercised | untested |
| Task Scheduler (scan/delay/enable), Event Log history, shell-extension enable/disable | Wine lacks/limited | untested |
| Installer install/upgrade/uninstall | 32-bit NSIS stub cannot run in 64-bit-only Wine | untested |
| Dark scrollbars, UI Automation, high-DPI, Narrator | not available | untested |
Not run: spec acceptance tests AT01-AT30 on a Windows VM, performance budgets, offline network trace.
