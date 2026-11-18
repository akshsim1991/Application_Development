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
        Console.WriteLine(failed == 0 ? "ALL PASSED" : failed + " FAILED");
        return failed;
    }
}
