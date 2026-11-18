namespace PneuSim;

/// <summary>Port names shown in the Workspace: ISO numbers (1, 2, 3…), letters (P, A, B, E…) or both, chosen in Settings.</summary>
public static class PortLabels
{
	public static string Letter(Port p)
	{
		string n = p.Name;
		CircuitElement o = p.Owner;
		if (p.Kind == PortKind.Electric || n.Length == 0 || !char.IsDigit(n[0]))
		{
			return n;
		}
		bool hyd = p.Kind == PortKind.Hydraulic;
		if (o is Valve22)
		{
			return n == "1" ? "P" : "A";
		}
		if (o is DirectionalValve)
		{
			switch (n)
			{
			case "1": return "P";
			case "2": return (o is Valve32) ? "A" : "B";
			case "4": return "A";
			case "3": return hyd ? "T" : ((o is Valve52 || o is Valve53) ? "E1" : "E");
			case "5": return "E2";
			case "12": return "Y";
			case "14": return "Z";
			case "10": return "Z";
			}
			return n;
		}
		if (o is Silencer)
		{
			return "E";
		}
		if (o is QuickExhaustValve)
		{
			return n == "1" ? "P" : (n == "2" ? "A" : "E");
		}
		if (o is CylinderBase)
		{
			return n == "1" ? "A" : "B";
		}
		if (o is Compressor)
		{
			return "C";
		}
		if (o is AirSupply)
		{
			return "P";
		}
		if (o is AirReceiver)
		{
			return "T" + n;
		}
		return n == "1" ? "A" : (n == "2" ? "B" : (n == "3" ? "C" : n));
	}

	public static string Text(Port p)
	{
		string mode = AppSettings.PortLabelMode;
		string l = Letter(p);
		switch (mode)
		{
		case "Numbers":
			return p.Name;
		case "Both":
			return (l == p.Name) ? l : (l + " (" + p.Name + ")");
		default:
			return l;
		}
	}
}
