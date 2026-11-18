using System;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class Accumulator : CircuitElement
{
	private double _stored;

	private double _capacity;

	private double _precharge;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Accumulator")]
	[DisplayName("Gas volume (l)")]
	public double CapacityL
	{
		get
		{
			return _capacity;
		}
		set
		{
			_capacity = Math.Max(0.05, Math.Min(500.0, value));
		}
	}

	[Category("Accumulator")]
	[DisplayName("Pre-charge pressure (bar)")]
	public double PrechargeBar
	{
		get
		{
			return _precharge;
		}
		set
		{
			_precharge = Math.Max(1.0, Math.Min(400.0, value));
		}
	}

	[Browsable(false)]
	public double StoredLitres => _stored;

	public double GasPressure
	{
		get
		{
			if (!(_stored <= 0.0001))
			{
				return PrechargeBar * CapacityL / Math.Max(0.01, CapacityL - _stored);
			}
			return 0.0;
		}
	}

	[Browsable(false)]
	public bool Discharging { get; set; }

	public override RectangleF LocalBounds => new RectangleF(4f, 0f, 32f, 70f);

	public override PointF LabelAnchor => new PointF(38f, 18f);

	public Accumulator()
	{
		TypeName = "Accumulator";
		DisplayName = "Accumulator";
		_capacity = 1.0;
		_precharge = 30.0;
		AddPort("1", 20f, 70f, 0f, 1f, vents: false, PortKind.Hydraulic);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, 58f, 20f, 70f);
		g.FillEllipse(r.BodyBrush, 6f, 4f, 28f, 54f);
		float num = (float)Math.Min(1.0, _stored / Math.Max(0.01, CapacityL));
		if (num > 0f)
		{
			using SolidBrush brush = new SolidBrush(Color.FromArgb(120, RenderContext.HydraulicColor));
			g.FillRectangle(brush, 9f, 56f - 50f * num, 22f, 50f * num);
		}
		g.DrawEllipse(r.Line, 6f, 4f, 28f, 54f);
		g.DrawLine(r.Line, 6f, 31f, 34f, 31f);
		if (r.Simulating)
		{
			g.DrawString($"{_stored:0.00} l", r.SmallFont, r.TextBrush, 38f, 40f);
		}
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_stored = 0.0;
		Discharging = false;
	}

	public override void AddTerminals(Simulator sim)
	{
		if (Discharging && _stored > 0.001)
		{
			sim.AddSupply(base.Ports[0], GasPressure);
		}
	}

	internal void Exchange(double systemPressure, bool pumpFed, double flowOutLpm, double dt)
	{
		if (pumpFed && systemPressure > PrechargeBar)
		{
			double val = CapacityL * (1.0 - PrechargeBar / systemPressure);
			_stored = Math.Min(val, _stored + 1.0 / 3.0 * dt);
		}
		else if (!pumpFed)
		{
			_stored = Math.Max(0.0, _stored - flowOutLpm / 60.0 * dt);
		}
	}
}
