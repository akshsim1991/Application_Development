# PneuSim application audit (18 Nov 2026)

Scope: the whole application as of 3.4.0, audited in the structured steps requested. No new features were added in this audit; only defects were fixed.

## 0. Honest limits of this audit
The audit was done in a Linux cloud session. The program is a Windows Forms application, and Linux has no GDI+, so **no button, menu or dialog could be clicked and no screen could be looked at by the auditor**. What was possible, and what was not:

| Method | Done | Notes |
|---|---|---|
| Static review of code (architecture, commands, settings, update, routing, tabs) | yes | |
| Automated command audit (every ribbon button maps to a registered command; every icon has a drawing; shortcut keys unique) | yes | scripted, see section 2 |
| Equivalence harness (engine behaviour identical to 3.2.0 on 31 circuits) | yes | every build |
| Self-tests (101 checks incl. sounds, units, files, physics, generator, GRAFCET, manifold, counterbalance circuit, update version logic) | yes | every build |
| Windows CI: build, harness, self-tests, GUI smoke test (starts PneuSim, opens the examples, screenshots), installer build | yes | every build |
| Interactive clicking of every button/dialog on Windows | **no** | needs a person at a Windows PC |
| Visual inspection of every screen | **partly** | only through screenshots the owner sent during development |
| Start-up time, memory and CPU on Windows, long-run (hours) stability | **no** | not measurable here; see section 7 |

Every bug below therefore states its **verification status** exactly: *Automated* (a test would fail without the fix), *CI-run* (the Windows build and smoke test pass), or *Code review* (fix reviewed and builds, but not seen on screen by the auditor; the owner should confirm).

## 1. Application-wide inspection
- Architecture: Windows Forms, C#, .NET Framework 4.7.2, about 30,000 lines in `Core`, `Elements`, `Generator`, `Learning`, `Tools`, `UI`; `MainForm` split by concern (partial classes). No database; persistence is XML files (`.pneu`), `settings.ini`, an autosave file and a text error log.
- Dependencies: none beyond the .NET Framework (no third-party packages); installer by Inno Setup.
- Workflows reviewed: start-up, open/save/close (multi-tab), simulate, check, explain, export, update, settings, fault finding.

## 2. Functional test results
| Check | Result |
|---|---|
| Ribbon buttons mapped to commands | Every key used by the ribbon is registered (Speed0–5 are registered in a loop) |
| Ribbon icons | Every icon key has a drawing (no blank buttons) |
| Menu shortcuts | 17 menu shortcuts, no duplicates; Ctrl+Z/Y/X/C/V/D/A/R/Del handled by the form; documented in README section 9 |
| Old menu/toolbar versus ribbon | Two commands were missing from the ribbon layout (replay slider, bottom-panel toggle): fixed |
| Settings | Every option present in the dialog is saved and loaded (checked in `AppSettings`); two new options added with their keys |
| Files | Load/save round trip, damaged file, newer-version file, safe-save `.bak`: automated self-tests pass |
| Engine | 31 circuits identical to 3.2.0 |
| Examples | 13 examples build; GRAFCET is generated for 11 of them |

## 3. UI/UX inspection (from code and owner screenshots)
Issues found are in the bug table (B2, B5, B6, B7, B9, B10, B11, B12). Layout was checked at the window sizes seen in the owner's screenshots (1920 × 1080); small-screen behaviour (1366 × 768) was **not** tested.

## 4. Bug report
Severity: Critical = data loss or cannot work; High = feature unusable or visibly broken; Medium = wrong but with a workaround; Low = cosmetic.

| ID | Bug | Steps to reproduce | Sev. | Root cause | Fix | Verification |
|---|---|---|---|---|---|---|
| B1 | Pneumatic counterbalance circuit lost its tubes and could not run | Open `Pneumatic_Counterbalance.pneu`, press Start | **Critical** | Counterbalance valve only had oil (hydraulic) ports; air tubes were removed on load | Valve got `Hydraulic` and `ExternalPilot` properties; air variant in library | **Automated** (self-test loads the file: 13 tubes kept; held, raised, lowered) |
| B2 | Rotating a selection scrambled the layout; components overlapped | Select all, press Rotate | **High** | Each part turned around its own bounds centre with grid snapping | Group turns rigidly about its centre; overlap resolver moves parts to free space | Code review + CI-run |
| B3 | Lines ran through components and lay on top of each other | Two ports of a valve to a cylinder side by side | **High** | Each line avoided others independently; only one pass | All lines routed together in order, penalty for lying on another line; crossover hops | **Automated on Windows** (self-test "valve-to-cylinder lines do not overlap" runs in CI) |
| B4 | Ribbon search list showed `PneuSim.RibbonBar+Item` | Type "cir" in the search box | **High** | List bound to a field (`DisplayMember`), which WinForms ignores | `ToString`, search in tooltip, ranking | Code review + CI-run |
| B5 | Ribbon groups cut off at the window edge | Home tab, window 1920 wide | Medium | Fixed group width | Fewer buttons plus More menu | Code review + CI-run |
| B6 | Old graph and panels visible after closing the last circuit | Close all circuits | Medium | Start screen only covered the top | Bottom panel and page tabs hidden while the start screen shows | Code review + CI-run |
| B7 | Port names unreadable in dark mode | Letters setting, dark theme | Low | Fixed dark-blue brush | Theme-dependent colour | Code review |
| B8 | Pressure unit ignored in Properties and Inspector | Settings: MPa; select an air supply | Medium | Property names and values hard-wired to bar | Property wrapper converts and renames | Code review + CI-run (Calculators and Sizing still bar, see section 9) |
| B9 | File view stayed open after choosing another tab | File, then View | Medium | No hook on other ribbon clicks | `Interacted` event closes it | Code review |
| B10 | Scroll bars light in dark mode | Dark theme | Low | Native scroll bars | `SetWindowTheme` dark mode (Windows 10+) | Code review |
| B11 | Update check only once a day; updates easy to miss | Start with an update available | Medium | Throttle and a transient message | Check at every start, status-bar link, prompt, automatic updates option | **Automated** for version comparison; flow code review |
| B12 | Replay slider and bottom-panel toggle missing | Look for them in the ribbon layout | Medium | Only in the hidden toolbar/menu | Slider in the status bar; toggle in View > Panels | Code review + CI-run |
| B13 | `check_golden.sh` printed "ALL PASSED" from a stale self-test when the self-test did not compile | Introduce a compile error in the self-test | Medium (tooling) | Build output piped to `tail` without `pipefail` | `set -o pipefail` | **Automated** (script now exits non-zero) |
| B14 | Equivalence check failed when a new example was added | Add example 13 | Low (process) | Harness compared all examples with the 3.2.0 reference | Harness limited to the 12 original examples | CI-run |
| B15 | Ctrl+W, Ctrl+Shift+W, Ctrl+F1, Ctrl+', Space+drag, Esc (File view) missing from the guide | Read the guide | Low | Documentation lag | Guide and README updated | Document review |

## 5. Fixes implemented
B1–B15 as above. No functionality was removed. Existing circuits behave as in 3.2.0 (harness identical).

## 6. Regression testing (after the last fix)
| Test | Result |
|---|---|
| Equivalence harness vs. 3.2.0 (31 circuits) | identical |
| Self-tests | 101 passed, 0 failed, 2 skipped on Linux (the skipped ones need Windows drawing code and run in CI) |
| Windows CI: build, harness, self-tests, GUI smoke test, installer | all green for the released build |
| Ribbon command/icon audit | no unmapped command, no missing icon |

## 7. Performance and reliability
| Item | Measured | Note |
|---|---|---|
| Simulation speed | 0.03 ms per step (24,000 steps in 0.77 s) | self-test requires < 5 ms |
| Memory (headless, 31 circuits) | peak 192 MB including the .NET runtime on Linux | Windows GUI memory **not measured** |
| Start-up time, CPU, hours-long run | **not measured** | needs a Windows session; recommended next step: run PneuSim for 8 hours on a repeating circuit and watch Task Manager |
| Resource handling | Fonts, pens and brushes are created with `using` in the new code; the router is cached per change of the circuit | line routing cost grows with parts × lines; fine for circuits of ~100 parts, not yet tested with 1,000 |
| Error handling | Errors are logged locally; the router and update code fall back silently (default line, no update) instead of crashing | |

## 8. Update mechanism assessment
| Aspect | Status |
|---|---|
| Version check | **Implemented**: manifest `latest.json` from the public release site (two HTTPS locations, with fallback); checked at every start; failures are silent |
| Notification | **Implemented**: prompt window with notes, status-bar link, manual check in Help |
| Secure download | **Implemented**: HTTPS only |
| Integrity | **Implemented**: SHA-256 from the manifest; a mismatch deletes the file |
| Installation | **Implemented**: silent in-place upgrade (same installer AppId), work is saved first; automatic mode installs after PneuSim is closed |
| Authenticity | **Gap**: the installer is not code-signed, and the checksum comes from the same site as the file, so someone who could change the release site could change both. Recommendation: buy a code-signing certificate and sign the installer; optionally sign the manifest |
| Rollback | **Gap**: none. Recommendation: keep the previous installer in `%LOCALAPPDATA%\PneuSim\previous` and offer "Go back to the previous version" |
| Failure recovery | **Partly**: a failed download leaves the installed version untouched; a failed silent install by the installer leaves the old version in place. Automatic installs are not logged and are not retried. Recommendation: write the result to the error log and retry at the next start |

## 9. Remaining known issues
1. Calculators and Sizing dialogs still use bar (pressure unit setting not applied there). *Medium.*
2. The router can still leave a line across a part in very crowded circuits; drag the line by hand. *Medium.*
3. Not verified on screen by the auditor: B2, B4–B10, B12 (the fixes are in the CI-green build; please confirm on a Windows PC).
4. 1366 × 768 screens, high-DPI scaling above 150 %, and long-run behaviour were not tested.
5. Update authenticity and rollback gaps (section 8).
6. The Complete my circuit tool does not insert manifolds. *Low (feature, not a bug).*
7. Windows 7/8: dark scroll bars unavailable (cosmetic).

## 10. Final stability assessment
The simulation engine, file handling, generator, checker and update logic are covered by automated tests and match the 3.2.0 reference, so the **core is stable**. The **user interface layer** (ribbon, tabs, dialogs, routing) changed a lot during development and is verified by CI and owner screenshots but not by a full manual pass; treat it as **release-candidate quality**. A 60–90 minute manual pass on Windows following the checklist in `docs/TEST-CHECKLIST.md` is the recommended last step before calling the foundation fully verified.
