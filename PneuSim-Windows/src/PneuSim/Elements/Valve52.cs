using System;

namespace PneuSim;

public class Valve52 : DirectionalValve
{
	public override string TypeName { get; }

	public override string DisplayName => $"5/2-way valve, {ActuationText()}";

	protected override float BoxWidth { get; }

	protected override string LeftPilotName { get; }

	protected override string RightPilotName { get; }

	public Valve52()
	{
		TypeName = "Valve52";
		BoxWidth = 60f;
		LeftPilotName = "14";
		RightPilotName = "12";
		RebuildPorts();
	}

	protected override WorkingPort[] WorkingPorts()
	{
		return new WorkingPort[5]
		{
			new WorkingPort("4", 20f, top: true),
			new WorkingPort("2", 40f, top: true),
			new WorkingPort("5", 10f, top: false, vents: true),
			new WorkingPort("1", 30f, top: false),
			new WorkingPort("3", 50f, top: false, vents: true)
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
			new string[2] { "4", "5" }
		};
	}

	protected override string[] ClosedPorts(int position)
	{
		return Array.Empty<string>();
	}
}
