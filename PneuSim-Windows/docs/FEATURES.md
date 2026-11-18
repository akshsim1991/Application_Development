# PneuSim 3.2 – feature list

Taken from the recovered code and the README that shipped with 3.2.0.

## Menus (exactly as built in `UI/MainForm.cs`)

| Menu | Items |
|---|---|
| File | New (Ctrl+N), Open… (Ctrl+O), Save (Ctrl+S), Save As…, Project information (title block)…, **Export** ▸ PDF report with title block… (Ctrl+P), SVG drawing (this page)…, DXF drawing for CAD (this page)…, PNG image (this page)…, Parts list (CSV)…; Exit |
| Edit | Undo, Redo, Cut, Copy, Paste, Duplicate, Select All, Delete, Rotate 90° |
| Page | Add page, Rename page…, Delete page, Next page, Previous page |
| View | Zoom In, Zoom Out, Actual Size, Zoom to Fit, Bottom panel (diagram, plotter, check, explain, lessons, inspector, troubleshoot), Cutaway view (animated), Dark mode |
| Simulation | Start (F9), Pause (F10), Stop and Reset (F11), Realistic physics, Record animated GIF, Step back / forward (Shift+F12 / F12), previous / next event (Ctrl+Shift+F12 / Ctrl+F12) |
| Tools | Circuit Generator (from a sequence)… (Ctrl+G), Check my circuit, Explain my circuit, Calculators…, Parts list and costs…, Parameter sweep (compare settings)… |
| Learn | Getting started tutorials, Cutaway view (inside the components) (F7) |
| Examples | 12 built-in examples (below) |
| Fault finding | New fault-finding task (hidden fault), Hint, I found the fault…, Show the fault, Put a fault into the selection… (with *Hidden*, for preparing a task for a colleague), Repair all faults |
| Help | Quick Guide, About PneuSim |

## Editor

- Place a part by click-and-click or drag-and-drop from the library. Connect by dragging port to
  port; red circles are electrical terminals.
- Box selection, Ctrl+click to add or remove; copy, cut, paste and duplicate (pasted parts get
  their own labels); rotate (R); delete; nudge with the arrow keys.
- Tubes have bend points and junctions; drag a tube's middle segment to move it.
- Ctrl+wheel zooms at the pointer; Ctrl+9 zooms to fit. Parts cannot be lost off the sheet.
- The Properties panel shows only the settings that apply, with readable names, and checks
  values as you type them.
- Automatic unique names: 1A, 2A, 1V1, 1M1, S1, K1 …
- Several pages per project, page connectors and cross-references (e.g. "1.5"), and column
  numbers along the top of every page.
- Undo/redo works wherever the focus is. Autosave every minute, with recovery after a crash.
  An error never closes the program.

## Simulation

- Ideal network solver (default), realistic ISO 6358 pneumatics, hydraulics, air generation,
  vacuum and flow measurement.
- Manual operation during the run: buttons and switches; solenoid manual override (left half
  of the valve = left solenoid, right half = right solenoid).
- Displacement-step diagram. Plotter (position, speed, chamber pressures, valve positions, air
  consumption) with A/B cursors, trigger (rising or falling, auto or single shot), wheel zoom,
  hold and scroll, min/max/average between cursors, CSV and PNG export.
- Right-click state inspector (port pressures, position, speed, force, coil voltage, timer,
  count …) and live sliders (supply pressure, regulator, throttle, load, mass, friction, timer
  delays, pump flow …).
- Replay: step back and forward, jump between events, time slider, resume from the moment shown.
- Status bar: cycle time, air per cycle, running cost in ₹ per year.
- Animated GIF recording.

## Checking, explaining, generating

- Live circuit checker: open ports, missing exhausts, no supply, cylinders that can never move,
  solenoids without a coil, short circuits, signal overlap, shared solenoids, duplicate marks,
  page connectors without a partner.
- One-click overlap fix that redesigns the circuit as a pneumatic cascade or an
  electro-pneumatic relay step chain. *Run full check* also test-runs the circuit.
- Explain my circuit: contents, how each actuator is controlled, the motion sequence, and a
  timed step-by-step story.
- Circuit generator: a sequence such as `A+ B+ B- A-` or `A+ (B+ C+) B- C- A-` produces a
  complete circuit, as an electro-pneumatic relay step chain or a pneumatic cascade (the
  cascade cannot place parallel moves). Single cycle or continuous.

## Measuring and reporting

- Calculators: cylinder force, air consumption and cost (free air per cycle, ₹ per year), valve size (Qn, Cv, Kv, sonic conductance C), hydraulics (pump flow, speed, efficiency).
- Parts list with costs (editable per project), hoses and tubing included, CSV export.
- PDF report with title block (author, company, drawing number, revision, date, description).
- SVG, DXF (for CAD) and PNG export of a page.

## Getting started and fault finding

- 10 short getting-started tutorials with automatic checking of the task and saved progress.
- Hover help for every component. Animated cutaway view of every component (F7).
- Fault finding: 10 fault kinds (leak, blocked, stuck normal or operated, burnt
  coil, open or welded contact, sticking, jammed, low output) on parts and tubes. Measure, ask
  for hints, name the faulty part, get a result and a diagnosis log that can be saved. Faults can be planted and hidden in a file
  to prepare a task for a colleague.

## Components (45 types)

| Group | Components |
|---|---|
| Supply and air preparation | Compressed air supply, compressor, air receiver, shut-off valve, service unit, pressure regulator, pressure gauge, flow meter, force sensor, pressure switch, vacuum switch, silencer, tube junction |
| Actuators and handling | Single-acting cylinder, double-acting cylinder, semi-rotary actuator, air motor, parallel gripper, vacuum generator, suction cup |
| Directional control valves | 2/2, 3/2 (NC/NO), 4/2, 5/2, 5/3 (closed, exhaust or pressure centre); operated by push button, selector, roller lever, idle-return roller, pilot, time-delayed pilot or solenoid; spring, pilot or solenoid return |
| Logic, non-return, flow | Shuttle (OR), two-pressure (AND), check, quick exhaust, one-way flow control, flow control |
| Electrical (24 V DC) | +24 V / 0 V, push button NO/NC, selector, emergency stop (latching), relay contact NO/NC, proximity sensor, limit switch, pressure-switch contact, preset counter, relay coil, on-delay and off-delay timer relays, valve solenoid, lamp, wire junction |
| Hydraulics | Pump, tank, relief valve, pressure reducing valve, 4/3 valve (closed, tandem, float, open centre), 4/2 valve, check, one-way flow control, throttle, pressure-compensated flow control, counterbalance valve, double- and single-acting cylinder, motor, accumulator, gauge (0–160 bar) |
| Pressure control | Pressure regulator, pressure sequence valve |
| Drawing | Text notes, page connectors |

## Built-in examples

1. Direct control of a single-acting cylinder
2. Indirect control of a double-acting cylinder with speed control
3. OR: operation from two places (shuttle valve)
4. AND: two-hand safety control (two-pressure valve)
5. Automatic reciprocation with limit valves
6. Delayed return with a time delay valve
7. Electro-pneumatic: direct control with a solenoid valve
8. Electro-pneumatic: self-holding circuit with start / stop and lamp
9. Hydraulics: cylinder with 4/3 valve, relief valve and gauge
10. Realistic mode: pressure sequence valve (clamp, then drill)
11. Vacuum handling: ejector, suction cup and vacuum switch
12. Air generation: compressor, receiver, service unit and flow meter

The installer also shipped six generator outputs (cascade and relay chain for A+B+A-B-,
A+B+B-A- and A+B+C+C-B-A-). All 18 files are in `examples/bundled/`.

## Added in 3.3 (18 Nov 2026)
- Tandem, double-end (through-rod) and telescopic cylinders; DC motor, servo motor and electric cylinder; live value labels; PDF user guide (Help > Quick Guide).
