using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class HydraulicPump : CircuitElement
{
	private bool _running;

	private double _flowLpm;

	private double _maxPressure;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Pump")]
	[DisplayName("Flow (l/min)")]
	[Description("Delivery of the pump; sets the speed of cylinders and motors.")]
	public double FlowLpm
	{
		get
		{
			return _flowLpm;
		}
		set
		{
			_flowLpm = Math.Max(0.1, Math.Min(2000.0, value));
		}
	}

	[Category("Pump")]
	[DisplayName("Maximum pressure (bar)")]
	[Description("Pressure the pump can build up when there is no relief valve.")]
	public double MaxPressureBar
	{
		get
		{
			return _maxPressure;
		}
		set
		{
			_maxPressure = Math.Max(1.0, Math.Min(700.0, value));
		}
	}

	[Browsable(false)]
	public double EffectiveFlowLpm
	{
		get
		{
			if (base.Fault != FaultKind.LowOutput)
			{
				return _flowLpm;
			}
			return _flowLpm * 0.35;
		}
	}

	[Browsable(false)]
	public bool Running => _running;

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 70f, 66f);

	public override PointF LabelAnchor => new PointF(46f, 22f);

	[Browsable(false)]
	public override bool IsManuallyOperated => true;

	public HydraulicPump()
	{
		_running = true;
		TypeName = "HydraulicPump";
		DisplayName = "Hydraulic power unit (pump)";
		_flowLpm = 8.0;
		_maxPressure = 250.0;
		AddPort("P", 30f, 0f, 0f, -1f, vents: false, PortKind.Hydraulic);
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
		return "Pump worn: it delivers much less oil";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Motor", _running ? "running" : "stopped"));
		list.Add(("Delivery", _running ? $"{EffectiveFlowLpm:0.0} l/min" : "0 l/min"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 30f, 0f, 30f, 14f);
		g.FillEllipse(r.BodyBrush, 16f, 14f, 28f, 28f);
		g.DrawEllipse(r.Line, 16f, 14f, 28f, 28f);
		using (SolidBrush brush = new SolidBrush((r.Simulating && _running) ? RenderContext.HydraulicColor : Color.Black))
		{
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(30f, 16f),
				new PointF(25f, 24f),
				new PointF(35f, 24f)
			});
		}
		g.DrawLine(r.Line, 16f, 28f, 6f, 28f);
		g.DrawEllipse(r.Line, -10f, 20f, 16f, 16f);
		g.DrawString("M", r.SmallFont, r.TextBrush, -6f, 22f);
		g.DrawLine(r.Line, 30f, 42f, 30f, 54f);
		g.DrawLines(r.Line, new PointF[4]
		{
			new PointF(20f, 52f),
			new PointF(20f, 60f),
			new PointF(40f, 60f),
			new PointF(40f, 52f)
		});
		g.DrawString($"{FlowLpm:0.#} l/min" + ((r.Simulating && !_running) ? " (off)" : ""), r.SmallFont, r.TextBrush, 46f, 34f);
	}

	public override void OnSimMouseDown(PointF local)
	{
		_running = !_running;
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_running = true;
	}

	public override void AddTerminals(Simulator sim)
	{
		if (_running)
		{
			sim.AddSupply(base.Ports[0], MaxPressureBar);
		}
	}
}
