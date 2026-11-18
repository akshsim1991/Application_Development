using System;

namespace PneuSim;

public class DoubleActingCylinder : CylinderBase
{
	private const double RodSideArea = 0.7;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	protected override bool HasRodPort { get; }

	public DoubleActingCylinder()
	{
		TypeName = "DoubleActingCylinder";
		DisplayName = "Double-acting cylinder";
		HasRodPort = true;
		AddPort("1", 10f, 40f, 0f, 1f, vents: true);
		AddPort("2", 110f, 40f, 0f, 1f, vents: true);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		DrawPortStub(g, r, base.Ports[0]);
		DrawPortStub(g, r, base.Ports[1]);
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		if (sim.RealPhysics)
		{
			PhysicsStep(sim, dt);
			return;
		}
		Port port = base.Ports[0];
		Port port2 = base.Ports[1];
		double num = port.Pressure - port2.Pressure * 0.7;
		if (num > 0.2)
		{
			Move(1, Speed(port, port2), dt, sim);
		}
		else if (num < -0.2)
		{
			Move(-1, Speed(port2, port), dt, sim);
		}
	}

	private static double Speed(Port drive, Port outlet)
	{
		double num = ((drive.State == PortState.Pressurized) ? drive.Factor : 0.0);
		return outlet.State switch
		{
			PortState.Exhausted => Math.Min(num, outlet.Factor), 
			PortState.Pressurized => num * 0.3, 
			_ => 0.0, 
		};
	}
}
