# Changelog

Entries for 3.0–3.2 are summarised from the README shipped with 3.2.0
(full text: `original/README-3.2.0-bundled.md`).

## 3.3.17 "Mistral" (18 Nov 2026)
- New example 13: Pneumatic counterbalance (vertical load held and lowered under control), in the Examples list and under File > New.

## 3.3.16 "Mistral" (18 Nov 2026)
- Routing rewritten: all lines are now routed together, one after the other, so each line keeps clear of the components and of the lines already placed (before, lines avoided each other independently and could still land on the same track).
- Rotating a group of components now rotates it exactly as one rigid piece (no drift, no overlap).
- Components that end up on top of other components (after rotating, moving, pasting or placing) are moved automatically to the nearest free place.

## 3.3.15 "Mistral" (18 Nov 2026)
- Crossing lines no longer look joined: where a horizontal line crosses another, unconnected line it hops over it with a small arch, as in electrical drawings.
- The automatic router also keeps lines from running on top of each other: a line that would lie along another line is moved over to its own track.

## 3.3.14 "Mistral" (18 Nov 2026)
- Rotate with several components selected (or Select all) now turns the whole group as one piece around its centre, so the layout and the connections stay as they were.

## 3.3.13 "Mistral" (18 Nov 2026)
- Connections now route themselves around components: when the normal neat line would run through a component, PneuSim finds a path of horizontal and vertical lines that goes around, with few bends. Works after rotating, moving or connecting. Lines you drag by hand are kept until they would cross a component.
- Scroll bars follow the dark theme (Windows 10 and later).
- Choosing any other ribbon tab or button closes the File view.

## 3.3.12 "Mistral" (18 Nov 2026)
- Ribbon completeness check against the old menu and toolbar: added the Bottom panel on/off button (View > Panels) and the replay slider (now in the status bar, next to "Replay"). Everything else from the old menus is in the ribbon (or in the File view / Quick access toolbar).

## 3.3.11 "Mistral" (18 Nov 2026)
- After closing the last circuit only the start screen is shown: the diagram, plotter and other bottom panels and the page tabs are hidden until a circuit is opened again.

## 3.3.10 "Mistral" (18 Nov 2026)
- New Manifold (distribution block, 2 to 8 outlets, air or hydraulic): one inlet feeds many tubes or hoses without a pile of T-junctions.
- Counterbalance valve now works with air as well as oil (property "Hydraulic"; new library entry "Counterbalance valve (air)", Logic & Flow tab). Circuits such as the pneumatic load-holding circuit now keep all their tubes and run: held, raised, lowered under control.
- Ribbon: groups that do not fit the window width now show fewer buttons plus a "More" menu instead of being cut off.
- Dark mode: the tab strip under the Workspace is drawn flat in the theme colours (no white outlines); buttons have a dark border.

## 3.3.9 "Mistral" (18 Nov 2026)
- New full ribbon replaces the menu bar and toolbar, giving the Workspace more room: File, Home, Supply & Air, Actuators, Valves, Logic & Flow, Electrical, Hydraulics, Annotate, Simulate, Tools, View, Learn, Help tabs; quick-access buttons (New, Open, Save, Undo, Redo); double-click a tab or press Ctrl+F1 to collapse the ribbon. The classic menu and toolbar can be switched back on in Settings.
- File opens a full-page view (New, Open, Recent, Save, Save As, Close, Export, Information, Options, Exit).
- Page tabs at the bottom of the Workspace now look like sheet tabs with arrows and a + button.
- Rotating (or moving) a component now re-routes its connecting lines automatically so they do not run through components or overlap; lines you drag by hand are kept unless they would cross a component.
- Check circuit now warns about components lying on top of each other, lines running through components, and lines lying on top of each other.
- Pressure properties in the Properties panel and the Inspector now follow the pressure unit chosen in Settings (bar, MPa, kPa), values converted.
- Settings > Workspace: port names on components as Numbers (1 2 3), Letters (P, A, B, E for exhaust, C compressor, T tank) or Both.
& (Save as PDF or Print).

## 3.3.8 "Mistral" (18 Nov 2026)
- Several circuits can be open at once: file tabs along the top (click to switch, x or middle-click to close, + for a new circuit). New, Open, examples and generated circuits open in a new tab; a file that is already open is switched to. File > Close (Ctrl+W) and Close all (Ctrl+Shift+W). Closing PneuSim asks about every tab with unsaved changes.
- Pages (sheets) of a circuit are now tabs at the bottom of the workspace, like Excel sheets.
- Panning: Pan (hand) button, middle mouse button drag, Space + drag. Touchpad: two-finger sideways swipe scrolls the workspace and the ribbon; Shift + wheel scrolls sideways.
- Sizing calculations library (cylinder bore and force, air use, valve and tube size, pump, motor, accumulator, compressor and receiver); dialog follows.

## 3.3.7 "Mistral" (18 Nov 2026)
- Circuit generator "Complete my circuit" is smarter: one valve drives several actuators in parallel (a valve is added only if no suitable one exists); shuttle (OR) and two-pressure (AND) valves are fed by the nearest push-button / selector / roller valves and drive the cylinder or a pilot valve; air-preparation chain (shut-off valve, service unit / regulator); gauges and switches on the air line; silencers on valve exhausts; flow control valves spliced into cylinder lines; hydraulic circuits (pump, tank, relief valve, 4/3 valve, cylinder or motor).

## 3.3.6 "Mistral" (18 Nov 2026)
- Settings > Simulation > Solenoid-operated valves: Automatic (PneuSim operates the solenoid override of valves that have no coil in the circuit, and brings a double-solenoid valve back) or Manual (you click the solenoid yourself).

## 3.3.5 "Mistral" (18 Nov 2026)
- Circuit generator: new main mode "Complete my circuit from the components already in the workspace". PneuSim reads what you placed (cylinders, motors, valves, supply, push buttons, coils, 24 V terminals), adds what is missing (a valve for each actuator, a compressed-air supply, and for solenoid valves a coil, push button, +24 V and 0 V) and connects everything, leaving existing parts and connections alone. Works for single- and double-acting cylinders and air motors; hydraulic circuits are not completed yet. The sequence generator is still available.

## 3.3.4 "Mistral" (18 Nov 2026)
- Automatic operation (Settings > Simulation): PneuSim presses the start button for you when you press Start. One cycle, or repeat continuously; direct-control circuits hold the button until the cylinders have moved, memory circuits get a short pulse. Manual mode is still available.
- Settings window rebuilt with tabs (General, Simulation, Workspace, Sounds), always showing OK and Cancel. New: AMOLED (pure black) theme, currency symbol, start screen on/off, recovery-copy interval, default simulation speed, realistic physics default, live value labels, hover values, grid and snap options.
- Fit now fills the workspace (zooms in as well as out) and centres the circuit.
- About box: large logo, no mention of AI. Whole-workspace cutaway shows valve interiors.

## 3.3.3 "Mistral" (18 Nov 2026)
- Whole-workspace cutaway now shows the inside of valves too (spool, passages, springs) instead of their symbol; symbols are used only for parts with no inside view.

## 3.3.2 "Mistral" (18 Nov 2026)
- Fixed: crash when choosing an item from a ribbon "More" menu (and it no longer repeats items already on the ribbon); Settings dialog no longer cuts off options (scrolls); error dialog message no longer cut off.
- Hovering a part while simulating shows its live values (force, pressure, speed, flow, current...) with its description.
- Ribbon icons are drawn without tiny text and smoothed. User-visible wording now says "workspace".
- Circuit generator: "Build around the cylinders already in the workspace" keeps your cylinders and everything else and adds valves, sensors and wiring (undoable).
- Cutaway window: new "Whole workspace" view (Shift+F7) showing all components together, connected by animated tubes and wires.

## 3.3.1 "Mistral" (18 Nov 2026)
Fixes after first test on a PC: ribbon clicks, update message, Quick Guide PDF, Fit, undo/redo icons, shortcut hints; adds live value labels and new actuators (see below).

## 3.3.0 "Mistral" (18 Nov 2026)
- Removed the practice quiz and timed exam. Lessons became Getting started tutorials;
  Troubleshooting became Fault finding (wording only; engine unchanged, harness identical).
- New user README and About text; the installer ships `installer/README-user.md`.
- New Settings dialog (Tools > Settings, Ctrl+,): theme Light / Dark / Follow Windows (default), pressure unit bar / MPa / kPa for canvas labels, Simple/Advanced flag, sound and update options.
- Synthesised sounds (valve click, end-stop thud, exhaust hiss, compressor/pump hum, relief chatter, alarm), on by default; settings for master, volume and each sound.
- Rated maximum pressure per component (10 bar air, 160 bar oil, -0.95 bar vacuum by default): red frame, status warning and alarm when exceeded.
- Own ribbon (tabs, groups, More drop-downs, search) replaces the left library panel. New start screen (new, open, drop a file, recent circuits, examples). New About box. Status bar shows "PneuSim 3.3.0 Mistral". Window title now "PneuSim 3.3".
- Internal: MainForm and CircuitAnalysis split into partial-class files; docs/MODULES.md added.
- Safer saving (checked temporary file, previous version kept as .bak), friendly messages for damaged files and files from a newer version, local error log with "Copy error report", stronger encryption of hidden faults (new files use hf2; 3.2 files still open).
- Automatic updates (Help > Update to new version, daily check): checksum-verified download, silent in-place upgrade. Installer checks for .NET Framework 4.7.2.
- Properties panel: name column wider; clicking a check result selects the part and scrolls to it.
- Automatic tests: physics invariants and speed on all examples, golden file of the 3.2.0 behaviour (see docs/VALIDATION.md).
- Ribbon fixes: buttons now respond to clicks (hit targets were rebuilt on every repaint), hover highlight and tooltips work, "&" no longer shows as an underscore in tab names, taller ribbon with three-line labels.
- Help > Quick Guide now opens a PDF user guide (what is new, quick start, screen tour, shortcuts, and every component with its symbol).
- Live value labels: choose values (speed, force, position, pressure, flow...) in a part's properties (Display > Live value label); View > Live value labels switches them all on or off.
- New components: tandem cylinder, double-end (through-rod) cylinder, telescopic cylinders (pneumatic and hydraulic, single- and double-acting), hydraulic double-end cylinder, DC motor (reversing, 12/24 V), servo motor (stored positions P1-P3, trapezoidal profile, In-position output, position marks), electric cylinder (lead screw).
- Fit now includes labels and tube bends and allows zooming out to 10%. New undo/redo icons. Shortcut hints fixed (Ctrl+, and Ctrl+PgUp/PgDn were shown with internal key names).
- Update check treats a missing latest.json as "up to date".
- Build tip: the project builds on Linux with `-p:EnableWindowsTargeting=true`.

## Source recovery (18 Nov 2026)
- Source of 3.2.0 recovered from the installer as a buildable C# project. Behaviour matches the
  original (see docs/RECOVERY.md). No functional changes.

## 3.2.0 (6 Oct 2026)
- Troubleshooting practice: hidden faults, measuring, hints, scoring, diagnosis log, and a
  teacher mode that plants faults and hides them in the file.
- State inspector (right-click) and live sliders during simulation.
- Replay: step back and forward, event jumps, time slider.
- Parameter sweep with table, chart and CSV.
- Oscilloscope upgrade: trigger, zoom, hold and scroll, min/max/average, CSV and PNG export.
- New components: compressor with pressure switch, air receiver, shut-off valve, pressure and
  vacuum switch, vacuum generator, suction cup, parallel gripper, flow meter, force sensor,
  preset counter, latching E-stop, idle-return roller valve.
- Two new examples (vacuum handling; air generation) and new quiz questions.

## 3.1.0
- 30 fixes. Cutaway view of every component. Input validation, an error never closes the
  program, autosave and recovery. Automatic unique names, checker rules for shared solenoids,
  duplicate marks and unpaired page connectors. More hydraulic parts. ISO 1219 triangles and
  page column numbers. A fairer quiz and exam. Editor fixes, dark mode in dialogs, and
  corrections to results and PDF fonts.

## 3.0.0
- Live circuit checker with a one-click overlap fix (cascade or relay chain). Explain my
  circuit. Realistic physics (ISO 6358) with air cost in ₹. Hydraulics. Lessons, quiz, exam
  and cutaways. Plotter and calculators. Parts list and PDF report. Multi-page projects, SVG,
  DXF and GIF export, dark mode. Installer with `.pneu` association.

## Earlier
- Automatic circuit generator from a motion sequence (relay step chain or cascade).
  Electro-pneumatics. Live simulation with a displacement-step diagram. A full editor.
