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

	public static string Explain(Circuit c, CircuitElement operate, bool realPhysics)
	{
		_Closure_0024__20_002D0 arg = default(_Closure_0024__20_002D0);
		_Closure_0024__20_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__20_002D0(arg);
		StringBuilder stringBuilder = new StringBuilder();
		List<CircuitElement> elements = c.Elements;
		stringBuilder.AppendLine("WHAT IS IN THE CIRCUIT");
		CS_0024_003C_003E8__locals0._0024VB_0024Local_parts = new List<string>();
		VB_0024AnonymousDelegate_15<int, string, string> obj = [SpecialName] (int n, string singular, string plural) =>
		{
			if (n > 0)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_parts.Add($"{n} {((n == 1) ? singular : plural)}");
			}
		};
		obj(elements.OfType<SingleActingCylinder>().Count([SpecialName] (SingleActingCylinder k) => !(k is HydraulicSingleActingCylinder)), "single-acting cylinder", "single-acting cylinders");
		obj(elements.OfType<DoubleActingCylinder>().Count([SpecialName] (DoubleActingCylinder k) => !(k is HydraulicCylinder) && !(k is SemiRotaryActuator)), "double-acting cylinder", "double-acting cylinders");
		obj(checked(elements.OfType<HydraulicCylinder>().Count() + elements.OfType<HydraulicSingleActingCylinder>().Count()), "hydraulic cylinder", "hydraulic cylinders");
		obj(elements.OfType<DirectionalValve>().Count(), "directional control valve", "directional control valves");
		obj(elements.OfType<FlowControlValve>().Count(), "flow control valve", "flow control valves");
		obj(elements.OfType<LogicValve>().Count(), "logic valve", "logic valves");
		obj(elements.OfType<ElectricCoil>().Count([SpecialName] (ElectricCoil k) => k.Kind == CoilKind.Relay || k.Kind == CoilKind.OnDelayTimer || k.Kind == CoilKind.OffDelayTimer), "relay", "relays");
		obj(elements.OfType<ElectricContact>().Count([SpecialName] (ElectricContact k) => k.Operator == ContactOperator.ProximitySensor || k.Operator == ContactOperator.LimitSwitch), "sensor contact", "sensor contacts");
		obj(elements.OfType<HydraulicPump>().Count(), "hydraulic pump", "hydraulic pumps");
		stringBuilder.AppendLine((CS_0024_003C_003E8__locals0._0024VB_0024Local_parts.Count == 0) ? "No actuators." : ("The circuit has " + string.Join(", ", CS_0024_003C_003E8__locals0._0024VB_0024Local_parts) + "."));
		stringBuilder.AppendLine();
		Dictionary<Port, List<Port>> map = BuildTubeMap(c);
		List<CircuitMove> list = FindMoves(c);
		if (list.Count > 0)
		{
			stringBuilder.AppendLine("HOW EACH ACTUATOR IS CONTROLLED");
			using (IEnumerator<CylinderBase> enumerator = (from k in elements.OfType<CylinderBase>()
				orderby k.Label
				select k).GetEnumerator())
			{
				_Closure_0024__20_002D1 closure_0024__20_002D = default(_Closure_0024__20_002D1);
				while (enumerator.MoveNext())
				{
					closure_0024__20_002D = new _Closure_0024__20_002D1(closure_0024__20_002D);
					closure_0024__20_002D._0024VB_0024Local_cyl = enumerator.Current;
					List<CircuitElement> list2 = new List<CircuitElement>();
					Port port = TraceToValve(map, closure_0024__20_002D._0024VB_0024Local_cyl.Ports[0], list2);
					if (closure_0024__20_002D._0024VB_0024Local_cyl.Ports.Count > 1)
					{
						TraceToValve(map, closure_0024__20_002D._0024VB_0024Local_cyl.Ports[1], list2);
					}
					if (port == null)
					{
						stringBuilder.AppendLine($"• {closure_0024__20_002D._0024VB_0024Local_cyl.DisplayName} {Name(closure_0024__20_002D._0024VB_0024Local_cyl)} is not connected to a directional valve.");
						continue;
					}
					DirectionalValve directionalValve = (DirectionalValve)port.Owner;
					stringBuilder.AppendLine($"• {closure_0024__20_002D._0024VB_0024Local_cyl.DisplayName} {Name(closure_0024__20_002D._0024VB_0024Local_cyl)} is controlled by valve {Name(directionalValve)} ({directionalValve.DisplayName}).");
					foreach (CircuitMove item in list.Where(closure_0024__20_002D._Lambda_0024__6))
					{
						stringBuilder.AppendLine(string.Format("    It {0} by {1}.", item.Extend ? "extends" : "retracts", (Operators.CompareString(item.Trigger.Description, "", TextCompare: false) == 0) ? "?" : item.Trigger.Description));
					}
					foreach (FlowControlValve item2 in list2.OfType<FlowControlValve>())
					{
						stringBuilder.AppendLine(string.Format("    Its speed is limited by flow control valve {0} ({1} % opening{2}).", Name(item2), item2.OpeningPercent, item2.HasCheckValve ? ", one-way" : ""));
					}
					foreach (QuickExhaustValve item3 in list2.OfType<QuickExhaustValve>())
					{
						stringBuilder.AppendLine($"    Quick exhaust valve {Name(item3)} lets it return faster.");
					}
				}
			}
			stringBuilder.AppendLine();
			List<CircuitMove> list3 = InferSequence(list);
			if (list3.Count >= 2)
			{
				stringBuilder.AppendLine("MOTION SEQUENCE");
				stringBuilder.AppendLine("  " + string.Join(" ", list3.Select([SpecialName] (CircuitMove m) => string.Format("{0}{1}", m.Cylinder.Label, m.Extend ? "+" : "-"))) + string.Format("   (as letters: {0})", string.Join(" ", list3)));
				foreach (string item4 in FindOverlaps(list, list3))
				{
					stringBuilder.AppendLine("  ! " + item4);
				}
				stringBuilder.AppendLine();
			}
		}
		stringBuilder.AppendLine("STEP BY STEP (test run)");
		stringBuilder.Append(Narrate(c, operate, realPhysics));
		return stringBuilder.ToString();
	}

	private static string PositionName(DirectionalValve v)
	{
		switch (v.State)
		{
		case 1:
			return "position a";
		case 2:
			return "position b";
		default:
			if (v.Positions == 3)
			{
				return "its centre position";
			}
			if (v.IsMemoryValve)
			{
				return "position b";
			}
			return "its normal position (spring return)";
		}
	}

	public static string Narrate(Circuit c, CircuitElement operate, bool realPhysics)
	{
		_Closure_0024__22_002D0 arg = default(_Closure_0024__22_002D0);
		_Closure_0024__22_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__22_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_operate = operate;
		StringBuilder stringBuilder = new StringBuilder();
		Circuit circuit = Circuit.FromXml(c.ToXml());
		CS_0024_003C_003E8__locals0._0024VB_0024Local_sim = new Simulator(circuit)
		{
			RealPhysics = realPhysics
		};
		CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Reset();
		CircuitElement circuitElement = ((CS_0024_003C_003E8__locals0._0024VB_0024Local_operate == null) ? null : circuit.Elements.FirstOrDefault([SpecialName] (CircuitElement e) => Operators.CompareString(e.TypeName, CS_0024_003C_003E8__locals0._0024VB_0024Local_operate.TypeName, TextCompare: false) == 0 && Operators.CompareString(e.Label, CS_0024_003C_003E8__locals0._0024VB_0024Local_operate.Label, TextCompare: false) == 0 && e.X == CS_0024_003C_003E8__locals0._0024VB_0024Local_operate.X && e.Y == CS_0024_003C_003E8__locals0._0024VB_0024Local_operate.Y));
		if (circuitElement == null)
		{
			circuitElement = ManualElements(circuit).FirstOrDefault();
		}
		CS_0024_003C_003E8__locals0._0024VB_0024Local_valveState = circuit.Elements.OfType<DirectionalValve>().ToDictionary([SpecialName] (DirectionalValve v) => v, [SpecialName] (DirectionalValve v) => v.State);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_coilState = circuit.Elements.OfType<ElectricCoil>().ToDictionary([SpecialName] (ElectricCoil k) => k, [SpecialName] (ElectricCoil k) => k.Active);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_cylDir = circuit.Elements.OfType<CylinderBase>().ToDictionary([SpecialName] (CylinderBase k) => k, [SpecialName] (CylinderBase k) => 0);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_cylPos = circuit.Elements.OfType<CylinderBase>().ToDictionary([SpecialName] (CylinderBase k) => k, [SpecialName] (CylinderBase k) => k.Position);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_lines = new List<string>();
		CS_0024_003C_003E8__locals0._0024VB_0024Local_lastEvent = 0.0;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_stepNo = 0;
		VB_0024AnonymousDelegate_16<bool, List<string>> vB_0024AnonymousDelegate_ = [SpecialName] (bool withMotion) =>
		{
			List<string> list = new List<string>();
			List<string> list2 = new List<string>();
			List<string> list3 = new List<string>();
			foreach (CylinderBase item in withMotion ? CS_0024_003C_003E8__locals0._0024VB_0024Local_cylDir.Keys.ToList() : new List<CylinderBase>())
			{
				int num3 = Math.Sign(item.Position - CS_0024_003C_003E8__locals0._0024VB_0024Local_cylPos[item]);
				if (Math.Abs(item.Position - CS_0024_003C_003E8__locals0._0024VB_0024Local_cylPos[item]) < 1E-05)
				{
					num3 = 0;
				}
				if (CS_0024_003C_003E8__locals0._0024VB_0024Local_cylPos[item] < 0.995 && item.Position >= 0.995)
				{
					list2.Add(string.Format("cylinder {0} is fully extended{1}", Name(item), string.IsNullOrEmpty(item.ExtendedMark) ? "" : $" (operates {item.ExtendedMark})"));
				}
				if (CS_0024_003C_003E8__locals0._0024VB_0024Local_cylPos[item] > 0.005 && item.Position <= 0.005)
				{
					list2.Add(string.Format("cylinder {0} is fully retracted{1}", Name(item), string.IsNullOrEmpty(item.RetractedMark) ? "" : $" (operates {item.RetractedMark})"));
				}
				if (num3 != CS_0024_003C_003E8__locals0._0024VB_0024Local_cylDir[item])
				{
					if (num3 > 0)
					{
						list3.Add($"cylinder {Name(item)} starts to extend");
					}
					if (num3 < 0)
					{
						list3.Add($"cylinder {Name(item)} starts to retract");
					}
					if (num3 == 0 && item.Position > 0.005 && item.Position < 0.995)
					{
						list3.Add($"cylinder {Name(item)} stops at {item.Position * 100.0:0} % of its stroke");
					}
					CS_0024_003C_003E8__locals0._0024VB_0024Local_cylDir[item] = num3;
				}
				CS_0024_003C_003E8__locals0._0024VB_0024Local_cylPos[item] = item.Position;
			}
			list.AddRange(list2);
			foreach (ElectricCoil item2 in CS_0024_003C_003E8__locals0._0024VB_0024Local_coilState.Keys.ToList())
			{
				if (item2.Active != CS_0024_003C_003E8__locals0._0024VB_0024Local_coilState[item2])
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_coilState[item2] = item2.Active;
					switch (item2.Kind)
					{
					case CoilKind.Solenoid:
						list.Add(string.Format("solenoid {0} {1}", item2.Label, item2.Active ? "is energized" : "is switched off"));
						break;
					case CoilKind.Lamp:
						list.Add(string.Format("lamp {0} {1}", item2.Label, item2.Active ? "lights up" : "goes out"));
						break;
					case CoilKind.OnDelayTimer:
					case CoilKind.OffDelayTimer:
						list.Add(string.Format("timer relay {0} {1}", item2.Label, item2.Active ? "switches on" : "switches off"));
						break;
					default:
						list.Add(string.Format("relay {0} {1}", item2.Label, item2.Active ? "picks up" : "drops out"));
						break;
					}
				}
			}
			foreach (DirectionalValve item3 in CS_0024_003C_003E8__locals0._0024VB_0024Local_valveState.Keys.OrderBy([SpecialName] (DirectionalValve x) => (!x.IsManuallyOperated) ? 1 : 0).ToList())
			{
				if (item3.State != CS_0024_003C_003E8__locals0._0024VB_0024Local_valveState[item3])
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_valveState[item3] = item3.State;
					if (!item3.IsRollerOperated)
					{
						list.Add($"valve {Name(item3)} switches to {PositionName(item3)}");
					}
				}
			}
			list.AddRange(list3);
			return list;
		};
		checked
		{
			VB_0024AnonymousDelegate_17<string, List<string>> vB_0024AnonymousDelegate_2 = [SpecialName] (string prefix, List<string> ev) =>
			{
				if (ev.Count != 0 || Operators.CompareString(prefix, "", TextCompare: false) != 0)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_stepNo++;
					string text = string.Join(" → ", ev);
					CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Add(string.Format("{0,2}. t = {1,5:0.00} s   {2}{3}{4}", CS_0024_003C_003E8__locals0._0024VB_0024Local_stepNo, CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Time, prefix, (Operators.CompareString(prefix, "", TextCompare: false) != 0 && Operators.CompareString(text, "", TextCompare: false) != 0) ? " → " : "", text));
					CS_0024_003C_003E8__locals0._0024VB_0024Local_lastEvent = CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Time;
				}
			};
			vB_0024AnonymousDelegate_(withMotion: true);
			if (circuitElement == null)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Add("There is nothing to operate by hand, so the circuit runs by itself:");
			}
			else
			{
				bool flag = (circuitElement is DirectionalValve && ((DirectionalValve)circuitElement).Actuator == ValveActuator.PushButton) || (circuitElement is ElectricContact && ((ElectricContact)circuitElement).Operator == ContactOperator.PushButton);
				circuitElement.OnSimMouseDown(PointF.Empty);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.RunLogic();
				vB_0024AnonymousDelegate_2(string.Format("You {0} {1}", flag ? "press" : "switch on", Name(circuitElement)), vB_0024AnonymousDelegate_(withMotion: false));
				if (flag)
				{
					int num = 1;
					do
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Step(0.005);
						vB_0024AnonymousDelegate_2("", vB_0024AnonymousDelegate_(withMotion: true));
						num++;
					}
					while (num <= 60);
					circuitElement.OnSimMouseUp();
					CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.RunLogic();
					vB_0024AnonymousDelegate_2($"You release {Name(circuitElement)}", vB_0024AnonymousDelegate_(withMotion: false));
				}
			}
			List<CylinderBase> source = CS_0024_003C_003E8__locals0._0024VB_0024Local_cylDir.Keys.ToList();
			bool flag2 = false;
			int num2 = 1;
			do
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Step(0.005);
				vB_0024AnonymousDelegate_2("", vB_0024AnonymousDelegate_(withMotion: true));
				if (source.Any([SpecialName] (CylinderBase k) => k.Position > 0.05))
				{
					flag2 = true;
				}
				if (flag2 && source.All([SpecialName] (CylinderBase k) => k.Position <= 0.005))
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Add($"       One complete cycle took {CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Time:0.00} s; all cylinders are back in their start position." + (source.Any([SpecialName] (CylinderBase k) => CS_0024_003C_003E8__locals0._0024VB_0024Local_cylDir[k] != 0) ? " The circuit keeps repeating the cycle." : ""));
					break;
				}
				if (CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Count >= 60)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Add("   ... (stopped after 60 events)");
					break;
				}
				if (CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Time - CS_0024_003C_003E8__locals0._0024VB_0024Local_lastEvent > 4.0)
				{
					break;
				}
				num2++;
			}
			while (num2 <= 6000);
			if (CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Count == 0)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Add("Nothing happens.");
			}
			foreach (string item4 in CS_0024_003C_003E8__locals0._0024VB_0024Local_sim.Warnings.Distinct())
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Add("   Warning: " + item4);
			}
			stringBuilder.AppendLine(string.Join(Environment.NewLine, CS_0024_003C_003E8__locals0._0024VB_0024Local_lines));
			return stringBuilder.ToString();
		}
	}
}
