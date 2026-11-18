# Competitor review and feature ideas (18 Nov 2026)

Method: web search of vendor pages, documentation and review sites. Vendor pages are marketing; verify details
before relying on them. famictech.com could not be fetched (outside the session's allowed network), so Automation Studio
details come from search summaries only. **Use as feature inspiration only; never copy artwork, layouts, wording or circuits.**

## What the others offer (summary)
- **FluidSIM (Festo Didactic):** pneumatics + hydraulics + electrical libraries; fault simulation; GRAFCET creation and simulation
  (GrafEdit/GrafView/GrafControl, GrafPLC in v6); EasyPort hardware coupling and OPC-UA so it can control a real system or replace one;
  subscription licence (FluidSIM 365) with web, Windows and LX access.
- **Automation Studio (Famic):** fluid power + electrical design, simulation and documentation; ISO/DIN/IEC/NEMA/JIC symbol sets;
  animated components and colour-coded lines; instructor-defined failures; PID controller, CANbus and servo components; Pro: dynamic
  simulation, mechanical links, 3D, sizing sheets, training environment and workflow; Educational and Professional editions.
- **Simulink / Simscape Fluids:** physics-based block modelling of pumps, valves, actuators, pipelines and heat exchangers; isothermal liquid,
  thermal liquid, gas, two-phase and moist-air domains; fluid property tables; multidomain coupling with electrical and mechanical;
  C-code generation and hardware-in-the-loop deployment.
- **Simcenter Amesim (Siemens):** detailed hydraulic and pneumatic libraries chosen by available data; MiL/SiL/HiL and real-time;
  FMI 2.0/3.0 FMU export; scripting.
- **SimulationX (ESI/ITI):** mechanical, hydraulic, pneumatic, electrical and combined systems; more expensive.
- **CADe SIMU (free):** electrical, pneumatic and ladder/PLC logic simulator; dated look; last update 2021.

## Where PneuSim already matches or leads
Pneumatic + electro-pneumatic + hydraulic in one tool; live animated simulation; checker and plain-English explainer; sequence generator and
complete-my-circuit; fault finding; rated-pressure alarm and sounds; live value labels and hover values; servo, DC motor and electric cylinder;
whole-workspace cutaway; automatic operation; start screen, auto-update.

## Candidate features to add (ranked by value for effort)
1. **GRAFCET / sequence-chart view** generated from the circuit (and a displacement-step diagram link). Competitors treat GRAFCET as a headline feature.
2. **PLC and hardware coupling:** OPC-UA / Modbus TCP link so a real or soft PLC drives the simulation (virtual commissioning); IEC 61131-3 Structured Text and ladder export.
3. **Ladder diagram editor** for the electrical part (relay logic, timers, counters), kept in step with the circuit.
4. **Sizing sheets and component selection** (bore, tube, valve flow, pump, motor, accumulator); generic catalogue, no brand part numbers.
5. **Closed-loop control blocks:** PID, comparators, analogue sensors and proportional valves; servo with position control; CAN-style signals.
6. **Mechanical links and loads:** levers, linkages, pulleys, a vertical load with gravity, workpiece and conveyor animation; simple 2D machine view.
7. **Instructor-free fault libraries:** ready-made fault scenarios per example, fault statistics, automatic FMEA (already on the differentiator list).
8. **Fluid and thermal detail:** oil viscosity and temperature, pressure drops in tubes, air receiver sizing, compressor curves, energy and cost over time.
9. **Interoperability:** FMU export/import (FMI 2.0/3.0) so PneuSim models run inside other tools; scripting (Python) for batch runs; DXF/SVG already present.
10. **Documentation tooling:** circuit documentation packs with parts lists, symbol legends, test reports, maintenance checklists; multi-language UI.
11. **Libraries and symbol sets:** user-defined components and symbols, import/export of component libraries; NEMA/JIC symbol option as well as ISO.
12. **Collaboration:** circuit compare (diff), comments and sign-off, cloud sharing of projects (later).
13. **Realism options:** 3D component views and 3D machine models (Pro-style), VR later.
14. **Licensing models:** subscription plus perpetual option, annual maintenance, professional/lite editions.
