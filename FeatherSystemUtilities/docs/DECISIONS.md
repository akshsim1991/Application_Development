# Decisions
- D1: Native C++17/Win32, one exe (spec §5.1, §8). No web shell, no deps.
- D2: Engines reimplemented from spec because old source unavailable; old EXEs kept only as reference.
- D3: `core/` has no Windows headers so logic is unit-testable on Linux; Win32 layer stays thin.
- D4: First slice is read-only (F007); mutations wait for backup/journal pipeline.
- D5: Cross-compile with MinGW-w64 (win32 threads, static) in this container; MSVC not available.
- D6: Name: Feather System Utilities.
- D7: UI = owner-drawn dark Win32 (toolbar/status/header), native popup menus forced dark via uxtheme ordinals; warm-light dialogs like the originals.
- D8: VirusTotal/Search online kept but consent-gated (opens browser only after confirm); app itself makes no connections.
