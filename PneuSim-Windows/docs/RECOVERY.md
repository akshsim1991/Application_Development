# How the PneuSim 3.2 source was recovered (18 Nov 2026)

## Starting point

- The original VB.NET source (last commit `e7f372dd6573576c3ed48bcbd623b900c2480180`) and its
  GitHub repository are gone. The Claude Code sessions that built it are not in the claude.ai
  chat history.
- What survived: `PneuSim-Setup-3.2.0.exe` (built 6 Oct 2026 with Inno Setup 6.7), the user's
  `.pneu` circuits, and exported PNG and GIF images, all in
  `OneDrive\SOFTWARE DEVELOPMENT\# Programmed\Windows based\PneuSim-Setup`.

## Steps

1. **Unpacked the installer.** The program files are in one LZMA2 chunk (`zlb\x1a` signature) at
   offset `0xD5800`. Decompressed (770,095 bytes), it holds `PneuSim.exe` (616,448 bytes),
   `PneuSim.exe.config`, `README.md` and 18 example `.pneu` files.
2. **Repaired the executable.** Inno Setup runs an x86 "call instruction" filter over `.exe`
   files before compressing them. This changes bytes after every `E8`/`E9` and corrupts .NET
   metadata. `tools/inno_unfilter.py` reverses the filter (64 KiB blocks, Inno 5.3.9+ variant).
   After that the assembly reads cleanly: 317 types, 2,718 methods.
3. **Decompiled.** ILSpy 8.2 (`ilspycmd`), run against the .NET Framework 4.7.2 reference
   assemblies so that every Windows Forms type resolves. ILSpy produces C# only; there is no VB
   output. The original was VB.NET, so VB idioms appear as calls to the VB runtime
   (`Operators.CompareString`, `Conversions.ToInteger`, `ProjectData`, …) and as compiler-named
   closure classes (`_Closure_0024__…`).
4. **Fixed what did not compile.** These are the only hand edits:

   | File | Problem | Fix |
   |---|---|---|
   | `Tools/Reports.cs` | VB `Iterator Function Wrap` came out as a broken state machine | Rewritten as a normal C# `yield` iterator with the same logic |
   | `VBCompat/VB-AnonymousType_0.cs` | Empty file; the VB anonymous type `{Port, X}` was missing | Recreated as a small generic class |
   | `Generator/SequenceGenerator.cs` | Private nested types used by compiler closures | `PowerUnit`, `LadderItem`, `Rung` made `internal` |
   | `Elements/CylinderBase.cs`, `Elements/SingleActingCylinder.cs` | VB parameterised property `SpringForce(x)` | Turned into a virtual method `SpringForce(double x)` |
   | `Tools/CircuitAnalysis.cs` | Tuple element names `(Mark, Cyl)` lost | Names restored |
   | `Tools/CircuitAnalysis.cs`, `UI/InspectorPanel.cs`, `Core/PdfDocument.cs` | Closure built from an unassigned variable | `null` passed instead, which is what the IL does |

5. **Rearranged** the files into the original folder layout (`Core`, `Elements`, `Generator`,
   `Learning`, `Tools`, `UI`) from the original README. The namespace stays `PneuSim`, so the
   move changes no code.

## Verification

- **Build:** `dotnet build -c Release` gives 0 errors.
- **Inventory:** every type and method of the original exists in the rebuilt assembly. The only
  differences are compiler-generated helpers and `get_SpringForce` → `SpringForce(double)`.
- **Behaviour:** `tools/equivalence-harness` loads an assembly by reflection and, without the
  GUI, runs:
  - all 12 built-in examples;
  - the generator for 4 sequences × 2 methods × single/continuous;
  - all 19 example and user `.pneu` files.

  For each it records the static checks, dynamic checks in ideal and realistic mode, the
  plain-English explanation (including the timed step-by-step story), a 10 s simulation run
  and the XML round-trip. The report from the original `PneuSim.exe` and the report from the
  rebuilt one are **identical (810 lines)**.
- **Not tested:** the harness runs on Linux (.NET 8), where GDI+ drawing and Windows Forms are
  unavailable, so the drawing, dialogs and UI paths were compiled but never executed. They need
  a first run on Windows (see HANDOFF.md). Code that failed under the harness because of GDI+
  failed the same way in both builds.

## Re-running the check

```
# 1. build the recovered program
dotnet build src/PneuSim -c Release
# 2. extract the reference exe from the installer (see steps 1–2), or use any saved copy
python3 tools/inno_unfilter.py PneuSim.exe.filtered PneuSim_original.exe
# 3. compare
cd tools/equivalence-harness && dotnet build -c Release
dotnet bin/Release/net8.0/Harness.dll PneuSim_original.exe ../../examples/*/*.pneu > orig.txt
dotnet bin/Release/net8.0/Harness.dll ../../src/PneuSim/bin/Release/net472/PneuSim.exe ../../examples/*/*.pneu > new.txt
diff orig.txt new.txt
```

The harness project references `System.Drawing.Common` 8.0 so that it gets as far as possible
on Linux. On Windows every path runs.
