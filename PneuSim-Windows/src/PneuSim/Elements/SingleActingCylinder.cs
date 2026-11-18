using System;
using System.ComponentModel;

namespace PneuSim;

public class SingleActingCylinder : CylinderBase
{
	private double _springPreloadN;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Physical data (realistic mode)")]
	[DisplayName("Spring force (N)")]
	[Description("Return spring force when retracted; it rises by 50 % at full stroke.")]
	public double SpringPreloadN
	{
		get
		{
			return _springPreloadN;
		}
		set
		{
			_springPreloadN = Math.Max(0.0, Math.Min(100000.0, value));
		}
	}

	protected override bool HasRodPort { get; }

	protected override double SpringForce(double x) => SpringPreloadN * (1.0 + 0.5 * x / Math.Max(0.001, (double)base.StrokeLength / 1000.0));

	public SingleActingCylinder()
	{
		TypeName = "SingleActingCylinder";
		DisplayName = "Single-acting cylinder";
		_springPreloadN = 40.0;
		HasRodPort = false;
		AddPort("1", 10f, 40f, 0f, 1f, vents: true);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		float num = base.PistonX + 8f;
		Symbols.Spring(g, r.Thin, num + 2f, 118f, 15f, 10f);
		DrawPortStub(g, r, base.Ports[0]);
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		if (sim.RealPhysics)
		{
			PhysicsStep(sim, dt);
			return;
		}
		Port port = base.Ports[0];
		switch (port.State)
		{
		case PortState.Pressurized:
			if (port.Pressure > 1.0)
			{
				Move(1, port.Factor, dt, sim);
			}
			break;
		case PortState.Exhausted:
			Move(-1, port.Factor, dt);
			break;
		}
	}
}
