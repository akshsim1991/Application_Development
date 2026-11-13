# Testing and measurements

**Important:** nothing here was run on a real Windows PC. The program was built with MinGW-w64 on Linux and exercised with Wine 9 (a Windows API re-implementation) under a virtual display. Wine is not Windows: results show the code runs, not that it behaves identically on Windows.

## Automated
| What | How | Result (this build) |
|---|---|---|
| Portable core (transforms, formatters, parsers, detection, hashing, SHA-256 vectors, query parser, `latest.json`) | `tests/run.sh` (native g++) | 107 checks, 0 failed |
| Storage, search, journal, retention, sensitive expiry, integrity check and repair, encrypted backup (right/wrong/no password, tampered file), torn-write recovery, clipboard text and picture round trips, ignore list | `FeatherClipboardManager.exe --selftest result.txt` (runs in a temporary folder, never your history) | 47 checks, 0 failed (under Wine) |
| Installer | NSIS installer run under Wine: `/S /D=…`, uninstall keeps history, `/DELETEDATA` removes it, Start-menu shortcuts and Add/Remove entry | passed |
| Window rendering | screenshots under Xvfb: light, dark, list with sample data, all settings pages | seen (Wine fonts differ: the toolbar icons and some symbols fall back to text/boxes) |

## Benchmark (`--bench 100000`, under Wine — not representative of Windows)
| Measure | Result |
|---|---|
| Rewrite the whole journal (100,000 items) | 244 ms |
| Load it at start | 338 ms |
| Record one copy (append), median / p95 | 0.22 / 0.37 ms |
| Search one word, median / p95 | 25 / 40 ms |
| Two words / exact phrase | 28 ms / 25 ms |
| `app:` + word / `type:` / `date:` | 15 / 6 / 3 ms |
| Fuzzy `~word` | about 685 ms (too slow at this size) |
| Regular expression | about 590 ms (too slow at this size) |
| Memory with 100,000 items in memory | about 178 MB working set (above the 60 MB idle goal at that size) |

The specification's other targets (idle CPU < 0.5 %, popup < 100 ms, idle memory < 60 MB with a normal history, multi-day soak) were **not measured** because they need Windows.

## Still to do on Windows (manual checklist)
Start/stop and duplicate launch · hotkey conflict message · copy text/rich text/image/files from real programs (Word, Excel, browsers, Explorer, Snipping Tool) and see them captured · paste into Notepad, Word, a browser, an elevated program · tray menu and pause icon · dark/light switching · 125/150/200 % DPI and two monitors · Narrator and keyboard-only run · installer UI, upgrade over an older version, `/S /relaunch`, uninstall choices · the update prompt against a real `latest.json` · OCR on a screenshot · backup/restore on another PC.
