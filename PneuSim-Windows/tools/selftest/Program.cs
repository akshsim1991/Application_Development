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
        Console.WriteLine(failed == 0 ? "ALL PASSED" : failed + " FAILED");
        return failed;
    }
}
