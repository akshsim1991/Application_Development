using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class PressureGauge : CircuitElement
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Pressure", Units.Pressure(base.Ports[0].Pressure, "0.0#")));
		return list;
	}

	[Category("Gauge")]
	[DisplayName("Hydraulic")]
	[Description("True: hydraulic gauge (0–160 bar). False: pneumatic gauge (0–10 bar).")]
	public bool Hydraulic
	{
		get
		{
			return base.Ports[0].Kind == PortKind.Hydraulic;
		}
		set
		{
			base.Ports[0].Kind = (value ? PortKind.Hydraulic : PortKind.Pneumatic);
		}
	}

	public override RectangleF LocalBounds => new RectangleF(2f, 2f, 36f, 48f);

	public override PointF LabelAnchor => new PointF(40f, 14f);

	public PressureGauge()
	{
		TypeName = "PressureGauge";
		DisplayName = "Pressure gauge";
		AddPort("1", 20f, 50f, 0f, 1f);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Port port = base.Ports[0];
		g.DrawLine(r.PenFor(port), 20f, 38f, 20f, 50f);
		g.FillEllipse(r.BodyBrush, 2f, 2f, 36f, 36f);
		g.DrawEllipse(r.Line, 2f, 2f, 36f, 36f);
		double num = (Hydraulic ? 160.0 : 10.0);
		double num2 = (-135.0 + 270.0 * Math.Max(-0.1, Math.Min(1.0, port.Pressure / num))) * Math.PI / 180.0;
		Symbols.Arrow(p2: new PointF((float)(20.0 + 14.0 * Math.Sin(num2)), (float)(20.0 - 14.0 * Math.Cos(num2))), g: g, pen: (r.Simulating && port.IsPressurized) ? r.Pressure : r.Line, p1: new PointF(20f, 20f), head: 5f);
		if (r.Simulating)
		{
			g.DrawString(Hydraulic ? Units.Pressure(port.Pressure, "0") : ((port.Pressure < -0.005) ? Units.Pressure(port.Pressure, "0.00") : Units.Pressure(port.Pressure, "0.0")), r.SmallFont, r.TextBrush, 40f, 26f);
		}
	}
}
