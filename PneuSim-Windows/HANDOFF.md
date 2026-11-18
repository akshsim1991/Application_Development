# PneuSim — handoff notes (18 Nov 2026)

Owner and designer: Akshaya Simha. A free teaching tool for Fluid Power, Automation and
Control lab work. FluidSIM-style, Windows only.

## Where things are

- Source: `src/PneuSim/` (C#, .NET Framework 4.7.2, Windows Forms, no NuGet runtime
  dependencies). Build with `dotnet build -c Release`.
- Reference build: `release/PneuSim-Setup-3.2.0.exe`. It installs per user into
  `%LOCALAPPDATA%\Programs\PneuSim`.
- Documentation: `docs/ARCHITECTURE.md`, `docs/FEATURES.md`, `docs/FILE_FORMAT.md`,
  `docs/CLASS_INVENTORY.md`, `docs/RECOVERY.md`.
- Test circuits: `examples/bundled/` (shipped with 3.2.0) and `examples/user/`.

## State at handover

- Version 3.2.0 recovered from the installer. It compiles, and the engine (simulation, checker,
  explainer, generator, file load/save) gives the same results as the original. Details in
  docs/RECOVERY.md.
- The GUI code compiled but was not run, because no Windows machine was available to the
  recovery session.

## Next steps (in order)

1. **First Windows run.** Build, start `PneuSim.exe`, open every file in `examples/` and run
   through the menus. Compare against the installed 3.2.0, using the GUI screenshots being
   collected by Akshaya. Note anything that looks different.
2. **Re-create the build pipeline.** A GitHub Actions workflow (windows-latest:
   build → equivalence harness → Inno Setup) and a new `installer/PneuSim.iss` (per-user
   install, desktop icon, `.pneu` association, uninstaller). Workflows must sit in the
   repository's root `.github/workflows/`. Optional code signing from the secrets
   `SIGNING_CERT_BASE64` and `SIGNING_CERT_PASSWORD`, as before.
3. **Clean-up pass (no behaviour change).** Rename the compiler-generated closures and
   `num12`-style locals, and replace VB runtime helpers with plain C# where the meaning is the
   same. Re-run the equivalence harness after each batch.
4. **Language decision.** Either keep C# (recommended: the recovered code is C#, and the
   tooling is best for it) or convert back to VB.NET with a code converter. Convert only after
   the clean-up pass, and re-verify with the harness.
5. **Tests.** Turn the harness report into checked-in expected output (golden files), so that
   every future change is compared against the 3.2.0 behaviour.
6. Then the revisions and patches (feature requests, bug fixes).

## Known caveats

- Hidden troubleshooting faults (`hf` attribute) are only obfuscated (reversed + Base64).
  Students who know this can read the answer.
- The source is decompiler output. It is correct but hard to read in places (VB closures,
  `_0024` names, `goto` in a few state-machine-like methods).
- Copyright text in About and the PDF reports: "(c) 2026 Akshaya Simha".
