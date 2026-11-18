# Known issues / limitations (1.6.0)
- KI1: Smoke-tested only under Wine; never on real Windows. Task Scheduler paths, event-log history, installer (32-bit NSIS stub), service control are unexercised.
- KI2: Scan scope: no policy Run, Winlogon/AppInit/IFEO/WMI, packaged apps, other users.
- KI3: StartupApproved format handled for 12-byte values only; others stay read-only.
- KI4: Sign-in impact is "Not measured". No boot/session metrics.
- KI5: No tray monitor, scheduled service operations, remote machines, CLI, code signing.
- KI6: UI Automation covered by native buttons/listview only; custom dashboard/settings panel has keyboard but no UIA provider.
- KI7: Custom service accounts/passwords and "restart computer" recovery intentionally unsupported.
- KI8: Cleaner is conservative by design (4 registry areas, fixed drives only); no file-extension/COM/typelib cleaning, no browser history/cookies. Temp deletion is permanent. Windows Update/Delivery Optimization caches not included.
