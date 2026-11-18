using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class VacuumGenerator : CircuitElement
{
	private double _level;

	private double _consumption;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Vacuum generator")]
	[DisplayName("Vacuum (bar)")]
	[Description("Vacuum reached with a closed suction line, e.g. -0.85 bar.")]
	public double VacuumLevel
	{
		get
		{
			return _level;
		}
		set
		{
			_level = Math.Max(-0.95, Math.Min(-0.1, value));
		}
	}

	[Category("Vacuum generator")]
	[DisplayName("Air consumption (NL/min)")]
	[Description("Free air blown through the nozzle while it works (at 6 bar).")]
	public double AirConsumptionNlMin
	{
		get
		{
			return _consumption;
		}
		set
		{
			_consumption = Math.Max(0.0, Math.Min(10000.0, value));
		}
	}

	[Browsable(false)]
	public bool Working
	{
		get
		{
			if (base.Ports[0].State == PortState.Pressurized && base.Ports[0].Pressure >= 2.0)
			{
				return base.Fault != FaultKind.Blocked;
			}
			return false;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 64f, 60f);

	public override PointF LabelAnchor => new PointF(44f, 14f);

	public VacuumGenerator()
	{
		TypeName = "VacuumGenerator";
		DisplayName = "Vacuum generator (ejector)";
		_level = -0.85;
		_consumption = 60.0;
		AddPort("1", 20f, 60f, 0f, 1f);
		AddPort("V", 20f, 0f, 0f, -1f);
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
		return "Nozzle clogged: no vacuum is produced";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Nozzle", Working ? "blowing (making vacuum)" : "off"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		bool num = r.Simulating && Working;
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, 60f, 20f, 42f);
		g.DrawLine(r.Line, 20f, 0f, 20f, 18f);
		g.FillRectangle(r.BodyBrush, 4f, 18f, 32f, 24f);
		g.DrawRectangle(r.Line, 4f, 18f, 32f, 24f);
		Pen pen = (num ? r.Pressure : r.Line);
		g.DrawLine(pen, 20f, 42f, 20f, 34f);
		g.DrawLines(r.Thin, new PointF[4]
		{
			new PointF(12f, 38f),
			new PointF(18f, 32f),
			new PointF(22f, 32f),
			new PointF(28f, 38f)
		});
		g.DrawLines(r.Thin, new PointF[2]
		{
			new PointF(18f, 24f),
			new PointF(14f, 20f)
		});
		g.DrawLines(r.Thin, new PointF[2]
		{
			new PointF(22f, 24f),
			new PointF(26f, 20f)
		});
		g.DrawLine(r.Line, 36f, 30f, 44f, 30f);
		g.DrawRectangle(r.Line, 44f, 26f, 8f, 8f);
		int num2 = 46;
		do
		{
			g.DrawLine(r.Thin, num2, 26f, num2, 34f);
			num2 = checked(num2 + 2);
		}
		while (num2 <= 50);
		if (r.Simulating)
		{
			g.DrawString(Working ? $"{base.Ports[1].Pressure:0.00} bar" : "off", r.SmallFont, r.TextBrush, 24f, 46f);
		}
	}

	public double ConsumptionNow()
	{
		if (!Working)
		{
			return 0.0;
		}
		return _consumption * (base.Ports[0].Pressure + 1.013) / 7.013;
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		sim.AddAirConsumption(ConsumptionNow() * dt / 60.0);
	}
}
