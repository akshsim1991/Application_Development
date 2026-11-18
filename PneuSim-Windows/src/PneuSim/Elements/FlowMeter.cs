using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class FlowMeter : CircuitElement
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Medium")]
	[DisplayName("Hydraulic")]
	[Description("True: fitted in a hydraulic (oil) line, shows l/min. False: compressed air, shows free air in NL/min.")]
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
	public double Flow { get; set; }

	[Browsable(false)]
	public string Unit
	{
		get
		{
			if (!Hydraulic)
			{
				return "NL/min";
			}
			return "l/min";
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 10f, 80f, 40f);

	public override PointF LabelAnchor => new PointF(22f, 14f);

	public FlowMeter()
	{
		TypeName = "FlowMeter";
		DisplayName = "Flow meter";
		AddPort("1", 0f, 30f, -1f, 0f);
		AddPort("2", 80f, 30f, 1f, 0f);
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Flow", $"{Math.Abs(Flow):0.0} {Unit}" + ((Math.Abs(Flow) < 0.05) ? "" : ((Flow > 0.0) ? " from 1 to 2" : " from 2 to 1"))));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 0f, 30f, 26f, 30f);
		g.DrawLine(r.PenFor(base.Ports[1]), 54f, 30f, 80f, 30f);
		g.FillEllipse(r.BodyBrush, 26f, 16f, 28f, 28f);
		g.DrawEllipse(r.Line, 26f, 16f, 28f, 28f);
		Symbols.Arrow(g, r.Line, new PointF(32f, 38f), new PointF(48f, 22f), 5f);
		if (!r.Simulating)
		{
			return;
		}
		g.DrawString($"{Math.Abs(Flow):0.0} {Unit}", r.SmallFont, r.TextBrush, 22f, 44f);
		if (Math.Abs(Flow) >= 0.05)
		{
			Pen pen = (Hydraulic ? r.HydraulicPressure : r.Pressure);
			if (Flow > 0.0)
			{
				Symbols.Arrow(g, pen, new PointF(4f, 22f), new PointF(22f, 22f), 5f);
			}
			else
			{
				Symbols.Arrow(g, pen, new PointF(76f, 22f), new PointF(58f, 22f), 5f);
			}
		}
	}

	public override void ResetSim()
	{
		base.ResetSim();
		Flow = 0.0;
	}

	public override void AddEdges(Simulator sim)
	{
		sim.AddEdge(base.Ports[0], base.Ports[1]);
	}
}
