# PneuSim — instructions for Claude Code

Owner: Akshaya Simha (akshsimha@gmail.com). Windows pneumatic, electro-pneumatic and hydraulic
circuit simulator. C# / Windows Forms / .NET Framework 4.7.2. Read `HANDOFF.md` first, then
`docs/ARCHITECTURE.md` and `docs/GUI.md`.

## Hard rules
- **Do not start development until the owner writes "START".** Planning, answering questions
  and reading code are fine before that.
- PneuSim is a **paid commercial product**. No student, teacher, exam, classroom or other
  academic/institutional wording anywhere (UI, help, About, README, installer).
- It must be **simple enough for a lay person**: plain-English names (ISO name in brackets),
  sensible defaults, Simple/Advanced switch, tooltips, undo everywhere, friendly warnings.
- Language: **C# only**. Target: **.NET Framework 4.7.2**, Windows 7 SP1, 8.1, 10, 11.
  Minimal dependencies; no paid UI libraries.
- Never change the behaviour of existing circuits by accident: after any engine change, run the
  equivalence harness (`tools/equivalence-harness`) against the 3.2.0 reference; the CI does
  this on every push (`.github/workflows/pneusim.yml`). Intended behaviour changes must be
  listed in `CHANGELOG.md`.
- Keep the installer AppId `{6F1E9A52-3C1B-4B7E-9D0A-5A2C3E7B8D41}` (in-place upgrades).
- Commit messages end with the owner's usual co-author line; push to `main` of
  `akshsim1991/Application_Development`.

## Build and test
```
dotnet build PneuSim-Windows/src/PneuSim/PneuSim.csproj -c Release
python PneuSim-Windows/tools/extract_installer.py PneuSim-Windows/release/PneuSim-Setup-3.2.0.exe ref
dotnet build PneuSim-Windows/tools/equivalence-harness/Harness.csproj -c Release
pwsh PneuSim-Windows/tools/gui-smoke.ps1 -Exe <PneuSim.exe> -Shot shot.png [-File x.pneu]
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DAppVersion=3.3.0 PneuSim-Windows\installer\PneuSim.iss
```
On the owner's PC Claude Code can also launch PneuSim and inspect the GUI directly.
