using System;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class CompensatedFlowControl : CircuitElement
{
	private double _flow;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Flow control")]
	[DisplayName("Flow (l/min)")]
	[Description("Flow passed from 1 to 2, independent of the load pressure.")]
	public double FlowLpm
	{
		get
		{
			return _flow;
		}
		set
		{
			_flow = Math.Max(0.1, Math.Min(2000.0, value));
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 90f, 50f);

	public CompensatedFlowControl()
	{
		TypeName = "CompensatedFlowControl";
		DisplayName = "Pressure-compensated flow control valve";
		_flow = 4.0;
		AddPort("1", 0f, 30f, -1f, 0f, vents: false, PortKind.Hydraulic);
		AddPort("2", 90f, 30f, 1f, 0f, vents: false, PortKind.Hydraulic);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Pen pen = r.PenFor(base.Ports[0]);
		Pen pen2 = r.PenFor(base.Ports[1]);
		g.DrawLine(pen, 0f, 30f, 20f, 30f);
		g.DrawLine(pen2, 70f, 30f, 90f, 30f);
		g.FillRectangle(r.BodyBrush, 20f, 14f, 50f, 32f);
		g.DrawRectangle(r.Line, 20f, 14f, 50f, 32f);
		g.DrawArc(r.Line, 36f, 23f, 18f, 6f, 200f, 140f);
		g.DrawArc(r.Line, 36f, 31f, 18f, 6f, 20f, 140f);
		g.DrawLine(r.Line, 20f, 30f, 36f, 30f);
		g.DrawLine(r.Line, 54f, 30f, 70f, 30f);
		Symbols.Arrow(g, r.Thin, new PointF(34f, 40f), new PointF(56f, 18f), 5f);
		Symbols.Arrow(g, r.Thin, new PointF(26f, 20f), new PointF(40f, 20f), 4f);
		g.DrawLines(r.Line, new PointF[4]
		{
			new PointF(10f, 30f),
			new PointF(10f, 6f),
			new PointF(80f, 6f),
			new PointF(80f, 30f)
		});
		g.FillEllipse(r.BodyBrush, 41f, 1f, 8f, 8f);
		g.DrawEllipse(r.Line, 41f, 1f, 8f, 8f);
		g.DrawLine(r.Line, 49f, 0f, 53f, 6f);
		g.DrawLine(r.Line, 53f, 6f, 49f, 12f);
		g.DrawString($"{FlowLpm:0.#} l/min", r.SmallFont, r.TextBrush, 24f, 47f);
	}

	public override void AddEdges(Simulator sim)
	{
		double capacityAB = Math.Min(1.0, FlowLpm / Math.Max(0.01, sim.HydraulicPumpFlow));
		sim.AddEdge(base.Ports[0], base.Ports[1], capacityAB);
	}
}
