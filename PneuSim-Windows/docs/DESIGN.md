# PneuSim visual identity (own design)

Rules: all icons are drawn in code in one consistent line-art style (2 px strokes, rounded
caps, 24/32/48 px grid); own colour palette (light, dark, follow-Windows); own layout and
wording. No copied artwork, text, palettes, screenshots, example circuits or terminology from
other products or standards documents. Ribbon is custom-built with PneuSim's own tab names
(Supply & Air, Actuators, Valves, Logic & Flow, Electrical, Hydraulics, Drawing, Tools), not
the Office look. Fonts: Segoe UI (system). Status: palette and icon grid to be defined in M1.

## Rules added 18 Nov 2026
- **No properties pop-up.** Double-clicking a component must never open a properties window.
  All properties live in the side **Properties** panel (right of the drawing), as in 3.2.
- Ribbon: own custom-painted control (`UI/RibbonBar.cs`), tabs Home, Supply & Air, Actuators,
  Valves, Logic & Flow, Electrical, Hydraulics, Drawing, Tools; long groups get a "More" drop-down;
  search box top right; click or drag a symbol onto the drawing. Command icons are drawn in
  code (`UI/RibbonIcons.cs`). The left component library panel is hidden (collapsed).
