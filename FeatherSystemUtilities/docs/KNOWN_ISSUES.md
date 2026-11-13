# Known issues / limitations
- KI1: Win32 layer never executed (no Windows runtime here). Treat as untested.
- KI2: Scan scope partial: no scheduled tasks, Explorer add-ons, policy Run, other users, drivers, shortcut target resolution (.lnk shown as file path).
- KI3: StartupApproved read-only interpretation (byte0 2/6 enabled, 3/7 disabled) is unverified across builds; unknown shows "Unknown".
- KI4: Light theme only; ListView not themed dark. No UI Automation toolbar (no custom toolbar yet).
- KI5: No installer; unsigned binary.
