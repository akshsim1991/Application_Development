using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace PneuSim;

public class ReliefValve : CircuitElement
{
	private double _setting;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Relief valve")]
	[DisplayName("Opening pressure (bar)")]
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

	[Browsable(false)]
	public double EffectiveSetting
	{
		get
		{
			if (base.Fault != FaultKind.LowOutput)
			{
				return _setting;
			}
			return Math.Max(1.0, _setting * 0.4);
		}
	}

	[Browsable(false)]
	public bool IsOpen
	{
		get
		{
			if (base.Ports[0].IsPressurized)
			{
				return base.Ports[0].Pressure >= EffectiveSetting - 0.01;
			}
			return false;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 50f, 70f);

	public override PointF LabelAnchor => new PointF(42f, 22f);

	public ReliefValve()
	{
		_setting = 60.0;
		TypeName = "ReliefValve";
		DisplayName = "Pressure relief valve";
		AddPort("P", 20f, 0f, 0f, -1f, vents: false, PortKind.Hydraulic);
		AddPort("T", 20f, 70f, 0f, 1f, vents: false, PortKind.Hydraulic);
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.LowOutput };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.LowOutput)
		{
			return base.FaultDescription(kind);
		}
		return "Relief valve opens far too early (weak or broken spring)";
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		bool flag = r.Simulating && IsOpen;
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, 0f, 20f, 20f);
		g.DrawLine(flag ? r.HydraulicPressure : r.Line, 20f, 50f, 20f, 70f);
		g.FillRectangle(r.BodyBrush, 6f, 20f, 28f, 30f);
		g.DrawRectangle(r.Line, 6f, 20f, 28f, 30f);
		Symbols.Arrow(g, flag ? r.HydraulicPressure : r.Line, new PointF(flag ? 20 : 14, 22f), new PointF(flag ? 20 : 14, 48f), 5f);
		using (Pen pen = new Pen(Color.Black, 1f)
		{
			DashStyle = DashStyle.Dash
		})
		{
			g.DrawLines(pen, new PointF[4]
			{
				new PointF(20f, 10f),
				new PointF(2f, 10f),
				new PointF(2f, 35f),
				new PointF(6f, 35f)
			});
		}
		Symbols.Spring(g, r.Thin, 34f, 46f, 35f, 4f);
		g.DrawLine(r.Thin, 36f, 44f, 46f, 26f);
		g.DrawString($"{_setting:0} bar", r.SmallFont, r.TextBrush, 22f, 54f);
	}
}
