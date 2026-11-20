# Test report - 1.8.0 (2026-11-13)
Environment: Linux container, MinGW-w64 GCC 13 cross-build; Wine 9 + Xvfb for UI smoke tests. NOT real Windows.
| Area | Method | Result |
|---|---|---|
| Core logic (command parsing, CSV formula-safety, filter, dependency order, journal, atomic write, UTF-8) | native unit tests | 36/36 pass |
| Build | cross-compile exe + NSIS installer | success, no errors |
| Dashboard, Startup, Services, Drivers, Settings pages render; themes Dark/Light/Warm; settings persist | Wine screenshots | pass |
| Run-key Disable (+journal, read-back), multi-select confirm, Details/Properties dialogs, Add dialog, Profiles menu and plan review | Wine screenshots / journal check | pass |
| Cleaner: temp scan/clean (old files + subfolder deleted, fresh file kept), registry detect/remove with backup, restore | Wine + seeded data | pass |
| Service Start/Stop/Kill/start-type/recovery edits | not exercised | untested |
| Task Scheduler (scan/delay/enable), Event Log history, shell-extension enable/disable | Wine lacks/limited | untested |
| Installer install/upgrade/uninstall | 32-bit NSIS stub cannot run in 64-bit-only Wine | untested |
| Dark scrollbars, UI Automation, high-DPI, Narrator | not available | untested |
Not run: spec acceptance tests AT01-AT30 on a Windows VM, performance budgets, offline network trace.

## Addendum 1.8.0 (Large files, Duplicates)
Unit tests cover the keeper-selection logic (36/36). The scanner, hashing, UI pages and Recycle Bin move compile cleanly but were NOT run: Wine stopped working in this container (wineboot aborts with "free(): invalid pointer") after an environment restart, so no runtime smoke test was possible. Treat 1.7 file-finder features as untested.

## Addendum 1.8.0
Built: x64 and x86 exes and the installer, all compiled with Windows 7 API headers (_WIN32_WINNT=0x0601). 36/36 unit checks pass. New Cleaner categories (Windows Update cache with service stop/restart, Delivery Optimization, shader cache, memory dumps, logs, prefetch, font cache, Firefox/Brave, recent files, DNS flush, Windows.old launcher), the risk column, the Empty folders page, Windows 7 fallbacks (service enumeration, DPI, dark mode guard) and the x86/x64 installer logic were NOT executed: Wine is unavailable in this container. Nothing has been run on real Windows 7, 10 or 11.
