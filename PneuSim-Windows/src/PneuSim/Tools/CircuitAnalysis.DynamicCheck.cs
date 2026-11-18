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

	public static List<CircuitElement> ManualElements(Circuit c)
	{
		VB_0024AnonymousDelegate_14<CircuitElement, int> vB_0024AnonymousDelegate_ = [SpecialName] (CircuitElement e) =>
		{
			string text = (e.Label ?? "").ToUpperInvariant();
			switch (text)
			{
			case "S1":
			case "1S0":
			case "1S1":
				return 0;
			default:
				if (text.StartsWith("S") || text.Contains("S"))
				{
					return 1;
				}
				return 2;
			}
		};
		IEnumerable<CircuitElement> source = c.Elements.Where([SpecialName] (CircuitElement e) => e.IsManuallyOperated && !(e is HydraulicPump) && !(e is Compressor) && !(e is ShutOffValve) && !(e is SuctionCup) && !(e is ElectricCounter) && (!(e is ElectricContact) || ((ElectricContact)e).Operator != ContactOperator.EmergencyStop) && (!(e is DirectionalValve) || ((DirectionalValve)e).Actuator != ValveActuator.Solenoid));
		VB_0024AnonymousDelegate_14<CircuitElement, int> vB_0024AnonymousDelegate_2 = vB_0024AnonymousDelegate_;
		return source.OrderBy((vB_0024AnonymousDelegate_2 == null) ? null : new Func<CircuitElement, int>(vB_0024AnonymousDelegate_2.Invoke)).ThenBy([SpecialName] (CircuitElement e) => e.Label).ToList();
	}

	public static List<CheckIssue> DynamicCheck(Circuit c, bool realPhysics)
	{
		_Closure_0024__18_002D1 arg = default(_Closure_0024__18_002D1);
		_Closure_0024__18_002D1 CS_0024_003C_003E8__locals0 = new _Closure_0024__18_002D1(arg);
		List<CheckIssue> list = new List<CheckIssue>();
		Circuit circuit = Circuit.FromXml(c.ToXml());
		Simulator simulator = new Simulator(circuit)
		{
			RealPhysics = realPhysics
		};
		simulator.Reset();
		HashSet<string> hashSet = new HashSet<string>(simulator.Warnings);
		List<CircuitElement> list2 = ManualElements(circuit);
		Dictionary<DirectionalValve, double> dictionary = new Dictionary<DirectionalValve, double>();
		CS_0024_003C_003E8__locals0._0024VB_0024Local_moved = new HashSet<CylinderBase>();
		if (list2.Count > 0)
		{
			list2[0].OnSimMouseDown(PointF.Empty);
			simulator.RunLogic();
		}
		int num = 1;
		_Closure_0024__18_002D0 closure_0024__18_002D = default(_Closure_0024__18_002D0);
		do
		{
			if (num == 61 && list2.Count > 0)
			{
				list2[0].OnSimMouseUp();
				simulator.RunLogic();
			}
			simulator.Step(0.005);
			foreach (string warning in simulator.Warnings)
			{
				hashSet.Add(warning);
			}
			foreach (CylinderBase item in circuit.Elements.OfType<CylinderBase>())
			{
				if (item.Position > 0.02)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_moved.Add(item);
				}
			}
			using (IEnumerator<DirectionalValve> enumerator3 = (from x in circuit.Elements.OfType<DirectionalValve>()
				where x.IsMemoryValve
				select x).GetEnumerator())
			{
				while (enumerator3.MoveNext())
				{
					closure_0024__18_002D = new _Closure_0024__18_002D0(closure_0024__18_002D);
					closure_0024__18_002D._0024VB_0024Local_v = enumerator3.Current;
					bool[] array = closure_0024__18_002D._0024VB_0024Local_v.ActiveSignals(simulator);
					if (array[0] && array[1])
					{
						dictionary[closure_0024__18_002D._0024VB_0024Local_v] = (dictionary.ContainsKey(closure_0024__18_002D._0024VB_0024Local_v) ? dictionary[closure_0024__18_002D._0024VB_0024Local_v] : 0.0) + 0.005;
					}
					else
					{
						dictionary[closure_0024__18_002D._0024VB_0024Local_v] = 0.0;
					}
					if (dictionary[closure_0024__18_002D._0024VB_0024Local_v] >= 0.2 && !list.Any([SpecialName] (CheckIssue x) => x.Message.Contains("both signals")))
					{
						CircuitElement element = c.Elements.FirstOrDefault(closure_0024__18_002D._Lambda_0024__2);
						list.Add(new CheckIssue
						{
							Severity = IssueSeverity.Error,
							Element = element,
							Message = $"Test run: valve {Name(closure_0024__18_002D._0024VB_0024Local_v)} receives both signals at the same time (signal overlap); the machine stops."
						});
					}
				}
			}
			num = checked(num + 1);
		}
		while (num <= 3060);
		foreach (string item2 in hashSet)
		{
			list.Add(new CheckIssue
			{
				Severity = IssueSeverity.Warning,
				Message = "Test run: " + item2
			});
		}
		if (list2.Count > 0)
		{
			_Closure_0024__18_002D2 arg2 = default(_Closure_0024__18_002D2);
			_Closure_0024__18_002D2 CS_0024_003C_003E8__locals1 = new _Closure_0024__18_002D2(arg2);
			List<CylinderBase> list3 = (from k in circuit.Elements.OfType<CylinderBase>()
				where !CS_0024_003C_003E8__locals0._0024VB_0024Local_moved.Contains(k)
				select k).ToList();
			CS_0024_003C_003E8__locals1._0024VB_0024Local_together = ((list3.Count > 0 && list2.Count > 1) ? MovedWithAllOperated(c, realPhysics) : new HashSet<string>());
			foreach (CylinderBase item3 in list3.Where([SpecialName] (CylinderBase k) => !CS_0024_003C_003E8__locals1._0024VB_0024Local_together.Contains(Name(k))))
			{
				list.Add(new CheckIssue
				{
					Severity = IssueSeverity.Info,
					Message = $"Test run: cylinder {Name(item3)} did not move within 15 s, neither after operating {Name(list2[0])} " + ((list2.Count > 1) ? "nor with all hand-operated elements operated together." : "alone.")
				});
			}
		}
		return list;
	}

	private static HashSet<string> MovedWithAllOperated(Circuit c, bool realPhysics)
	{
		Circuit circuit = Circuit.FromXml(c.ToXml());
		Simulator simulator = new Simulator(circuit)
		{
			RealPhysics = realPhysics
		};
		simulator.Reset();
		foreach (CircuitElement item in ManualElements(circuit))
		{
			item.OnSimMouseDown(PointF.Empty);
		}
		simulator.RunLogic();
		HashSet<string> hashSet = new HashSet<string>();
		int num = 1;
		do
		{
			simulator.Step(0.005);
			foreach (CylinderBase item2 in circuit.Elements.OfType<CylinderBase>())
			{
				if (item2.Position > 0.02)
				{
					hashSet.Add(Name(item2));
				}
			}
			num = checked(num + 1);
		}
		while (num <= 3000);
		return hashSet;
	}
}
