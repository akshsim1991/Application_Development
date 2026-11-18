# PneuSim (Windows) – Pneumatic, Electro-pneumatic & Hydraulic Circuit Simulator

PneuSim is a free, FluidSIM-style desktop program for designing, simulating, checking and
learning pneumatic, electro-pneumatic and hydraulic circuits. It is written for Windows Forms on
.NET Framework 4.7.2.

Designed and developed by Akshaya Simha (© 2026). The first version was written in VB.NET and
later versions were extended with Claude Code (Claude Pro).

> **Source recovery notice.** The original VB.NET source of version 3.2 was lost. The code in
> `src/PneuSim/` was **recovered from the released program** (`release/PneuSim-Setup-3.2.0.exe`)
> on 18 Nov 2026. It is C#, it builds, and its simulation engine gives the same results as the
> original program. The recovered code still carries some decompiler noise, which a clean-up
> pass will tidy. See [docs/RECOVERY.md](docs/RECOVERY.md) for how it was recovered and checked.

## Folder contents

| Path | Contents |
|---|---|
| `src/PneuSim/` | Recovered source (135 files, about 29,000 lines) in the original folder layout: `Core`, `Elements`, `Generator`, `Learning`, `Tools`, `UI` |
| `docs/ARCHITECTURE.md` | How the program is built: modules, element model, simulation engines, UI |
| `docs/FEATURES.md` | Complete feature list for version 3.2 (menus, components, tools, learning) |
| `docs/GUI.md`, `docs/screenshots/` | GUI reference: 40 screenshots of 3.2.0 (window layout, every menu, the library, properties, all 12 examples) and the UI issues found in them |
| `docs/FILE_FORMAT.md` | The `.pneu` circuit and project file format |
| `docs/CLASS_INVENTORY.md` | Every source file with its type, base class and size |
| `docs/RECOVERY.md` | How the source was recovered, what was fixed by hand, how equivalence was checked |
| `examples/bundled/` | The 18 circuit files shipped inside the 3.2.0 installer |
| `examples/user/` | Circuits made with PneuSim (counterbalance circuit, 9 Oct 2026) |
| `original/README-3.2.0-bundled.md` | The README that shipped inside the installer (original user guide and change history) |
| `release/PneuSim-Setup-3.2.0.exe` | The last known-good installer. This is the reference build for comparisons |
| `tools/` | `inno_unfilter.py` (repairs the executable taken out of the installer) and the equivalence test harness |
| `HANDOFF.md` | Current state, known issues, next steps |
| `CHANGELOG.md` | Version history |

## Build

Windows (Visual Studio 2022 or .NET SDK 6+):

```
cd PneuSim-Windows\src\PneuSim
dotnet build -c Release
```

The output is `bin\Release\net472\PneuSim.exe` (with `PneuSim.exe.config`). Linux and macOS can
also compile it, because the project pulls in the .NET Framework reference assemblies from
NuGet. The program itself runs only on Windows.

## Run

`PneuSim.exe` needs .NET Framework 4.7.2 or newer, which is built into Windows 10 and 11.
Settings are kept in `%APPDATA%\PneuSim\settings.ini`, and the one-minute crash autosave in
`%APPDATA%\PneuSim\autosave.pneu`.
