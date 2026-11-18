using System;

namespace PneuSim;

public class HydraulicValve42 : DirectionalValve
{
	public override string TypeName { get; }

	public override string DisplayName => $"4/2-way hydraulic valve, {ActuationText()}";

	protected override PortKind Medium => PortKind.Hydraulic;

	protected override float BoxWidth { get; }

	protected override string LeftPilotName { get; }

	protected override string RightPilotName { get; }

	public HydraulicValve42()
	{
		TypeName = "HydraulicValve42";
		BoxWidth = 40f;
		LeftPilotName = "X";
		RightPilotName = "Y";
		base.Actuator = ValveActuator.Solenoid;
		RebuildPorts();
	}

	protected override WorkingPort[] WorkingPorts()
	{
		return new WorkingPort[4]
		{
			new WorkingPort("A", 10f, top: true),
			new WorkingPort("B", 30f, top: true),
			new WorkingPort("P", 10f, top: false),
			new WorkingPort("T", 30f, top: false)
		};
	}

	protected override string[][] Flows(int position)
	{
		if (position == 1)
		{
			return new string[2][]
			{
				new string[2] { "P", "A" },
				new string[2] { "B", "T" }
			};
		}
		return new string[2][]
		{
			new string[2] { "P", "B" },
			new string[2] { "A", "T" }
		};
	}

	protected override string[] ClosedPorts(int position)
	{
		return Array.Empty<string>();
	}
}
