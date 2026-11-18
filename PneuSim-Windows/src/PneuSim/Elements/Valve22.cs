using System;
using System.ComponentModel;

namespace PneuSim;

public class Valve22 : DirectionalValve
{
	public override string TypeName { get; }

	public override string DisplayName => string.Format("2/2-way valve ({0}), {1}", NormallyOpen ? "NO" : "NC", ActuationText());

	[Category("Valve")]
	[DisplayName("Normally open")]
	[Description("False: closed at rest (NC). True: open at rest (NO).")]
	public bool NormallyOpen { get; set; }

	protected override float BoxWidth { get; }

	protected override string LeftPilotName { get; }

	protected override string RightPilotName { get; }

	public Valve22()
	{
		TypeName = "Valve22";
		BoxWidth = 40f;
		LeftPilotName = "12";
		RightPilotName = "10";
		RebuildPorts();
	}

	protected override WorkingPort[] WorkingPorts()
	{
		return new WorkingPort[2]
		{
			new WorkingPort("2", 20f, top: true),
			new WorkingPort("1", 20f, top: false)
		};
	}

	protected override string[][] Flows(int position)
	{
		if (!((position == 1) ^ NormallyOpen))
		{
			return Array.Empty<string[]>();
		}
		return new string[1][] { new string[2] { "1", "2" } };
	}

	protected override string[] ClosedPorts(int position)
	{
		if (!((position == 1) ^ NormallyOpen))
		{
			return new string[2] { "1", "2" };
		}
		return Array.Empty<string>();
	}
}
