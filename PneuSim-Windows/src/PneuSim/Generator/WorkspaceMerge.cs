using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace PneuSim;

/// <summary>
/// Lets the circuit generator build around components that are already in the workspace: the double-acting cylinders (and everything else
/// already there, with its connections) stay exactly as they are; the generator adds the valves, supplies, sensors and logic and connects them.
/// </summary>
public static class WorkspaceMerge
{
	private static readonly Regex Numbered = new Regex("^\\s*(\\d+)\\s*A\\s*$", RegexOptions.IgnoreCase);

	private static int NumberOf(CircuitElement e)
	{
		Match match = Numbered.Match(e.Label ?? "");
		return match.Success ? int.Parse(match.Groups[1].Value) : int.MaxValue;
	}

	/// <summary>The plain double-acting cylinders of a circuit, ordered by label (1A, 2A...) and then by position.</summary>
	public static List<DoubleActingCylinder> FindCylinders(Circuit workspace)
	{
		return (from c in workspace.Elements.OfType<DoubleActingCylinder>()
			where c.GetType() == typeof(DoubleActingCylinder)
			orderby NumberOf(c), c.X, c.Y
			select c).ToList();
	}

	/// <summary>Short text for the dialog: which workspace cylinders will play the roles of A, B, C...</summary>
	public static string Describe(Circuit workspace, MotionSequence seq)
	{
		List<DoubleActingCylinder> list = FindCylinders(workspace);
		StringBuilder stringBuilder = new StringBuilder();
		stringBuilder.AppendLine($"Building around the cylinders already in the workspace ({list.Count} found).");
		for (int i = 0; i < seq.Letters.Count; i++)
		{
			stringBuilder.AppendLine((i < list.Count) ? $"  Cylinder {seq.Letters[i]} = {(string.IsNullOrWhiteSpace(list[i].Label) ? "the unnamed cylinder" : list[i].Label)} (kept where it is, with its settings)" : $"  Cylinder {seq.Letters[i]} = a new cylinder (not enough cylinders in the workspace)");
		}
		stringBuilder.AppendLine();
		return stringBuilder.ToString();
	}

	/// <summary>Returns a copy of the workspace circuit with the generated parts added around its cylinders.</summary>
	public static GeneratedCircuit Merge(Circuit workspace, GeneratedCircuit generated)
	{
		Circuit circuit = Circuit.FromXml(workspace.ToXml());
		List<DoubleActingCylinder> list = FindCylinders(circuit);
		List<DoubleActingCylinder> list2 = (from c in generated.Circuit.Elements.OfType<DoubleActingCylinder>()
			orderby NumberOf(c)
			select c).ToList();
		Dictionary<CircuitElement, CircuitElement> map = new Dictionary<CircuitElement, CircuitElement>();
		int num = Math.Min(list.Count, list2.Count);
		for (int i = 0; i < num; i++)
		{
			map[list2[i]] = list[i];
		}
		if (num > 0)
		{
			float num2 = list[0].X - list2[0].X;
			float num3 = list[0].Y - list2[0].Y;
			foreach (CircuitElement element in generated.Circuit.Elements)
			{
				element.X += num2;
				element.Y += num3;
			}
			foreach (Tube tube in generated.Circuit.Tubes)
			{
				if (tube.Mid.HasValue)
				{
					tube.Mid = tube.Mid.Value + ((tube.MidAxis() == "X") ? num2 : num3);
				}
			}
		}
		StringBuilder stringBuilder = new StringBuilder();
		foreach (KeyValuePair<CircuitElement, CircuitElement> item in map)
		{
			DoubleActingCylinder doubleActingCylinder = (DoubleActingCylinder)item.Key;
			DoubleActingCylinder doubleActingCylinder2 = (DoubleActingCylinder)item.Value;
			doubleActingCylinder2.RetractedMark = doubleActingCylinder.RetractedMark;
			doubleActingCylinder2.ExtendedMark = doubleActingCylinder.ExtendedMark;
			if (string.IsNullOrWhiteSpace(doubleActingCylinder2.Label))
			{
				doubleActingCylinder2.Label = doubleActingCylinder.Label;
			}
			foreach (Port port in doubleActingCylinder2.Ports)
			{
				if (port.ConnectionCount > 0)
				{
					stringBuilder.AppendLine($"Note: port {port.Name} of {doubleActingCylinder2.Label} was already connected; the generated connection was added next to it. Delete the one you do not want.");
				}
			}
		}
		foreach (CircuitElement element2 in generated.Circuit.Elements)
		{
			if (!map.ContainsKey(element2))
			{
				circuit.Add(element2, element2.X, element2.Y);
			}
		}
		foreach (Tube tube2 in generated.Circuit.Tubes)
		{
			CircuitElement value;
			CircuitElement a = (map.TryGetValue(tube2.A.Owner, out value) ? value : tube2.A.Owner);
			CircuitElement value2;
			CircuitElement b = (map.TryGetValue(tube2.B.Owner, out value2) ? value2 : tube2.B.Owner);
			Tube tube3 = circuit.Connect(a.GetPort(tube2.A.Name), b.GetPort(tube2.B.Name));
			if (tube3 != null)
			{
				tube3.Mid = tube2.Mid;
			}
		}
		string text = ((num > 0) ? ("BUILT AROUND THE WORKSPACE\n" + string.Join("\n", map.Select((KeyValuePair<CircuitElement, CircuitElement> kv) => $"  {kv.Key.Label} = your cylinder {(string.IsNullOrWhiteSpace(kv.Value.Label) ? "(unnamed)" : kv.Value.Label)}")) + "\n" + stringBuilder.ToString() + "Everything that was already in the workspace was kept.\n\n") : "No cylinders were found in the workspace, so a new circuit was added.\n\n");
		return new GeneratedCircuit
		{
			Circuit = circuit,
			Explanation = text + generated.Explanation,
			MergedIntoWorkspace = true
		};
	}
}
