using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class PressureRegulator : CircuitElement
{
	private double _setting;

	public override string TypeName { get; }

	public override string DisplayName
	{
		get
		{
			if (Hydraulic)
			{
				return "Pressure reducing valve";
			}
			return (Style == RegulatorStyle.ServiceUnit) ? "Service unit" : "Pressure regulator";
		}
	}

	[Category("Regulator")]
	[DisplayName("Output pressure (bar)")]
	[Description("Pressure delivered at port 2.")]
	public double Setting
	{
		get
		{
			return _setting;
		}
		set
		{
			_setting = Math.Max(0.2, Math.Min(400.0, value));
		}
	}

	[Category("Medium")]
	[DisplayName("Hydraulic")]
	[Description("True: fitted in a hydraulic (oil) line. False: compressed air (pressure regulator).")]
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

	[Category("Regulator")]
	[DisplayName("Symbol")]
	[Description("Plain regulator, or a service unit with filter and gauge.")]
	public RegulatorStyle Style { get; set; }

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 100f, 56f);

	private double EffectiveSetting
	{
		get
		{
			if (base.Fault != FaultKind.LowOutput)
			{
				return _setting;
			}
			return Math.Max(0.2, _setting * 0.25);
		}
	}

	public PressureRegulator()
	{
		_setting = 4.0;
		TypeName = "PressureRegulator";
		Style = RegulatorStyle.Regulator;
		AddPort("1", 0f, 30f, -1f, 0f);
		AddPort("2", 100f, 30f, 1f, 0f);
	}

	public override bool ShowProperty(string name)
	{
		if (Operators.CompareString(name, "Style", TextCompare: false) == 0)
		{
			return !Hydraulic;
		}
		return true;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Pen pen = r.PenFor(base.Ports[0]);
		Pen pen2 = r.PenFor(base.Ports[1]);
		if (Style == RegulatorStyle.ServiceUnit && !Hydraulic)
		{
			g.DrawLine(pen, 0f, 30f, 14f, 30f);
			g.DrawLine(pen2, 86f, 30f, 100f, 30f);
			using (Pen pen3 = new Pen(Color.Black, 1f)
			{
				DashStyle = DashStyle.Dash
			})
			{
				g.DrawRectangle(pen3, 14f, 6f, 72f, 44f);
			}
			float num = 30f;
			g.DrawPolygon(r.Line, new PointF[4]
			{
				new PointF(num, 18f),
				new PointF(num + 12f, 30f),
				new PointF(num, 42f),
				new PointF(num - 12f, 30f)
			});
			using (Pen pen4 = new Pen(Color.Black, 1f)
			{
				DashStyle = DashStyle.Dash
			})
			{
				g.DrawLine(pen4, num, 19f, num, 41f);
			}
			g.DrawLine(pen, 14f, 30f, num - 12f, 30f);
			g.DrawLine(pen, num + 12f, 30f, 52f, 30f);
			DrawRegulatorBox(g, r, 52f, 22f, 16f);
			g.DrawLine(pen2, 68f, 30f, 86f, 30f);
			g.DrawLine(r.Thin, 78f, 30f, 78f, 22f);
			g.FillEllipse(r.BodyBrush, 72f, 10f, 12f, 12f);
			g.DrawEllipse(r.Thin, 72f, 10f, 12f, 12f);
			g.DrawLine(r.Thin, 78f, 16f, 81f, 12f);
		}
		else
		{
			g.DrawLine(pen, 0f, 30f, 34f, 30f);
			g.DrawLine(pen2, 66f, 30f, 100f, 30f);
			DrawRegulatorBox(g, r, 34f, 14f, 32f);
		}
		string text = (r.Simulating ? Units.Pressure(base.Ports[1].Pressure, "0.0") : Units.Pressure(_setting, "0.#"));
		g.DrawString(text, r.SmallFont, r.TextBrush, 36f, 44f);
	}

	private void DrawRegulatorBox(DrawSurface g, RenderContext r, float x, float y, float size)
	{
		g.FillRectangle(r.BodyBrush, x, y, size, size);
		g.DrawRectangle(r.Line, x, y, size, size);
		float num = y + size / 2f;
		Symbols.Arrow(g, (r.Simulating && base.Ports[1].IsPressurized) ? r.Pressure : r.Line, new PointF(x + 3f, num + 3f), new PointF(x + size - 3f, num + 3f), 4f);
		Symbols.Spring(g, r.Thin, x + size / 2f - 1f, x + size / 2f + 1f, y - 4f, 3f);
		g.DrawLine(r.Thin, x + 2f, y - 2f, x + size - 2f, y - 10f);
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[2]
		{
			FaultKind.LowOutput,
			FaultKind.Blocked
		};
	}

	public override string FaultDescription(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.LowOutput => Hydraulic ? "Reducing valve faulty: the output pressure is far too low" : "Regulator diaphragm torn: the output pressure is far too low", 
			FaultKind.Blocked => (Style == RegulatorStyle.ServiceUnit && !Hydraulic) ? "Filter element clogged: very little air gets through" : "Clogged: very little flow gets through", 
			_ => base.FaultDescription(kind), 
		};
	}

	public override void AddEdges(Simulator sim)
	{
		double num = ((base.Fault == FaultKind.Blocked) ? 0.12 : 1.0);
		sim.AddEdge(base.Ports[0], base.Ports[1], num, num, EffectiveSetting);
	}
}
