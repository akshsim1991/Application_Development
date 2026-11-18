using System;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class HydraulicMotor : CircuitElement, IHydraulicConsumer
{
	private double _angle;

	private double _rpm;

	private double _displacement;

	private double _loadPressure;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Motor")]
	[DisplayName("Displacement (cm³/rev)")]
	public double DisplacementCc
	{
		get
		{
			return _displacement;
		}
		set
		{
			_displacement = Math.Max(0.1, Math.Min(10000.0, value));
		}
	}

	[Category("Motor")]
	[DisplayName("Load pressure (bar)")]
	[Description("Pressure needed to turn the load (torque / displacement).")]
	public double LoadPressureBar
	{
		get
		{
			return _loadPressure;
		}
		set
		{
			_loadPressure = Math.Max(0.0, Math.Min(700.0, value));
		}
	}

	public double Rpm => _rpm;

	public override System.Collections.Generic.List<(string Name, string Value)> InspectValues()
	{
		System.Collections.Generic.List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Speed", $"{_rpm:0} rpm"));
		return list;
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 60f, 60f);

	public override PointF LabelAnchor => new PointF(62f, 18f);

	public double LoadPressure => Math.Max(2.0, LoadPressureBar);

	public HydraulicMotor()
	{
		TypeName = "HydraulicMotor";
		DisplayName = "Hydraulic motor";
		_displacement = 10.0;
		_loadPressure = 20.0;
		AddPort("A", 10f, 60f, 0f, 1f, vents: false, PortKind.Hydraulic);
		AddPort("B", 50f, 60f, 0f, 1f, vents: false, PortKind.Hydraulic);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 10f, 60f, 10f, 30f);
		g.DrawLine(r.PenFor(base.Ports[0]), 10f, 30f, 16f, 30f);
		g.DrawLine(r.PenFor(base.Ports[1]), 50f, 60f, 50f, 30f);
		g.DrawLine(r.PenFor(base.Ports[1]), 50f, 30f, 44f, 30f);
		g.FillEllipse(r.BodyBrush, 16f, 16f, 28f, 28f);
		g.DrawEllipse(r.Line, 16f, 16f, 28f, 28f);
		using (SolidBrush brush = new SolidBrush((r.Simulating && _rpm != 0.0) ? RenderContext.HydraulicColor : Color.Black))
		{
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(18f, 30f),
				new PointF(25f, 26f),
				new PointF(25f, 34f)
			});
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(42f, 30f),
				new PointF(35f, 26f),
				new PointF(35f, 34f)
			});
		}
		double num = _angle * Math.PI / 180.0;
		g.DrawLine(r.Thin, 30f, 30f, (float)(30.0 + 10.0 * Math.Cos(num)), (float)(30.0 + 10.0 * Math.Sin(num)));
		if (r.Simulating)
		{
			g.DrawString($"{_rpm:0} rpm", r.SmallFont, r.TextBrush, 46f, 6f);
		}
	}

	public bool WantsFlow()
	{
		Port port = base.Ports[0];
		Port port2 = base.Ports[1];
		if (port.State != PortState.Pressurized || port2.State != PortState.Exhausted)
		{
			if (port2.State == PortState.Pressurized)
			{
				return port.State == PortState.Exhausted;
			}
			return false;
		}
		return true;
	}

	bool IHydraulicConsumer.WantsFlow()
	{
		//ILSpy generated this explicit interface implementation from .override directive in WantsFlow
		return this.WantsFlow();
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_angle = 0.0;
		_rpm = 0.0;
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		Port port = base.Ports[0];
		Port port2 = base.Ports[1];
		int num = ((port.State == PortState.Pressurized && port2.State == PortState.Exhausted) ? 1 : ((port2.State == PortState.Pressurized && port.State == PortState.Exhausted) ? (-1) : 0));
		double num2 = Math.Max(port.Pressure, port2.Pressure);
		double num3 = Math.Min(port.Factor, port2.Factor);
		_rpm = ((num != 0 && num2 + 0.01 >= LoadPressure) ? ((double)num * sim.HydraulicFlowShare * 1000.0 / Math.Max(0.1, DisplacementCc) * num3) : 0.0);
		_angle = (_angle + _rpm * 6.0 * dt * 0.2) % 360.0;
	}
}
