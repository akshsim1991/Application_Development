using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class CircuitMove
{
	public CylinderBase Cylinder;

	public bool Extend;

	public DirectionalValve Valve;

	public MoveTrigger Trigger;

	public char Letter;

	public string EndMark
	{
		get
		{
			if (!Extend)
			{
				return Cylinder.RetractedMark;
			}
			return Cylinder.ExtendedMark;
		}
	}

	public CircuitMove()
	{
		Trigger = new MoveTrigger();
	}

	public override string ToString()
	{
		return Conversions.ToString(Letter) + (Extend ? "+" : "-");
	}
}
