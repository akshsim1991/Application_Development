# Feather System Utilities

One Windows application (one window, one offline installer) combining a Startup manager and a Services manager.

Designed and developed by Akshaya Simha.

## Status
Phase 0 (audit) — no implementation yet.

- `docs/Development_Specification.md` — full requirements (F001–F240), architecture, safety rules, acceptance tests, roadmap.
- `reference-binaries/` — original, unmodified StartupManager 2.0 and ServicesManager 2.2 (unsigned, x64) with SHA-256 sums. Kept for parity checks; **not** the integration path (see spec §5.1).

## Blocker
Source for the two existing tools is not in this repo. `ServicesManager.exe` references `akshsim1991/Project_1` (`ServicesManager/`), which was not accessible to the session. Next step: add that source (e.g. as `src/` imports) or approve reimplementation per the spec.

## Findings so far
- Both binaries make network references (online service advice on GitHub, VirusTotal, Bing) — to be removed/disabled for the offline baseline (F205, F207).
- Settings keys seen: `Software\StartupManager`, `Software\StartupManager\Delayed`, `Software\WindowsServicesManager`.
