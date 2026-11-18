# Changelog

Entries for 3.0–3.2 are summarised from the README shipped with 3.2.0
(full text: `original/README-3.2.0-bundled.md`).

## 3.3.0 "Mistral" (in progress)
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
