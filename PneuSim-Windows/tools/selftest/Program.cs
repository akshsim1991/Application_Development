// Headless self-test of PneuSim features added in 3.3 (sounds, rated pressure, units).
// Usage: dotnet SelfTest.dll <PneuSim.exe>. Exit code 0 = all passed.
using System.Collections;
using System.Reflection;

class SelfTest
{
    static Assembly A;
    static int failed;
    static Type T(string n) => A.GetType("PneuSim." + n, true);
    static object P(object o, string p) => o.GetType().GetProperty(p).GetValue(o);
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "PASS " : "FAIL ") + what); if (!ok) failed++; }

    static int Main(string[] args)
    {
        A = Assembly.LoadFrom(Path.GetFullPath(args[0]));
        // Sounds: every sample finite and in range, loops non-empty.
        foreach (var n in new[] { "valve", "thud", "hiss", "hum", "relief", "alarm" })
        {
            var s = (float[])T("SoundEngine").GetMethod("Samples").Invoke(null, new object[] { n });
            Check(s != null && s.Length > 100 && s.All(x => !float.IsNaN(x) && Math.Abs(x) <= 1.5f), $"sound '{n}' synthesised ({s?.Length} samples)");
        }
        // Units
        var units = T("Units");
        var pa = T("AppSettings").GetProperty("PressureUnit");
        string U(string u, double v) { pa.SetValue(null, u); return (string)units.GetMethod("Pressure").Invoke(null, new object[] { v, "0.#" }); }
        Check(U("bar", 6) == "6 bar", "6 bar shown as bar");
        Check(U("MPa", 6) == "0.6 MPa", "6 bar shown as 0.6 MPa");
        Check(U("kPa", 6) == "600 kPa", "6 bar shown as 600 kPa");
        pa.SetValue(null, "bar");
        // Rated pressure: example at 6 bar is fine; raising the supply to 12 bar must raise the warning.
        var all = (Array)T("Examples").GetField("All").GetValue(null);
        object build = all.GetValue(0).GetType().GetField("Item2").GetValue(all.GetValue(0));
        for (int pass = 0; pass < 2; pass++)
        {
            var circuit = ((Delegate)build).DynamicInvoke();
            var els = (IList)P(circuit, "Elements");
            if (pass == 1)
                foreach (var e in els) if (e.GetType().Name == "AirSupply") e.GetType().GetProperty("Pressure").SetValue(e, 12.0);
            var sim = Activator.CreateInstance(T("Simulator"), circuit);
            sim.GetType().GetMethod("Reset").Invoke(sim, null);
            var mon = Activator.CreateInstance(T("SoundMonitor"));
            for (int k = 0; k < 400; k++)
            {
                sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 });
                if (k % 4 == 0) mon.GetType().GetMethod("Update").Invoke(mon, new object[] { circuit });
            }
            var text = (string)mon.GetType().GetMethod("WarningText").Invoke(mon, null);
            if (pass == 0) Check(text == null, "6 bar supply: no rated-pressure warning");
            else Check(text != null && text.Contains("Rated pressure exceeded"), "12 bar supply: warning raised -> " + text);
        }
        // Hidden-fault encryption
        var faults = T("Faults"); var kindT = T("FaultKind");
        var leak = Enum.Parse(kindT, Enum.GetNames(kindT).First(n => n != "None"));
        var prot = (string)faults.GetMethod("Protect").Invoke(null, new object[] { leak });
        Check(!prot.Contains(leak.ToString()) && Convert.FromBase64String(prot).Length > 48, "hidden fault is encrypted (not readable text)");
        Check(faults.GetMethod("Unprotect").Invoke(null, new object[] { prot }).Equals(leak), "hidden fault decrypts to the original");
        var bytes = Convert.FromBase64String(prot); bytes[20] ^= 1;
        Check(faults.GetMethod("Unprotect").Invoke(null, new object[] { Convert.ToBase64String(bytes) }).ToString() == "None", "tampered hidden fault is rejected");
        // Safe save / load
        var dir = Path.Combine(Path.GetTempPath(), "pneusim-selftest-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(dir);
        var file = Path.Combine(dir, "t.pneu");
        object NewProject() => Activator.CreateInstance(T("Project"), Activator.CreateInstance(T("Circuit")), null);
        var proj = NewProject();
        proj.GetType().GetMethod("Save").Invoke(proj, new object[] { file });
        proj.GetType().GetMethod("Save").Invoke(proj, new object[] { file });
        Check(File.Exists(file) && File.Exists(file + ".bak") && !File.Exists(file + ".tmp"), "save keeps a .bak copy and leaves no temp file");
        File.WriteAllText(file, "<PneuSimProject version=\"3\"><oops");
        try { T("Project").GetMethod("Load").Invoke(null, new object[] { file }); Check(false, "damaged file rejected"); }
        catch (System.Reflection.TargetInvocationException e) { Check(e.InnerException.Message.Contains("damaged") && e.InnerException.Message.Contains(".bak"), "damaged file gives a friendly message mentioning the backup"); }
        File.WriteAllText(file, "<PneuSimProject version=\"99\"></PneuSimProject>");
        try { T("Project").GetMethod("Load").Invoke(null, new object[] { file }); Check(false, "newer file rejected"); }
        catch (System.Reflection.TargetInvocationException e) { Check(e.InnerException.Message.Contains("newer version"), "file from a newer version gives a friendly message"); }
        Directory.Delete(dir, true);
        // Updater
        var us = T("UpdateService"); var um = T("UpdateManifest");
        bool Newer(string a, string b) => (bool)us.GetMethod("IsNewer").Invoke(null, new object[] { a, b });
        Check(Newer("3.3.1", "3.3.0") && Newer("3.10.0", "3.9.0") && !Newer("3.3.0", "3.3.0") && !Newer("3.2.0", "3.3.0") && !Newer("junk", "3.3.0"), "version comparison");
        var tmpf = Path.GetTempFileName(); File.WriteAllText(tmpf, "abc");
        var sha = (string)us.GetMethod("Sha256Of").Invoke(null, new object[] { tmpf });
        Check(sha == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 of a known file");
        var good = "{\"version\":\"3.4.0\",\"codename\":\"Sirocco\",\"released\":\"2027-01-05\",\"installerUrl\":\"https://example.org/x.exe\",\"sha256\":\"" + sha.ToUpper() + "\",\"notes\":\"n\"}";
        var man = um.GetMethod("Parse").Invoke(null, new object[] { good });
        Check(man != null && um.GetProperty("Sha256").GetValue(man).ToString() == sha, "update manifest parses (hash normalised)");
        Check((bool)us.GetMethod("Verify").Invoke(null, new object[] { tmpf, man }), "verify accepts the matching installer");
        File.WriteAllText(tmpf, "abd");
        Check(!(bool)us.GetMethod("Verify").Invoke(null, new object[] { tmpf, man }), "verify rejects a changed installer");
        File.Delete(tmpf);
        foreach (var bad in new[] { good.Replace("https://", "http://"), "{\"version\":\"x\"}", "not json" })
        {
            try { um.GetMethod("Parse").Invoke(null, new object[] { bad }); Check(false, "bad manifest rejected"); }
            catch (System.Reflection.TargetInvocationException) { Check(true, "bad manifest rejected"); }
        }
        // Physics invariants and speed on every bundled example (10 s of simulated time each)
        var sw = System.Diagnostics.Stopwatch.StartNew(); int steps = 0;
        foreach (var item in all)
        {
            var name = (string)item.GetType().GetField("Item1").GetValue(item);
            var circuit = ((Delegate)item.GetType().GetField("Item2").GetValue(item)).DynamicInvoke();
            var els = (IList)P(circuit, "Elements");
            double supply = 0;
            foreach (var e in els) if (e.GetType().Name == "AirSupply") supply = Math.Max(supply, (double)P(e, "Pressure"));
            foreach (var e in els) if (e.GetType().Name == "Compressor") supply = Math.Max(supply, (double)P(e, "CutOutPressure"));
            foreach (var e in els) if (e.GetType().Name == "HydraulicPump") supply = Math.Max(supply, (double)P(e, "MaxPressureBar"));
            var sim = Activator.CreateInstance(T("Simulator"), circuit);
            sim.GetType().GetMethod("Reset").Invoke(sim, null);
            bool ok = true; string why = "";
            for (int k = 0; k < 2000 && ok; k++)
            {
                sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 }); steps++;
                if (k % 100 != 99) continue;
                foreach (var e in els)
                {
                    foreach (var port in (IEnumerable)P(e, "Ports"))
                    {
                        if (P(port, "Kind").ToString() == "Electric") continue;
                        var pr = (double)P(port, "Pressure");
                        if (double.IsNaN(pr) || pr < -1.1 || pr > supply + 1.0) { ok = false; why = $"{e.GetType().Name} port pressure {pr}"; }
                    }
                    if (e.GetType().BaseType?.Name == "CylinderBase" || e.GetType().BaseType?.BaseType?.Name == "CylinderBase")
                    {
                        var pos = (double)P(e, "Position");
                        if (double.IsNaN(pos) || pos < -0.001 || pos > 1.001) { ok = false; why = $"cylinder position {pos}"; }
                    }
                }
            }
            Check(ok, $"physics invariants hold: {name} {why}");
        }
        sw.Stop();
        Console.WriteLine($"INFO {steps} simulation steps in {sw.ElapsedMilliseconds} ms ({(double)sw.ElapsedMilliseconds / Math.Max(1, steps):0.000} ms/step)");
        Check((double)sw.ElapsedMilliseconds / Math.Max(1, steps) < 5.0, "simulation speed under 5 ms per step");
        // User guide PDF
        var guidePath = Environment.GetEnvironmentVariable("PNEUSIM_GUIDE_OUT") ?? Path.Combine(Path.GetTempPath(), "pneusim-guide-selftest.pdf");
        try
        {
            T("UserGuide").GetMethod("Create").Invoke(null, new object[] { guidePath, true });
            var gb = File.ReadAllBytes(guidePath);
            Console.WriteLine("INFO symbol error: " + T("UserGuide").GetField("LastSymbolError").GetValue(null));
            Check(gb.Length > 20000 && System.Text.Encoding.ASCII.GetString(gb, 0, 5) == "%PDF-", $"user guide PDF created ({gb.Length} bytes)");
        }
        catch (Exception e) { Check(false, "user guide PDF created: " + (e.InnerException ?? e).Message); }
        // Live value labels
        {
            var circuit = ((Delegate)build).DynamicInvoke();
            var els = (IList)P(circuit, "Elements");
            var cyl = els.Cast<object>().First(e => e.GetType().Name.EndsWith("Cylinder"));
            Check(cyl.GetType().GetMethod("LiveValueText").Invoke(cyl, null) == null, "no live label by default");
            var xmlBefore = (string)circuit.GetType().GetMethod("ToXml").Invoke(circuit, null);
            Check(!xmlBefore.Contains("ValueLabel"), "empty live label is not saved (old files unchanged)");
            cyl.GetType().GetProperty("ValueLabel").SetValue(cyl, "Speed; Piston force");
            var txt = (string)cyl.GetType().GetMethod("LiveValueText").Invoke(cyl, null);
            Check(txt != null && txt.Contains("Speed:") && txt.Contains("Piston force:"), "live label shows chosen values -> " + txt?.Replace("\n", " | "));
            var xmlAfter = (string)circuit.GetType().GetMethod("ToXml").Invoke(circuit, null);
            Check(xmlAfter.Contains("ValueLabel"), "chosen live label is saved");
        }
        // New actuators
        {
            object Make(string n) => T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { n });
            object Port(object e, int i) => ((IList)P(e, "Ports"))[i];
            void SetP(object e, int i, double bar) => Port(e, i).GetType().GetProperty("Pressure").SetValue(Port(e, i), bar);
            void SetS(object e, int i, string st) => Port(e, i).GetType().GetProperty("State").SetValue(Port(e, i), Enum.Parse(T("PortState"), st));
            double Force(object e) => (double)e.GetType().GetMethod("PistonForce").Invoke(e, null);
            void SetPos(object e, double v) { var f = T("CylinderBase").GetField("_position", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance); f.SetValue(e, v); }
            var da = Make("DoubleActingCylinder"); var tandem = Make("TandemCylinder"); var tr = Make("ThroughRodCylinder"); var tele = Make("TelescopicCylinder");
            foreach (var c in new[] { da, tandem }) { SetP(c, 0, 6); SetP(c, 1, 0); }
            Check(Math.Abs(Force(tandem) / Force(da) - 2.0) < 0.01, $"tandem cylinder gives twice the force ({Force(da):0} N -> {Force(tandem):0} N)");
            SetP(tr, 0, 6); SetP(tr, 1, 0); var fOut = Force(tr);
            SetP(tr, 0, 0); SetP(tr, 1, 6); var fIn = Force(tr);
            Check(Math.Abs(fOut + fIn) < 1e-6 * Math.Abs(fOut) && fOut > 0, $"double-end cylinder pushes and pulls with equal force ({fOut:0} N / {fIn:0} N)");
            SetP(tele, 0, 6); SetP(tele, 1, 0);
            SetPos(tele, 0.05); var f1 = Force(tele); SetPos(tele, 0.5); var f2 = Force(tele); SetPos(tele, 0.95); var f3 = Force(tele);
            Check(f1 > f2 && f2 > f3 && f3 > 0, $"telescopic cylinder: force falls as smaller stages extend ({f1:0} > {f2:0} > {f3:0} N)");
            // DC motor
            var m = Make("DCMotor"); SetS(m, 0, "Pressurized"); SetS(m, 1, "Exhausted");
            for (int k = 0; k < 600; k++) m.GetType().GetMethod("UpdateDynamics").Invoke(m, new object[] { null, 0.005 });
            var rpm = (double)P(m, "Rpm");
            Check(rpm > 3600 && rpm < 3850, $"DC motor on 24 V reaches about 3750 rpm ({rpm:0} rpm)");
            SetS(m, 0, "Exhausted"); SetS(m, 1, "Pressurized");
            for (int k = 0; k < 1200; k++) m.GetType().GetMethod("UpdateDynamics").Invoke(m, new object[] { null, 0.005 });
            Check((double)P(m, "Rpm") < -3600, $"DC motor reverses with reversed polarity ({(double)P(m, "Rpm"):0} rpm)");
            SetS(m, 0, "Exhausted"); SetS(m, 1, "Exhausted");
            for (int k = 0; k < 2000; k++) m.GetType().GetMethod("UpdateDynamics").Invoke(m, new object[] { null, 0.005 });
            Check(Math.Abs((double)P(m, "Rpm")) < 1, "DC motor stops when the supply is removed");
            // Servo
            var sv = Make("ServoMotor"); sv.GetType().GetProperty("MarkP2").SetValue(sv, "atP2");
            void Servo(int select) { SetS(sv, 0, "Pressurized"); SetS(sv, 1, "Exhausted"); SetS(sv, 2, "Pressurized"); for (int i = 3; i <= 5; i++) SetS(sv, i, i - 2 == select ? "Pressurized" : "Exhausted"); }
            Servo(2);
            for (int k = 0; k < 800; k++) { sv.GetType().GetMethod("UpdateLogic").Invoke(sv, new object[] { null }); sv.GetType().GetMethod("UpdateDynamics").Invoke(sv, new object[] { null, 0.005 }); }
            var pos = (double)P(sv, "Position");
            Check(Math.Abs(pos - 180) <= 0.6 && (bool)P(sv, "InPosition"), $"servo goes to stored position P2 and reports in position ({pos:0.0} deg)");
            Check((bool)sv.GetType().GetMethod("IsSignalOn").Invoke(sv, new object[] { "atP2" }), "servo position mark switches on at P2");
            Servo(0);
            for (int k = 0; k < 1200; k++) { sv.GetType().GetMethod("UpdateLogic").Invoke(sv, new object[] { null }); sv.GetType().GetMethod("UpdateDynamics").Invoke(sv, new object[] { null, 0.005 }); }
            Check(Math.Abs((double)P(sv, "Position")) <= 0.6, "servo returns home with no position selected");
            // Electric cylinder
            var ec = Make("ElectricCylinder");
            SetS(ec, 0, "Pressurized"); SetS(ec, 1, "Exhausted"); SetS(ec, 2, "Pressurized"); SetS(ec, 3, "Exhausted"); SetS(ec, 4, "Exhausted"); SetS(ec, 5, "Pressurized");
            for (int k = 0; k < 800; k++) { ec.GetType().GetMethod("UpdateLogic").Invoke(ec, new object[] { null }); ec.GetType().GetMethod("UpdateDynamics").Invoke(ec, new object[] { null, 0.005 }); }
            Check(Math.Abs((double)P(ec, "Position") - 150) <= 0.3 && (bool)P(ec, "InPosition"), $"electric cylinder reaches P3 = 150 mm ({(double)P(ec, "Position"):0.0} mm)");
            // every new component can be created from the library, saved and described
            foreach (var n in new[] { "TandemCylinder", "ThroughRodCylinder", "HydraulicThroughRodCylinder", "TelescopicCylinder", "TelescopicSingleActingCylinder", "HydraulicTelescopicCylinder", "HydraulicTelescopicSingleActingCylinder", "DCMotor", "ServoMotor", "ElectricCylinder" })
            {
                var el = Make(n); var cc = Activator.CreateInstance(T("Circuit")); cc.GetType().GetMethod("Add").MakeGenericMethod(T("CircuitElement")).Invoke(cc, new object[] { el, 100f, 100f, null });
                var xml = (string)cc.GetType().GetMethod("ToXml").Invoke(cc, null);
                var back = cc.GetType().GetMethod("FromXml", new[] { typeof(string) })?.Invoke(null, new object[] { xml });
                Check(xml.Contains($"type=\"{n}\"") && back != null, $"{n}: saves and loads");
            }
        }
        // Servo wired to 24 V terminals and run through the real simulator
        {
            var c = Activator.CreateInstance(T("Circuit"));
            var add = c.GetType().GetMethod("Add").MakeGenericMethod(T("CircuitElement"));
            object NewEl(string n, float x, float y) { var e = T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { n }); add.Invoke(c, new object[] { e, x, y, null }); return e; }
            var servo = NewEl("ServoMotor", 300f, 100f);
            var conn = c.GetType().GetMethod("Connect", new[] { T("CircuitElement"), typeof(string), T("CircuitElement"), typeof(string) });
            object Term(string pol, float x, float y) { var t = NewEl("PowerTerminal", x, y); t.GetType().GetProperty("Polarity").SetValue(t, Enum.Parse(T("Polarity"), pol)); return t; }
            foreach (var (port, pol) in new[] { ("24V", "Plus24V"), ("0V", "Zero0V"), ("EN", "Plus24V"), ("P2", "Plus24V") })
            { var t = Term(pol, 100f, 100f + 40f * Array.IndexOf(new[] { "24V", "0V", "EN", "P2" }, port)); conn.Invoke(c, new object[] { t, "1", servo, port }); }
            var sim = Activator.CreateInstance(T("Simulator"), c);
            sim.GetType().GetMethod("Reset").Invoke(sim, null);
            for (int k = 0; k < 800; k++) sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 });
            var pos = (double)P(servo, "Position");
            Check(Math.Abs(pos - 180) <= 0.6 && (bool)P(servo, "InPosition"), $"servo wired to terminals runs in the real simulator ({pos:0.0} deg, in position)");
            var okPort = ((IList)P(servo, "Ports"))[6];
            Check(P(okPort, "State").ToString() == "Pressurized", "servo OK output is 24 V when in position");
        }
        // Circuit generator building around cylinders already in the workspace (needs Windows drawing code)
        if (!OperatingSystem.IsWindows()) Console.WriteLine("SKIP generator-around-workspace test (needs Windows)");
        else {
            var c = Activator.CreateInstance(T("Circuit"));
            var add = c.GetType().GetMethod("Add").MakeGenericMethod(T("CircuitElement"));
            var c1 = T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { "DoubleActingCylinder" });
            var c2 = T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { "DoubleActingCylinder" });
            c1.GetType().GetProperty("BoreMm").SetValue(c1, 63.0);
            add.Invoke(c, new object[] { c1, 500f, 400f, "1A" }); add.Invoke(c, new object[] { c2, 900f, 400f, "2A" });
            var seq = T("MotionSequence").GetMethod("Parse").Invoke(null, new object[] { "A+ B+ B- A-" });
            var gen = T("SequenceGenerator").GetMethod("Generate").Invoke(null, new object[] { seq, Enum.Parse(T("GeneratorMethod"), "ElectroRelayChain"), false });
            var merged = T("WorkspaceMerge").GetMethod("Merge").Invoke(null, new object[] { c, gen });
            Check((bool)P(merged, "MergedIntoWorkspace"), "workspace merge returns a merged circuit");
            var mc = P(merged, "Circuit"); var mels = ((IList)P(mc, "Elements")).Cast<object>().ToList();
            var cyl = mels.Where(e => e.GetType().Name == "DoubleActingCylinder").ToList();
            Check(cyl.Count == 2 && (float)P(cyl[0], "X") == 500f && (float)P(cyl[1], "X") == 900f && (double)P(cyl[0], "BoreMm") == 63.0, "existing cylinders keep their place and settings");
            Check(P(cyl[0], "ExtendedMark").ToString() != "" && mels.Count(e => e.GetType().Name == "Valve52") == 2, "generated valves added, cylinder marks set");
            Check(((IList)P(mc, "Tubes")).Count >= 8, $"generated connections added ({((IList)P(mc, "Tubes")).Count} tubes)");
            var issues = (IEnumerable)T("CircuitAnalysis").GetMethods().First(m => m.Name == "StaticChecks").Invoke(null, new object[] { mc });
            Check(!issues.Cast<object>().Any(i => P(i, "Severity").ToString() == "Error"), "merged circuit has no checker errors");
            var original = ((IList)P(c, "Elements")).Count;
            Check(original == 2, "the original workspace is not modified by the preview");
        }
        // Automatic operation presses the start button for you
        foreach (var idx in new[] { 0, 6, 4 })
        {
            var item = all.GetValue(idx);
            var name = (string)item.GetType().GetField("Item1").GetValue(item);
            var circuit = ((Delegate)item.GetType().GetField("Item2").GetValue(item)).DynamicInvoke();
            var sim = Activator.CreateInstance(T("Simulator"), circuit);
            sim.GetType().GetMethod("Reset").Invoke(sim, null);
            var auto = Activator.CreateInstance(T("AutoOperator"), circuit, "Once");
            var cyls = ((IList)P(circuit, "Elements")).Cast<object>().Where(e => e.GetType().BaseType?.Name == "CylinderBase" || e.GetType().BaseType?.BaseType?.Name == "CylinderBase").ToList();
            double max = 0; double last = 1;
            for (int k = 0; k < 2000; k++)
            {
                for (int j = 0; j < 4; j++) sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 });
                var t = (double)P(sim, "Time");
                if ((bool)auto.GetType().GetMethod("Update").Invoke(auto, new object[] { t })) sim.GetType().GetMethod("RunLogic").Invoke(sim, null);
                foreach (var c in cyls) { var pos = (double)P(c, "Position"); max = Math.Max(max, pos); last = pos; }
            }
            Check(max > 0.97, $"automatic operation moves the cylinder without clicking: {name} (max position {max:0.00}, end {last:0.00})");
        }
        // "Complete my circuit": connect the components placed in the workspace
        {
            object NewEl(object c, string n, float x, float y) { var e = T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { n }); c.GetType().GetMethod("Add").MakeGenericMethod(T("CircuitElement")).Invoke(c, new object[] { e, x, y, null }); return e; }
            (double max, double last) Run(object circ)
            {
                var sim = Activator.CreateInstance(T("Simulator"), circ); sim.GetType().GetMethod("Reset").Invoke(sim, null);
                var auto = Activator.CreateInstance(T("AutoOperator"), circ, "Once");
                var cs = ((IList)P(circ, "Elements")).Cast<object>().Where(e => e.GetType().BaseType?.Name == "CylinderBase" || e.GetType().BaseType?.BaseType?.Name == "CylinderBase").ToList();
                double mx = 0, ls = 1;
                for (int k = 0; k < 1500; k++)
                {
                    for (int j = 0; j < 4; j++) sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 });
                    if ((bool)auto.GetType().GetMethod("Update").Invoke(auto, new object[] { (double)P(sim, "Time") })) sim.GetType().GetMethod("RunLogic").Invoke(sim, null);
                    foreach (var c2 in cs) { var pos = (double)P(c2, "Position"); mx = Math.Max(mx, pos); ls = pos; }
                }
                return (mx, ls);
            }
            // 1) supply + push-button valve + single-acting cylinder, nothing connected (the owner's example)
            var ws = Activator.CreateInstance(T("Circuit"));
            NewEl(ws, "SingleActingCylinder", 110f, 240f); NewEl(ws, "Valve32", 60f, 420f); NewEl(ws, "AirSupply", 120f, 570f);
            var v = ((IList)P(ws, "Elements")).Cast<object>().First(e => e.GetType().Name == "Valve32"); v.GetType().GetProperty("Actuator").SetValue(v, Enum.Parse(T("ValveActuator"), "PushButton"));
            var comp = T("CircuitCompleter").GetMethod("Complete").Invoke(null, new object[] { ws });
            var cc = P(comp, "Circuit");
            Check(((IList)P(cc, "Tubes")).Count == 2, $"complete: supply, valve and cylinder get connected ({((IList)P(cc, "Tubes")).Count} tubes)");
            var r1 = Run(cc);
            Check(r1.max > 0.97, $"complete: the completed circuit works by itself (max position {r1.max:0.00}, end {r1.last:0.00})");
            Check(((IList)P(ws, "Tubes")).Count == 0, "complete: the original workspace is untouched until the user accepts");
            // 2) only a double-acting cylinder: valve and supply are added
            var ws2 = Activator.CreateInstance(T("Circuit")); NewEl(ws2, "DoubleActingCylinder", 300f, 100f);
            var comp2 = T("CircuitCompleter").GetMethod("Complete").Invoke(null, new object[] { ws2 }); var c2c = P(comp2, "Circuit");
            var names = ((IList)P(c2c, "Elements")).Cast<object>().Select(e => e.GetType().Name).ToList();
            Check(names.Contains("Valve52") && names.Contains("AirSupply") && ((IList)P(c2c, "Tubes")).Count == 3, $"complete: a lone cylinder gets a 5/2 valve and a supply ({string.Join(", ", names)})");
            var r2 = Run(c2c); Check(r2.max > 0.97, $"complete: the added valve circuit moves the cylinder (max {r2.max:0.00})");
            // 3) electro-pneumatic: solenoid valve placed, nothing else
            var ws3 = Activator.CreateInstance(T("Circuit")); NewEl(ws3, "SingleActingCylinder", 300f, 100f);
            var sv = NewEl(ws3, "Valve32", 250f, 300f); sv.GetType().GetProperty("Actuator").SetValue(sv, Enum.Parse(T("ValveActuator"), "Solenoid"));
            NewEl(ws3, "PowerTerminal", 700f, 100f);
            var comp3 = T("CircuitCompleter").GetMethod("Complete").Invoke(null, new object[] { ws3 }); var c3c = P(comp3, "Circuit");
            var r3 = Run(c3c); Check(r3.max > 0.97, $"complete: electro-pneumatic circuit gets coil, push button and 24 V and works (max {r3.max:0.00})");
        }
        // Solenoid valves with no coil: automatic operation works their override (single and double solenoid), manual leaves them alone
        {
            object NewEl2(object c, string n, float x, float y) { var e = T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { n }); c.GetType().GetMethod("Add").MakeGenericMethod(T("CircuitElement")).Invoke(c, new object[] { e, x, y, null }); return e; }
            var conn2 = T("Circuit").GetMethod("Connect", new[] { T("CircuitElement"), typeof(string), T("CircuitElement"), typeof(string) });
            var asProp = T("AppSettings").GetProperty("AutoSolenoids");
            foreach (var (valveType, act, ret, cylType, vPorts) in new[] { ("Valve32", "Solenoid", "Spring", "SingleActingCylinder", new[] { ("2", "1") }), ("Valve52", "Solenoid", "Solenoid", "DoubleActingCylinder", new[] { ("4", "1"), ("2", "2") }) })
            {
                foreach (bool auto in new[] { true, false })
                {
                    asProp.SetValue(null, auto);
                    var c = Activator.CreateInstance(T("Circuit"));
                    var cyl = NewEl2(c, cylType, 300f, 100f); var valve = NewEl2(c, valveType, 250f, 300f); var sup = NewEl2(c, "AirSupply", 260f, 450f);
                    valve.GetType().GetProperty("Actuator").SetValue(valve, Enum.Parse(T("ValveActuator"), act)); valve.GetType().GetProperty("ReturnType").SetValue(valve, Enum.Parse(T("ValveReturn"), ret));
                    conn2.Invoke(c, new object[] { sup, "1", valve, "1" });
                    foreach (var (vp, cp) in vPorts) conn2.Invoke(c, new object[] { valve, vp, cyl, cp });
                    var sim = Activator.CreateInstance(T("Simulator"), c); sim.GetType().GetMethod("Reset").Invoke(sim, null);
                    var op = Activator.CreateInstance(T("AutoOperator"), c, "Once");
                    double mx = 0, ls = 1;
                    for (int k = 0; k < 1500; k++)
                    {
                        for (int j = 0; j < 4; j++) sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 });
                        if ((bool)op.GetType().GetMethod("Update").Invoke(op, new object[] { (double)P(sim, "Time") })) sim.GetType().GetMethod("RunLogic").Invoke(sim, null);
                        var pos = (double)P(cyl, "Position"); mx = Math.Max(mx, pos); ls = pos;
                    }
                    if (auto) Check(mx > 0.97 && ls < 0.1, $"solenoid automatic ({valveType}, {ret}): cylinder extends and returns by itself (max {mx:0.00}, end {ls:0.00})");
                    else Check(mx < 0.01, $"solenoid manual ({valveType}): nothing moves until the user clicks (max {mx:0.00})");
                }
            }
            asProp.SetValue(null, true);
        }
        if (Environment.GetEnvironmentVariable("PORTNAMES") != null) foreach (var n in new[] { "HydraulicValve43", "HydraulicValve42", "HydraulicCylinder", "HydraulicSingleActingCylinder", "HydraulicPump", "HydraulicTank", "ReliefValve", "Valve52", "Valve32", "Valve42", "Valve22", "FlowControlValve", "AirReceiver", "ShuttleValve", "TwoPressureValve", "ShutOffValve", "PressureRegulator", "PressureGauge", "Silencer", "SemiRotaryActuator", "Gripper" })
        {
            var e = T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { n });
            Console.WriteLine($"PORTS {n}: " + string.Join(" ", ((IList)P(e, "Ports")).Cast<object>().Select(p => P(p, "Name") + "/" + P(p, "Kind") + (P(p, "VentsWhenOpen").Equals(true) ? "(vent)" : ""))) + "  working: " + (e.GetType().GetMethod("WorkingPortNames") != null ? string.Join(",", (string[])e.GetType().GetMethod("WorkingPortNames").Invoke(e, null)) : "-"));
        }
        // Complete: one valve drives two cylinders; OR logic with a shuttle valve; hydraulics
        {
            object NE(object c, string n, float x, float y) { var e = T("ElementFactory").GetMethod("Create").Invoke(null, new object[] { n }); c.GetType().GetMethod("Add").MakeGenericMethod(T("CircuitElement")).Invoke(c, new object[] { e, x, y, null }); return e; }
            (double max, double last) Run2(object circ, int cyls)
            {
                var sim = Activator.CreateInstance(T("Simulator"), circ); sim.GetType().GetMethod("Reset").Invoke(sim, null);
                var auto = Activator.CreateInstance(T("AutoOperator"), circ, "Once");
                var cs = ((IList)P(circ, "Elements")).Cast<object>().Where(e => e.GetType().Name.EndsWith("Cylinder")).ToList();
                double mx = 1, ls = 0;
                for (int k = 0; k < 1500; k++)
                {
                    for (int j = 0; j < 4; j++) sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 });
                    if ((bool)auto.GetType().GetMethod("Update").Invoke(auto, new object[] { (double)P(sim, "Time") })) sim.GetType().GetMethod("RunLogic").Invoke(sim, null);
                    var mn = cs.Min(c => (double)P(c, "Position")); if (k == 0) mx = 0; mx = Math.Max(mx, mn); ls = mn;
                }
                return (mx, ls);
            }
            // A) two single-acting cylinders, one push-button valve, one supply -> no extra valve
            var wa = Activator.CreateInstance(T("Circuit"));
            NE(wa, "SingleActingCylinder", 110f, 100f); NE(wa, "SingleActingCylinder", 460f, 100f);
            var va = NE(wa, "Valve32", 60f, 300f); va.GetType().GetProperty("Actuator").SetValue(va, Enum.Parse(T("ValveActuator"), "PushButton")); NE(wa, "AirSupply", 120f, 450f);
            var ca = P(T("CircuitCompleter").GetMethod("Complete").Invoke(null, new object[] { wa }), "Circuit");
            var valvesA = ((IList)P(ca, "Elements")).Cast<object>().Count(e => e.GetType().Name == "Valve32");
            Check(valvesA == 1 && ((IList)P(ca, "Tubes")).Count == 3, $"complete: one valve drives both cylinders, no extra valve ({valvesA} valve, {((IList)P(ca, "Tubes")).Count} tubes)");
            var rA = Run2(ca, 2); Check(rA.max > 0.97, $"complete: both cylinders move together (min position max {rA.max:0.00})");
            // B) OR control: cylinder, shuttle valve, two push-button valves, supply
            var wb = Activator.CreateInstance(T("Circuit"));
            NE(wb, "SingleActingCylinder", 230f, 60f); NE(wb, "ShuttleValve", 200f, 200f);
            foreach (var x in new[] { 60f, 400f }) { var vb = NE(wb, "Valve32", x, 300f); vb.GetType().GetProperty("Actuator").SetValue(vb, Enum.Parse(T("ValveActuator"), "PushButton")); }
            NE(wb, "AirSupply", 250f, 450f);
            var cb = P(T("CircuitCompleter").GetMethod("Complete").Invoke(null, new object[] { wb }), "Circuit");
            Check(((IList)P(cb, "Tubes")).Count == 5, $"complete: shuttle valve gets both inputs and drives the cylinder ({((IList)P(cb, "Tubes")).Count} tubes)");
            var rB = Run2(cb, 1); Check(rB.max > 0.97, $"complete: the OR circuit works (max {rB.max:0.00})");
            // C) hydraulic cylinder alone
            var wc = Activator.CreateInstance(T("Circuit")); NE(wc, "HydraulicCylinder", 300f, 100f);
            var cc = P(T("CircuitCompleter").GetMethod("Complete").Invoke(null, new object[] { wc }), "Circuit");
            var namesC = ((IList)P(cc, "Elements")).Cast<object>().Select(e => e.GetType().Name).ToList();
            Check(namesC.Contains("HydraulicValve43") && namesC.Contains("HydraulicPump") && namesC.Contains("HydraulicTank") && namesC.Contains("ReliefValve"), $"complete: hydraulic cylinder gets valve, pump, tank, relief valve ({((IList)P(cc, "Tubes")).Count} tubes)");
        }
        // Sizing sheets (generic standard sizes)
        {
            var sz = T("Sizing");
            double Call(string name, params object[] a) => (double)sz.GetMethod(name).Invoke(null, a);
            Check(Call("RecommendedBoreMm", 1000.0, 6.0, 0.7) == 63.0, "sizing: 1000 N at 6 bar needs a 63 mm cylinder");
            Check(Math.Abs(Call("Force", 63.0, 20.0, 6.0, 0.7, false) - 1309.0) < 5, $"sizing: force of a 63 mm cylinder at 6 bar ({Call("Force", 63.0, 20.0, 6.0, 0.7, false):0} N)");
            Check(Math.Abs(Call("MotorPowerKw", 20.0, 160.0, 0.85) - 6.27) < 0.05, $"sizing: 20 l/min at 160 bar needs about 6.3 kW ({Call("MotorPowerKw", 20.0, 160.0, 0.85):0.00} kW)");
            Check(Call("AccumulatorLitres", 1.0, 80.0, 100.0, 200.0, 1.4) > 3.0 && Call("AccumulatorLitres", 1.0, 80.0, 100.0, 200.0, 1.4) < 12.0, $"sizing: accumulator for 1 l between 100 and 200 bar ({Call("AccumulatorLitres", 1.0, 80.0, 100.0, 200.0, 1.4):0.0} l)");
            Check(Call("RequiredAirTubeIdMm", 1000.0, 6.0, 15.0) > 12.0 && Call("RequiredAirTubeIdMm", 1000.0, 6.0, 15.0) < 16.0, $"sizing: tube for 1000 NL/min ({Call("RequiredAirTubeIdMm", 1000.0, 6.0, 15.0):0.0} mm inner)");
        }
        // GRAFCET generated from the circuit, and the step tracker
        {
            var gb = T("GrafcetBuilder"); int built = 0; string firstText = null;
            foreach (var item in all)
            {
                var name = (string)item.GetType().GetField("Item1").GetValue(item);
                var circuit = ((Delegate)item.GetType().GetField("Item2").GetValue(item)).DynamicInvoke();
                try
                {
                    var model = gb.GetMethod("Build").Invoke(null, new object[] { circuit });
                    var gSteps = (IList)model.GetType().GetField("Steps").GetValue(model);
                    if (gSteps.Count >= 2) { built++; firstText ??= (string)model.GetType().GetMethod("ToText").Invoke(model, null); }
                    Console.WriteLine($"INFO GRAFCET {name}: {gSteps.Count} steps");
                }
                catch (Exception e) { Console.WriteLine("INFO GRAFCET " + name + " threw " + (e.InnerException ?? e).GetType().Name + " " + ((e.InnerException ?? e).StackTrace ?? "").Split((char)10)[0]); }
            }
            Check(built >= 3, $"GRAFCET: a chart is generated for {built} bundled examples");
        }
        Console.WriteLine(failed == 0 ? "ALL PASSED" : failed + " FAILED");
        return failed;
    }
}
