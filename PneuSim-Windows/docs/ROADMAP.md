# PneuSim roadmap (updated 18 Nov 2026, from the owner's requests in the build sessions)

## A. Next release 3.3.8 - polish after the owner's on-screen testing
1. Check on a real PC and fix: ribbon, start screen, Settings tabs, whole-workspace cutaway, sounds, new components, About logo.
2. Fit/zoom edge cases; canvas scrollbar and splitter (G2); Properties panel when nothing is selected shows page/project settings (G6).
3. Plain-English component names with the ISO name in brackets (ribbon labels are still technical).
4. Simple / Advanced switch applied to all engineering properties (only rated pressure uses it today).
5. Unit-aware property inputs (bar / MPa / kPa) and unit-aware cutaway text.
6. Cutaway drawings for the new parts (tandem, double-end, telescopic, DC motor, servo, electric cylinder); today they show their symbol.
7. Ready-made examples and tutorials using the new parts (servo positioning, telescopic lift, DC motor reversing).

## B. 3.4.0 "Sirocco" - actuators and smarter completion
1. Multi-position (back-to-back, 4-port) double-end cylinder; hydraulic tandem cylinder.
2. Servo analogue 0-10 V setpoint (analogue source element); stepper motor; rodless cylinder.
3. Complete-my-circuit for: relays, timers and counters; limit switches and proximity sensors; two-hand control (AND); vacuum (ejector, suction cup); accumulators and counterbalance valves; sequences from existing components; preview with highlighting of what will be added.
4. Automatic operation refinements: sequences with several buttons, per-button choice, record the run.
5. Whole-workspace cutaway: better layout, export as picture or GIF.

## C. 3.5 - commercial differentiators (market check before any "first/only" claim; see DIFFERENTIATORS.md)
Automatic FMEA; safe-restart and air-loss analysis; circuit tests (pass/fail); interactive "Why?"; energy and leak audit; sizing assistant; target cycle-time solver; calibrate from measurements; circuit diff and review; one-click PDF dossier; neutral PLC export (Structured Text); pattern library with intent search.

## D. Quality and engineering
High-DPI (G4); remove decompiler artefacts (closures, num12 names, VB helpers); Windows golden file and GUI automation tests; large-circuit performance test; long-run stability; user guide regenerated per version.

## E. Before selling (owner decisions)
Symbol audit against ISO 1219 / IEC 60617 (buy copies; `SYMBOLS.md`); code-signing certificate; licence-key activation; EULA; trademark search for "PneuSim"; employer IP policy check; lawyer review of symbols and look-and-feel; obfuscation (limits apply).
