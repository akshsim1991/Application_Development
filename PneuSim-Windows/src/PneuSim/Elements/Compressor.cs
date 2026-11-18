using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class Compressor : CircuitElement
{
	private bool _switchedOn;

	private bool _running;

	private double _delivery;

	private double _cutOut;

	private double _cutIn;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Compressor")]
	[DisplayName("Delivery (NL/min)")]
	[Description("Free air delivered while running.")]
	public double DeliveryNlMin
	{
		get
		{
			return _delivery;
		}
		set
		{
			_delivery = Math.Max(1.0, Math.Min(100000.0, value));
		}
	}

	[Category("Compressor")]
	[DisplayName("Cut-out pressure (bar)")]
	[Description("The compressor stops when the receiver reaches this pressure.")]
	public double CutOutPressure
	{
		get
		{
			return _cutOut;
		}
		set
		{
			_cutOut = Math.Max(1.0, Math.Min(16.0, value));
		}
	}

	[Category("Compressor")]
	[DisplayName("Cut-in pressure (bar)")]
	[Description("The compressor starts again when the receiver falls to this pressure.")]
	public double CutInPressure
	{
		get
		{
			return Math.Min(_cutIn, _cutOut - 0.2);
		}
		set
		{
			_cutIn = Math.Max(0.5, Math.Min(15.8, value));
		}
	}

	[Browsable(false)]
	public bool ChargesReceiver { get; set; }

	[Browsable(false)]
	public bool Running => _running;

	[Browsable(false)]
	public double EffectiveDelivery
	{
		get
		{
			if (base.Fault != FaultKind.LowOutput)
			{
				return _delivery;
			}
			return _delivery * 0.3;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(-10f, 0f, 80f, 50f);

	public override PointF LabelAnchor => new PointF(46f, 22f);

	[Browsable(false)]
	public override bool IsManuallyOperated => true;

	public Compressor()
	{
		_switchedOn = true;
		TypeName = "Compressor";
		DisplayName = "Compressor";
		_delivery = 250.0;
		_cutOut = 8.0;
		_cutIn = 6.5;
		AddPort("1", 30f, 0f, 0f, -1f);
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
		return "Compressor worn (valves or rings): it delivers far less air";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Main switch", _switchedOn ? "on" : "off"));
		list.Add(("Motor", _running ? "running" : "stopped"));
		list.Add(("Delivery", _running ? $"{EffectiveDelivery:0} NL/min" : "0 NL/min"));
		return list;
	}

	internal void UpdatePressureSwitch(double lowest, double highest)
	{
		if (!_switchedOn)
		{
			_running = false;
		}
		else if (highest >= _cutOut)
		{
			_running = false;
		}
		else if (lowest <= CutInPressure)
		{
			_running = true;
		}
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 30f, 0f, 30f, 14f);
		g.FillEllipse(r.BodyBrush, 16f, 14f, 28f, 28f);
		g.DrawEllipse(r.Line, 16f, 14f, 28f, 28f);
		Symbols.EnergyTriangle(g, r, new PointF[3]
		{
			new PointF(30f, 16f),
			new PointF(25f, 24f),
			new PointF(35f, 24f)
		}, hydraulic: false, r.Simulating && _running);
		g.DrawLine(r.Line, 16f, 28f, 6f, 28f);
		g.DrawEllipse(r.Line, -10f, 20f, 16f, 16f);
		g.DrawString("M", r.SmallFont, r.TextBrush, -6f, 22f);
		string text = $"{_cutIn:0.#}–{_cutOut:0.#} bar";
		if (r.Simulating)
		{
			text = ((!_switchedOn) ? "switched off" : (_running ? "running" : "stopped (full)"));
		}
		g.DrawString(text, r.SmallFont, r.TextBrush, 46f, 34f);
	}

	public override void OnSimMouseDown(PointF local)
	{
		_switchedOn = !_switchedOn;
		if (!_switchedOn)
		{
			_running = false;
		}
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_switchedOn = true;
		_running = true;
	}

	public override void AddTerminals(Simulator sim)
	{
		if (!ChargesReceiver && _switchedOn)
		{
			_running = true;
			sim.AddSupply(base.Ports[0], (base.Fault == FaultKind.LowOutput) ? (_cutOut * 0.5) : _cutOut);
		}
	}
}
