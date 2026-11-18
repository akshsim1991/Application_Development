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

	public static List<CircuitMove> FindMoves(Circuit c)
	{
		Dictionary<Port, List<Port>> map = BuildTubeMap(c);
		List<CircuitMove> list = new List<CircuitMove>();
		List<CylinderBase> list2 = (from k in c.Elements.OfType<CylinderBase>()
			orderby k.Label
			select k).ToList();
		checked
		{
			int num = list2.Count - 1;
			_Closure_0024__8_002D0 closure_0024__8_002D = default(_Closure_0024__8_002D0);
			_Closure_0024__8_002D1 closure_0024__8_002D2 = default(_Closure_0024__8_002D1);
			for (int i = 0; i <= num; i++)
			{
				closure_0024__8_002D = new _Closure_0024__8_002D0(closure_0024__8_002D);
				CylinderBase cylinderBase = list2[i];
				List<CircuitElement> inline = new List<CircuitElement>();
				closure_0024__8_002D._0024VB_0024Local_vp = TraceToValve(map, cylinderBase.Ports[0], inline);
				if (closure_0024__8_002D._0024VB_0024Local_vp == null)
				{
					continue;
				}
				closure_0024__8_002D._0024VB_0024Local_v = (DirectionalValve)closure_0024__8_002D._0024VB_0024Local_vp.Owner;
				closure_0024__8_002D._0024VB_0024Local_supplyName = (closure_0024__8_002D._0024VB_0024Local_v.WorkingPortNames().Contains("P") ? "P" : "1");
				bool[] array = new bool[2] { true, false };
				foreach (bool flag in array)
				{
					VB_0024AnonymousDelegate_11<int, bool> vB_0024AnonymousDelegate_ = closure_0024__8_002D._Lambda_0024__1;
					bool flag2 = flag;
					int num2 = -1;
					int num3 = closure_0024__8_002D._0024VB_0024Local_v.Positions - 1;
					for (int l = 0; l <= num3; l++)
					{
						if (vB_0024AnonymousDelegate_(l) == flag2 && l != 0)
						{
							num2 = l;
							break;
						}
					}
					if (num2 < 0 && vB_0024AnonymousDelegate_(0) == flag2)
					{
						num2 = 0;
					}
					CircuitMove circuitMove = new CircuitMove
					{
						Cylinder = cylinderBase,
						Extend = flag,
						Valve = closure_0024__8_002D._0024VB_0024Local_v,
						Letter = Strings.ChrW(65 + i)
					};
					bool flag3 = num2 == 1;
					if (num2 == 0 && (closure_0024__8_002D._0024VB_0024Local_v.Positions != 2 || closure_0024__8_002D._0024VB_0024Local_v.ReturnType == ValveReturn.Spring))
					{
						circuitMove.Trigger.Description = $"spring return of {Name(closure_0024__8_002D._0024VB_0024Local_v)}";
					}
					else if ((closure_0024__8_002D._0024VB_0024Local_v.Actuator == ValveActuator.Solenoid && flag3) || (closure_0024__8_002D._0024VB_0024Local_v.ReturnType == ValveReturn.Solenoid && !flag3))
					{
						closure_0024__8_002D2 = new _Closure_0024__8_002D1(closure_0024__8_002D2);
						closure_0024__8_002D2._0024VB_0024Local_sol = (flag3 ? closure_0024__8_002D._0024VB_0024Local_v.SolenoidLabel : closure_0024__8_002D._0024VB_0024Local_v.ReturnSolenoidLabel);
						ElectricCoil electricCoil = c.Elements.OfType<ElectricCoil>().FirstOrDefault(closure_0024__8_002D2._Lambda_0024__3);
						if (electricCoil != null)
						{
							string text = CoilCondition(c, electricCoil, circuitMove.Trigger);
							circuitMove.Trigger.Description = string.Format("solenoid {0}, energized when {1}", closure_0024__8_002D2._0024VB_0024Local_sol, (Operators.CompareString(text, "", TextCompare: false) == 0) ? "always" : text);
						}
						else
						{
							circuitMove.Trigger.Description = $"solenoid {closure_0024__8_002D2._0024VB_0024Local_sol} (no coil with this label!)";
						}
					}
					else if (flag3 && (closure_0024__8_002D._0024VB_0024Local_v.Actuator == ValveActuator.PushButton || closure_0024__8_002D._0024VB_0024Local_v.Actuator == ValveActuator.Selector))
					{
						circuitMove.Trigger.Manual.Add(Name(closure_0024__8_002D._0024VB_0024Local_v));
						circuitMove.Trigger.Description = $"operating valve {Name(closure_0024__8_002D._0024VB_0024Local_v)} by hand";
					}
					else if (flag3 && closure_0024__8_002D._0024VB_0024Local_v.IsRollerOperated)
					{
						circuitMove.Trigger.Marks.Add(closure_0024__8_002D._0024VB_0024Local_v.TriggerMark);
						circuitMove.Trigger.Description = $"its roller at {closure_0024__8_002D._0024VB_0024Local_v.TriggerMark}";
					}
					else
					{
						TraceSignal(map, closure_0024__8_002D._0024VB_0024Local_v.GetPort(closure_0024__8_002D._0024VB_0024Local_v.PilotPortName(flag3)), circuitMove.Trigger, 0);
						if (Operators.CompareString(circuitMove.Trigger.Description, "", TextCompare: false) == 0)
						{
							circuitMove.Trigger.Description = $"pilot {closure_0024__8_002D._0024VB_0024Local_v.PilotPortName(flag3)} (not connected!)";
						}
					}
					list.Add(circuitMove);
				}
			}
			return list;
		}
	}

	public static List<CircuitMove> InferSequence(List<CircuitMove> moves)
	{
		_Closure_0024__9_002D0 arg = new _Closure_0024__9_002D0(null);
		arg._0024VB_0024Local_result = new List<CircuitMove>();
		CircuitMove circuitMove = moves.FirstOrDefault([SpecialName] (CircuitMove m) => m.Trigger.Manual.Count > 0 && m.Extend);
		if (circuitMove == null)
		{
			circuitMove = moves.FirstOrDefault([SpecialName] (CircuitMove m) => m.Trigger.Manual.Count > 0);
		}
		if (circuitMove == null)
		{
			return arg._0024VB_0024Local_result;
		}
		CircuitMove circuitMove2 = circuitMove;
		_Closure_0024__9_002D1 closure_0024__9_002D = default(_Closure_0024__9_002D1);
		while (circuitMove2 != null && !arg._0024VB_0024Local_result.Contains(circuitMove2))
		{
			closure_0024__9_002D = new _Closure_0024__9_002D1(closure_0024__9_002D);
			closure_0024__9_002D._0024VB_0024NonLocal__0024VB_0024Closure_2 = arg;
			closure_0024__9_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_result.Add(circuitMove2);
			closure_0024__9_002D._0024VB_0024Local_mark = circuitMove2.EndMark;
			if (string.IsNullOrWhiteSpace(closure_0024__9_002D._0024VB_0024Local_mark))
			{
				break;
			}
			circuitMove2 = moves.FirstOrDefault(closure_0024__9_002D._Lambda_0024__2);
		}
		return arg._0024VB_0024Local_result;
	}

	public static List<string> FindOverlaps(List<CircuitMove> moves, List<CircuitMove> sequence)
	{
		List<string> list = new List<string>();
		if (sequence.Count < 2)
		{
			return list;
		}
		Dictionary<CylinderBase, bool> dictionary = new Dictionary<CylinderBase, bool>();
		foreach (CircuitMove move in moves)
		{
			dictionary[move.Cylinder] = false;
		}
		using (List<CircuitMove>.Enumerator enumerator2 = sequence.GetEnumerator())
		{
			_Closure_0024__10_002D0 closure_0024__10_002D = default(_Closure_0024__10_002D0);
			_Closure_0024__10_002D1 closure_0024__10_002D2 = default(_Closure_0024__10_002D1);
			while (enumerator2.MoveNext())
			{
				closure_0024__10_002D = new _Closure_0024__10_002D0(closure_0024__10_002D);
				closure_0024__10_002D._0024VB_0024Local_m = enumerator2.Current;
				CircuitMove circuitMove = moves.FirstOrDefault(closure_0024__10_002D._Lambda_0024__0);
				if (circuitMove != null && closure_0024__10_002D._0024VB_0024Local_m.Valve.IsMemoryValve && circuitMove.Trigger.Marks.Count > 0 && !circuitMove.Trigger.Gated)
				{
					closure_0024__10_002D2 = new _Closure_0024__10_002D1(closure_0024__10_002D2);
					closure_0024__10_002D2._0024VB_0024Local_activeMarks = (from kv in dictionary
						select (!kv.Value) ? kv.Key.RetractedMark : kv.Key.ExtendedMark into k
						where !string.IsNullOrWhiteSpace(k)
						select k).ToList();
					List<string> list2 = circuitMove.Trigger.Marks.Where(closure_0024__10_002D2._Lambda_0024__3).ToList();
					if (list2.Count == circuitMove.Trigger.Marks.Count)
					{
						list.Add($"Signal overlap at valve {Name(closure_0024__10_002D._0024VB_0024Local_m.Valve)}: when {closure_0024__10_002D._0024VB_0024Local_m} should start, the signal for {circuitMove} " + string.Format("is still on ({0} is still operated), so the valve cannot switch and the sequence stops.", string.Join(", ", list2)));
					}
				}
				dictionary[closure_0024__10_002D._0024VB_0024Local_m.Cylinder] = closure_0024__10_002D._0024VB_0024Local_m.Extend;
			}
		}
		return list;
	}

	private static CheckIssue OverlapIssue(Circuit c, string msg, List<CircuitMove> seq)
	{
		CheckIssue checkIssue = new CheckIssue
		{
			Severity = IssueSeverity.Error,
			Message = msg
		};
		string text = string.Join(" ", seq.Select([SpecialName] (CircuitMove m) => m.ToString()));
		try
		{
			MotionSequence seq2 = MotionSequence.Parse(text);
			checkIssue.Message += $" Intended sequence: {text}.";
			checkIssue.FixLabel = $"Redesign {text} with the cascade method";
			checkIssue.Fix = [SpecialName] () => SequenceGenerator.Generate(seq2, GeneratorMethod.PneumaticCascade, continuous: false).Circuit;
		}
		catch (FormatException ex)
		{
			ProjectData.SetProjectError(ex);
			FormatException ex2 = ex;
			checkIssue.Message += " Use the cascade method or a relay step chain (Tools > Circuit Generator).";
			ProjectData.ClearProjectError();
		}
		return checkIssue;
	}

	public static Func<Circuit> ElectroFix(CheckIssue issue)
	{
		string text = "Intended sequence: ";
		int num = issue.Message.IndexOf(text);
		if (num < 0)
		{
			return null;
		}
		string text2 = issue.Message.Substring(checked(num + text.Length)).TrimEnd('.');
		return [SpecialName] () => SequenceGenerator.Generate(MotionSequence.Parse(text2), GeneratorMethod.ElectroRelayChain, continuous: false).Circuit;
	}
}
