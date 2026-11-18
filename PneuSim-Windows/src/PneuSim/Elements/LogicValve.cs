using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public abstract class LogicValve : CircuitElement
{
	protected int Side;

	[Browsable(false)]
	public int ConnectedInput => Side;

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 80f, 30f);

	public override PointF LabelAnchor => new PointF(50f, 9f);

	protected LogicValve()
	{
		AddPort("1a", 0f, 20f, -1f, 0f);
		AddPort("1b", 80f, 20f, 1f, 0f);
		AddPort("2", 40f, 0f, 0f, -1f);
	}

	public override void ResetSim()
	{
		base.ResetSim();
		Side = 0;
	}

	public override void AddEdges(Simulator sim)
	{
		sim.AddEdge(base.Ports[Side], base.Ports[2]);
	}

	protected void DrawFrame(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 0f, 20f, 10f, 20f);
		g.DrawLine(r.PenFor(base.Ports[1]), 70f, 20f, 80f, 20f);
		g.DrawLine(r.PenFor(base.Ports[2]), 40f, 0f, 40f, 10f);
		g.FillRectangle(r.BodyBrush, 10f, 10f, 60f, 20f);
		g.DrawRectangle(r.Line, 10f, 10f, 60f, 20f);
	}
}
