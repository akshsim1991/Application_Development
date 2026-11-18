#!/usr/bin/env python3
"""Regenerates the file tables in docs/MODULES.md (between the AUTO markers).
Usage: python tools/module_map.py   (run from PneuSim-Windows)"""
import os, re, glob
SRC = "src/PneuSim"
MODULES = [
 ("M-ENGINE", "Engine and data model", ["Core/Simulator.cs", "Core/Circuit.cs", "Core/Project*.cs", "Core/Port*.cs", "Core/Tube.cs", "Core/CircuitElement.cs", "Core/Faults.cs", "Core/FaultKind.cs", "Core/Polarity.cs", "Core/LiveTuning.cs", "Core/Naming.cs", "Core/I*.cs", "Core/SimulationRecorder.cs", "Core/EnumDescriptionConverter.cs", "Core/ElementDescriptionProvider.cs"]),
 ("M-SETTINGS", "Settings, units, sound", ["Core/AppSettings.cs", "Core/Units.cs", "Core/SoundEngine.cs", "UI/SoundMonitor.cs", "UI/SettingsDialog.cs", "UI/Theme.cs", "UI/ColorScheme.cs", "Core/AppInfo.cs", "Core/ErrorLog.cs", "UI/ErrorDialog.cs", "UI/AboutDialog.cs", "UI/StartScreen.cs", "UI/MainForm.StartScreen.cs"]),
 ("M-DRAWING", "Drawing surfaces and export", ["Core/DrawSurface.cs", "Core/GdiSurface.cs", "Core/Vector*.cs", "Core/Pdf*.cs", "Core/RenderContext.cs", "UI/Symbols.cs", "UI/Icons.cs"]),
 ("M-ELEMENTS", "Components (one file per component)", ["Elements/*.cs", "Core/ValueLabelConverter.cs"]),
 ("M-GENERATOR", "Circuit generator (sequence to circuit)", ["Generator/*.cs", "UI/GeneratorDialog.cs"]),
 ("M-ANALYSIS", "Checker, explainer, sequence finder", ["Tools/CircuitAnalysis*.cs", "Tools/CheckIssue.cs", "Tools/IssueSeverity.cs", "Tools/CircuitMove.cs", "Tools/MoveTrigger.cs"]),
 ("M-TOOLS", "Parts list, reports, sweep, GIF, fault finding", ["Tools/PartLine.cs", "Tools/PartsList.cs", "Tools/Reports.cs", "Tools/UserGuide.cs", "Tools/Sweep*.cs", "Tools/ParameterSweep.cs", "Tools/GifRecorder.cs", "Tools/TroubleshootExercise.cs", "UI/PartsListDialog.cs", "UI/SweepDialog.cs", "UI/FaultDialog.cs", "UI/TroubleshootPanel.cs", "UI/CalculatorsDialog.cs"]),
 ("M-LEARNING", "Getting-started tutorials and component help", ["Learning/*.cs", "UI/LessonsPanel.cs"]),
 ("M-CANVAS", "Drawing canvas and live views", ["UI/CircuitCanvas.cs", "UI/DiagramPanel.cs", "UI/PlotterPanel.cs", "UI/InspectorPanel.cs", "UI/Cutaway*.cs", "UI/SystemCutawayView.cs", "UI/Library*.cs", "UI/Examples.cs"]),
 ("M-UPDATE", "Automatic updates", ["Core/UpdateService.cs", "UI/UpdateDialog.cs", "UI/MainForm.Update.cs"]),
 ("M-RIBBON", "Ribbon", ["UI/Ribbon*.cs"]),
 ("M-SHELL", "Main window (partial class, split by concern)", ["UI/MainForm*.cs", "UI/Program.cs"]),
 ("M-COMPAT", "Visual Basic compatibility shims", ["VBCompat/*.cs"]),
]
def lines(p): return sum(1 for _ in open(p, encoding="utf-8-sig", errors="ignore"))
seen = set(); out = []
for key, title, pats in MODULES:
    files = []
    for pat in pats:
        for f in sorted(glob.glob(os.path.join(SRC, pat))):
            if f not in seen: seen.add(f); files.append(f)
    total = sum(lines(f) for f in files)
    out.append(f"### {key} - {title}  ({len(files)} files, {total} lines)\n")
    out.append("| File | Lines |\n|---|---|")
    for f in files: out.append(f"| `{os.path.relpath(f, SRC).replace(os.sep, '/')}` | {lines(f)} |")
    out.append("")
rest = [f for f in sorted(glob.glob(SRC + "/**/*.cs", recursive=True)) if f not in seen and "/obj/" not in f and "/bin/" not in f and "AssemblyInfo" not in f]
if rest:
    out.append("### Unassigned\n"); out += [f"- `{os.path.relpath(f, SRC)}`" for f in rest]
text = "\n".join(out)
p = "docs/MODULES.md"; s = open(p, encoding="utf-8").read()
s = re.sub(r"(<!-- AUTO-START -->).*(<!-- AUTO-END -->)", lambda m: m.group(1) + "\n" + text + "\n" + m.group(2), s, flags=re.S)
open(p, "w", encoding="utf-8").write(s)
print("updated", p, "unassigned:", len(rest))
