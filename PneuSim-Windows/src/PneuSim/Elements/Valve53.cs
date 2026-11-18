using System;
using System.ComponentModel;

namespace PneuSim;

public class Valve53 : DirectionalValve
{
	public override string TypeName { get; }

	public override string DisplayName => $"5/3-way valve ({DescribeCentre()}), {ActuationText()}";

	[Category("Valve")]
	[DisplayName("Centre position")]
	[Description("Closed: all ports blocked (cylinder stops and holds). Exhausted: 2 and 4 vented (cylinder free). Pressurized: 1 to 2 and 4.")]
	public CentrePosition Centre { get; set; }

	protected override int PositionCount => 3;

	protected override float BoxWidth { get; }

	protected override string LeftPilotName { get; }

	protected override string RightPilotName { get; }

	public Valve53()
	{
		TypeName = "Valve53";
		Centre = CentrePosition.Closed;
		BoxWidth = 60f;
		LeftPilotName = "14";
		RightPilotName = "12";
		base.Actuator = ValveActuator.Pilot;
		base.ReturnType = ValveReturn.Pilot;
	}

	private string DescribeCentre()
	{
		return Centre switch
		{
			CentrePosition.Exhausted => "exhaust centre", 
			CentrePosition.Pressurized => "pressure centre", 
			_ => "closed centre", 
		};
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
		return position switch
		{
			1 => new string[2][]
			{
				new string[2] { "1", "4" },
				new string[2] { "2", "3" }
			}, 
			2 => new string[2][]
			{
				new string[2] { "1", "2" },
				new string[2] { "4", "5" }
			}, 
			_ => Centre switch
			{
				CentrePosition.Exhausted => new string[2][]
				{
					new string[2] { "4", "5" },
					new string[2] { "2", "3" }
				}, 
				CentrePosition.Pressurized => new string[2][]
				{
					new string[2] { "1", "4" },
					new string[2] { "1", "2" }
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
			CentrePosition.Exhausted => new string[1] { "1" }, 
			CentrePosition.Pressurized => new string[2] { "5", "3" }, 
			_ => new string[5] { "4", "2", "5", "1", "3" }, 
		};
	}
}
