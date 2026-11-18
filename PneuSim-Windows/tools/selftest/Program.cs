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
        Console.WriteLine(failed == 0 ? "ALL PASSED" : failed + " FAILED");
        return failed;
    }
}
