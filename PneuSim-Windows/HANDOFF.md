# PneuSim — handoff notes (18 Nov 2026)

Owner and designer: Akshaya Simha (akshsimha@gmail.com). A **paid commercial** pneumatic,
electro-pneumatic and hydraulic circuit simulator for Windows. See `CLAUDE.md` for the hard rules.

## Where things are

- Source: `src/PneuSim/` (C#, .NET Framework 4.7.2, Windows Forms, no NuGet runtime
  dependencies). Build with `dotnet build -c Release`.
- Reference build: `release/PneuSim-Setup-3.2.0.exe`. It installs per user into
  `%LOCALAPPDATA%\Programs\PneuSim`.
- Documentation: `docs/GUI.md` (screenshots and UI backlog), `docs/ARCHITECTURE.md`, `docs/FEATURES.md`, `docs/FILE_FORMAT.md`,
  `docs/CLASS_INVENTORY.md`, `docs/RECOVERY.md`.
- Test circuits: `examples/bundled/` (shipped with 3.2.0) and `examples/user/`.

## State at handover

- Version 3.2.0 recovered from the installer. It compiles, and the engine (simulation, checker,
  explainer, generator, file load/save) gives the same results as the original. Details in
  docs/RECOVERY.md.
- CI (`.github/workflows/pneusim.yml`, GitHub's Windows runner) builds the source, confirms the
  engine report matches the original 3.2.0 (1,571 lines, identical), starts the rebuilt GUI empty
  and with all 12 examples plus the counterbalance circuit (correct window title, no error
  dialog, screenshots in the *test-results* artifact), and builds `PneuSim-Setup-<version>.exe`
  with the original installer's AppId, so it upgrades 3.2.0 in place. First green run: 18 Nov 2026.
- Not yet done by hand: clicking through the rebuilt GUI (simulation, dialogs, export).

## Next steps (in order)

1. **First Windows run.** Build, start `PneuSim.exe`, open every file in `examples/` and run
   through the menus. Compare against the installed 3.2.0, using the GUI screenshots being
   in `docs/GUI.md` (40 screenshots of 3.2.0). Note anything that looks different.
2. (Done 18 Nov 2026) Build pipeline and installer rebuilt. Still to add: code signing from
   the secrets `SIGNING_CERT_BASE64` / `SIGNING_CERT_PASSWORD` (needs a bought certificate).
3. **Clean-up pass (no behaviour change).** Rename the compiler-generated closures and
   `num12`-style locals, and replace VB runtime helpers with plain C# where the meaning is the
   same. Re-run the equivalence harness after each batch.
4. **Language decision.** Either keep C# (recommended: the recovered code is C#, and the
   tooling is best for it) or convert back to VB.NET with a code converter. Convert only after
   the clean-up pass, and re-verify with the harness.
5. **Tests.** Turn the harness report into checked-in expected output (golden files), so that
   every future change is compared against the 3.2.0 behaviour.
6. Then the revisions and patches: start with the UI backlog G1–G6 in `docs/GUI.md`, then feature requests and bug fixes.

## Known caveats

- Hidden troubleshooting faults (`hf` attribute) are only obfuscated (reversed + Base64).
  Students who know this can read the answer.
- The source is decompiler output. It is correct but hard to read in places (VB closures,
  `_0024` names, `goto` in a few state-machine-like methods).
- Copyright text in About and the PDF reports: "(c) 2026 Akshaya Simha".

## Plan agreed on 18 Nov 2026 (waiting for "START")

### 3.3.0 "Mistral" (code names: winds — Mistral, Sirocco, Chinook, Zephyr, Bora …)
1. **Commercial, non-academic product.** Remove the practice quiz and timed exam; turn lessons
   into short "Getting started" tutorials; reframe Troubleshooting as **Fault finding** for
   maintenance/technical teams (no teacher/student wording); rewrite README, About, help and
   installer text without academic references.
2. **Lay-person simplicity.** Plain-English component names (ISO name in brackets), sensible
   defaults, **Simple / Advanced** switch for engineering properties, tooltips, friendly messages.
3. **Start screen** like Feather PDF: title, tagline, drag-and-drop area, New circuit,
   Open circuit…, Examples, Recent files.
4. **Ribbon only** (custom-built, no third-party library): tabs Home, Supply & Air, Actuators,
   Valves, Logic & Flow, Electrical, Hydraulics, Drawing, Tools; large symbol buttons,
   galleries for valve variants, drag to canvas, **search box**. The left library panel goes.
5. **About box exactly like Feather PDF** (see the owner's screenshot of Feather PDF 2.22.0
   "Kuga"): scrolling text box with name, "Version 3.3.0 "Mistral", built <date>",
   description, "Created by: Akshaya Simha", DEVELOPMENT HISTORY (began in VB.NET, recovered
   and continued in C# from 3.2), DEVELOPMENT AND AI CONTRIBUTIONS (same wording style as
   Feather PDF); checkbox "Write to the developer now (opens your e-mail program)" →
   akshsimha@gmail.com; Close / Cancel. Status bar right: "PneuSim 3.3.0 Mistral".
6. **Automatic updates like Feather PDF**: `latest.json` + installer in the public repo
   `akshsim1991/PneuSim-releases` (the owner creates it), raw.githubusercontent.com with a
   jsDelivr mirror fallback, SHA-256 check, Help ▸ Update to new version, daily check,
   "Update now / Later", silent in-place upgrade (same AppId), save work first, restart.
7. **Sounds, ON by default** (synthesised in code, no files): valve click, end-stop thud,
   exhaust hiss, compressor/pump hum, relief-valve chatter, alarm for **rated pressure
   exceeded** and E-stop; every warning also shown visually (red flash + status message).
   Settings: master on/off, volume, per-sound toggles.
8. **Settings dialog**: theme Light / Dark / **Follow Windows (default)**; units **bar / MPa /
   kPa**; sounds; Simple/Advanced; update checks.
9. **Rated maximum pressure per component** (defaults: pneumatic 10 bar, vacuum −0.95 bar,
   hydraulic 160 bar), used by the warning.
10. **Robustness**: clean up decompiler artefacts (no behaviour change, harness-verified),
    safe atomic saves with backup, file-version checks and friendly handling of damaged
    files, local error log in `%LOCALAPPDATA%\PneuSim\logs` + "Copy error report", high-DPI
    awareness (G4), large-circuit performance, automatic physics tests, validation document,
    proper encryption for hidden faults.
11. UI fixes G1, G2, G3 from `docs/GUI.md`.
12. Installer: check for .NET Framework 4.7.2 on Windows 7 SP1 / 8.1 and offer to install it.
13. Licensing: **owner still deciding**. Build 3.3 so licence-key activation can be added later
    without restructuring.

### 3.4.0 (after 3.3)
Tandem cylinder; double-end double cylinder in both forms (multi-position, through-rod);
telescopic cylinder (pneumatic and hydraulic, single- and double-acting); DC motor (12 V or
24 V, polarity reversing, ω = (V − IR)/k_E, T = k_T·I); servo motor (stored positions via 24 V
inputs Enable / Select P1–P3 / "In position", plus optional 0–10 V setpoint in Advanced
mode; trapezoidal profile; position marks); electric cylinder (lead screw, position marks);
**live value labels** on the canvas (force, velocity, position, pressure, flow, rpm, torque,
current; choose per component, global on/off). Optional later: stepper motor, rodless cylinder.

### Before selling (owner's decisions)
Code-signing certificate; licensing model; EULA; trademark search for "PneuSim"; check the
employer's IP policy; obfuscation (limits: .NET can always be decompiled).
