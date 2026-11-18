using System.Drawing;

namespace PneuSim;

public class Port
{
	public CircuitElement Owner { get; }

	public string Name { get; }

	public PointF Local { get; set; }

	public PointF Direction { get; set; }

	public bool VentsWhenOpen { get; set; }

	public PortKind Kind { get; set; }

	public bool IsOmnidirectional
	{
		get
		{
			if (Direction.X == 0f)
			{
				return Direction.Y == 0f;
			}
			return false;
		}
	}

	public int ConnectionCount { get; set; }

	public double Pressure { get; set; }

	public double SupplyPressure { get; set; }

	public PortState State { get; set; }

	public double Factor { get; set; }

	internal int NodeIndex { get; set; }

	public bool IsPressurized => Pressure > 0.5;

	public Port(CircuitElement owner, string name)
	{
		Kind = PortKind.Pneumatic;
		State = PortState.Floating;
		Owner = owner;
		Name = name;
	}

	public PointF WorldPos()
	{
		return Owner.ToWorld(Local);
	}

	public PointF WorldDir()
	{
		return Owner.RotateVector(Direction);
	}

	public void ResetSim()
	{
		Pressure = 0.0;
		SupplyPressure = 0.0;
		State = PortState.Floating;
		Factor = 0.0;
	}
}
