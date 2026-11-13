# Project state (update after milestones)
Last verified: 2026-11-13. Branch `claude/new-session-ann5h4`.
## Done
- Core lib + 26 native unit checks passing (ParseCommand, CSV formula-safe, filter, dep topo/cycles, journal line, atomic write, UTF-8).
- Win32 shell compiles (3.1 MB static exe): sidebar (Dashboard/Startup/Services/History/Settings), virtual ListView, live search, column sort, copy, CSV export, details, single-instance + `--startup/--services`, async scan w/ generation IDs, target/rights indicator in title, per-monitor DPI manifest.
- Read-only startup scan: HKCU/HKLM Run+RunOnce (64/32 views), Startup folders, StartupApproved state (read), target-missing warning.
- Read-only services scan: SCM enum, state, start type (+delayed, trigger flag), PID, account, path, per-user/shared labels.
## Not verified
Everything Win32 is UNTESTED at runtime (no Windows here). Needs VM smoke test.
## Next (in order)
1. Windows VM smoke test of the exe (owner or CI runner); fix runtime bugs.
2. Change pipeline (`src/changes`): plan→backup→apply→verify→journal; startup enable/disable (HKCU StartupApproved) first, then service start-type/start/stop with dependency plan (core StartOrder/StopOrder ready).
3. Dark theme + accessible toolbar; settings persistence; scheduled tasks provider; bundled offline service advice.
4. Offline installer (tool choice needs licence review), migration of old `HKCU\Software\StartupManager`/`WindowsServicesManager` data.
