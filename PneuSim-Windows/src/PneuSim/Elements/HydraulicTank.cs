using System.Drawing;

namespace PneuSim;

public class HydraulicTank : CircuitElement
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override RectangleF LocalBounds => new RectangleF(4f, 0f, 32f, 22f);

	public HydraulicTank()
	{
		TypeName = "HydraulicTank";
		DisplayName = "Tank";
		AddPort("T", 20f, 0f, 0f, -1f, vents: false, PortKind.Hydraulic);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, 0f, 20f, 16f);
		g.DrawLines(r.Line, new PointF[4]
		{
			new PointF(6f, 8f),
			new PointF(6f, 20f),
			new PointF(34f, 20f),
			new PointF(34f, 8f)
		});
	}

	public override void AddTerminals(Simulator sim)
	{
		sim.AddExhaust(base.Ports[0]);
	}
}
