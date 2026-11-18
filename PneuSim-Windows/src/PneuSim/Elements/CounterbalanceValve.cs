using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace PneuSim;

public class CounterbalanceValve : CircuitElement
{
	private bool _open;

	private double _setting;

	private double _pilot;

	[Browsable(false)]
	public bool IsOpen => _open;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Valve")]
	[DisplayName("Opening pressure (bar)")]
	[Description("Set about 1.3 times the load-induced pressure.")]
	public double Setting
	{
		get
		{
			return _setting;
		}
		set
		{
			_setting = Math.Max(1.0, Math.Min(400.0, value));
		}
	}

	[Category("Valve")]
	[DisplayName("Pilot pressure to open (bar)")]
	public double PilotPressure
	{
		get
		{
			return _pilot;
		}
		set
		{
			_pilot = Math.Max(1.0, Math.Min(400.0, value));
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 70f, 66f);

	public CounterbalanceValve()
	{
		TypeName = "CounterbalanceValve";
		DisplayName = "Counterbalance valve";
		_setting = 50.0;
		_pilot = 10.0;
		AddPort("1", 0f, 50f, -1f, 0f, vents: false, PortKind.Hydraulic);
		AddPort("2", 70f, 50f, 1f, 0f, vents: false, PortKind.Hydraulic);
		AddPort("X", 35f, 0f, 0f, -1f, vents: false, PortKind.Hydraulic);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 0f, 50f, 20f, 50f);
		g.DrawLine(r.PenFor(base.Ports[1]), 50f, 50f, 70f, 50f);
		g.FillRectangle(r.BodyBrush, 20f, 20f, 30f, 40f);
		g.DrawRectangle(r.Line, 20f, 20f, 30f, 40f);
		Symbols.Arrow(g, (r.Simulating && _open) ? r.HydraulicPressure : r.Line, new PointF(_open ? 46 : 40, 50f), new PointF(_open ? 24 : 30, 50f), 5f);
		Symbols.Spring(g, r.Thin, 50f, 60f, 30f, 3f);
		using (Pen pen = new Pen(Color.Black, 1f)
		{
			DashStyle = DashStyle.Dash
		})
		{
			g.DrawLine(pen, 35f, 0f, 35f, 20f);
		}
		g.DrawLines(r.Line, new PointF[4]
		{
			new PointF(10f, 50f),
			new PointF(10f, 64f),
			new PointF(60f, 64f),
			new PointF(60f, 50f)
		});
		g.FillEllipse(r.BodyBrush, 33f, 60f, 8f, 8f);
		g.DrawEllipse(r.Line, 33f, 60f, 8f, 8f);
		g.DrawString($"{Setting:0} bar", r.SmallFont, r.TextBrush, 52f, 14f);
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_open = false;
	}

	public override bool UpdateLogic(Simulator sim)
	{
		bool flag = base.Ports[1].Pressure >= Setting - 0.01 || base.Ports[2].Pressure >= PilotPressure;
		if (flag == _open)
		{
			return false;
		}
		_open = flag;
		return true;
	}

	public override void AddEdges(Simulator sim)
	{
		sim.AddEdge(base.Ports[0], base.Ports[1], 1.0, _open ? 1 : 0);
	}
}
