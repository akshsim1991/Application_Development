using System.ComponentModel;

namespace PneuSim;

public class Valve32 : DirectionalValve
{
	public override string TypeName { get; }

	public override string DisplayName => string.Format("3/2-way valve ({0}), {1}", NormallyOpen ? "NO" : "NC", ActuationText());

	[Category("Valve")]
	[DisplayName("Normally open")]
	[Description("False: 1 is closed at rest (NC). True: 1 is connected to 2 at rest (NO).")]
	public bool NormallyOpen { get; set; }

	protected override float BoxWidth { get; }

	protected override string LeftPilotName { get; }

	protected override string RightPilotName { get; }

	public Valve32()
	{
		TypeName = "Valve32";
		BoxWidth = 40f;
		LeftPilotName = "12";
		RightPilotName = "10";
		RebuildPorts();
	}

	protected override WorkingPort[] WorkingPorts()
	{
		return new WorkingPort[3]
		{
			new WorkingPort("2", 10f, top: true),
			new WorkingPort("1", 10f, top: false),
			new WorkingPort("3", 30f, top: false, vents: true)
		};
	}

	protected override string[][] Flows(int position)
	{
		if ((position == 1) ^ NormallyOpen)
		{
			return new string[1][] { new string[2] { "1", "2" } };
		}
		return new string[1][] { new string[2] { "2", "3" } };
	}

	protected override string[] ClosedPorts(int position)
	{
		if ((position == 1) ^ NormallyOpen)
		{
			return new string[1] { "3" };
		}
		return new string[1] { "1" };
	}
}
