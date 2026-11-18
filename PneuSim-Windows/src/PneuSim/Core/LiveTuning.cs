using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class LiveTuning
{
	public class Knob
	{
		public string PropertyName { get; set; }

		public string Caption { get; set; }

		public double Min { get; set; }

		public double Max { get; set; }
	}

	private static Knob K(string name, string caption, double min, double max)
	{
		return new Knob
		{
			PropertyName = name,
			Caption = caption,
			Min = min,
			Max = max
		};
	}

	public static List<Knob> KnobsFor(CircuitElement e)
	{
		List<Knob> list = new List<Knob>();
		bool flag = true;
		if (flag == e is AirSupply)
		{
			list.Add(K("Pressure", "Supply pressure (bar)", 0.5, 10.0));
		}
		else if (flag == e is PressureRegulator)
		{
			list.Add(((PressureRegulator)e).Hydraulic ? K("Setting", "Reduced pressure (bar)", 1.0, 250.0) : K("Setting", "Output pressure (bar)", 0.2, 10.0));
		}
		else if (flag == e is FlowControlValve)
		{
			list.Add(K("OpeningPercent", "Throttle opening (%)", 1.0, 100.0));
		}
		else if (flag == e is CompensatedFlowControl)
		{
			list.Add(K("FlowLpm", "Flow (l/min)", 0.5, 50.0));
		}
		else if (flag == e is SemiRotaryActuator)
		{
			list.Add(K("StrokeTime", "Swivel time (s)", 0.1, 10.0));
		}
		else if (flag == e is CylinderBase)
		{
			bool flag2 = e.Ports[0].Kind == PortKind.Hydraulic;
			if (!flag2)
			{
				list.Add(K("StrokeTime", "Stroke time, ideal mode (s)", 0.1, 10.0));
			}
			list.Add(K("LoadForceN", "Load force (N)", -2000.0, flag2 ? 50000 : 3000));
			if (!flag2)
			{
				list.Add(K("LoadMassKg", "Moving mass (kg)", 0.0, 200.0));
			}
			list.Add(K("FrictionN", "Friction (N)", 0.0, 500.0));
		}
		else if (flag == e is DirectionalValve)
		{
			DirectionalValve obj = (DirectionalValve)e;
			if (obj.ShowProperty("DelaySeconds"))
			{
				list.Add(K("DelaySeconds", "Delay (s)", 0.0, 20.0));
			}
			if (obj.ShowProperty("SwitchingPressure"))
			{
				list.Add(K("SwitchingPressure", "Switching pressure (bar)", 0.5, 10.0));
			}
		}
		else if (flag == e is ElectricCoil)
		{
			if (e.ShowProperty("DelaySeconds"))
			{
				list.Add(K("DelaySeconds", "Delay (s)", 0.0, 20.0));
			}
		}
		else if (flag == e is AirMotor)
		{
			list.Add(K("NominalSpeed", "Speed at full flow (rpm)", 0.0, 3000.0));
		}
		else if (flag == e is HydraulicPump)
		{
			list.Add(K("FlowLpm", "Pump flow (l/min)", 0.5, 100.0));
		}
		else if (flag == e is ReliefValve)
		{
			list.Add(K("Setting", "Opening pressure (bar)", 5.0, 250.0));
		}
		else if (flag == e is HydraulicMotor)
		{
			list.Add(K("LoadPressureBar", "Load pressure (bar)", 0.0, 200.0));
		}
		else if (flag == e is Compressor)
		{
			list.Add(K("DeliveryNlMin", "Delivery (NL/min)", 10.0, 2000.0));
		}
		else if (flag == e is PressureSwitch)
		{
			list.Add(K("Setting", "Switching pressure (bar)", -0.9, 10.0));
		}
		else if (flag == e is VacuumGenerator)
		{
			list.Add(K("VacuumLevel", "Vacuum (bar)", -0.95, -0.1));
		}
		list.RemoveAll([SpecialName] (Knob kn) => (object)e.GetType().GetProperty(kn.PropertyName) == null);
		return list;
	}

	public static double GetValue(CircuitElement e, Knob knob)
	{
		return Convert.ToDouble(RuntimeHelpers.GetObjectValue(e.GetType().GetProperty(knob.PropertyName).GetValue(e)));
	}

	public static void SetValue(CircuitElement e, Knob knob, double value)
	{
		PropertyInfo property = e.GetType().GetProperty(knob.PropertyName);
		property.SetValue(e, RuntimeHelpers.GetObjectValue(Convert.ChangeType(((object)property.PropertyType == typeof(int)) ? Math.Round(value) : value, property.PropertyType)));
	}

	public static List<(string Name, string Value)> Inspect(object target)
	{
		List<(string, string)> list = new List<(string, string)>();
		VB_0024AnonymousDelegate_6<Port, string> vB_0024AnonymousDelegate_ = [SpecialName] (Port p) =>
		{
			if (p.Kind == PortKind.Electric)
			{
				return (p.State == PortState.Pressurized) ? "+24 V" : ((p.State == PortState.Exhausted) ? "0 V" : "not connected to a supply");
			}
			string arg = ((p.State == PortState.Pressurized) ? "supplied" : ((p.State == PortState.Exhausted) ? "vented" : "closed in"));
			return $"{p.Pressure:0.00} bar ({arg})";
		};
		if (target is CircuitElement)
		{
			CircuitElement circuitElement = (CircuitElement)target;
			list.AddRange(circuitElement.InspectValues());
			foreach (Port port in circuitElement.Ports)
			{
				list.Add(($"Port {port.Name}", vB_0024AnonymousDelegate_(port)));
			}
		}
		else if (target is Tube)
		{
			Tube tube = (Tube)target;
			list.Add(($"End at {tube.A.Owner.Label}:{tube.A.Name}", vB_0024AnonymousDelegate_(tube.A)));
			list.Add(($"End at {tube.B.Owner.Label}:{tube.B.Name}", vB_0024AnonymousDelegate_(tube.B)));
		}
		return list;
	}
}
