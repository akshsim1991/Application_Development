using System;
using System.Linq;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class ComponentHelp
{
	public static string HelpFor(CircuitElement e)
	{
		return string.Concat(e.DisplayName + (string.IsNullOrWhiteSpace(e.Label) ? "" : $"  ({e.Label})"), Environment.NewLine, Body(e));
	}

	private static string Body(CircuitElement e)
	{
		bool flag = true;
		if (e is TandemCylinder)
		{
			return "Tandem cylinder: two (or more) pistons on one rod, so it pushes and pulls with twice the force of an ordinary cylinder of the same bore and pressure, and uses twice the air.";
		}
		if (e is ThroughRodCylinder || e is HydraulicThroughRodCylinder)
		{
			return "Double-end cylinder: the rod comes out of both ends, so the area is the same on both sides and it pushes and pulls with equal force at equal speed. The second rod can carry a sensor or a second load.";
		}
		if (e is TelescopicCylinder || e is HydraulicTelescopicCylinder || e is TelescopicSingleActingCylinder || e is HydraulicTelescopicSingleActingCylinder)
		{
			return "Telescopic cylinder: nested stages give a long stroke from a short body. The largest stage extends first (most force, slowest), then the smaller ones (less force, faster); they retract in reverse order.";
		}
		if (e is DCMotor)
		{
			return "DC motor: +24 V on M1 and 0 V on M2 turns it one way, the reverse turns it the other way. Speed follows voltage, current and torque follow the load. A 12 V motor on the 24 V supply is shown as over-voltage.";
		}
		if (e is ServoMotor)
		{
			return "Servo motor: connect +24 V, 0 V and Enable (24 V). The inputs P1, P2 and P3 select a stored angle; with none selected it goes home (0 deg). It moves with a smooth speed profile and gives 24 V on OK when it is in position. A sensor with the same name as a position mark reacts at that angle.";
		}
		if (e is ElectricCylinder)
		{
			return "Electric cylinder: a servo motor turning a lead screw. Connect +24 V, 0 V and Enable; inputs P1 to P3 select stored positions (none = home, 0 mm); OK shows 24 V when in position. Position marks can drive proximity sensors.";
		}
		if (flag == e is Compressor)
		{
			return "Compressor with pressure switch: charges the air receiver, starting at the cut-in pressure and stopping at the cut-out pressure. Click it during simulation to switch it off or on.";
		}
		if (flag == e is AirReceiver)
		{
			return "Air receiver (tank): stores compressed air so short peaks of air use do not make the pressure drop; its pressure falls as air is used until the compressor starts again.";
		}
		if (flag == e is ShutOffValve)
		{
			return "Shut-off (ball) valve: opens or closes the line by hand. Click it during simulation to turn the handle.";
		}
		if (flag == e is FlowMeter)
		{
			return string.Format("Flow meter: shows how much {0} flows through the line and in which direction.", ((FlowMeter)e).Hydraulic ? "oil (l/min)" : "free air (NL/min)");
		}
		if (flag == e is ForceSensor)
		{
			return $"Force sensor (load cell) on the rod of cylinder {((ForceSensor)e).Cylinder}: shows the force the piston pushes or pulls with.";
		}
		if (flag == e is PressureSwitch)
		{
			PressureSwitch pressureSwitch = (PressureSwitch)e;
			return (pressureSwitch.Setting < 0.0) ? $"Vacuum switch: switches when the vacuum reaches {pressureSwitch.Setting:0.0#} bar, e.g. to report that a suction cup holds its part. Contacts with Reference {pressureSwitch.Label} follow it." : $"Pressure switch: switches when the pressure reaches {pressureSwitch.Setting:0.0#} bar (e.g. clamping pressure reached). Contacts with Reference {pressureSwitch.Label} follow it.";
		}
		if (flag == e is VacuumGenerator)
		{
			return "Vacuum generator (ejector): compressed air rushing through a nozzle sucks the air out of port V. It only works while it gets compressed air.";
		}
		if (flag == e is SuctionCup)
		{
			return "Suction cup: holds the workpiece while the vacuum is strong enough. Click it during simulation to put a workpiece under it or take it away.";
		}
		if (flag == e is Gripper)
		{
			return "Parallel gripper: air at port 1 closes the jaws, air at port 2 opens them. The position marks report open and closed.";
		}
		if (flag == e is ElectricCounter)
		{
			return $"Preset counter: counts the pulses on A1/A2. When the count reaches {((ElectricCounter)e).Preset}, its contacts (Reference {e.Label}) switch. A pulse on R1/R2, or a click during simulation, resets it.";
		}
		if (flag == e is AirSupply)
		{
			return "Delivers compressed air at the set pressure (usually 6 bar) from the compressor and air preparation.";
		}
		if (flag == (e is PressureRegulator && ((PressureRegulator)e).Hydraulic))
		{
			return "Pressure reducing valve: keeps the oil pressure after the valve at its setting (e.g. a lower clamping pressure), whatever the pump pressure.";
		}
		if (flag == e is PressureRegulator)
		{
			return (((PressureRegulator)e).Style == RegulatorStyle.ServiceUnit) ? "Service unit: filters the air, removes water and sets a constant working pressure (with a gauge)." : "Keeps the outlet pressure constant at the set value, whatever the inlet pressure.";
		}
		if (flag == e is PressureGauge)
		{
			return ((PressureGauge)e).Hydraulic ? "Hydraulic pressure gauge (0–160 bar): shows the oil pressure at its connection." : "Pressure gauge (0–10 bar): shows the air pressure at its connection.";
		}
		if (flag == e is Silencer)
		{
			return "Reduces the noise of exhausting air.";
		}
		if (flag == e is Junction)
		{
			return "A T-connection or bend point for tubes or wires. Select it and drag to move it.";
		}
		if (flag == e is HydraulicCylinder)
		{
			return "Hydraulic cylinder: speed = pump flow ÷ piston area; pressure = load force ÷ area.";
		}
		if (flag == e is HydraulicSingleActingCylinder)
		{
			return "Single-acting hydraulic cylinder (ram): oil pushes it out; the spring or the load pushes it back when the valve opens the line to tank.";
		}
		if (flag == e is SingleActingCylinder)
		{
			return "Air pushes the piston out; a spring returns it. Uses air in one direction only.";
		}
		if (flag == e is SemiRotaryActuator)
		{
			return "Turns a shaft through 0–180° with air: extending turns one way, retracting the other.";
		}
		if (flag == e is DoubleActingCylinder)
		{
			return "Air pushes the piston out (port 1) and back (port 2). The rod side has a smaller area, so retracting force is lower and speed higher.";
		}
		if (flag == e is AirMotor)
		{
			return "Turns continuously while air flows through it.";
		}
		if (flag == e is Valve53)
		{
			return "5/3-way valve: two working positions and a spring-centred middle position. " + ((Valve53)e).Centre switch
			{
				CentrePosition.Exhausted => "Exhaust centre: both cylinder lines are vented, so the cylinder can be moved by hand and stops pushing.", 
				CentrePosition.Pressurized => "Pressure centre: both cylinder lines get air; a double-acting cylinder creeps out (larger piston area).", 
				_ => "Closed centre: all ports are blocked, so the cylinder stops and holds its position.", 
			};
		}
		if (flag == e is HydraulicValve43)
		{
			return "4/3-way hydraulic valve: P (pump), T (tank), A and B (actuator). The centre position decides what happens when no solenoid is on.";
		}
		if (flag == e is DirectionalValve)
		{
			DirectionalValve directionalValve = (DirectionalValve)e;
			string text = "";
			switch (directionalValve.Actuator)
			{
			case ValveActuator.PushButton:
				text = "Operated while you hold the push button.";
				break;
			case ValveActuator.Selector:
				text = "A selector switch stays where you put it (detent).";
				break;
			case ValveActuator.IdleReturnRoller:
				text = string.Format("Idle-return roller: gives only a short pulse when the cylinder arrives at {0}, and none when it moves back. Used to avoid signal overlap.", string.IsNullOrWhiteSpace(directionalValve.TriggerMark) ? "its mark" : directionalValve.TriggerMark);
				break;
			case ValveActuator.RollerLever:
				text = (string.IsNullOrWhiteSpace(directionalValve.TriggerMark) ? "Roller lever valve: enter the cylinder position mark that should operate it under 'Roller mark'." : $"Operated when a cylinder reaches position mark {directionalValve.TriggerMark}.");
				break;
			case ValveActuator.Pilot:
				text = ((directionalValve.SwitchingPressure > 1.51) ? $"Pressure sequence valve: switches when the pilot pressure reaches {directionalValve.SwitchingPressure:0.#} bar." : "Switched by air pressure on its pilot port.");
				break;
			case ValveActuator.DelayedPilot:
				text = $"Time delay valve: switches {directionalValve.DelaySeconds:0.#} s after the pilot signal arrives.";
				break;
			case ValveActuator.Solenoid:
				text = $"Switched by the electrical solenoid {directionalValve.SolenoidLabel}. Click it during simulation for the manual override.";
				break;
			}
			string text2 = (directionalValve.IsMemoryValve ? " It is a memory (impulse) valve: it stays in its last position until the opposite signal arrives." : ((directionalValve.ReturnType == ValveReturn.Spring) ? " A spring returns it when the signal goes." : ""));
			string arg = (directionalValve.Ports.Any([SpecialName] (Port p) => p.Kind == PortKind.Hydraulic) ? "oil" : "air");
			return $"Directs the {arg}: the boxes show the flow paths in each position; the active box sits over the ports. " + text + text2;
		}
		if (flag == e is ShuttleValve)
		{
			return "OR function: the output gets air if input 1a OR input 1b has air.";
		}
		if (flag == e is TwoPressureValve)
		{
			return "AND function: the output gets air only if input 1a AND input 1b have air (two-hand safety controls).";
		}
		if (flag == e is CheckValve)
		{
			return string.Format("Non-return valve: {0} flows freely from 1 to 2 and is blocked from 2 to 1.", ((CheckValve)e).Hydraulic ? "oil" : "air");
		}
		if (flag == e is QuickExhaustValve)
		{
			return "Lets a cylinder exhaust straight to atmosphere next to the cylinder, so it returns much faster.";
		}
		if (flag == e is FlowControlValve)
		{
			FlowControlValve obj = (FlowControlValve)e;
			string arg2 = (obj.Hydraulic ? "oil" : "air");
			return obj.HasCheckValve ? $"One-way flow control: throttles flow from 2 to 1, free flow from 1 to 2. Fit it so it throttles the {arg2} leaving the cylinder (meter-out)." : $"Throttles the {arg2} flow in both directions; slows the actuator either way.";
		}
		if (flag == e is PowerTerminal)
		{
			return "Connection to the 24 V DC control voltage.";
		}
		if (flag == e is ElectricContact)
		{
			ElectricContact electricContact = (ElectricContact)e;
			string text3 = (electricContact.NormallyClosed ? "Normally closed (NC) contact: conducts until it is operated." : "Normally open (NO) contact: conducts while it is operated.");
			return electricContact.Operator switch
			{
				ContactOperator.Relay => text3 + $" Operated by relay {electricContact.Reference}.", 
				ContactOperator.ProximitySensor => text3 + $" Proximity sensor at cylinder position {electricContact.Reference}.", 
				ContactOperator.LimitSwitch => text3 + $" Limit switch at cylinder position {electricContact.Reference}.", 
				ContactOperator.PressureSwitch => text3 + $" Operated by pressure switch {electricContact.Reference}.", 
				ContactOperator.EmergencyStop => "Emergency stop: a red mushroom button that latches when pressed and opens the control circuit. Click again to release it (as turning a real one).", 
				_ => text3 + " Operated by hand.", 
			};
		}
		if (flag == e is ElectricCoil)
		{
			return ((ElectricCoil)e).Kind switch
			{
				CoilKind.Solenoid => $"Valve solenoid: when current flows it switches the valve that uses solenoid {e.Label}.", 
				CoilKind.Lamp => "Indicator lamp.", 
				CoilKind.OnDelayTimer => "On-delay timer relay: its contacts switch a set time after the coil is energized.", 
				CoilKind.OffDelayTimer => "Off-delay timer relay: its contacts switch at once and switch back a set time after the coil goes off.", 
				_ => $"Relay coil: when energized, all contacts referring to {e.Label} change over.", 
			};
		}
		if (flag == e is HydraulicPump)
		{
			return "Pump driven by an electric motor; delivers a fixed flow. Click it during simulation to start or stop it.";
		}
		if (flag == e is HydraulicTank)
		{
			return "Oil tank: return lines from valves end here.";
		}
		if (flag == e is ReliefValve)
		{
			return "Pressure relief valve: opens at its setting and sends the pump flow back to tank, so the pressure cannot rise higher. Every hydraulic circuit needs one.";
		}
		if (flag == e is HydraulicMotor)
		{
			return "Turns when oil flows through it; speed = flow ÷ displacement.";
		}
		if (flag == e is Accumulator)
		{
			return "Stores oil under gas pressure; it can move actuators when the pump is off.";
		}
		if (flag == e is CompensatedFlowControl)
		{
			return "Keeps the flow constant even when the load pressure changes, so the speed stays constant.";
		}
		if (flag == e is CounterbalanceValve)
		{
			return "Holds a hanging load: oil can leave the cylinder only when the pressure reaches the setting or the pilot opens it.";
		}
		if (flag == e is PageConnector)
		{
			return "Continues the line on another page: connectors with the same name are connected.";
		}
		if (flag == e is TextNote)
		{
			return "Text on the drawing (title, notes). Edit it in the Properties panel.";
		}
		return "";
	}
}
