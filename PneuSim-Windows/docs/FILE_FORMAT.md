# The `.pneu` file format

`.pneu` files are UTF-8 XML (written with a byte-order mark) and come in two forms.
`Project.Load` reads both; `Circuit.Load` reads a single circuit.

## 1. Project file (version 3, current)

```xml
<PneuSimProject version="3">
  <Info Title="…" Author="…" Company="…" DrawingNumber="…" Revision="A" DateText="09-10-2026"
        Description="…" AirCostPerM3="2" CyclesPerMinute="2" HoursPerDay="8" DaysPerYear="300" />
  <PartCosts>
    <Part name="Valve52" cost="2450" />          <!-- optional per-project price overrides, ₹ -->
  </PartCosts>
  <Page name="Page 1">
    <PneuSimCircuit version="2"> … </PneuSimCircuit>
  </Page>
  <Page name="Page 2"> … </Page>
</PneuSimProject>
```

`Info` is the title block, and its cost fields feed the ₹-per-year running cost.

## 2. Circuit (version 2)

```xml
<PneuSimCircuit version="2">
  <Element type="DoubleActingCylinder" id="3" x="300" y="440" rotation="3">
    <Property name="BoreMm" value="50" />
    <Property name="RodMm" value="20" />
    <Property name="StrokeLength" value="200" />
    <Property name="Label" value="1A" />
    …
  </Element>
  <Tube from="81" fromPort="A2" to="82" toPort="1" />
  <Tube from="84" fromPort="2" to="85" toPort="A1" mid="930" />
</PneuSimCircuit>
```

| Item | Meaning |
|---|---|
| `Element/@type` | Class name, resolved by `ElementFactory.Create` (e.g. `Valve52`, `ElectricCoil`, `HydraulicValve43`) |
| `@id` | Unique within the circuit; tubes refer to it |
| `@x`, `@y` | Position on the sheet (invariant culture) |
| `@rotation` | Quarter turns, 0–3 |
| `Property` | Any public writable property of the element, set by reflection. Enums are written by name. Unknown names and bad values are skipped, so old files still open |
| `Tube` | Joins port `fromPort` of element `from` to port `toPort` of element `to`. `mid` is the position of the movable middle segment |
| `@fault` / `@hf` | Optional fault on an element or tube. `fault="Leak"` is visible; a *hidden* fault is written as `hf="…"` (text reversed, then Base64-encoded), so it is not readable by eye but is not secure |

Port names follow ISO 5599 / ISO 1219 numbering: `1` supply, `2`/`4` outputs, `3`/`5`
exhausts, `12`/`14` pilots. Coils use `A1`/`A2`; electrical contacts use `1`/`2`.

Examples of both forms are in `examples/`.
