using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class AirMotor : CircuitElement
{
	private double _angle;

	private double _rpm;

	private double _nominalSpeed;

	private double _consumption;

	[Browsable(false)]
	public double Rpm => _rpm;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Motor")]
	[DisplayName("Speed (rpm)")]
	[Description("Speed at full flow. Shown slowed down in the animation.")]
	public double NominalSpeed
	{
		get
		{
			return _nominalSpeed;
		}
		set
		{
			_nominalSpeed = Math.Max(0.0, Math.Min(30000.0, value));
		}
	}

	[Category("Motor")]
	[DisplayName("Air consumption (NL/min)")]
	[Description("Free air used at full speed and 6 bar; counted in the air consumption and running cost.")]
	public double AirConsumptionNlMin
	{
		get
		{
			return _consumption;
		}
		set
		{
			_consumption = Math.Max(0.0, Math.Min(100000.0, value));
		}
	}

	public override RectangleF LocalBounds => new RectangleF(2f, 2f, 36f, 48f);

	public override PointF LabelAnchor => new PointF(40f, 14f);

	public AirMotor()
	{
		TypeName = "AirMotor";
		DisplayName = "Air motor";
		_nominalSpeed = 60.0;
		_consumption = 150.0;
		AddPort("1", 20f, 50f, 0f, 1f);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Port port = base.Ports[0];
		g.DrawLine(r.PenFor(port), 20f, 38f, 20f, 50f);
		g.FillEllipse(r.BodyBrush, 2f, 2f, 36f, 36f);
		g.DrawEllipse(r.Line, 2f, 2f, 36f, 36f);
		Symbols.EnergyTriangle(g, r, new PointF[3]
		{
			new PointF(20f, 36f),
			new PointF(14f, 27f),
			new PointF(26f, 27f)
		}, hydraulic: false, r.Simulating && port.IsPressurized);
		double num = _angle * Math.PI / 180.0;
		g.DrawLine(r.Thin, 20f, 20f, (float)(20.0 + 12.0 * Math.Cos(num)), (float)(20.0 + 12.0 * Math.Sin(num)));
		g.DrawArc(r.Thin, -4f, -4f, 48f, 48f, 200f, 40f);
		if (r.Simulating)
		{
			g.DrawString($"{_rpm:0} rpm", r.SmallFont, r.TextBrush, 40f, 26f);
		}
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_angle = 0.0;
		_rpm = 0.0;
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.Jammed };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.Jammed)
		{
			return base.FaultDescription(kind);
		}
		return "Motor seized: the vanes are stuck";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Speed", $"{_rpm:0} rpm"));
		return list;
	}

	public double ConsumptionNow()
	{
		Port port = base.Ports[0];
		if (port.State != PortState.Pressurized || port.Pressure <= 0.2 || base.Fault == FaultKind.Jammed)
		{
			return 0.0;
		}
		return _consumption * port.Factor * (port.Pressure + 1.013) / 7.013;
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		Port port = base.Ports[0];
		_rpm = ((port.State == PortState.Pressurized && base.Fault != FaultKind.Jammed) ? (NominalSpeed * port.Factor * Math.Min(1.0, port.Pressure / 6.0)) : 0.0);
		_angle = (_angle + _rpm * 6.0 * dt) % 360.0;
		if (port.State == PortState.Pressurized && port.Pressure > 0.2 && base.Fault != FaultKind.Jammed)
		{
			sim.AddAirConsumption(_consumption * port.Factor * (port.Pressure + 1.013) / 7.013 * dt / 60.0);
		}
	}
}
