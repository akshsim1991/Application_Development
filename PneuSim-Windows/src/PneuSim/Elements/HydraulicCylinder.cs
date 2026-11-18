using System;

namespace PneuSim;

public class HydraulicCylinder : DoubleActingCylinder, IHydraulicConsumer
{
	private int _direction;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	public double LoadPressure
	{
		get
		{
			double num = ((_direction >= 0) ? CapArea() : RodArea());
			double num2 = ((_direction >= 0) ? base.LoadForceN : (0.0 - base.LoadForceN)) + base.EffectiveFrictionN;
			return Math.Max(2.0, num2 / num / 100000.0);
		}
	}

	public double OverrunPressure
	{
		get
		{
			if (!(base.LoadForceN < 0.0))
			{
				return 0.0;
			}
			return (0.0 - base.LoadForceN) / RodArea() / 100000.0;
		}
	}

	public HydraulicCylinder()
	{
		TypeName = "HydraulicCylinder";
		DisplayName = "Hydraulic cylinder";
		foreach (Port port in base.Ports)
		{
			port.Kind = PortKind.Hydraulic;
			port.VentsWhenOpen = false;
		}
		base.BoreMm = 40.0;
		base.RodMm = 22.0;
		base.StrokeLength = 200;
		base.LoadForceN = 2000.0;
	}

	private int DesiredDirection()
	{
		Port port = base.Ports[0];
		Port port2 = base.Ports[1];
		if (port.State == PortState.Pressurized && port2.State == PortState.Exhausted)
		{
			return (base.Position < 1.0) ? 1 : 0;
		}
		if (port2.State == PortState.Pressurized && port.State == PortState.Exhausted)
		{
			return (base.Position > 0.0) ? (-1) : 0;
		}
		if (port.State == PortState.Pressurized && port2.State == PortState.Pressurized)
		{
			return (base.Position < 1.0) ? 1 : 0;
		}
		if (port.State == PortState.Exhausted && port2.State == PortState.Exhausted && base.LoadForceN < 0.0)
		{
			return (base.Position < 1.0) ? 1 : 0;
		}
		return 0;
	}

	public bool WantsFlow()
	{
		if (base.Fault == FaultKind.Jammed)
		{
			return false;
		}
		return DesiredDirection() != 0 && (base.Ports[0].State != PortState.Exhausted || base.Ports[1].State != PortState.Exhausted);
	}

	bool IHydraulicConsumer.WantsFlow()
	{
		//ILSpy generated this explicit interface implementation from .override directive in WantsFlow
		return this.WantsFlow();
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		_direction = DesiredDirection();
		if (_direction == 0)
		{
			MoveAtSpeed(0.0, dt);
			return;
		}
		Port port = base.Ports[0];
		Port port2 = base.Ports[1];
		double num = sim.HydraulicFlowShare / 60000.0;
		double num2 = ((port.State == PortState.Exhausted && port2.State == PortState.Exhausted) ? 0.5 : ((_direction > 0 && port2.State == PortState.Pressurized) ? (num / Math.Max(CapArea() - RodArea(), 1E-05) * port.Factor) : ((_direction <= 0) ? (num / RodArea() * Math.Min(port2.Factor, port.Factor)) : (num / CapArea() * Math.Min(port.Factor, port2.Factor)))));
		if (Math.Max(port.Pressure, port2.Pressure) + 0.01 < LoadPressure && (port.State != PortState.Exhausted || port2.State != PortState.Exhausted))
		{
			num2 = 0.0;
		}
		MoveAtSpeed((double)_direction * num2, dt);
	}
}
