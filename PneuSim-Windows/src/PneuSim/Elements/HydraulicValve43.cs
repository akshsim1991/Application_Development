using System;
using System.ComponentModel;

namespace PneuSim;

public class HydraulicValve43 : DirectionalValve
{
	public override string TypeName { get; }

	public override string DisplayName => $"4/3-way hydraulic valve ({Centre.ToString().ToLowerInvariant()} centre), {ActuationText()}";

	[Category("Valve")]
	[DisplayName("Centre position")]
	public HydraulicCentre Centre { get; set; }

	protected override PortKind Medium => PortKind.Hydraulic;

	protected override int PositionCount => 3;

	protected override float BoxWidth { get; }

	protected override string LeftPilotName { get; }

	protected override string RightPilotName { get; }

	public HydraulicValve43()
	{
		TypeName = "HydraulicValve43";
		Centre = HydraulicCentre.Tandem;
		BoxWidth = 40f;
		LeftPilotName = "X";
		RightPilotName = "Y";
		base.Actuator = ValveActuator.Solenoid;
		base.ReturnType = ValveReturn.Solenoid;
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
		return position switch
		{
			1 => new string[2][]
			{
				new string[2] { "P", "A" },
				new string[2] { "B", "T" }
			}, 
			2 => new string[2][]
			{
				new string[2] { "P", "B" },
				new string[2] { "A", "T" }
			}, 
			_ => Centre switch
			{
				HydraulicCentre.Tandem => new string[1][] { new string[2] { "P", "T" } }, 
				HydraulicCentre.Float => new string[2][]
				{
					new string[2] { "A", "T" },
					new string[2] { "B", "T" }
				}, 
				HydraulicCentre.Open => new string[3][]
				{
					new string[2] { "P", "T" },
					new string[2] { "A", "T" },
					new string[2] { "B", "T" }
				}, 
				_ => Array.Empty<string[]>(), 
			}, 
		};
	}

	protected override string[] ClosedPorts(int position)
	{
		if (position != 0)
		{
			return Array.Empty<string>();
		}
		return Centre switch
		{
			HydraulicCentre.Tandem => new string[2] { "A", "B" }, 
			HydraulicCentre.Float => new string[1] { "P" }, 
			HydraulicCentre.Open => Array.Empty<string>(), 
			_ => new string[4] { "A", "B", "P", "T" }, 
		};
	}
}
