using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace PneuSim;

/// <summary>
/// "Complete my circuit": reads the components already placed in the workspace and adds what is missing and the connections between them.
/// Pneumatic: each actuator gets a valve (an existing one nearby, or a new one), every valve gets compressed air (an existing supply, or a new one)
/// and, if the circuit is electro-pneumatic, every solenoid gets a coil, a push button, +24 V and 0 V. Everything already connected is left alone.
/// </summary>
public static class CircuitCompleter
{
	public static bool CanComplete(Circuit workspace)
	{
		return workspace != null && workspace.Elements.Any((CircuitElement e) => !(e is TextNote) && !(e is PageConnector));
	}

	private static bool Free(Port p)
	{
		return p.ConnectionCount == 0;
	}

	private static float Dist(CircuitElement a, CircuitElement b)
	{
		return Math.Abs(a.X - b.X) + Math.Abs(a.Y - b.Y);
	}

	private static string Name(CircuitElement e)
	{
		return string.IsNullOrWhiteSpace(e.Label) ? e.DisplayName : (e.DisplayName + " " + e.Label);
	}

	private static List<Port> WorkingPorts(DirectionalValve v)
	{
		HashSet<string> exhausts = new HashSet<string>(from w in v.WorkingPortLayout()
			where w.Vents
			select w.Name);
		return (from n in v.WorkingPortNames()
			where n != "1" && !exhausts.Contains(n)
			orderby n descending
			select v.GetPort(n) into p
			where p != null && p.Kind == PortKind.Pneumatic
			select p).ToList();
	}

	/// <summary>Returns a completed copy of the workspace circuit and a plain-English list of what was added and connected.</summary>
	public static GeneratedCircuit Complete(Circuit workspace)
	{
		Circuit circuit = Circuit.FromXml(workspace.ToXml());
		List<string> log = new List<string>();
		List<CircuitElement> list = circuit.Elements.ToList();
		bool electric = list.Any((CircuitElement e) => e is ElectricContact || e is ElectricCoil || e is PowerTerminal);
		List<CircuitElement> actuators = (from e in list
			where ((e is CylinderBase cylinderBase && cylinderBase.Ports[0].Kind == PortKind.Pneumatic) || e is AirMotor) && e.Ports.Any((Port p) => p.Kind == PortKind.Pneumatic && Free(p))
			orderby e.X, e.Y
			select e).ToList();
		List<DirectionalValve> valves = list.OfType<DirectionalValve>().Where((DirectionalValve v) => v.Ports.Any((Port p) => p.Kind == PortKind.Pneumatic)).ToList();
		HashSet<DirectionalValve> used = new HashSet<DirectionalValve>();
		foreach (CircuitElement a in actuators)
		{
			List<Port> freePorts = a.Ports.Where((Port p) => p.Kind == PortKind.Pneumatic && Free(p)).ToList();
			int need = freePorts.Count;
			DirectionalValve valve = (from v in valves
				where !used.Contains(v) && WorkingPorts(v).Count((Port p) => Free(p)) >= need
				orderby Dist(v, a)
				select v).FirstOrDefault();
			if (valve == null)
			{
				ValveActuator act = (electric ? ValveActuator.Solenoid : ValveActuator.PushButton);
				DirectionalValve directionalValve = ((need >= 2) ? ((DirectionalValve)Library.V52(act, ValveReturn.Spring)) : ((DirectionalValve)Library.V32(act)));
				circuit.Add(directionalValve, a.X - 90f, a.Y + 170f);
				Naming.NameNewElement(directionalValve, circuit.Elements);
				valves.Add(directionalValve);
				valve = directionalValve;
				log.Add("Added " + Name(directionalValve) + " to control " + Name(a) + ".");
			}
			used.Add(valve);
			List<Port> list2 = WorkingPorts(valve).Where((Port p) => Free(p)).ToList();
			for (int i = 0; i < freePorts.Count && i < list2.Count; i++)
			{
				if (circuit.Connect(list2[i], freePorts[i]) != null)
				{
					log.Add("Connected " + Name(valve) + " port " + list2[i].Name + " to " + Name(a) + " port " + freePorts[i].Name + ".");
				}
			}
		}
		List<DirectionalValve> list3 = valves.Where((DirectionalValve v) => v.GetPort("1") != null && Free(v.GetPort("1"))).ToList();
		if (list3.Count > 0)
		{
			AirSupply airSupply = (from e in circuit.Elements.OfType<AirSupply>()
				orderby Dist(e, list3[0])
				select e).FirstOrDefault();
			if (airSupply == null)
			{
				airSupply = circuit.Add(new AirSupply(), list3.Average((DirectionalValve v) => v.X) + 30f, list3.Max((DirectionalValve v) => v.Y) + 170f);
				Naming.NameNewElement(airSupply, circuit.Elements);
				log.Add("Added " + Name(airSupply) + " (compressed air).");
			}
			Port port = airSupply.Ports[0];
			PressureRegulator pressureRegulator = circuit.Elements.OfType<PressureRegulator>().FirstOrDefault((PressureRegulator r) => Free(r.GetPort("1")) && Free(r.GetPort("2")));
			if (pressureRegulator != null && circuit.Connect(port, pressureRegulator.GetPort("1")) != null)
			{
				log.Add("Connected " + Name(airSupply) + " to " + Name(pressureRegulator) + " (air preparation).");
				port = pressureRegulator.GetPort("2");
			}
			foreach (DirectionalValve item in list3)
			{
				if (circuit.Connect(port, item.GetPort("1")) != null)
				{
					log.Add("Connected compressed air to " + Name(item) + " port 1.");
				}
			}
		}
		foreach (DirectionalValve item2 in valves.ToList())
		{
			List<string> list4 = new List<string>();
			if (item2.Actuator == ValveActuator.Solenoid)
			{
				list4.Add(item2.SolenoidLabel);
			}
			if (item2.ReturnType == ValveReturn.Solenoid)
			{
				list4.Add(item2.ReturnSolenoidLabel);
			}
			foreach (string label in list4)
			{
				ElectricCoil electricCoil = circuit.Elements.OfType<ElectricCoil>().FirstOrDefault((ElectricCoil c) => c.Kind == CoilKind.Solenoid && string.Equals(c.Label, label, StringComparison.OrdinalIgnoreCase));
				if (electricCoil == null)
				{
					float num = circuit.Elements.Max((CircuitElement e) => e.X) + 260f;
					electricCoil = circuit.Add(new ElectricCoil
					{
						Kind = CoilKind.Solenoid
					}, num, item2.Y);
					electricCoil.Label = label;
					log.Add("Added solenoid " + label + " for " + Name(item2) + ".");
				}
				if (Free(electricCoil.GetPort("A1")) && Free(electricCoil.GetPort("A2")))
				{
					WireCoil(circuit, electricCoil, log);
				}
			}
		}
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
			foreach (string item3 in log)
			{
				stringBuilder.AppendLine("  - " + item3);
			}
			stringBuilder.AppendLine();
			stringBuilder.AppendLine("Components and connections that were already in the workspace were kept.");
			stringBuilder.AppendLine("Press Start (F9). With Settings > Simulation > Automatic operation the start button is pressed for you.");
		}
		return new GeneratedCircuit
		{
			Circuit = circuit,
			Explanation = stringBuilder.ToString(),
			MergedIntoWorkspace = true
		};
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
