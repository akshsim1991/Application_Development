using System;

namespace PneuSim;

public class Valve42 : DirectionalValve
{
	public override string TypeName { get; }

	public override string DisplayName => $"4/2-way valve, {ActuationText()}";

	protected override float BoxWidth { get; }

	protected override string LeftPilotName { get; }

	protected override string RightPilotName { get; }

	public Valve42()
	{
		TypeName = "Valve42";
		BoxWidth = 40f;
		LeftPilotName = "14";
		RightPilotName = "12";
		RebuildPorts();
	}

	protected override WorkingPort[] WorkingPorts()
	{
		return new WorkingPort[4]
		{
			new WorkingPort("4", 10f, top: true),
			new WorkingPort("2", 30f, top: true),
			new WorkingPort("1", 10f, top: false),
			new WorkingPort("3", 30f, top: false, vents: true)
		};
	}

	protected override string[][] Flows(int position)
	{
		if (position == 1)
		{
			return new string[2][]
			{
				new string[2] { "1", "4" },
				new string[2] { "2", "3" }
			};
		}
		return new string[2][]
		{
			new string[2] { "1", "2" },
			new string[2] { "4", "3" }
		};
	}

	protected override string[] ClosedPorts(int position)
	{
		return Array.Empty<string>();
	}
}
