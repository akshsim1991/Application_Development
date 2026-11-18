# Audit before 3.4.0 (18 Nov 2026)

Method: automated checks (equivalence harness against 3.2.0, self-tests, CI GUI smoke test of the start-up and the examples) plus a code review of the new ribbon, update, routing and file-tab code, and a command-by-command comparison of the old menu/toolbar with the ribbon.

## Bugs found and removed in 3.4.0
| # | Bug | Fix |
|---|-----|-----|
| 1 | Ribbon search list showed `PneuSim.RibbonBar+Item` (data binding to a field) | Item.ToString, tooltip matching, ranking |
| 2 | Ribbon groups cut off at the window edge | Groups show fewer buttons plus "More" |
| 3 | Bottom panels and page tabs stayed visible on the start screen | Hidden until a circuit is open |
| 4 | Port names unreadable in dark mode (dark blue on dark) | Light blue in dark themes |
| 5 | Update check only once a day; an available update was easy to miss | Checked at every start, update link in the status bar, prompt on start |
| 6 | No way to update without clicking | Settings > automatic updates (background download, SHA-256 check, silent install when PneuSim closes) |
| 7 | Rotating a group drifted and overlapped | Exact rigid rotation; overlap resolver |
| 8 | Lines overlapped components and each other | Joint routing of all lines, crossover hops |
| 9 | Pneumatic counterbalance circuit lost its tubes | Counterbalance valve works with air |
| 10 | Pressure unit not followed in Properties/Inspector | Converted with the Settings unit |
| 11 | File view stayed open when another tab was chosen | Closes on any other ribbon use |
| 12 | Scroll bars light in dark mode | Dark scroll bar theme (Windows 10+) |
| 13 | Replay slider and Bottom-panel toggle missing from the ribbon layout | Added |

## Known limits (not bugs in behaviour, listed for the next session)
- Calculators and Sizing dialogs still use bar only (the unit setting does not change them).
- The router has been exercised on the example circuits and one Windows-only self-test; very crowded circuits may still need a hand-dragged line.
- Manifold is not yet inserted automatically by Complete my circuit.
- Not started: machine view (levers, conveyors), fluid and thermal detail.
