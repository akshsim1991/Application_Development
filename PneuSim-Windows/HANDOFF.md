# PneuSim — handoff notes (18 Nov 2026)

Owner and designer: Akshaya Simha. A free teaching tool for Fluid Power, Automation and
Control lab work. FluidSIM-style, Windows only.

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
