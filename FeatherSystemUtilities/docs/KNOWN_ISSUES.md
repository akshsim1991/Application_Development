# Known issues / limitations
- KI1: Only smoke-tested under Wine with synthetic data; not yet on real Windows. Task/shell-extension/service mutations unexercised.
- KI2: Scan scope partial: no policy Run, Winlogon/AppInit/IFEO/WMI, packaged apps, other users, drivers.
- KI3: StartupApproved read-only interpretation (byte0 2/6 enabled, 3/7 disabled) is unverified across builds; unknown shows "Unknown".
- KI4: Toolbar is owner-drawn without UI Automation providers (F220 open). Dark scrollbars/menus need Win10 1903+; verified only in Wine (not dark there).
- KI5: No installer; unsigned. CSV export removed with the old minimal UI (re-add).
