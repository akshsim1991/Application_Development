# Feather System Utilities 3.4.0
- Startup and Services have a new "Recommendation" column (Safe to disable / Optional / Caution / Keep / No advice) with the reason on hover and in the details panel, a "Safe to disable" filter and sorting. Built-in knowledge base (works offline); when the PC is online Feather downloads a newer copy of the same list from the release page (one small request; switch in Settings). Suggestions only.
- Admin rights: before delete/disable/enable/service changes Feather now tells you when administrator rights are needed and offers "Restart as administrator", instead of failing silently.
- Safe Mode: detected automatically. A banner explains what is affected; "Check & fix" can start Task Scheduler, Windows Installer and WMI (needs administrator). MSI uninstalls start the Windows Installer service in Safe Mode.
- Fixed: folder paths on drive roots were built with a doubled backslash ("E:\\folder"), which broke Open location and empty-folder deletion on whole drives.
- Fixed: light scrollbars/checkboxes in dark mode; table rows no longer wrap into tall rows.
- If the web view does not come up within 25 s the classic window opens instead.
- Run on real Windows by the owner (3.0.0 screenshots); this build is again untested by the developer.
