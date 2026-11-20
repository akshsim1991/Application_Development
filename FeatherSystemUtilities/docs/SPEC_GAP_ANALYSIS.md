# Spec gap analysis — "Windows PC Maintenance Suite Specification v1.0" vs Feather System Utilities 3.1.0

Legend: ✅ built · 🟡 partly built · ❌ not built · ⚠ built, but **contradicts** the spec (decision needed)
Important: ✅ means "implemented and compiles". Nothing has been run on real Windows yet (see KNOWN_ISSUES.md).

## 1. Summary
- **Strong:** startup manager (with backups/restore), junk cleaner, programs + leftover review, large/duplicate/empty-folder finders, services, journal/rollback, driver inventory, multi-select/sort/filter UI.
- **Missing modules:** Privacy & Browser Data (5.7), Recovery Center proper (5.9), Troubleshooting reports (5.8), Store apps, code signing, licensing.
- **Contradictions to decide:** the spec targets Windows 11 and says *no* registry cleaning, *no* Windows Update/prefetch/component-store cleanup, *no* forced removal, *no* service disabling, drivers one at a time. Feather (by your earlier requests) supports Windows 7–11 and does several of these. See section 4.

## 2. Module-by-module

### 5.1 PC Health Dashboard
| Spec item | Status | Note |
|---|---|---|
| Device model + Windows version | 🟡 | OS/build/CPU/RAM on Tools page; device model (manufacturer/model) missing |
| Storage by drive | ✅ | Today + Storage map |
| Startup apps + recent changes | ✅ | counts, "new" flag, journal timeline |
| Windows Update status / available updates | ❌ | only a link to Windows Update settings |
| Backup / restore-point status | ❌ | |
| CPU / memory / disk / network activity (optional, low-frequency) | ❌ | Processes page shows memory per process only |
| Storage trends | ❌ | |
| Notifications needing review | ❌ | |
| No fake health score | ✅ | none shown |

### 5.2 Junk and Storage Cleanup
| Spec item | Status | Note |
|---|---|---|
| User/Windows temp, Recycle Bin | ✅ | |
| Browser cache | ✅ | Edge/Chrome/Brave/Firefox cache. Cookies/history/selected browsing data ❌ |
| Application caches | ✅ | Deep scan (developer/app caches) |
| Old downloads / installer files (review only) | ❌ | |
| Large files with preview + open location | 🟡 | list + open location; no content preview |
| Duplicates: size + hash, content comparison, keep one | ✅ | size → head/tail hash → SHA-256 → byte-for-byte proof; keep-newest/oldest/shortest |
| Exact paths, sizes, dates before deletion | 🟡 | Files page: yes. Clean items are folder-level, not file-by-file |
| Exclude documents/app data by default | ✅ | |
| Never delete just because large/old | ✅ | |
| Avoid WU cache, prefetch, component store | ⚠ | Feather cleans all three (WU cache via stopping services, prefetch, DISM) |
| Recycle Bin by default, permanent separate | 🟡 | Files page: yes. Cleaner deletes temp files permanently |
| Exclusion lists | ❌ | |
| Dry-run preview | 🟡 | confirmation list, no dedicated dry run |

### 5.3 Startup and Background App Manager
| Spec item | Status | Note |
|---|---|---|
| List from supported locations | ✅ | Run/RunOnce, Startup folders, scheduled tasks, Explorer add-ons |
| Publisher, path, signature, source | ✅ | |
| Estimated impact | ❌ | |
| Disable / re-enable, remember previous state, restore | ✅ | plus delete (with backup) and "start later" |
| Open file location / publisher info | 🟡 | file location fix unconfirmed on real Windows |
| Link to Task Manager / Settings for unsupported types | ❌ | |
| Identify Microsoft/security/accessibility/device/backup software | 🟡 | Microsoft + system only |
| Warn on hardware/security/sync/backup impact | ❌ | generic warnings only |
| Do not auto-disable services/tasks | ✅ | |

### 5.4 Software Uninstaller
| Spec item | Status | Note |
|---|---|---|
| Desktop software list | ✅ | |
| Microsoft Store apps | ❌ | |
| Search, sort, filter, batch | ✅ | |
| Launch built-in uninstaller | ✅ | standard mode |
| Quiet only with documented command | 🟡 | MSI /qn, vendor QuietUninstallString, plus Inno/NSIS switch conventions (heuristic) |
| Show uninstall command + source | 🟡 | classic UI details; new UI shows silent note only |
| Open install folder / publisher details | 🟡 | location text; open-folder button only in classic UI |
| Leftover scan after uninstall | ✅ | |
| Uninstall Windows updates | ❌ | |
| Software relocation | ❌ | |
| Wait for user to confirm completion | 🟡 | Feather waits for the process and checks the uninstall key instead |
| Quarantine / Recycle Bin for leftovers | ❌ | leftovers are deleted permanently (registry keys backed up first) |
| Never remove drivers/security/system components by name match | ❌ | only protected system paths are blocked |
| Stubborn entries: guidance only, no forced removal | ⚠ | "Force remove" exists (still review-before-delete) |

### 5.5 Leftover File Review
| Spec item | Status | Note |
|---|---|---|
| Install dir, profile folders, uninstall keys, linked tasks/services | ✅ | |
| Labelled as candidates with detection rule | 🟡 | reason shown; wording says "found" |
| Avoid broad registry scans | 🟡 | vendor-key name matching is moderately broad |
| Protect shared libs / user data by default | 🟡 | shared-folder check; user data folders can be pre-ticked when low risk |
| Confirm + export every registry change | ✅ | |
| Prefer leaving uncertain items | ✅ | medium-risk items start unticked |

### 5.6 Driver Manager
| Spec item | Status | Note |
|---|---|---|
| Inventory via documented interface | ✅ | WMI Win32_PnPSignedDriver |
| Name, provider, version, date, signing | ✅ | |
| Hardware ID, device instance ID, device status/problem code | ❌ | |
| Links: Device Manager, Windows Update | ✅ | Tools page |
| Link to manufacturer support page | ❌ | |
| Identify missing/failed drivers | ❌ | |
| Save inventory report | ✅ | Export CSV |
| Instructions for recovery point | ✅ | |
| Update source: Windows Update | ✅ | Windows Update Agent |
| Show installed vs proposed version, compatibility, why | ❌ | shows Windows Update title/description only |
| User selects each update | ✅ | |
| App creates restore point and reports success | ❌ | only offers the Tools shortcut, no verification |
| Install one driver at a time | ⚠ | installs the selected batch in one operation |
| Verify device status after restart, rollback guidance | ❌ | |
| "Ignore this update" | ❌ | |
| Warn: graphics/storage/chipset/network/firmware are higher risk | ❌ | |
| No unsigned/scraped drivers, no BIOS/firmware | ✅ | |

### 5.7 Privacy and Browser Data Manager — **❌ not built**
Browser profile listing, per-category review (cookies/history/downloads/site data), privacy checklist, links to browser privacy settings: all missing.

### 5.8 System Troubleshooting
| Spec item | Status | Note |
|---|---|---|
| Open troubleshooters / Settings pages | 🟡 | Device Manager, Disk Mgmt, Task Scheduler, Services, Programs, Windows Update; Windows Troubleshooters ❌ |
| Service/device status checks | ✅ | Services + Drivers pages |
| Event-log collection for a time range | ❌ | |
| Export diagnostic report with redaction | ❌ | |
| Built-in repair tools with explanation | ✅ | SFC, DISM, chkdsk, memory diagnostic (each asks first) |
| Links to official Microsoft/manufacturer help | ❌ | |

### 5.9 Recovery and Restore Center
| Spec item | Status | Note |
|---|---|---|
| List Windows restore points / check System Protection | ❌ | |
| Create restore point | 🟡 | PowerShell Checkpoint-Computer; success not verified |
| Local action history | ✅ | Journal |
| Re-enable changed startup entries | ✅ | |
| Restore quarantined files | ❌ | no quarantine exists |
| Restore backed-up config/registry values | ✅ | Rollback shelf |
| Instructions when not undoable | 🟡 | result column says "not undoable" |

### 5.10 Settings, Updates, Diagnostics
| Spec item | Status | Note |
|---|---|---|
| Start with Windows / background monitoring / automatic maintenance / beta / diagnostics sharing | ❌ | none exist (also no scheduler) |
| Language and accessibility | 🟡 | ARIA labels, keyboard; single language |
| Exclusions and ignored findings | ❌ | |
| Action-history retention / clear log | ❌ | journal can be viewed, not cleared in UI |
| Uninstall application | ✅ | |
| Update version + notes | ✅ | |
| Verify update **signature** | ❌ | size + SHA-256 only; installer unsigned |
| Defer update | ❌ | |
| Inspect/delete diagnostic logs | 🟡 | open data folder only |

## 3. Cross-cutting requirements
| Requirement | Status | Note |
|---|---|---|
| Windows 11 x64 | ✅ | also Win 7–10 and x86 (see section 4) |
| Windows 11 ARM64 | ❌ | |
| Detect organization-managed device, respect policy | ❌ | |
| UAC / elevate only for the action | 🟡 | whole app restarts elevated; no per-action privilege broker |
| Read-only scan separated from modification | 🟡 | separate code paths, same process |
| Path validation, reparse-point protection | ✅ | UI never sends raw paths; scans skip links; deletes refuse links |
| Signed binaries/installer, SBOM, vulnerability contact, privacy policy | ❌ | THIRD_PARTY_NOTICES.md only |
| HTTPS updates + hash check | ✅ | |
| SQLite storage | ❌ | JSON/jsonl files in %LOCALAPPDATA% |
| Typed actions with preview + rollback plan | 🟡 | CleanItem kinds + journal, not a formal action model |
| Audit record for every modification | ✅ | journal.jsonl |
| Feature flags, signed rule catalogs | ❌ | |
| Responsive scans, high-DPI | ✅ | worker threads; WebView2 scaling |
| Honest wording, no scareware | ✅ | |
| Licensing/monetization (tokens, grace period) | ❌ | |

## 4. Where Feather deliberately differs from the spec (needs your decision)
1. **Windows 7/8/10 support and x86** (your request) vs "Windows 11 only".
2. **Registry cleaner** (your request) — spec: "automatic registry cleaning" is out of scope. Feather's is manual, review-first, with exported backups.
3. **Windows Update cache, prefetch, DISM component cleanup** (your request) — spec says avoid.
4. **Force remove** and **service start-type/stop/kill** — spec says no forced removal / no service disabling.
5. **Permanent deletion** in the Cleaner and for leftovers — spec: Recycle Bin by default.
6. **Batch driver install** — spec: one at a time.
7. **Registry-based "Care/one-click" style flows** in the classic UI — spec forbids "fix all".

## 5. Suggested build order (to meet the spec and be sellable)
1. Code-sign binaries + installer, verify update signatures, SBOM, privacy policy, vulnerability contact.
2. Privacy & Browser Data module (5.7).
3. Driver Manager gaps: hardware IDs, device status/problem codes, installed-vs-proposed comparison, one-at-a-time install, verified restore point, ignore list, risk warnings by class, manufacturer link.
4. Quarantine for leftovers + Recovery Center (list restore points, System Protection check, restore quarantined files).
5. Exclusions, ignored findings, history retention/clear log, settings (start with Windows, scheduler off by default).
6. Troubleshooting: event-log collection by time range, redacted local diagnostic report, Windows Troubleshooter links.
7. Dashboard: device model, Windows Update status, optional low-frequency CPU/RAM/disk/network, storage trend.
8. Store apps in the uninstaller; uninstall Windows updates.
9. Licensing/activation, ARM64, SQLite, privilege broker.


---
## Phase 1 status (version 3.2.0)
Built (compiles; UI tested against mock data; not run on Windows): launch-time update prompt with Update now / Remind me later / Skip this version; quarantine for leftovers and stray files with restore and permanent delete; Recovery page (restore point list, System Protection status, create restore point with verification, quarantine, backups); dry-run preview of exact files before cleaning; "recoverable / permanent / backed up" labels; exclusion list and ignored findings (Clean, Startup, Driver updates) with management in Settings; Start with Windows (off by default), journal retention and clear log; uninstaller safety (protected security/driver/system programs: no force remove, no leftover search; leftovers never include drivers, system folders or system registry); uninstall command/source and open-install-folder in the new UI; startup warnings for security/accessibility/device/backup software; Task Manager and Startup-settings links; System Protection shortcut.
Not done in Phase 1 (on purpose): "automatic maintenance" and "background monitoring" switches (no such features exist yet), beta channel switch, language selection.

---
## Phase 2 status (version 3.3.0)
Built (compiles; UI tested with mock data; not run on Windows): Driver Manager gaps (hardware ID, instance ID, device status/problem code, missing-driver detection, installed-vs-proposed, risk classes, OEM trade-off, restore point + honest result, one-at-a-time install, post-restart verification, rollback guidance, maker support page, ignore list); Privacy and Browser Data module (profiles, per-category clearing, running-browser guard, Windows privacy checklist read-only, links to browser controls); Troubleshooting (status summary, Windows tool links, event-log collection, redacted local diagnostic report with preview); cleanup additions (old downloads/installers review, file preview).
Still open from Phase 2: "Windows Troubleshooter" per-area launchers beyond Settings > Troubleshoot (msdt is deprecated); browser cookie/history cleaning for Firefox history (shares a file with bookmarks, intentionally not offered); live event "time range" is limited to last 30 days / 300 events per log.


---
## Phase 3 status (version 3.5.0)
Built (compiles; UI tested with mock data; not run on Windows): dashboard (device model, Windows Update status + on-demand check, optional live CPU/RAM/network that is off by default, storage trend per drive, "Needs your review" list, organization/policy detection); Microsoft Store apps and Windows updates in the uninstaller (Store removal via PowerShell with validated package names; updates via the supported wusa tool); startup boot impact from Windows' own boot diagnostics; license-token verification (offline ECDSA P-256) with Settings UI and issuing tools; SBOM generator; privacy policy (PRIVACY.md); vulnerability reporting (SECURITY.md); update signature check (when manifest names a signer); signing scripts and release script.
Needs YOU: a code-signing certificate (tools/sign.sh, docs/SIGNING.md); your own license key pair (tools/license_keygen.py - the embedded key is a DEV placeholder); the decision on which features a license unlocks and which payment provider to use (no feature is gated yet).
Not done: privilege broker (the whole app still elevates by restarting), formal typed-action model, feature flags, SQLite storage, Windows 11 ARM64 build (no ARM64 toolchain available in this build environment).

---
## Phase 3 remaining items (version 3.6.0)
Built (compiles; UI tested with mock data; NOT run on Windows): feature flags + "Beta features" master switch (Store app removal, Windows update uninstall and the administrator helper are beta, off by default); typed action records in the journal (id, type, how-to-undo; shown in the Journal page and CSV export); administrator helper / privilege broker (one UAC prompt, named pipe limited to the same user, client-PID + token check, whitelisted commands, exits with the window).
Not done: Windows 11 ARM64 build (an ARM64 toolchain download was blocked by a permission check - see the chat; a build script is not included); SQLite storage (journal stays append-only JSONL: simpler and crash-safe; adding SQLite needs a third-party source drop); signing, license key pair, paid-feature decision and security contact still need the owner.
