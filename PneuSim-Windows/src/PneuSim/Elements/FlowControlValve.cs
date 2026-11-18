using System;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class FlowControlValve : CircuitElement
{
	private int _opening;

	private bool _hasCheck;

	public override string TypeName { get; }

	public override string DisplayName
	{
		get
		{
			if (!Hydraulic)
			{
				if (!_hasCheck)
				{
					return "Flow control valve";
				}
				return "One-way flow control valve";
			}
			if (!_hasCheck)
			{
				return "Hydraulic throttle valve";
			}
			return "Hydraulic one-way flow control valve";
		}
	}

	[Category("Medium")]
	[DisplayName("Hydraulic")]
	[Description("True: fitted in a hydraulic (oil) line. False: compressed air.")]
	public bool Hydraulic
	{
		get
		{
			return base.Ports[0].Kind == PortKind.Hydraulic;
		}
		set
		{
			foreach (Port port in base.Ports)
			{
				port.Kind = (value ? PortKind.Hydraulic : PortKind.Pneumatic);
			}
		}
	}

	[Category("Flow control")]
	[DisplayName("Opening (%)")]
	[Description("Throttle opening. 100 % is unrestricted flow; smaller values slow down the cylinder.")]
	public int OpeningPercent
	{
		get
		{
			return _opening;
		}
		set
		{
			_opening = Math.Max(1, Math.Min(100, value));
		}
	}

	[Category("Flow control")]
	[DisplayName("Check valve")]
	[Description("True: free flow from 1 to 2, throttled from 2 to 1. False: throttled in both directions.")]
	public bool HasCheckValve
	{
		get
		{
			return _hasCheck;
		}
		set
		{
			_hasCheck = value;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, (!_hasCheck) ? 18 : 0, 80f, _hasCheck ? 42 : 24);

	public override PointF LabelAnchor => new PointF(0f, (!_hasCheck) ? 18 : 0);

	public FlowControlValve()
	{
		_opening = 30;
		_hasCheck = true;
		TypeName = "FlowControlValve";
		AddPort("1", 0f, 30f, -1f, 0f);
		AddPort("2", 80f, 30f, 1f, 0f);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Pen pen = r.PenFor(base.Ports[0]);
		Pen pen2 = r.PenFor(base.Ports[1]);
		g.DrawLine(pen, 0f, 30f, 32f, 30f);
		g.DrawLine(pen2, 48f, 30f, 80f, 30f);
		g.DrawArc(r.Line, 30f, 21f, 20f, 8f, 200f, 140f);
		g.DrawArc(r.Line, 30f, 31f, 20f, 8f, 20f, 140f);
		g.DrawLine((base.Ports[0].IsPressurized && r.Simulating) ? r.Pressure : r.Thin, 32f, 30f, 48f, 30f);
		Symbols.Arrow(g, r.Thin, new PointF(30f, 40f), new PointF(52f, 20f), 5f);
		if (_hasCheck)
		{
			g.DrawLines(pen, new PointF[3]
			{
				new PointF(16f, 30f),
				new PointF(16f, 8f),
				new PointF(34f, 8f)
			});
			g.DrawLines(pen2, new PointF[3]
			{
				new PointF(48f, 8f),
				new PointF(64f, 8f),
				new PointF(64f, 30f)
			});
			g.DrawLine(pen, 34f, 8f, 38f, 8f);
			g.DrawLine(r.Line, 44f, 2f, 38f, 8f);
			g.DrawLine(r.Line, 38f, 8f, 44f, 14f);
			g.FillEllipse(r.BodyBrush, 40f, 4f, 8f, 8f);
			g.DrawEllipse(r.Line, 40f, 4f, 8f, 8f);
		}
		g.DrawString($"{_opening}%", r.SmallFont, r.TextBrush, 52f, 32f);
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.Blocked };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.Blocked)
		{
			return base.FaultDescription(kind);
		}
		return "Throttle clogged with dirt (almost no flow through it)";
	}

	public override void AddEdges(Simulator sim)
	{
		double num = (double)_opening / 100.0;
		if (base.Fault == FaultKind.Blocked)
		{
			num = Math.Min(num, 0.03);
		}
		if (_hasCheck)
		{
			sim.AddEdge(base.Ports[0], base.Ports[1], 1.0, num);
		}
		else
		{
			sim.AddEdge(base.Ports[0], base.Ports[1], num, num);
		}
	}
}
