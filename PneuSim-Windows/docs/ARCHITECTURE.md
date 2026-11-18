# PneuSim 3.2 – architecture

This document describes the program as recovered from release 3.2.0. Everything is in one
assembly, `PneuSim.exe`, and one namespace, `PneuSim`. The only references are the .NET
Framework 4.7.2 base libraries, Windows Forms, System.Drawing, System.Xml.Linq and the VB
runtime (`Microsoft.VisualBasic`). There are no third-party packages and no embedded resources:
icons, symbols, cutaways and the examples are all drawn or built in code.

## 1. Layers

```
┌──────────────────────────────── UI (Windows Forms) ───────────────────────────────┐
│ MainForm ─ menus, toolbar, library, Properties grid, page tabs, bottom panel tabs │
│ CircuitCanvas (editor + live simulation)   DiagramPanel   PlotterPanel            │
│ InspectorPanel   LessonsPanel   TroubleshootPanel   Cutaway{View,Window}          │
│ Dialogs: Generator, Calculators, PartsList, Quiz/Exam, Sweep, Fault, ProjectInfo   │
├───────────────────────────── Tools / Generator / Learning ────────────────────────┤
│ CircuitAnalysis (checker + explainer + overlap fix)   TroubleshootExercise       │
│ ParameterSweep   PartsList   Reports (PDF)   GifRecorder                         │
│ MotionSequence → SequenceGenerator → GeneratedCircuit (relay chain / cascade)    │
│ Lessons   QuestionBank   ComponentHelp                                           │
├───────────────────────────────────── Core ────────────────────────────────────────┤
│ Project ─ ProjectPage* ─ Circuit ─ CircuitElement* + Tube*  (Port, PortKind)      │
│ Simulator (ideal network solver, realistic ISO 6358, hydraulics, air generation) │
│ SimulationRecorder (replay)   Faults   LiveTuning   Naming   AppSettings          │
│ DrawSurface ← GdiSurface / VectorSurface → VectorExport (SVG, DXF), PdfDocument   │
├──────────────────────────────────── Elements ─────────────────────────────────────┤
│ 45 component types derived from CircuitElement, created by ElementFactory        │
└───────────────────────────────────────────────────────────────────────────────────┘
```

## 2. Data model (Core)

| Class | Role |
|---|---|
| `Project` | The document. It holds `ProjectInfo` (title block, revision, ₹ air cost, duty cycle), a list of `ProjectPage` and the part-cost overrides. Files are saved as `<PneuSimProject version="3">` |
| `ProjectPage` | A named page containing one `Circuit`. Pages are linked by `PageConnector`s with the same name |
| `Circuit` | Holds `Elements` (`List<CircuitElement>`) and `Tubes`. Handles `Add`, `Connect`, `CanConnect`, hit-testing and XML save/load (`<PneuSimCircuit version="2">`) |
| `CircuitElement` | Abstract base of every component. Its members are listed below |
| `Port` | A connection point with a `PortKind` (air, oil, electrical), a live `PortState` (pressurised, exhausted, trapped, live …) and a pressure |
| `Tube` | Links two ports (air or oil line, or electrical wire). It has bend points (`mid`) and a fault slot |
| `Faults`, `FaultKind` | Fault model for troubleshooting: Leak, Blocked, StuckNormal, StuckOperated, BurntCoil, ContactOpen, ContactWelded, Sticking, Jammed, LowOutput. Hidden faults are written to the file in encoded form |
| `Naming` | Automatic unique labels (1A, 1V1, 1M1, S1, K1 …) and renaming of pasted parts |
| `AppSettings` | `%APPDATA%\PneuSim\settings.ini`: dark mode, lesson progress, recent files and similar |

### CircuitElement contract

To add a component (from the original README), derive from `CircuitElement`, then:

| Member | Purpose |
|---|---|
| constructor | Create the ports and set `TypeName` |
| `TypeName`, `DisplayName`, `LocalBounds` | Identity and size of the symbol |
| `DrawSymbol(DrawSurface, RenderContext)` | ISO 1219 symbol. The same code draws to the screen, SVG, DXF, PDF and GIF |
| `AddEdges(Simulator)` | The flow passages this component offers in its current state |
| `AddTerminals(Simulator)` | Electrical connections |
| `UpdateLogic(Simulator)` | Switch on pilot pressure, coils, buttons or timers. Returns `true` if something changed |
| `UpdateDynamics(Simulator, dt)` | Motion and time behaviour (cylinders, timers, compressor and so on) |
| `ResetSim`, `OnSimMouseDown/Up` | Simulation reset and manual operation |
| `PossibleFaults`, `FaultDescription` | Troubleshooting support |
| `InspectValues` | Live values for the right-click state inspector |
| `ShowProperty(name)` | Hides settings that do not apply in the Properties panel |

Then register the component in `Elements/ElementFactory.cs` (type name → instance) and
`UI/Library.cs` (palette), and optionally in `UI/Cutaways.cs`, `Learning/ComponentHelp.cs` and
`Tools/PartsList.cs` (price).

## 3. Element families (`Elements/`)

| Family | Classes |
|---|---|
| Base classes | `CylinderBase`, `DirectionalValve`, `LogicValve`, `ElectricContact`, `ElectricCoil` |
| Supply and air preparation | `AirSupply`, `Compressor`, `AirReceiver`, `ShutOffValve`, `PressureRegulator` (service unit or regulator, see `RegulatorStyle`), `PressureGauge`, `FlowMeter`, `Silencer`, `Junction` |
| Actuators and handling | `SingleActingCylinder`, `DoubleActingCylinder`, `SemiRotaryActuator`, `AirMotor`, `Gripper`, `VacuumGenerator`, `SuctionCup` |
| Directional valves | `Valve22`, `Valve32`, `Valve42`, `Valve52`, `Valve53` (with `CentrePosition`). The operator is set by `ValveActuator` (PushButton, Selector, RollerLever, idle return, Pilot, DelayedPilot, Solenoid) and the return by `ValveReturn` |
| Logic and flow | `ShuttleValve`, `TwoPressureValve`, `CheckValve`, `QuickExhaustValve`, `FlowControlValve` (one-way or two-way) |
| Pressure | `PressureRegulator`, `PressureSwitch` (pressure or vacuum), `ReliefValve`, `CounterbalanceValve`, `CompensatedFlowControl` |
| Electrical (24 V DC) | `PowerTerminal` (+24 V / 0 V), `ElectricContact` (buttons, selector, E-stop, relay contacts, proximity and limit switches, pressure-switch contact, by `ContactOperator`), `ElectricCoil` (relay, on and off delay timers, solenoid, lamp, by `CoilKind`), `ElectricCounter`, `ForceSensor` |
| Hydraulics | `HydraulicPump`, `HydraulicTank`, `HydraulicValve42`, `HydraulicValve43` (`HydraulicCentre`), `HydraulicCylinder`, `HydraulicSingleActingCylinder`, `HydraulicMotor`, `Accumulator`, plus the oil-line forms of the check, flow, relief and counterbalance valves (`IHydraulicConsumer`) |
| Drawing | `TextNote`, `PageConnector` |

Names link elements together. A valve's solenoid label (`1M1`) matches an `ElectricCoil`; a
relay contact's reference (`K1`) matches a relay coil; a roller valve or sensor's mark (`1S2`)
matches a cylinder's `RetractedMark` or `ExtendedMark`.

## 4. Simulation engine (`Core/Simulator.cs`)

Constants: `PilotThreshold = 1.5 bar`, `ControlVoltage = 24 V`, at most 60 logic passes per step,
history sampled every 0.05 s (60 s kept), plotter channels every 0.02 s (120 s kept), leak
40 Nl/min.

**Ideal mode (default).**

1. Every port is a node. Tubes, wires, open valve passages and closed contacts become edges
   (`AddEdge(a, b, capacityAB, capacityBA, pressureLimitAB)`). An edge can be one-way (check
   or quick exhaust valve) and can limit the pressure it passes on (regulator).
2. Nodes reachable from a supply (or +24 V) are *pressurised* (or *live*). Nodes that can reach
   an exhaust (or 0 V) are *exhausted*. All other nodes keep their trapped pressure. Connected
   groups are found with a union-find.
3. `RunLogic` lets valves, relays, contacts, timers and counters switch (pilot ≥ 1.5 bar,
   energised coils, buttons, rollers), then solves again until nothing changes.
4. Flow-control valves reduce passage capacity, possibly by direction. Cylinder speed is set by
   the narrowest point on the supply path and on the exhaust path, so meter-in and meter-out
   behave as they do in a real circuit. A double-acting cylinder compares forces on both sides
   (the rod side has 70 % of the piston area). A cylinder whose outlet air is trapped cannot
   move.

**Realistic mode** (`RealPhysics = true`). Each cylinder chamber holds a mass of air. Flow
follows ISO 6358 (sonic conductance C, critical pressure ratio b). Piston force comes from the
chamber pressures, load mass and force, friction, spring and cushioning, integrated in 0.2 ms
steps (`CylinderBase.UpdateDynamics`). Sequence valves switch at the real pressure. Air
consumption is totalled into normal litres per cycle and a ₹-per-year running cost
(`CostSummary` uses `ProjectInfo`: ₹/m³, cycles/min, h/day, days/year).

**Hydraulics.** Cylinder speed = pump flow ÷ piston area (shared between consumers through
`HydraulicFlowShare`). Pressure is set by the load and limited by the relief valve. The
accumulator and counterbalance valve are modelled. Oil lines are drawn in orange.

**Air generation and vacuum.** The compressor fills the receiver with a pressure-switch
hysteresis. The ejector produces vacuum at the cup, which a vacuum switch reads. The flow meter
shows the flow through its passage.

**Recording and replay.** `SimulationRecorder` stores snapshots so that the UI can step
backwards and forwards, jump between events, or drag the time slider. *Start* resumes from the
moment shown.

## 5. Tools

| Class | What it does |
|---|---|
| `CircuitAnalysis.StaticChecks` | Live checker: open ports, missing exhausts, no supply, cylinders that cannot move, solenoids without a coil, short circuits, duplicate solenoids, marks or page connectors |
| `CircuitAnalysis.FindMoves / InferSequence / FindOverlaps` | Works out the motion sequence and detects signal overlap |
| `CircuitAnalysis.ElectroFix` | One-click redesign of an overlapping circuit as a cascade or relay step chain |
| `CircuitAnalysis.DynamicCheck` | Test-runs the circuit (ideal or realistic) and reports what happens |
| `CircuitAnalysis.Explain` | Plain-English description and a timed step-by-step story |
| `TroubleshootExercise` | Hides a random fault, records measurements, gives hints (area → neighbours → cause), scores the answer and keeps a diagnosis log |
| `ParameterSweep` | Re-runs the circuit with one setting stepped over a range (cycle time, air per cycle, timings, top speed). Results shown as a table and chart, exportable to CSV |
| `PartsList`, `Reports` | Parts list with costs (CSV) and a PDF report with title block. `PdfDocument` is a small built-in PDF writer |
| `GifRecorder` | Animated GIF of the canvas, using a built-in encoder |

## 6. Generator

`MotionSequence.Parse("A+ (B+ C+) B- C- A-")` reads a sequence; brackets mark simultaneous
moves. `SequenceGenerator.Generate(seq, method, continuous)` builds a complete `Circuit` and an
explanation using one of two methods (`GeneratorMethod`):

- `ElectroRelayChain`: an electro-pneumatic relay step chain with a ladder diagram (rungs,
  relay latches, solenoid outputs).
- `PneumaticCascade`: group lines and a cascade valve chain. This method cannot place parallel
  moves and says so.

## 7. Learning

`Lessons` has 10 lessons whose tasks are checked automatically (direct control → sequence
A+ B+ B- A-), with progress saved in settings. `QuestionBank` has 39 written questions plus
symbol-recognition questions, used by the practice quiz and the timed exam. `ComponentHelp`
supplies hover help, and `Cutaways` draws an animated cutaway of every component (F7).

## 8. UI (`UI/`)

`MainForm` (about 3,200 lines) builds every menu, toolbar and panel in code; there are no
designer files. The menus are File, Export, Edit, Page, View, Simulation, Tools, Learn,
Examples, Troubleshooting and Help (full list in FEATURES.md). `CircuitCanvas` handles
drawing, selection, dragging, tube routing, zoom and pan, keyboard shortcuts, drag and drop
from the library, and manual operation during simulation. `Theme` and `ColorScheme` provide
light and dark mode, `Icons` the toolbar icons, and `Symbols` the shared ISO 1219 drawing
helpers.

## 9. Drawing pipeline

All rendering goes through the abstract `DrawSurface`. `GdiSurface` draws on screen, in PNG and
GIF. `VectorSurface` records `VectorShape`s that `VectorExport` writes as SVG or DXF and
`Reports` writes into PDF pages. Each element's symbol code is therefore written once.

## 10. Build, packaging and settings

- Target .NET Framework 4.7.2, `WinExe`, icon `app.ico`, file version 3.2.0.0.
- Original Git commit of the 3.2.0 build: `e7f372dd6573576c3ed48bcbd623b900c2480180` (from the
  assembly's informational version). The original repository is no longer on the GitHub account.
- The installer was built with Inno Setup 6.7 (`installer/PneuSim.iss` in the original, now
  lost). It installed per user into `%LOCALAPPDATA%\Programs\PneuSim` and added a desktop icon
  and the `.pneu` file association.
- The original GitHub Actions workflow built the program, ran tests and produced
  *PneuSim-Windows* and *PneuSim-Setup* artifacts, with optional code signing from the secrets
  `SIGNING_CERT_BASE64` and `SIGNING_CERT_PASSWORD`. Both the workflow and the tests are lost.
