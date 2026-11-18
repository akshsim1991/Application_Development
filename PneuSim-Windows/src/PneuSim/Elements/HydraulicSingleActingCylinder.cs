using System;

namespace PneuSim;

public class HydraulicSingleActingCylinder : SingleActingCylinder, IHydraulicConsumer
{
	private const double FreeReturnSpeed = 0.15;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	public double LoadPressure
	{
		get
		{
			double num = base.LoadForceN + base.SpringPreloadN * 1.5 + base.EffectiveFrictionN;
			return Math.Max(2.0, num / CapArea() / 100000.0);
		}
	}

	public HydraulicSingleActingCylinder()
	{
		TypeName = "HydraulicSingleActingCylinder";
		DisplayName = "Hydraulic single-acting cylinder";
		base.Ports[0].Kind = PortKind.Hydraulic;
		base.Ports[0].VentsWhenOpen = false;
		base.BoreMm = 40.0;
		base.RodMm = 22.0;
		base.StrokeLength = 200;
		base.SpringPreloadN = 300.0;
	}

	public bool WantsFlow()
	{
		if (base.Ports[0].State == PortState.Pressurized && base.Position < 1.0)
		{
			return base.Fault != FaultKind.Jammed;
		}
		return false;
	}

	bool IHydraulicConsumer.WantsFlow()
	{
		//ILSpy generated this explicit interface implementation from .override directive in WantsFlow
		return this.WantsFlow();
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		Port port = base.Ports[0];
		switch (port.State)
		{
		case PortState.Pressurized:
			if (base.Position >= 1.0 || port.Pressure + 0.01 < LoadPressure)
			{
				MoveAtSpeed(0.0, dt);
			}
			else
			{
				MoveAtSpeed(sim.HydraulicFlowShare / 60000.0 / CapArea() * port.Factor, dt);
			}
			break;
		case PortState.Exhausted:
			MoveAtSpeed(-0.15 * Math.Max(port.Factor, 0.02), dt);
			break;
		default:
			MoveAtSpeed(0.0, dt);
			break;
		}
	}
}
