# Candidate commercial differentiators (written 18 Nov 2026)

**Important:** nobody can promise that no competitor has a feature "to date": products change and I
cannot check every vendor's current release. Treat this as a ranked shortlist of features that build
on what PneuSim already has (fault engine, checker, explainer, generator, cost model) and that are
rarely offered together. **Before marketing any feature as "first" or "only", do a dated market check
and keep the evidence.** Rank = value for paying customers / effort.

| # | Feature | Why customers pay | Builds on | Effort |
|---|---|---|---|---|
| 1 | **Automatic FMEA** - inject every possible fault one at a time, simulate, classify the effect (no effect / downtime / unsafe motion / pressure over rating) and export an FMEA table | Maintenance and safety teams must produce this by hand today | Fault engine (10 fault kinds), rated-pressure alarm | Medium |
| 2 | **Safe-restart and air-loss analysis** - one click simulates air loss, E-stop and pressure restore; reports any unexpected motion, drop of a load or restart without command | Directly addresses machine-safety reviews | Simulator, E-stop contact, checker | Medium |
| 3 | **Circuit tests** - define input sequences and expected outputs (pass/fail, "every valve state exercised"), run automatically after each edit | Treats a circuit like tested software; catches regressions when circuits are modified | Simulator, recorder, harness code | Medium |
| 4 | **Interactive "Why?"** - click any part during simulation for a plain-English chain of cause ("cylinder 1A not moving because 1V1 is not switched because K1 is off because ...") | Lay users and fault-finders get answers, not just animation | Explainer, signal tracing | Medium |
| 5 | **Energy and leak audit** - air use in kWh, cost and CO2e, leak cost, what-if at lower pressure, advice (regulator, shorter tube, smaller bore) | Hard money savings; sustainability reporting | Air-consumption and cost model | Low-Medium |
| 6 | **Sizing assistant** - enter force, stroke, cycle time; get bore, tube size and valve flow, using generic sizes | Removes catalogue lookups for non-experts | Calculators, physics | Medium |
| 7 | **Target cycle-time solver** - automatically tune flow controls and settings to hit a cycle time | Saves trial and error | Parameter sweep | Low |
| 8 | **Calibrate from measurements** - import a pressure/position CSV and fit friction, leakage and volume so the model matches the real machine | Turns the simulator into a digital twin of the plant | Plotter, sweep | Medium-High |
| 9 | **Circuit diff and review** - visual comparison of two versions, change list, comments, sign-off page in the PDF | Engineering change control | Project XML | Medium |
| 10 | **One-click dossier** - diagrams, step diagram, parts and costs, tests, FMEA, maintenance checklist in one PDF | Customer-ready deliverable | Reports, PDF module | Low-Medium |
| 11 | **Neutral PLC export** - IEC 61131-3 Structured Text and an I/O list from the electrical part | Bridges design to commissioning | Electrical model | Medium-High |
| 12 | **Pattern library with intent search** ("two-hand control", "soft start", "hold on air loss") | Speeds design for non-experts | Ribbon search, generator | Low-Medium |
| 13 | **Opt-in AI assistant** - describe the machine in words, get a draft circuit and explanations (needs internet, off by default, no data stored) | Large perceived value | Generator, checker | High (privacy and cost) |

## Recommended first three for 3.4 (best value for effort, strongest story)
1 Automatic FMEA, 2 Safe-restart analysis, 3 Circuit tests. They reuse the fault engine and simulator
and speak directly to maintenance and safety teams, who are the paying customers.

## Not recommended as "unique"
Hardware coupling to a real PLC (OPC UA / Modbus), 3D machine views and cloud collaboration exist in
other products in some form; they are worth building only if customers ask.

Safety note: any safety feature (items 1-3) must be described as **design guidance, not a certification**.
