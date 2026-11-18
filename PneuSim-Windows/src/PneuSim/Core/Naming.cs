using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text.RegularExpressions;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Naming
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__5_002D0
	{
		public IList<CircuitElement> _0024VB_0024Local_pasted;

		public Dictionary<string, string> _0024VB_0024Local_nameMap;

		public Dictionary<string, string> _0024VB_0024Local_markMap;

		public _Closure_0024__5_002D0(_Closure_0024__5_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_pasted = arg0._0024VB_0024Local_pasted;
				_0024VB_0024Local_nameMap = arg0._0024VB_0024Local_nameMap;
				_0024VB_0024Local_markMap = arg0._0024VB_0024Local_markMap;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(CircuitElement o)
		{
			return !_0024VB_0024Local_pasted.Contains(o);
		}

		[SpecialName]
		internal string _Lambda_0024__4(string n)
		{
			if (n == null || !_0024VB_0024Local_nameMap.ContainsKey(n.Trim()))
			{
				return n;
			}
			return _0024VB_0024Local_nameMap[n.Trim()];
		}

		[SpecialName]
		internal string _Lambda_0024__5(string n)
		{
			if (n == null || !_0024VB_0024Local_markMap.ContainsKey(n.Trim()))
			{
				return n;
			}
			return _0024VB_0024Local_markMap[n.Trim()];
		}
	}

	public static HashSet<string> UsedNames(IEnumerable<CircuitElement> elements)
	{
		HashSet<string> hashSet = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
		foreach (CircuitElement element in elements)
		{
			if (!string.IsNullOrWhiteSpace(element.Label))
			{
				hashSet.Add(element.Label.Trim());
			}
			if (element is DirectionalValve directionalValve)
			{
				if (directionalValve.Actuator == ValveActuator.Solenoid && !string.IsNullOrWhiteSpace(directionalValve.SolenoidLabel))
				{
					hashSet.Add(directionalValve.SolenoidLabel.Trim());
				}
				if (directionalValve.ReturnType == ValveReturn.Solenoid && !string.IsNullOrWhiteSpace(directionalValve.ReturnSolenoidLabel))
				{
					hashSet.Add(directionalValve.ReturnSolenoidLabel.Trim());
				}
			}
		}
		return hashSet;
	}

	public static HashSet<string> UsedMarks(IEnumerable<CircuitElement> elements)
	{
		HashSet<string> hashSet = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
		foreach (CylinderBase item in elements.OfType<CylinderBase>())
		{
			if (!string.IsNullOrWhiteSpace(item.RetractedMark))
			{
				hashSet.Add(item.RetractedMark.Trim());
			}
			if (!string.IsNullOrWhiteSpace(item.ExtendedMark))
			{
				hashSet.Add(item.ExtendedMark.Trim());
			}
		}
		return hashSet;
	}

	public static string NextFree(string start, ICollection<string> used)
	{
		if (!used.Contains(start))
		{
			return start;
		}
		Match match = Regex.Match(start, "^(.*?)(\\d+)(\\D*)$");
		string text = (match.Success ? match.Groups[1].Value : start);
		int num = ((!match.Success) ? 1 : int.Parse(match.Groups[2].Value));
		string text2 = (match.Success ? match.Groups[3].Value : "");
		string text3;
		do
		{
			num = checked(num + 1);
			text3 = text + Conversions.ToString(num) + text2;
		}
		while (used.Contains(text3));
		return text3;
	}

	public static void NameNewElement(CircuitElement e, IEnumerable<CircuitElement> others)
	{
		List<CircuitElement> list = others.Where([SpecialName] (CircuitElement o) => o != e).ToList();
		HashSet<string> used = UsedNames(list);
		bool flag = true;
		if (flag == e is CylinderBase)
		{
			e.Label = NextFree("1A", used);
		}
		else if (flag == e is DirectionalValve)
		{
			DirectionalValve directionalValve = (DirectionalValve)e;
			bool flag2 = directionalValve.Actuator == ValveActuator.PushButton || directionalValve.Actuator == ValveActuator.Selector || directionalValve.IsRollerOperated;
			if (string.IsNullOrWhiteSpace(directionalValve.Label))
			{
				directionalValve.Label = NextFree(flag2 ? "1S1" : "1V1", used);
			}
			List<string> freeCoils = (from k in list.OfType<ElectricCoil>()
				where k.Kind == CoilKind.Solenoid && !string.IsNullOrWhiteSpace(k.Label) && !list.OfType<DirectionalValve>().Any([SpecialName] (DirectionalValve o) => o.UsesSolenoid(k.Label))
				select k.Label.Trim()).ToList();
			if (directionalValve.Actuator == ValveActuator.Solenoid)
			{
				directionalValve.SolenoidLabel = TakeSolenoid(freeCoils, used);
			}
			if (directionalValve.ReturnType == ValveReturn.Solenoid)
			{
				directionalValve.ReturnSolenoidLabel = TakeSolenoid(freeCoils, used);
			}
		}
		else if (flag == e is ElectricCoil)
		{
			ElectricCoil electricCoil = (ElectricCoil)e;
			switch (electricCoil.Kind)
			{
			case CoilKind.Solenoid:
			{
				List<string> source = (from x in list.OfType<ElectricCoil>()
					where x.Kind == CoilKind.Solenoid
					select x.Label?.Trim()).ToList();
				electricCoil.Label = (from n in list.OfType<DirectionalValve>().SelectMany([SpecialName] (DirectionalValve v) => new string[2]
					{
						(v.Actuator == ValveActuator.Solenoid) ? v.SolenoidLabel : null,
						(v.ReturnType == ValveReturn.Solenoid) ? v.ReturnSolenoidLabel : null
					})
					where !string.IsNullOrWhiteSpace(n) && !source.Contains(n.Trim(), StringComparer.OrdinalIgnoreCase)
					select n).FirstOrDefault()?.Trim() ?? NextFree("1M1", used);
				break;
			}
			case CoilKind.Lamp:
				electricCoil.Label = NextFree("H1", used);
				break;
			default:
				electricCoil.Label = NextFree(string.IsNullOrWhiteSpace(electricCoil.Label) ? "K1" : electricCoil.Label, used);
				break;
			}
		}
		else if (flag == e is ElectricContact)
		{
			ElectricContact electricContact = (ElectricContact)e;
			if (electricContact.Operator == ContactOperator.PushButton || electricContact.Operator == ContactOperator.Selector || electricContact.Operator == ContactOperator.EmergencyStop)
			{
				electricContact.Label = NextFree(string.IsNullOrWhiteSpace(electricContact.Label) ? "S1" : electricContact.Label, used);
			}
			else if (electricContact.Operator == ContactOperator.Relay)
			{
				ElectricCoil electricCoil2 = list.OfType<ElectricCoil>().LastOrDefault([SpecialName] (ElectricCoil x) => x.Kind != CoilKind.Solenoid && x.Kind != CoilKind.Lamp);
				if (electricCoil2 != null && !string.IsNullOrWhiteSpace(electricCoil2.Label))
				{
					electricContact.Reference = electricCoil2.Label;
				}
			}
			else if (electricContact.Operator == ContactOperator.PressureSwitch)
			{
				PressureSwitch pressureSwitch = list.OfType<PressureSwitch>().LastOrDefault([SpecialName] (PressureSwitch x) => !string.IsNullOrWhiteSpace(x.Label));
				if (pressureSwitch != null)
				{
					electricContact.Reference = pressureSwitch.Label;
				}
			}
		}
		else if (flag == e is PressureSwitch)
		{
			e.Label = NextFree("B1", used);
		}
		else if (flag == e is ElectricCounter)
		{
			e.Label = NextFree(string.IsNullOrWhiteSpace(e.Label) ? "C1" : e.Label, used);
		}
		else if (flag == e is ForceSensor)
		{
			ForceSensor forceSensor = (ForceSensor)e;
			CylinderBase cylinderBase = list.OfType<CylinderBase>().LastOrDefault([SpecialName] (CylinderBase x) => !string.IsNullOrWhiteSpace(x.Label));
			if (cylinderBase != null)
			{
				forceSensor.Cylinder = cylinderBase.Label.Trim();
			}
		}
		else if (flag == e is PageConnector)
		{
			string key = (from x in list.OfType<PageConnector>()
				where !string.IsNullOrWhiteSpace(x.Label)
				select x).GroupBy([SpecialName] (PageConnector x) => x.Label.Trim(), StringComparer.OrdinalIgnoreCase).ToDictionary([SpecialName] (IGrouping<string, PageConnector> g) => g.Key, [SpecialName] (IGrouping<string, PageConnector> g) => g.Count(), StringComparer.OrdinalIgnoreCase).FirstOrDefault([SpecialName] (KeyValuePair<string, int> kv) => kv.Value == 1)
				.Key;
			e.Label = key ?? NextFree("X1", used);
		}
	}

	private static string TakeSolenoid(List<string> freeCoils, HashSet<string> used)
	{
		if (freeCoils.Count > 0)
		{
			string result = freeCoils[0];
			freeCoils.RemoveAt(0);
			return result;
		}
		string text = NextFree("1M1", used);
		used.Add(text);
		return text;
	}

	public static void RenamePasted(IList<CircuitElement> pasted, IEnumerable<CircuitElement> others)
	{
		_Closure_0024__5_002D0 arg = default(_Closure_0024__5_002D0);
		_Closure_0024__5_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__5_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_pasted = pasted;
		List<CircuitElement> elements = others.Where([SpecialName] (CircuitElement o) => !CS_0024_003C_003E8__locals0._0024VB_0024Local_pasted.Contains(o)).ToList();
		HashSet<string> hashSet = UsedNames(elements);
		HashSet<string> hashSet2 = UsedMarks(elements);
		HashSet<string> hashSet3 = UsedNames(CS_0024_003C_003E8__locals0._0024VB_0024Local_pasted.Where([SpecialName] (CircuitElement e) => !(e is PageConnector)));
		CS_0024_003C_003E8__locals0._0024VB_0024Local_nameMap = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		HashSet<string> hashSet4 = new HashSet<string>(hashSet.Concat(hashSet3), StringComparer.OrdinalIgnoreCase);
		foreach (string item in hashSet3.OrderBy([SpecialName] (string x) => x))
		{
			if (hashSet.Contains(item))
			{
				string text = NextFree(item, hashSet4);
				hashSet4.Add(text);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_nameMap[item] = text;
			}
		}
		HashSet<string> hashSet5 = UsedMarks(CS_0024_003C_003E8__locals0._0024VB_0024Local_pasted);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_markMap = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		HashSet<string> hashSet6 = new HashSet<string>(hashSet2.Concat(hashSet5), StringComparer.OrdinalIgnoreCase);
		foreach (string item2 in hashSet5.OrderBy([SpecialName] (string x) => x))
		{
			if (hashSet2.Contains(item2))
			{
				string text2 = NextFree(item2, hashSet6);
				hashSet6.Add(text2);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_markMap[item2] = text2;
			}
		}
		VB_0024AnonymousDelegate_0<string, string> vB_0024AnonymousDelegate_ = [SpecialName] (string n) => (n == null || !CS_0024_003C_003E8__locals0._0024VB_0024Local_nameMap.ContainsKey(n.Trim())) ? n : CS_0024_003C_003E8__locals0._0024VB_0024Local_nameMap[n.Trim()];
		VB_0024AnonymousDelegate_0<string, string> vB_0024AnonymousDelegate_2 = [SpecialName] (string n) => (n == null || !CS_0024_003C_003E8__locals0._0024VB_0024Local_markMap.ContainsKey(n.Trim())) ? n : CS_0024_003C_003E8__locals0._0024VB_0024Local_markMap[n.Trim()];
		foreach (CircuitElement item3 in CS_0024_003C_003E8__locals0._0024VB_0024Local_pasted)
		{
			if (!(item3 is PageConnector))
			{
				item3.Label = vB_0024AnonymousDelegate_(item3.Label);
				bool flag = true;
				if (flag == item3 is DirectionalValve)
				{
					DirectionalValve directionalValve = (DirectionalValve)item3;
					directionalValve.SolenoidLabel = vB_0024AnonymousDelegate_(directionalValve.SolenoidLabel);
					directionalValve.ReturnSolenoidLabel = vB_0024AnonymousDelegate_(directionalValve.ReturnSolenoidLabel);
					directionalValve.TriggerMark = vB_0024AnonymousDelegate_2(directionalValve.TriggerMark);
				}
				else if (flag == item3 is CylinderBase)
				{
					CylinderBase cylinderBase = (CylinderBase)item3;
					cylinderBase.RetractedMark = vB_0024AnonymousDelegate_2(cylinderBase.RetractedMark);
					cylinderBase.ExtendedMark = vB_0024AnonymousDelegate_2(cylinderBase.ExtendedMark);
				}
				else if (flag == item3 is ElectricContact)
				{
					ElectricContact electricContact = (ElectricContact)item3;
					electricContact.Reference = ((electricContact.Operator == ContactOperator.Relay || electricContact.Operator == ContactOperator.PressureSwitch) ? vB_0024AnonymousDelegate_(electricContact.Reference) : vB_0024AnonymousDelegate_2(electricContact.Reference));
				}
				else if (flag == item3 is ForceSensor)
				{
					ForceSensor forceSensor = (ForceSensor)item3;
					forceSensor.Cylinder = vB_0024AnonymousDelegate_(forceSensor.Cylinder);
				}
			}
		}
	}
}
