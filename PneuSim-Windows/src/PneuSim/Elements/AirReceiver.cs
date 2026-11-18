using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class AirReceiver : CircuitElement
{
	private double _pressure;

	private double _volume;

	private double _initial;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Receiver")]
	[DisplayName("Volume (litres)")]
	public double VolumeLitres
	{
		get
		{
			return _volume;
		}
		set
		{
			_volume = Math.Max(0.1, Math.Min(100000.0, value));
		}
	}

	[Category("Receiver")]
	[DisplayName("Pressure at start (bar)")]
	[Description("Use 0 to watch the compressor fill an empty receiver.")]
	public double InitialPressure
	{
		get
		{
			return _initial;
		}
		set
		{
			_initial = Math.Max(0.0, Math.Min(16.0, value));
		}
	}

	[Browsable(false)]
	public double Pressure => _pressure;

	public override RectangleF LocalBounds => new RectangleF(0f, 10f, 100f, 40f);

	public override PointF LabelAnchor => new PointF(20f, 12f);

	public AirReceiver()
	{
		TypeName = "AirReceiver";
		DisplayName = "Air receiver";
		_volume = 20.0;
		_initial = 6.0;
		AddPort("1", 0f, 30f, -1f, 0f);
		AddPort("2", 100f, 30f, 1f, 0f);
	}

	internal void Exchange(double normalLitres)
	{
		_pressure = Math.Max(0.0, Math.Min(40.0, _pressure + normalLitres * 1.013 / _volume));
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.Leak };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.Leak)
		{
			return base.FaultDescription(kind);
		}
		return "Drain valve left open: air keeps escaping from the receiver";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Receiver pressure", $"{_pressure:0.00} bar"));
		list.Add(("Stored free air", $"{(_pressure + 1.013) / 1.013 * _volume:0} NL"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 0f, 30f, 20f, 30f);
		g.DrawLine(r.PenFor(base.Ports[1]), 80f, 30f, 100f, 30f);
		g.FillEllipse(r.BodyBrush, 20f, 18f, 16f, 24f);
		g.FillEllipse(r.BodyBrush, 64f, 18f, 16f, 24f);
		g.FillRectangle(r.BodyBrush, 28f, 18f, 44f, 24f);
		g.DrawArc(r.Line, 20f, 18f, 16f, 24f, 90f, 180f);
		g.DrawArc(r.Line, 64f, 18f, 16f, 24f, 270f, 180f);
		g.DrawLine(r.Line, 28f, 18f, 72f, 18f);
		g.DrawLine(r.Line, 28f, 42f, 72f, 42f);
		string text = (r.Simulating ? $"{_pressure:0.0} bar" : $"{_volume:0.#} l");
		g.DrawString(text, r.SmallFont, r.TextBrush, 38f, 23f);
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_pressure = _initial;
	}

	public override void AddEdges(Simulator sim)
	{
		sim.AddEdge(base.Ports[0], base.Ports[1]);
	}

	public override void AddTerminals(Simulator sim)
	{
		if (_pressure > 0.05)
		{
			sim.AddSupply(base.Ports[0], _pressure);
			sim.AddSupply(base.Ports[1], _pressure);
		}
	}
}
