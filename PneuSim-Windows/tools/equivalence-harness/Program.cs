// Headless equivalence harness: loads a PneuSim.exe by reflection and prints a deterministic
// report of what the non-GUI engine does. Run once against the original and once against the
// rebuilt assembly, then diff the two reports.
using System.Collections;
using System.Reflection;
using System.Text;

class H
{
    static Assembly A;
    static Type T(string n) => A.GetType("PneuSim." + n, true);
    static object S(string type, string m, params object[] a) =>
        T(type).GetMethods(BindingFlags.Public | BindingFlags.Static).First(x => x.Name == m && x.GetParameters().Length == a.Length).Invoke(null, a);
    static object P(object o, string p) => o.GetType().GetProperty(p).GetValue(o);

    static string Issues(object list)
    {
        var sb = new StringBuilder();
        foreach (var i in (IEnumerable)list)
            sb.Append("    ").Append(P(i, "Severity")).Append(": ").Append(P(i, "Message")).Append('\n');
        return sb.ToString();
    }

    static void Report(StringBuilder o, string name, object circuit)
    {
        o.Append("== ").Append(name).Append('\n');
        var els = (IList)P(circuit, "Elements");
        o.Append("  elements: ").Append(els.Count).Append('\n');
        foreach (var step in new[] { "StaticChecks" })
        {
            try { o.Append("  static checks:\n").Append(Issues(S("CircuitAnalysis", "StaticChecks", circuit))); }
            catch (Exception e) { o.Append("  static checks threw ").Append((e.InnerException ?? e).GetType().Name).Append('\n'); }
        }
        foreach (bool real in new[] { false, true })
        {
            try { o.Append($"  dynamic check (realistic={real}):\n").Append(Issues(S("CircuitAnalysis", "DynamicCheck", circuit, real))); }
            catch (Exception e) { o.Append("  dynamic check threw ").Append((e.InnerException ?? e).GetType().Name + ": " + (e.InnerException ?? e).Message).Append('\n'); }
        }
        try { o.Append("  explain:\n").Append(S("CircuitAnalysis", "Explain", circuit, null, false)).Append('\n'); }
        catch (Exception e) { o.Append("  explain threw ").Append((e.InnerException ?? e).GetType().Name).Append('\n'); }
        try
        {
            var sim = Activator.CreateInstance(T("Simulator"), circuit);
            sim.GetType().GetMethod("Reset").Invoke(sim, null);
            for (int k = 0; k < 2000; k++) sim.GetType().GetMethod("Step").Invoke(sim, new object[] { 0.005 });
            o.Append("  10 s free run, state: ").Append(sim.GetType().GetMethod("DiscreteState").Invoke(sim, null)).Append('\n');
        }
        catch (Exception e) { o.Append("  simulate threw ").Append((e.InnerException ?? e).GetType().Name + ": " + (e.InnerException ?? e).Message).Append('\n'); }
        try { o.Append("  xml length: ").Append(((string)circuit.GetType().GetMethod("ToXml").Invoke(circuit, null)).Length).Append('\n'); }
        catch (Exception e) { o.Append("  toxml threw ").Append((e.InnerException ?? e).GetType().Name).Append('\n'); }
    }

    static int Main(string[] args)
    {
        A = Assembly.LoadFrom(Path.GetFullPath(args[0]));
        var o = new StringBuilder();
        // 1) Built-in examples
        var all = (Array)T("Examples").GetField("All").GetValue(null);
        foreach (var item in all.Cast<object>().Take(12)) // the 12 examples of 3.2.0; later examples are new and have no reference
        {
            var name = (string)item.GetType().GetField("Item1").GetValue(item);
            var build = (Delegate)item.GetType().GetField("Item2").GetValue(item);
            try { Report(o, "example: " + name, build.DynamicInvoke()); }
            catch (Exception e) { o.Append("== example ").Append(name).Append(" threw ").Append((e.InnerException ?? e).Message).Append('\n'); }
        }
        // 2) Circuit generator
        foreach (var seq in new[] { "A+ B+ B- A-", "A+ B+ A- B-", "A+ B+ C+ C- B- A-", "A+ (B+ C+) B- C- A-" })
            foreach (var method in Enum.GetValues(T("GeneratorMethod")))
                foreach (bool cont in new[] { false, true })
                {
                    try
                    {
                        var ms = S("MotionSequence", "Parse", seq);
                        var gc = S("SequenceGenerator", "Generate", ms, method, cont);
                        o.Append("-- generated explanation:\n").Append(P(gc, "Explanation")).Append('\n');
                        Report(o, $"generated {seq} {method} continuous={cont}", P(gc, "Circuit"));
                    }
                    catch (Exception e) { o.Append($"== generate {seq} {method} threw ").Append((e.InnerException ?? e).Message).Append('\n'); }
                }
        // 3) Bundled and user circuit files
        foreach (var f in args.Skip(1).OrderBy(x => x))
        {
            try
            {
                var txt = File.ReadAllText(f);
                if (txt.Contains("<PneuSimProject"))
                {
                    var proj = S("Project", "Load", f);
                    o.Append("== project file ").Append(Path.GetFileName(f)).Append(" xml length ").Append(((string)proj.GetType().GetMethod("ToXml").Invoke(proj, null)).Length).Append('\n');
                }
                else Report(o, "file: " + Path.GetFileName(f), S("Circuit", "Load", f));
            }
            catch (Exception e) { o.Append("== file ").Append(Path.GetFileName(f)).Append(" threw ").Append((e.InnerException ?? e).Message).Append('\n'); }
        }
        Console.Write(o.ToString());
        return 0;
    }
}
