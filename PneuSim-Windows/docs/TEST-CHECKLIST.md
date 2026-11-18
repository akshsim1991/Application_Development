# Manual test checklist (Windows, about 60-90 minutes)

Tick each line; write the PneuSim version and the date at the top of your copy. Test light, dark and AMOLED themes where marked (T).

## Start and files
- [ ] Start PneuSim: start screen shows (T); Esc/New/Open/examples work; recent files listed
- [ ] File tab: New, Open, Recent, Save, Save As, Close, Export (5 kinds), Information, Options, Exit; another tab closes the File view
- [ ] Open two files: file tabs, switch, close with x and middle-click; unsaved star; closing asks to save
- [ ] Close the last circuit: only the start screen is visible
- [ ] Save, then damage the file: friendly message; .bak copy exists
## Drawing
- [ ] Place each tab's first component; Shift+click places several; Esc stops
- [ ] Connect port to port; air to wire refused; manifold and junction with 3+ lines
- [ ] Rotate one part and a whole selection (Select all, R): nothing overlaps, lines stay connected and tidy
- [ ] Undo and redo ten steps; copy, paste, duplicate; delete
- [ ] Pages: + adds, rename, delete, next/previous; page connector joins pages
## Simulation
- [ ] Start/Pause/Stop; speed 0.1x to 4x; Realistic on/off; Record GIF
- [ ] Replay slider and step/event buttons
- [ ] Point at a part: live values; right-click: Inspector; value labels on/off
- [ ] Plotter cursors, trigger, CSV and PNG export; displacement-step diagram; GRAFCET
## Tools
- [ ] Circuit generator (sequence and Complete my circuit); Check; Explain
- [ ] Calculators; Sizing sheets; Parts list; Parameter sweep; Cutaway (F7, Shift+F7)
- [ ] PDF print preview, Save as PDF, Print; SVG, DXF, PNG, CSV; open the Quick Guide (F1)
- [ ] Fault finding: new task, hint, I found, show, add fault, repair
## Settings (every option: change, OK, reopen, restart)
- [ ] Theme (4), pressure unit, currency, advanced mode, start screen, autosave minutes
- [ ] Automatic operation (3), timings, solenoid mode, speed at start-up, realistic default
- [ ] Value labels, hover values, snap, grid, port names (4), classic menu
- [ ] Sounds master/volume/each; Cancel discards; update options
## Ribbon and screen
- [ ] Every tab (T): no cut-off buttons at 1366 and 1920 width, More menus, tooltips, collapse (Ctrl+F1)
- [ ] Search box finds commands and components; Enter runs the first one
- [ ] Scroll bars, touchpad sideways scroll, Pan, Fit, zoom, grid on/off (T)
## Updates
- [ ] Start with network and a newer version published: prompt and status link; Later keeps working
- [ ] Update now: download, checksum, work saved, installer, PneuSim starts again
- [ ] Automatic updates on: close PneuSim, the new version is installed silently
- [ ] No network: no message, no delay
## Reliability
- [ ] Kill PneuSim during work: autosave offered on next start
- [ ] Leave a repeating circuit running for 8 hours: memory and CPU stay flat
