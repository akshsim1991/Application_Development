using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class ShutOffValve : CircuitElement
{
	private bool _open;

	public override string TypeName { get; }

	public override string DisplayName
	{
		get
		{
			if (!Hydraulic)
			{
				return "Shut-off valve";
			}
			return "Hydraulic shut-off valve";
		}
	}

	[Category("Valve")]
	[DisplayName("Open at start")]
	[Description("Position of the handle when the simulation starts.")]
	public bool OpenAtStart { get; set; }

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

	[Browsable(false)]
	public bool IsOpen
	{
		get
		{
			if (_open)
			{
				return base.Fault != FaultKind.Blocked;
			}
			return false;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 4f, 60f, 48f);

	[Browsable(false)]
	public override bool IsManuallyOperated => true;

	public ShutOffValve()
	{
		_open = true;
		TypeName = "ShutOffValve";
		OpenAtStart = true;
		AddPort("1", 0f, 30f, -1f, 0f);
		AddPort("2", 60f, 30f, 1f, 0f);
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
		return "Ball seized: the valve stays closed whatever the handle shows";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Handle", _open ? "open" : "closed"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		bool flag = (r.Simulating ? _open : OpenAtStart);
		bool flag2 = r.Simulating && IsOpen;
		g.DrawLine(r.PenFor(base.Ports[0]), 0f, 30f, 18f, 30f);
		g.DrawLine(flag2 ? r.PenFor(base.Ports[0]) : r.PenFor(base.Ports[1]), 42f, 30f, 60f, 30f);
		PointF[] points = new PointF[3]
		{
			new PointF(18f, 22f),
			new PointF(30f, 30f),
			new PointF(18f, 38f)
		};
		PointF[] points2 = new PointF[3]
		{
			new PointF(42f, 22f),
			new PointF(30f, 30f),
			new PointF(42f, 38f)
		};
		if (flag)
		{
			g.FillPolygon(r.BodyBrush, points);
			g.FillPolygon(r.BodyBrush, points2);
		}
		else
		{
			g.FillPolygon(Brushes.Black, points);
			g.FillPolygon(Brushes.Black, points2);
		}
		g.DrawPolygon(r.Line, points);
		g.DrawPolygon(r.Line, points2);
		g.DrawLine(r.Thin, 30f, 30f, 30f, 18f);
		if (flag)
		{
			g.DrawLine(r.Line, 30f, 14f, 46f, 14f);
		}
		else
		{
			g.DrawLine(r.Line, 30f, 6f, 30f, 18f);
		}
		if (r.Simulating)
		{
			g.DrawString(flag ? "open" : "closed", r.SmallFont, r.TextBrush, 18f, 40f);
		}
	}

	public override void OnSimMouseDown(PointF local)
	{
		_open = !_open;
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_open = OpenAtStart;
	}

	public override void AddEdges(Simulator sim)
	{
		if (IsOpen)
		{
			sim.AddEdge(base.Ports[0], base.Ports[1]);
		}
	}
}
