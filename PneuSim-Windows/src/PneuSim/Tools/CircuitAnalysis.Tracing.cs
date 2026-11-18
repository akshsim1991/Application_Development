using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using Microsoft.VisualBasic;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public sealed partial class CircuitAnalysis
{

	private static Dictionary<Port, List<Port>> BuildTubeMap(Circuit c)
	{
		_Closure_0024__0_002D0 arg = default(_Closure_0024__0_002D0);
		_Closure_0024__0_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__0_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_map = new Dictionary<Port, List<Port>>();
		VB_0024AnonymousDelegate_10<Port, Port> vB_0024AnonymousDelegate_ = [SpecialName] (Port a, Port b) =>
		{
			if (!CS_0024_003C_003E8__locals0._0024VB_0024Local_map.ContainsKey(a))
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_map[a] = new List<Port>();
			}
			CS_0024_003C_003E8__locals0._0024VB_0024Local_map[a].Add(b);
		};
		foreach (Tube tube in c.Tubes)
		{
			vB_0024AnonymousDelegate_(tube.A, tube.B);
			vB_0024AnonymousDelegate_(tube.B, tube.A);
		}
		List<PageConnector> list = c.Elements.OfType<PageConnector>().ToList();
		foreach (PageConnector item in list)
		{
			foreach (PageConnector item2 in list)
			{
				if (item != item2 && item.Matches(item2))
				{
					vB_0024AnonymousDelegate_(item.Ports[0], item2.Ports[0]);
				}
			}
		}
		return CS_0024_003C_003E8__locals0._0024VB_0024Local_map;
	}

	private static IEnumerable<Port> Neighbours(Dictionary<Port, List<Port>> map, Port p)
	{
		List<Port> value = null;
		if (!map.TryGetValue(p, out value))
		{
			return Enumerable.Empty<Port>();
		}
		return value;
	}

	private static string Name(CircuitElement e)
	{
		if (!string.IsNullOrWhiteSpace(e.Label))
		{
			return e.Label;
		}
		return e.DisplayName.ToLowerInvariant();
	}

	private static IEnumerable<Port> PassThrough(Port p)
	{
		CircuitElement owner = p.Owner;
		if (owner is Junction)
		{
			return Enumerable.Empty<Port>();
		}
		if (owner is FlowControlValve || owner is CheckValve || owner is PressureRegulator || owner is CompensatedFlowControl)
		{
			return owner.Ports.Where([SpecialName] (Port o) => o != p);
		}
		if (owner is CounterbalanceValve && Operators.CompareString(p.Name, "X", TextCompare: false) != 0)
		{
			return owner.Ports.Where([SpecialName] (Port o) => o != p && Operators.CompareString(o.Name, "X", TextCompare: false) != 0);
		}
		if (owner is QuickExhaustValve && Operators.CompareString(p.Name, "3", TextCompare: false) != 0)
		{
			return owner.Ports.Where([SpecialName] (Port o) => o != p && Operators.CompareString(o.Name, "3", TextCompare: false) != 0);
		}
		return Enumerable.Empty<Port>();
	}

	private static Port TraceToValve(Dictionary<Port, List<Port>> map, Port start, List<CircuitElement> inline)
	{
		HashSet<Port> hashSet = new HashSet<Port> { start };
		Queue<Port> queue = new Queue<Port>();
		queue.Enqueue(start);
		while (queue.Count > 0)
		{
			Port p = queue.Dequeue();
			foreach (Port item in Neighbours(map, p))
			{
				if (!hashSet.Add(item))
				{
					continue;
				}
				if (item.Owner is DirectionalValve && ((DirectionalValve)item.Owner).WorkingPortNames().Contains(item.Name))
				{
					return item;
				}
				List<Port> list = PassThrough(item).ToList();
				if (list.Count > 0 && !inline.Contains(item.Owner))
				{
					inline.Add(item.Owner);
				}
				foreach (Port item2 in list)
				{
					if (hashSet.Add(item2))
					{
						queue.Enqueue(item2);
					}
				}
				if (item.Owner is Junction)
				{
					queue.Enqueue(item);
				}
			}
		}
		return null;
	}

	private static void TraceSignal(Dictionary<Port, List<Port>> map, Port target, MoveTrigger trigger, int depth)
	{
		if (target == null || depth > 4)
		{
			return;
		}
		HashSet<Port> hashSet = new HashSet<Port> { target };
		Queue<Port> queue = new Queue<Port>();
		queue.Enqueue(target);
		List<string> list = new List<string>();
		checked
		{
			while (queue.Count > 0)
			{
				Port p = queue.Dequeue();
				foreach (Port item in Neighbours(map, p))
				{
					if (!hashSet.Add(item))
					{
						continue;
					}
					CircuitElement owner = item.Owner;
					if (owner is Junction)
					{
						queue.Enqueue(item);
						continue;
					}
					if (IsAirSource(owner))
					{
						trigger.AlwaysOn = true;
						list.Add("the air supply directly");
						continue;
					}
					if ((owner is ShuttleValve || owner is TwoPressureValve) && Operators.CompareString(item.Name, "2", TextCompare: false) == 0)
					{
						foreach (Port item2 in owner.Ports.Where([SpecialName] (Port o) => Operators.CompareString(o.Name, "2", TextCompare: false) != 0))
						{
							if (hashSet.Add(item2))
							{
								queue.Enqueue(item2);
							}
						}
						list.Add((owner is ShuttleValve) ? $"shuttle valve {Name(owner)} (OR)" : $"two-pressure valve {Name(owner)} (AND)");
						continue;
					}
					if ((owner is DirectionalValve && Operators.CompareString(item.Name, "2", TextCompare: false) == 0) || (owner is DirectionalValve && Operators.CompareString(item.Name, "4", TextCompare: false) == 0))
					{
						DirectionalValve directionalValve = (DirectionalValve)owner;
						switch (directionalValve.Actuator)
						{
						case ValveActuator.RollerLever:
						case ValveActuator.IdleReturnRoller:
							if (!string.IsNullOrWhiteSpace(directionalValve.TriggerMark))
							{
								trigger.Marks.Add(directionalValve.TriggerMark);
							}
							list.Add($"roller valve {Name(directionalValve)} (operated at {directionalValve.TriggerMark})");
							break;
						case ValveActuator.PushButton:
						case ValveActuator.Selector:
							trigger.Manual.Add(Name(directionalValve));
							list.Add(string.Format("{0} valve {1}", (directionalValve.Actuator == ValveActuator.PushButton) ? "push button" : "selector", Name(directionalValve)));
							break;
						case ValveActuator.DelayedPilot:
							list.Add($"time delay valve {Name(directionalValve)} ({directionalValve.DelaySeconds:0.#} s)");
							TraceSignal(map, directionalValve.GetPort(directionalValve.PilotPortName(leftSide: true)), trigger, depth + 1);
							break;
						default:
							list.Add($"valve {Name(directionalValve)}");
							trigger.Gated = true;
							TraceSignal(map, directionalValve.GetPort(directionalValve.PilotPortName(leftSide: true)), trigger, depth + 1);
							break;
						}
						TraceSignal(map, directionalValve.GetPort("1"), trigger, depth + 1);
						continue;
					}
					foreach (Port item3 in PassThrough(item))
					{
						if (hashSet.Add(item3))
						{
							queue.Enqueue(item3);
						}
					}
				}
			}
			if (list.Count > 0 && depth == 0)
			{
				trigger.Description = string.Join(", ", list);
			}
		}
	}

	public static string CoilCondition(Circuit c, ElectricCoil coil, MoveTrigger trigger)
	{
		return Expression(BuildTubeMap(c), coil.GetPort("A1"), new HashSet<CircuitElement>(), trigger, 0);
	}

	private static string Expression(Dictionary<Port, List<Port>> map, Port node, HashSet<CircuitElement> visited, MoveTrigger trigger, int depth)
	{
		if (depth > 12)
		{
			return "?";
		}
		List<string> list = new List<string>();
		HashSet<Port> hashSet = new HashSet<Port> { node };
		Queue<Port> queue = new Queue<Port>();
		queue.Enqueue(node);
		_Closure_0024__7_002D0 closure_0024__7_002D = default(_Closure_0024__7_002D0);
		while (queue.Count > 0)
		{
			Port port = queue.Dequeue();
			using IEnumerator<Port> enumerator = Neighbours(map, port).Concat((port.Owner is Junction) ? Neighbours(map, port) : Enumerable.Empty<Port>()).GetEnumerator();
			while (enumerator.MoveNext())
			{
				closure_0024__7_002D = new _Closure_0024__7_002D0(closure_0024__7_002D);
				closure_0024__7_002D._0024VB_0024Local_q = enumerator.Current;
				if (!hashSet.Add(closure_0024__7_002D._0024VB_0024Local_q))
				{
					continue;
				}
				CircuitElement owner = closure_0024__7_002D._0024VB_0024Local_q.Owner;
				if (owner is Junction || owner is PageConnector)
				{
					queue.Enqueue(closure_0024__7_002D._0024VB_0024Local_q);
				}
				else if (owner is PowerTerminal && ((PowerTerminal)owner).Polarity == Polarity.Plus24V)
				{
					list.Add("");
				}
				else
				{
					if (!(owner is ElectricContact) || visited.Contains(owner))
					{
						continue;
					}
					ElectricContact electricContact = (ElectricContact)owner;
					visited.Add(electricContact);
					Port node2 = electricContact.Ports.First(closure_0024__7_002D._Lambda_0024__0);
					string text = Expression(map, node2, visited, trigger, checked(depth + 1));
					visited.Remove(electricContact);
					if (Operators.CompareString(text, "?", TextCompare: false) == 0)
					{
						continue;
					}
					string text2 = (string.IsNullOrEmpty(electricContact.Label) ? electricContact.Reference : electricContact.Label);
					if (electricContact.IsSensor)
					{
						if (!electricContact.NormallyClosed)
						{
							trigger?.Marks.Add(electricContact.Reference);
						}
					}
					else if (electricContact.Operator != ContactOperator.Relay && !electricContact.NormallyClosed)
					{
						trigger?.Manual.Add(text2);
					}
					string text3 = (electricContact.NormallyClosed ? ("NOT " + text2) : text2);
					list.Add((Operators.CompareString(text, "", TextCompare: false) == 0) ? text3 : (text + " AND " + text3));
				}
			}
		}
		if (list.Count == 0)
		{
			return "?";
		}
		if (list.Contains(""))
		{
			return "";
		}
		return (list.Count == 1) ? list[0] : ("(" + string.Join(") OR (", list) + ")");
	}
}
