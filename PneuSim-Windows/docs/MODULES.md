# PneuSim modules (read only the module you need)

The program is one assembly (`PneuSim.exe`) but is organised in **logical modules**. A task should
normally touch **one module**; read its files only. Do not read the whole source tree.

## How the modules depend on each other (allowed direction: top to bottom)

```
M-SHELL (main window) -> M-RIBBON, M-CANVAS, M-TOOLS, M-LEARNING, M-GENERATOR, M-SETTINGS
M-CANVAS, M-TOOLS, M-LEARNING, M-GENERATOR -> M-ANALYSIS -> M-ENGINE -> M-ELEMENTS
M-DRAWING and M-SETTINGS are leaf services (used by many, depend on nothing above)
```
Rules: lower modules never call upward into the UI (the engine must stay headless so the
equivalence harness and self-test can run it). New features go into the module that owns the
concept; talk to other modules through their public classes only.

## Which module for which task

| Task | Module | Start with |
|---|---|---|
| New or changed component | M-ELEMENTS | the component file + `ElementFactory.cs`, symbol in its `DrawSymbol` |
| Physics, simulation step, ports | M-ENGINE | `Core/Simulator.cs` |
| Save / load, `.pneu` format | M-ENGINE | `Core/Circuit.cs`, `docs/FILE_FORMAT.md` |
| Checker or explainer text | M-ANALYSIS | `Tools/CircuitAnalysis.StaticChecks.cs` / `.Explain.cs` |
| Sequence generator | M-GENERATOR | `Generator/SequenceGenerator.cs` |
| Settings, theme, units, sounds | M-SETTINGS | `Core/AppSettings.cs` |
| Ribbon tabs or buttons | M-RIBBON | `UI/RibbonBuilder.cs` (layout data), `UI/RibbonBar.cs` (control) |
| Menus, toolbar, window layout | M-SHELL | `UI/MainForm.Layout.cs` |
| Start/stop simulation, replay, sound hooks | M-SHELL | `UI/MainForm.Simulation.cs` |
| Open, save, export | M-SHELL | `UI/MainForm.Files.cs` |
| Fault finding | M-TOOLS + M-SHELL | `Tools/TroubleshootExercise.cs`, `UI/MainForm.FaultFinding.cs` |
| Drawing area behaviour | M-CANVAS | `UI/CircuitCanvas.cs` |

## Tests per module
- Engine / analysis / generator: `tools/equivalence-harness` (must stay identical to the 3.2.0 reference).
- Settings, sound, units, rated pressure: `tools/selftest`.
- Whole GUI starts: `tools/gui-smoke.ps1` (CI).

## Refreshing the tables
`python tools/module_map.py` rewrites the table below from the source tree.

<!-- AUTO-START -->
### M-ENGINE - Engine and data model  (20 files, 4099 lines)

| File | Lines |
|---|---|
| `Core/Simulator.cs` | 1414 |
| `Core/Circuit.cs` | 517 |
| `Core/Project.cs` | 424 |
| `Core/ProjectInfo.cs` | 112 |
| `Core/ProjectPage.cs` | 13 |
| `Core/Port.cs` | 70 |
| `Core/PortKind.cs` | 14 |
| `Core/PortState.cs` | 8 |
| `Core/Tube.cs` | 195 |
| `Core/CircuitElement.cs` | 307 |
| `Core/Faults.cs` | 152 |
| `Core/FaultKind.cs` | 30 |
| `Core/Polarity.cs` | 12 |
| `Core/LiveTuning.cs` | 163 |
| `Core/Naming.cs` | 306 |
| `Core/IHydraulicConsumer.cs` | 8 |
| `Core/ISignalSource.cs` | 10 |
| `Core/SimulationRecorder.cs` | 242 |
| `Core/EnumDescriptionConverter.cs` | 51 |
| `Core/ElementDescriptionProvider.cs` | 51 |

### M-SETTINGS - Settings, units, sound  (13 files, 1629 lines)

| File | Lines |
|---|---|
| `Core/AppSettings.cs` | 213 |
| `Core/Units.cs` | 33 |
| `Core/SoundEngine.cs` | 387 |
| `UI/SoundMonitor.cs` | 117 |
| `UI/SettingsDialog.cs` | 165 |
| `UI/Theme.cs` | 107 |
| `UI/ColorScheme.cs` | 66 |
| `Core/AppInfo.cs` | 40 |
| `Core/ErrorLog.cs` | 51 |
| `UI/ErrorDialog.cs` | 67 |
| `UI/AboutDialog.cs` | 119 |
| `UI/StartScreen.cs` | 211 |
| `UI/MainForm.StartScreen.cs` | 53 |

### M-DRAWING - Drawing surfaces and export  (11 files, 1709 lines)

| File | Lines |
|---|---|
| `Core/DrawSurface.cs` | 96 |
| `Core/GdiSurface.cs` | 164 |
| `Core/VectorExport.cs` | 271 |
| `Core/VectorShape.cs` | 37 |
| `Core/VectorSurface.cs` | 202 |
| `Core/PdfDocument.cs` | 198 |
| `Core/PdfImage.cs` | 40 |
| `Core/PdfPage.cs` | 183 |
| `Core/RenderContext.cs` | 132 |
| `UI/Symbols.cs` | 94 |
| `UI/Icons.cs` | 292 |

### M-ELEMENTS - Components (one file per component)  (59 files, 7866 lines)

| File | Lines |
|---|---|
| `Elements/Accumulator.cs` | 131 |
| `Elements/AirMotor.cs` | 137 |
| `Elements/AirReceiver.cs` | 127 |
| `Elements/AirSupply.cs` | 75 |
| `Elements/CentrePosition.cs` | 14 |
| `Elements/CheckValve.cs` | 92 |
| `Elements/CoilKind.cs` | 18 |
| `Elements/CompensatedFlowControl.cs` | 74 |
| `Elements/Compressor.cs` | 191 |
| `Elements/ContactOperator.cs` | 22 |
| `Elements/CounterbalanceValve.cs` | 113 |
| `Elements/CylinderBase.cs` | 667 |
| `Elements/CylinderVariants.cs` | 372 |
| `Elements/DirectionalValve.cs` | 878 |
| `Elements/DoubleActingCylinder.cs` | 64 |
| `Elements/ElectricCoil.cs` | 218 |
| `Elements/ElectricContact.cs` | 300 |
| `Elements/ElectricCounter.cs` | 158 |
| `Elements/ElectricDrives.cs` | 940 |
| `Elements/ElementFactory.cs` | 70 |
| `Elements/FlowControlValve.cs` | 159 |
| `Elements/FlowMeter.cs` | 103 |
| `Elements/ForceSensor.cs` | 62 |
| `Elements/Gripper.cs` | 57 |
| `Elements/HydraulicCentre.cs` | 16 |
| `Elements/HydraulicCylinder.cs` | 106 |
| `Elements/HydraulicMotor.cs` | 144 |
| `Elements/HydraulicPump.cs` | 151 |
| `Elements/HydraulicSingleActingCylinder.cs` | 72 |
| `Elements/HydraulicTank.cs` | 36 |
| `Elements/HydraulicValve42.cs` | 61 |
| `Elements/HydraulicValve43.cs` | 95 |
| `Elements/Junction.cs` | 78 |
| `Elements/LogicValve.cs` | 43 |
| `Elements/PageConnector.cs` | 82 |
| `Elements/PowerTerminal.cs` | 83 |
| `Elements/PressureGauge.cs` | 61 |
| `Elements/PressureRegulator.cs` | 180 |
| `Elements/PressureSwitch.cs` | 184 |
| `Elements/QuickExhaustValve.cs` | 96 |
| `Elements/RegulatorStyle.cs` | 12 |
| `Elements/ReliefValve.cs` | 108 |
| `Elements/SemiRotaryActuator.cs` | 46 |
| `Elements/ShutOffValve.cs` | 159 |
| `Elements/ShuttleValve.cs` | 45 |
| `Elements/Silencer.cs` | 57 |
| `Elements/SingleActingCylinder.cs` | 71 |
| `Elements/SuctionCup.cs` | 159 |
| `Elements/TextNote.cs` | 85 |
| `Elements/TwoPressureValve.cs` | 43 |
| `Elements/VacuumGenerator.cs` | 150 |
| `Elements/Valve22.cs` | 58 |
| `Elements/Valve32.cs` | 58 |
| `Elements/Valve42.cs` | 58 |
| `Elements/Valve52.cs` | 59 |
| `Elements/Valve53.cs` | 102 |
| `Elements/ValveActuator.cs` | 22 |
| `Elements/ValveReturn.cs` | 14 |
| `Core/ValueLabelConverter.cs` | 30 |

### M-GENERATOR - Circuit generator (sequence to circuit)  (7 files, 1475 lines)

| File | Lines |
|---|---|
| `Generator/GeneratedCircuit.cs` | 11 |
| `Generator/GeneratorDialog.cs` | 226 |
| `Generator/GeneratorMethod.cs` | 7 |
| `Generator/MotionSequence.cs` | 150 |
| `Generator/SequenceGenerator.cs` | 937 |
| `Generator/SequenceMove.cs` | 21 |
| `Generator/WorkspaceMerge.cs` | 123 |

### M-ANALYSIS - Checker, explainer, sequence finder  (10 files, 2691 lines)

| File | Lines |
|---|---|
| `Tools/CircuitAnalysis.DynamicCheck.cs` | 170 |
| `Tools/CircuitAnalysis.Explain.cs` | 299 |
| `Tools/CircuitAnalysis.Sequence.cs` | 216 |
| `Tools/CircuitAnalysis.StaticChecks.cs` | 453 |
| `Tools/CircuitAnalysis.Tracing.cs` | 298 |
| `Tools/CircuitAnalysis.cs` | 1165 |
| `Tools/CheckIssue.cs` | 21 |
| `Tools/IssueSeverity.cs` | 8 |
| `Tools/CircuitMove.cs` | 38 |
| `Tools/MoveTrigger.cs` | 23 |

### M-TOOLS - Parts list, reports, sweep, GIF, fault finding  (14 files, 3091 lines)

| File | Lines |
|---|---|
| `Tools/PartLine.cs` | 14 |
| `Tools/PartsList.cs` | 446 |
| `Tools/Reports.cs` | 143 |
| `Tools/UserGuide.cs` | 339 |
| `Tools/SweepResult.cs` | 64 |
| `Tools/SweepSettings.cs` | 29 |
| `Tools/ParameterSweep.cs` | 97 |
| `Tools/GifRecorder.cs` | 266 |
| `Tools/TroubleshootExercise.cs` | 418 |
| `UI/PartsListDialog.cs` | 147 |
| `UI/SweepDialog.cs` | 612 |
| `UI/FaultDialog.cs` | 131 |
| `UI/TroubleshootPanel.cs` | 146 |
| `UI/CalculatorsDialog.cs` | 239 |

### M-LEARNING - Getting-started tutorials and component help  (5 files, 985 lines)

| File | Lines |
|---|---|
| `Learning/ComponentHelp.cs` | 266 |
| `Learning/Lesson.cs` | 21 |
| `Learning/LessonResult.cs` | 8 |
| `Learning/Lessons.cs` | 480 |
| `UI/LessonsPanel.cs` | 210 |

### M-CANVAS - Drawing canvas and live views  (11 files, 5180 lines)

| File | Lines |
|---|---|
| `UI/CircuitCanvas.cs` | 1295 |
| `UI/DiagramPanel.cs` | 131 |
| `UI/PlotterPanel.cs` | 964 |
| `UI/InspectorPanel.cs` | 266 |
| `UI/CutawayPalette.cs` | 63 |
| `UI/CutawayView.cs` | 325 |
| `UI/CutawayWindow.cs` | 123 |
| `UI/Cutaways.cs` | 1302 |
| `UI/Library.cs` | 278 |
| `UI/LibraryPreset.cs` | 19 |
| `UI/Examples.cs` | 414 |

### M-UPDATE - Automatic updates  (3 files, 417 lines)

| File | Lines |
|---|---|
| `Core/UpdateService.cs` | 149 |
| `UI/UpdateDialog.cs` | 142 |
| `UI/MainForm.Update.cs` | 126 |

### M-RIBBON - Ribbon  (3 files, 779 lines)

| File | Lines |
|---|---|
| `UI/RibbonBar.cs` | 536 |
| `UI/RibbonBuilder.cs` | 126 |
| `UI/RibbonIcons.cs` | 117 |

### M-SHELL - Main window (partial class, split by concern)  (11 files, 3692 lines)

| File | Lines |
|---|---|
| `UI/MainForm.Editing.cs` | 247 |
| `UI/MainForm.FaultFinding.cs` | 185 |
| `UI/MainForm.Files.cs` | 376 |
| `UI/MainForm.Lambdas.cs` | 324 |
| `UI/MainForm.Layout.cs` | 645 |
| `UI/MainForm.Other.cs` | 446 |
| `UI/MainForm.Simulation.cs` | 374 |
| `UI/MainForm.Theme.cs` | 202 |
| `UI/MainForm.Tools.cs` | 282 |
| `UI/MainForm.cs` | 550 |
| `UI/Program.cs` | 61 |

### M-COMPAT - Visual Basic compatibility shims  (26 files, 163 lines)

| File | Lines |
|---|---|
| `VBCompat/VB-AnonymousDelegate_0.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_1.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_10.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_11.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_12.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_13.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_14.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_15.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_16.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_17.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_18.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_19.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_2.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_20.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_21.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_22.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_23.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_24.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_3.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_4.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_5.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_6.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_7.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_8.cs` | 6 |
| `VBCompat/VB-AnonymousDelegate_9.cs` | 6 |
| `VBCompat/VB-AnonymousType_0.cs` | 13 |

### Unassigned

- `UI/SystemCutawayView.cs`
<!-- AUTO-END -->
