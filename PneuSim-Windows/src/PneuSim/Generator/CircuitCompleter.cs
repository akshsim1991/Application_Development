using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace PneuSim;

/// <summary>
/// "Complete my circuit": reads the components placed in the workspace, works out what they are for, and adds what is missing and the
/// connections. What it understands:
///  - logic valves (shuttle = OR, two-pressure = AND): the nearest push-button / selector / roller valves feed their inputs, the output
///    drives the cylinder (single-acting) or a pilot valve (added if needed);
///  - one valve can drive several actuators in parallel; a valve is only added when no suitable valve exists;
///  - compressed air: supply, then shut-off valve and service unit / regulator in a chain, then every valve;
///  - pressure gauges and switches on the supply line, silencers on valve exhausts, flow control valves in the cylinder lines;
///  - solenoid valves get a coil, push button, +24 V and 0 V;
///  - hydraulics: pump, tank, relief valve, 4/2 or 4/3 valve and cylinder or motor.
/// Everything that is already connected is left alone.
/// </summary>
public static class CircuitCompleter
{
	public static bool CanComplete(Circuit workspace)
	{
		return workspace != null && workspace.Elements.Any((CircuitElement e) => !(e is TextNote) && !(e is PageConnector));
	}

	private static bool Free(Port p)
	{
		return p != null && p.ConnectionCount == 0;
	}

	private static float Dist(CircuitElement a, CircuitElement b)
	{
		return Math.Abs(a.X - b.X) + Math.Abs(a.Y - b.Y);
	}

	private static string Name(CircuitElement e)
	{
		return string.IsNullOrWhiteSpace(e.Label) ? e.DisplayName : (e.DisplayName + " " + e.Label);
	}

	private static bool IsLogic(CircuitElement e)
	{
		return e is ShuttleValve || e is TwoPressureValve;
	}

	/// <summary>Working (outlet) ports of a directional valve in the order used to feed actuators: 4 then 2.</summary>
	private static List<Port> WorkingPorts(DirectionalValve v, PortKind kind)
	{
		HashSet<string> exhausts = new HashSet<string>(from w in v.WorkingPortLayout()
			where w.Vents
			select w.Name);
		return (from n in v.WorkingPortNames()
			where n != "1" && n != "P" && n != "T" && !exhausts.Contains(n)
			orderby n descending
			select v.GetPort(n) into p
			where p != null && p.Kind == kind
			select p).ToList();
	}

	private static bool IsSignalValve(DirectionalValve v)
	{
		return v.Ports.Any((Port p) => p.Kind == PortKind.Pneumatic) && (v.Actuator == ValveActuator.PushButton || v.Actuator == ValveActuator.Selector || v.Actuator == ValveActuator.RollerLever || v.Actuator == ValveActuator.IdleReturnRoller) && v.ReturnType == ValveReturn.Spring && v.Positions == 2 && WorkingPorts(v, PortKind.Pneumatic).Count == 1;
	}

	private static Port PilotPort(DirectionalValve v)
	{
		if (v.Actuator == ValveActuator.Pilot || v.Actuator == ValveActuator.DelayedPilot)
		{
			return v.GetPort(v.PilotPortName(leftSide: true));
		}
		if (v.ReturnType == ValveReturn.Pilot)
		{
			return v.GetPort(v.PilotPortName(leftSide: false));
		}
		return null;
	}

	private static void Link(Circuit c, List<string> log, Port a, Port b, string text)
	{
		if (a != null && b != null && c.Connect(a, b) != null)
		{
			log.Add(text);
		}
	}

	/// <summary>Returns a completed copy of the workspace circuit and a plain-English list of what was added and connected.</summary>
	public static GeneratedCircuit Complete(Circuit workspace)
	{
		Circuit c = Circuit.FromXml(workspace.ToXml());
		List<string> log = new List<string>();
		List<CircuitElement> els = c.Elements.ToList();
		bool electric = els.Any((CircuitElement e) => e is ElectricContact || e is ElectricCoil || e is PowerTerminal);
		List<DirectionalValve> valves = els.OfType<DirectionalValve>().Where((DirectionalValve v) => v.Ports.Any((Port p) => p.Kind == PortKind.Pneumatic)).ToList();
		List<CircuitElement> actuators = (from e in els
			where ((e is CylinderBase cb && cb.Ports[0].Kind == PortKind.Pneumatic) || e is AirMotor) && e.Ports.Any((Port p) => p.Kind == PortKind.Pneumatic && Free(p))
			orderby e.X, e.Y
			select e).ToList();
		HashSet<DirectionalValve> used = new HashSet<DirectionalValve>();
		HashSet<CircuitElement> handled = new HashSet<CircuitElement>();
		HashSet<DirectionalValve> inLogic = new HashSet<DirectionalValve>();
		foreach (CircuitElement L in els.Where(IsLogic).OrderBy((CircuitElement e) => e.X).ThenBy((CircuitElement e) => e.Y).ToList())
		{
			List<Port> list = new List<Port>();
			foreach (string n in new string[2] { "1a", "1b" })
			{
				if (Free(L.GetPort(n)))
				{
					list.Add(L.GetPort(n));
				}
			}
			foreach (Port inPort in list)
			{
				DirectionalValve directionalValve = (from v in valves
					where !used.Contains(v) && IsSignalValve(v) && Free(WorkingPorts(v, PortKind.Pneumatic)[0])
					orderby Dist(v, L)
					select v).FirstOrDefault();
				if (directionalValve == null)
				{
					directionalValve = Library.V32(ValveActuator.PushButton);
					c.Add(directionalValve, L.X - 170f + (float)list.IndexOf(inPort) * 150f, L.Y + 120f);
					Naming.NameNewElement(directionalValve, c.Elements);
					valves.Add(directionalValve);
					log.Add("Added " + Name(directionalValve) + " as an input of " + Name(L) + ".");
				}
				used.Add(directionalValve);
				inLogic.Add(directionalValve);
				Link(c, log, WorkingPorts(directionalValve, PortKind.Pneumatic)[0], inPort, "Connected " + Name(directionalValve) + " port 2 to " + Name(L) + " input " + inPort.Name + ".");
			}
			Port port = L.GetPort("2");
			if (!Free(port))
			{
				continue;
			}
			DirectionalValve directionalValve2 = (from v in valves
				where !used.Contains(v) && PilotPort(v) != null && Free(PilotPort(v))
				orderby Dist(v, L)
				select v).FirstOrDefault();
			if (directionalValve2 != null)
			{
				used.Add(directionalValve2);
				Link(c, log, port, PilotPort(directionalValve2), "Connected the output of " + Name(L) + " to the pilot of " + Name(directionalValve2) + ".");
				continue;
			}
			CircuitElement circuitElement = actuators.Where((CircuitElement a) => !handled.Contains(a)).OrderBy((CircuitElement a) => Dist(a, L)).FirstOrDefault();
			if (circuitElement == null)
			{
				continue;
			}
			List<Port> list2 = circuitElement.Ports.Where((Port p) => p.Kind == PortKind.Pneumatic && Free(p)).ToList();
			if (list2.Count == 1)
			{
				handled.Add(circuitElement);
				Link(c, log, port, list2[0], "Connected the output of " + Name(L) + " to " + Name(circuitElement) + " port " + list2[0].Name + " (the logic valve drives it directly).");
				continue;
			}
			Valve52 valve52 = Library.V52(ValveActuator.Pilot, ValveReturn.Spring);
			c.Add(valve52, circuitElement.X - 90f, circuitElement.Y + 170f);
			Naming.NameNewElement(valve52, c.Elements);
			valves.Add(valve52);
			used.Add(valve52);
			handled.Add(circuitElement);
			log.Add("Added " + Name(valve52) + " to drive " + Name(circuitElement) + " from " + Name(L) + ".");
			Link(c, log, port, PilotPort(valve52), "Connected the output of " + Name(L) + " to the pilot of " + Name(valve52) + ".");
			List<Port> list3 = WorkingPorts(valve52, PortKind.Pneumatic);
			for (int i = 0; i < list2.Count && i < list3.Count; i++)
			{
				Link(c, log, list3[i], list2[i], "Connected " + Name(valve52) + " port " + list3[i].Name + " to " + Name(circuitElement) + " port " + list2[i].Name + ".");
			}
		}
		foreach (CircuitElement a2 in actuators.Where((CircuitElement a) => !handled.Contains(a)).ToList())
		{
			List<Port> freePorts = a2.Ports.Where((Port p) => p.Kind == PortKind.Pneumatic && Free(p)).ToList();
			int need = freePorts.Count;
			List<DirectionalValve> list4 = valves.Where((DirectionalValve v) => !inLogic.Contains(v) && WorkingPorts(v, PortKind.Pneumatic).Count >= need && PilotPort(v) == null).ToList();
			DirectionalValve directionalValve3 = (from v in list4
				where !used.Contains(v)
				orderby Dist(v, a2)
				select v).FirstOrDefault();
			bool flag = false;
			if (directionalValve3 == null && list4.Count > 0)
			{
				directionalValve3 = list4.OrderBy((DirectionalValve v) => Dist(v, a2)).First();
				flag = true;
			}
			if (directionalValve3 == null)
			{
				ValveActuator actuator = (electric ? ValveActuator.Solenoid : ValveActuator.PushButton);
				directionalValve3 = ((need >= 2) ? ((DirectionalValve)Library.V52(actuator, ValveReturn.Spring)) : ((DirectionalValve)Library.V32(actuator)));
				c.Add(directionalValve3, a2.X - 90f, a2.Y + 170f);
				Naming.NameNewElement(directionalValve3, c.Elements);
				valves.Add(directionalValve3);
				log.Add("Added " + Name(directionalValve3) + " to control " + Name(a2) + ".");
			}
			used.Add(directionalValve3);
			List<Port> list5 = WorkingPorts(directionalValve3, PortKind.Pneumatic);
			if (!flag)
			{
				list5 = list5.Where((Port p) => Free(p)).ToList();
			}
			for (int j = 0; j < freePorts.Count && j < list5.Count; j++)
			{
				Link(c, log, list5[j], freePorts[j], (flag ? ("Connected " + Name(a2) + " port " + freePorts[j].Name + " in parallel to " + Name(directionalValve3) + " port " + list5[j].Name + " (one valve drives both).") : ("Connected " + Name(directionalValve3) + " port " + list5[j].Name + " to " + Name(a2) + " port " + freePorts[j].Name + ".")));
			}
		}
		List<DirectionalValve> list6 = valves.Where((DirectionalValve v) => Free(v.GetPort("1"))).ToList();
		Port port2 = null;
		if (list6.Count > 0 || els.Any((CircuitElement e) => (e is PressureGauge || e is Silencer) && Free(e.Ports[0])))
		{
			CircuitElement reference = (list6.Count > 0) ? ((CircuitElement)list6[0]) : els.First((CircuitElement e) => e is PressureGauge || e is Silencer);
			AirSupply airSupply = (from e in c.Elements.OfType<AirSupply>()
				orderby Dist(e, reference)
				select e).FirstOrDefault();
			if (airSupply == null && list6.Count > 0)
			{
				airSupply = c.Add(new AirSupply(), list6.Average((DirectionalValve v) => v.X) + 30f, list6.Max((DirectionalValve v) => v.Y) + 170f);
				Naming.NameNewElement(airSupply, c.Elements);
				log.Add("Added " + Name(airSupply) + " (compressed air).");
			}
			if (airSupply != null)
			{
				port2 = airSupply.Ports[0];
				foreach (CircuitElement item in (from e in c.Elements
					where (e is ShutOffValve && Free(e.GetPort("1")) && Free(e.GetPort("2"))) || (e is PressureRegulator && Free(e.GetPort("1")) && Free(e.GetPort("2")))
					orderby (e is PressureRegulator) ? 1 : 0, Dist(e, airSupply)
					select e).ToList())
				{
					Port prev = port2;
					Link(c, log, prev, item.GetPort("1"), "Connected the air line to " + Name(item) + " (air preparation).");
					port2 = item.GetPort("2");
				}
				foreach (DirectionalValve item2 in list6)
				{
					Link(c, log, port2, item2.GetPort("1"), "Connected compressed air to " + Name(item2) + " port 1.");
				}
				foreach (CircuitElement item3 in c.Elements.Where((CircuitElement e) => (e is PressureGauge || e is PressureSwitch) && Free(e.Ports[0]) && e.Ports[0].Kind == PortKind.Pneumatic).ToList())
				{
					Link(c, log, port2, item3.Ports[0], "Connected " + Name(item3) + " to the air line.");
				}
			}
		}
		foreach (CircuitElement item4 in c.Elements.Where((CircuitElement e) => e is Silencer && Free(e.Ports[0])).ToList())
		{
			Port port3 = (from v in valves
				from p in v.Ports
				where p.Kind == PortKind.Pneumatic && p.VentsWhenOpen && Free(p)
				orderby Dist(v, item4)
				select p).FirstOrDefault();
			if (port3 != null)
			{
				Link(c, log, port3.Owner.GetPort(port3.Name), item4.Ports[0], "Connected " + Name(item4) + " to the exhaust of " + Name(port3.Owner) + ".");
			}
		}
		foreach (FlowControlValve fcv in c.Elements.OfType<FlowControlValve>().Where((FlowControlValve f) => f.Ports[0].Kind == PortKind.Pneumatic && Free(f.GetPort("1")) && Free(f.GetPort("2"))).ToList())
		{
			Tube tube = (from t in c.Tubes
				where t.A.Kind == PortKind.Pneumatic && ((t.A.Owner is DirectionalValve && (t.B.Owner is CylinderBase || t.B.Owner is AirMotor)) || (t.B.Owner is DirectionalValve && (t.A.Owner is CylinderBase || t.A.Owner is AirMotor)))
				orderby Math.Min(Dist(t.A.Owner, fcv), Dist(t.B.Owner, fcv))
				select t).FirstOrDefault();
			if (tube != null)
			{
				Port a3 = ((tube.A.Owner is DirectionalValve) ? tube.A : tube.B);
				Port b2 = ((tube.A.Owner is DirectionalValve) ? tube.B : tube.A);
				c.RemoveTube(tube);
				c.Connect(a3, fcv.GetPort("1"));
				c.Connect(fcv.GetPort("2"), b2);
				log.Add("Placed " + Name(fcv) + " in the line between " + Name(a3.Owner) + " and " + Name(b2.Owner) + " (speed control).");
			}
		}
		foreach (DirectionalValve item5 in valves.ToList())
		{
			List<string> list7 = new List<string>();
			if (item5.Actuator == ValveActuator.Solenoid)
			{
				list7.Add(item5.SolenoidLabel);
			}
			if (item5.ReturnType == ValveReturn.Solenoid)
			{
				list7.Add(item5.ReturnSolenoidLabel);
			}
			foreach (string label in list7)
			{
				ElectricCoil electricCoil = c.Elements.OfType<ElectricCoil>().FirstOrDefault((ElectricCoil k) => k.Kind == CoilKind.Solenoid && string.Equals(k.Label, label, StringComparison.OrdinalIgnoreCase));
				if (electricCoil == null)
				{
					float x = c.Elements.Max((CircuitElement e) => e.X) + 260f;
					electricCoil = c.Add(new ElectricCoil
					{
						Kind = CoilKind.Solenoid
					}, x, item5.Y);
					electricCoil.Label = label;
					log.Add("Added solenoid " + label + " for " + Name(item5) + ".");
				}
				if (Free(electricCoil.GetPort("A1")) && Free(electricCoil.GetPort("A2")))
				{
					WireCoil(c, electricCoil, log);
				}
			}
		}
		CompleteHydraulics(c, log);
		StringBuilder stringBuilder = new StringBuilder();
		stringBuilder.AppendLine("COMPLETED FROM THE COMPONENTS IN THE WORKSPACE");
		stringBuilder.AppendLine();
		if (log.Count == 0)
		{
			stringBuilder.AppendLine("Nothing to add: every component that PneuSim can complete already has its connections.");
			stringBuilder.AppendLine("Place an actuator (cylinder or motor), a valve or a compressed air supply, and try again, or generate a circuit from a movement sequence.");
		}
		else
		{
			foreach (string item6 in log)
			{
				stringBuilder.AppendLine("  - " + item6);
			}
			stringBuilder.AppendLine();
			stringBuilder.AppendLine("Components and connections that were already in the workspace were kept.");
			stringBuilder.AppendLine("Press Start (F9). With Settings > Simulation > Automatic operation the start button is pressed for you.");
		}
		return new GeneratedCircuit
		{
			Circuit = c,
			Explanation = stringBuilder.ToString(),
			MergedIntoWorkspace = true
		};
	}

	/// <summary>Oil circuits: pump, tank, relief valve, 4/2 or 4/3 valve, cylinders and motors.</summary>
	private static void CompleteHydraulics(Circuit c, List<string> log)
	{
		List<CircuitElement> list = c.Elements.ToList();
		List<CircuitElement> list2 = list.Where((CircuitElement e) => (e is HydraulicCylinder || e is HydraulicSingleActingCylinder || e is HydraulicMotor) && e.Ports.Any((Port p) => p.Kind == PortKind.Hydraulic && Free(p))).ToList();
		List<DirectionalValve> list3 = list.OfType<DirectionalValve>().Where((DirectionalValve v) => v.GetPort("P") != null && v.GetPort("P").Kind == PortKind.Hydraulic).ToList();
		if (list2.Count == 0 && list3.Count == 0)
		{
			return;
		}
		HashSet<DirectionalValve> hashSet = new HashSet<DirectionalValve>();
		foreach (CircuitElement item in list2)
		{
			List<Port> list4 = item.Ports.Where((Port p) => p.Kind == PortKind.Hydraulic && Free(p)).ToList();
			DirectionalValve directionalValve = list3.Where((DirectionalValve v) => !hashSet.Contains(v) && Free(v.GetPort("A")) && Free(v.GetPort("B"))).OrderBy((DirectionalValve v) => Dist(v, item)).FirstOrDefault() ?? list3.OrderBy((DirectionalValve v) => Dist(v, item)).FirstOrDefault();
			bool flag = directionalValve != null && hashSet.Contains(directionalValve);
			if (directionalValve == null)
			{
				directionalValve = new HydraulicValve43();
				c.Add(directionalValve, item.X - 90f, item.Y + 170f);
				Naming.NameNewElement(directionalValve, c.Elements);
				list3.Add(directionalValve);
				log.Add("Added " + Name(directionalValve) + " to control " + Name(item) + ".");
			}
			hashSet.Add(directionalValve);
			string[] array = new string[2] { "A", "B" };
			for (int i = 0; i < list4.Count && i < 2; i++)
			{
				Port port = directionalValve.GetPort(array[i]);
				if (flag || Free(port))
				{
					Link(c, log, port, list4[i], "Connected " + Name(directionalValve) + " port " + array[i] + " to " + Name(item) + " port " + list4[i].Name + (flag ? " (one valve drives both)." : "."));
				}
			}
		}
		HydraulicPump hydraulicPump = c.Elements.OfType<HydraulicPump>().FirstOrDefault();
		HydraulicTank hydraulicTank = c.Elements.OfType<HydraulicTank>().FirstOrDefault();
		List<DirectionalValve> list5 = list3.Where((DirectionalValve v) => Free(v.GetPort("P")) || Free(v.GetPort("T"))).ToList();
		if (list5.Count == 0)
		{
			return;
		}
		if (hydraulicPump == null)
		{
			hydraulicPump = c.Add(new HydraulicPump(), list5.Average((DirectionalValve v) => v.X) - 150f, list5.Max((DirectionalValve v) => v.Y) + 170f);
			Naming.NameNewElement(hydraulicPump, c.Elements);
			log.Add("Added " + Name(hydraulicPump) + ".");
		}
		if (hydraulicTank == null)
		{
			hydraulicTank = c.Add(new HydraulicTank(), list5.Average((DirectionalValve v) => v.X) + 150f, list5.Max((DirectionalValve v) => v.Y) + 170f);
			Naming.NameNewElement(hydraulicTank, c.Elements);
			log.Add("Added " + Name(hydraulicTank) + ".");
		}
		ReliefValve reliefValve = c.Elements.OfType<ReliefValve>().FirstOrDefault((ReliefValve r) => r.Ports[0].Kind == PortKind.Hydraulic);
		if (reliefValve == null)
		{
			reliefValve = c.Add(new ReliefValve(), hydraulicPump.X + 70f, hydraulicPump.Y - 90f);
			Naming.NameNewElement(reliefValve, c.Elements);
			log.Add("Added a pressure relief valve to protect the pump.");
		}
		if (Free(reliefValve.GetPort("P")))
		{
			Link(c, log, hydraulicPump.GetPort("P"), reliefValve.GetPort("P"), "Connected " + Name(reliefValve) + " to the pump line.");
		}
		if (Free(reliefValve.GetPort("T")))
		{
			Link(c, log, hydraulicTank.GetPort("T"), reliefValve.GetPort("T"), "Connected " + Name(reliefValve) + " to the tank.");
		}
		foreach (DirectionalValve item2 in list5)
		{
			if (Free(item2.GetPort("P")))
			{
				Link(c, log, hydraulicPump.GetPort("P"), item2.GetPort("P"), "Connected the pump to " + Name(item2) + " port P.");
			}
			if (Free(item2.GetPort("T")))
			{
				Link(c, log, hydraulicTank.GetPort("T"), item2.GetPort("T"), "Connected " + Name(item2) + " port T to the tank.");
			}
		}
	}

	private static void WireCoil(Circuit c, ElectricCoil coil, List<string> log)
	{
		PowerTerminal powerTerminal = c.Elements.OfType<PowerTerminal>().FirstOrDefault((PowerTerminal t) => t.Polarity == Polarity.Plus24V);
		PowerTerminal powerTerminal2 = c.Elements.OfType<PowerTerminal>().FirstOrDefault((PowerTerminal t) => t.Polarity == Polarity.Zero0V);
		ElectricContact electricContact = c.Elements.OfType<ElectricContact>().FirstOrDefault((ElectricContact k) => k.Operator == ContactOperator.PushButton && !k.NormallyClosed && Free(k.GetPort("1")) && Free(k.GetPort("2")));
		if (electricContact == null)
		{
			electricContact = c.Add(new ElectricContact
			{
				Operator = ContactOperator.PushButton
			}, coil.X, coil.Y - 150f);
			Naming.NameNewElement(electricContact, c.Elements);
			log.Add("Added push button " + electricContact.Label + " for " + coil.Label + ".");
		}
		if (powerTerminal == null)
		{
			powerTerminal = c.Add(new PowerTerminal
			{
				Polarity = Polarity.Plus24V
			}, coil.X, electricContact.Y - 90f);
			log.Add("Added +24 V.");
		}
		if (powerTerminal2 == null)
		{
			powerTerminal2 = c.Add(new PowerTerminal
			{
				Polarity = Polarity.Zero0V
			}, coil.X, coil.Y + 110f);
			log.Add("Added 0 V.");
		}
		if (c.Connect(powerTerminal.GetPort("1"), electricContact.GetPort("1")) != null && c.Connect(electricContact.GetPort("2"), coil.GetPort("A1")) != null && c.Connect(coil.GetPort("A2"), powerTerminal2.GetPort("1")) != null)
		{
			log.Add("Wired +24 V -> " + electricContact.Label + " -> " + coil.Label + " -> 0 V.");
		}
	}
}
