# Project state (update after milestones)
Last verified: 2026-11-13. Branch `claude/new-session-ann5h4`. Release: `releases/v0.2.0`.
## Done (verified = ran under Wine+Xvfb with synthetic data; NOT real Windows)
- Core lib + 26 native unit checks (ParseCommand, CSV, filter, dep topo, journal line, atomic write, UTF-8).
- Dark shell matching old apps: rail nav, owner-drawn toolbar, filter dropdown, More menu, right-click menus, header/list custom draw, status bar w/ result messages. Verified in Wine screenshots.
- Startup: Run/RunOnce (64/32 views), Startup folders (+.lnk resolve), scheduled tasks (logon/boot), Explorer context-menu/overlay add-ons, publisher/signature (offline Authenticode+catalog), warnings, first-seen. Enable/Disable (StartupApproved, tasks, Shell Blocked), Delete+backup, Restore, Add (HKCU), Undo, multi-select with confirm. Verified in Wine: Run-key disable + journal + read-back, multi-select confirm, details dialog.
- Services: full inventory (safety, warnings, description), Start/Stop/Restart/Pause/Resume (worker thread, dependency order, critical blocklist), Start type (incl. delayed) with read-back + undo, Properties tabs. Verified in Wine: list, multi-select, Start type menu, Properties. Actual service control NOT exercised.
## Not verified / missing
Real Windows runtime; scheduled-task & shell-extension & service mutations; dark scrollbars/menus (rely on Win10+/11 uxtheme); UIA for toolbar; high-DPI visuals.
Missing: Start later, Profiles, Drivers, Kill process, remote, themes, settings persistence, installer, offline service advice, boot metrics.
## Next
1. Owner smoke test on Windows 11; fix reported issues.
2. Settings persistence + themes; Start later (managed task, spec 5.5); service profiles/snapshots; installer + migration of old HKCU\Software\StartupManager / WindowsServicesManager data.
