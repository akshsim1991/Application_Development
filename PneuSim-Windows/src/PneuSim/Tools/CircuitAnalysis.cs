using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using Microsoft.VisualBasic;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class CircuitAnalysis
{
	[Serializable]
	[CompilerGenerated]
	internal sealed class _Closure_0024__
	{
		public static readonly _Closure_0024__ _0024I;

		public static Func<Port, bool> _0024I5_002D0;

		public static Func<CylinderBase, string> _0024I8_002D0;

		public static Func<CircuitMove, bool> _0024I9_002D0;

		public static Func<CircuitMove, bool> _0024I9_002D1;

		public static Func<KeyValuePair<CylinderBase, bool>, string> _0024I10_002D1;

		public static Func<string, bool> _0024I10_002D2;

		public static Func<Port, bool> _0024I11_002D2;

		public static Func<CircuitElement, bool> _0024I11_002D1;

		public static Func<CircuitElement, bool> _0024I11_002D3;

		public static Func<Port, bool> _0024I11_002D5;

		public static Func<CircuitElement, bool> _0024I11_002D4;

		public static Func<CircuitElement, bool> _0024I11_002D6;

		public static Func<PowerTerminal, bool> _0024I11_002D7;

		public static Func<PowerTerminal, bool> _0024I11_002D8;

		public static Func<Port, bool> _0024I11_002D9;

		public static VB_0024AnonymousDelegate_13<string, string, bool> _0024I11_002D10;

		public static Func<ElectricCoil, bool> _0024I11_002D12;

		public static Func<ElectricCoil, bool> _0024I11_002D17;

		public static Func<ElectricCoil, string> _0024I11_002D18;

		public static Func<DirectionalValve, string> _0024I11_002D19;

		public static Func<CylinderBase, IEnumerable<(string Mark, CylinderBase Cyl)>> _0024I11_002D20;

		public static Func<(string Mark, CylinderBase Cyl), bool> _0024I11_002D21;

		public static Func<(string Mark, CylinderBase Cyl), string> _0024I11_002D22;

		public static Func<IGrouping<string, (string Mark, CylinderBase Cyl)>, bool> _0024I11_002D23;

		public static Func<(string Mark, CylinderBase Cyl), string> _0024I11_002D24;

		public static Func<CircuitElement, bool> _0024I11_002D25;

		public static Func<CircuitElement, string> _0024I11_002D26;

		public static Func<IGrouping<string, CircuitElement>, bool> _0024I11_002D27;

		public static Func<PageConnector, bool> _0024I11_002D28;

		public static Func<PageConnector, string> _0024I11_002D29;

		public static Func<CircuitElement, bool> _0024I11_002D30;

		public static Func<CircuitMove, string> _0024I14_002D0;

		public static VB_0024AnonymousDelegate_14<CircuitElement, int> _0024I17_002D0;

		public static Func<CircuitElement, bool> _0024I17_002D1;

		public static Func<CircuitElement, string> _0024I17_002D2;

		public static Func<DirectionalValve, bool> _0024I18_002D0;

		public static Func<CheckIssue, bool> _0024I18_002D1;

		public static Func<SingleActingCylinder, bool> _0024I20_002D1;

		public static Func<DoubleActingCylinder, bool> _0024I20_002D2;

		public static Func<ElectricCoil, bool> _0024I20_002D3;

		public static Func<ElectricContact, bool> _0024I20_002D4;

		public static Func<CylinderBase, string> _0024I20_002D5;

		public static Func<CircuitMove, string> _0024I20_002D7;

		public static Func<DirectionalValve, DirectionalValve> _0024I22_002D1;

		public static Func<DirectionalValve, int> _0024I22_002D2;

		public static Func<ElectricCoil, ElectricCoil> _0024I22_002D3;

		public static Func<ElectricCoil, bool> _0024I22_002D4;

		public static Func<CylinderBase, CylinderBase> _0024I22_002D5;

		public static Func<CylinderBase, int> _0024I22_002D6;

		public static Func<CylinderBase, CylinderBase> _0024I22_002D7;

		public static Func<CylinderBase, double> _0024I22_002D8;

		public static Func<DirectionalValve, int> _0024I22_002D10;

		public static Func<CylinderBase, bool> _0024I22_002D12;

		public static Func<CylinderBase, bool> _0024I22_002D13;

		static _Closure_0024__()
		{
			_0024I = new _Closure_0024__();
		}

		[SpecialName]
		internal bool _Lambda_0024__5_002D0(Port o)
		{
			return Operators.CompareString(o.Name, "2", TextCompare: false) != 0;
		}

		[SpecialName]
		internal string _Lambda_0024__8_002D0(CylinderBase k)
		{
			return k.Label;
		}

		[SpecialName]
		internal bool _Lambda_0024__9_002D0(CircuitMove m)
		{
			if (m.Trigger.Manual.Count > 0)
			{
				return m.Extend;
			}
			return false;
		}

		[SpecialName]
		internal bool _Lambda_0024__9_002D1(CircuitMove m)
		{
			return m.Trigger.Manual.Count > 0;
		}

		[SpecialName]
		internal string _Lambda_0024__10_002D1(KeyValuePair<CylinderBase, bool> kv)
		{
			if (!kv.Value)
			{
				return kv.Key.RetractedMark;
			}
			return kv.Key.ExtendedMark;
		}

		[SpecialName]
		internal bool _Lambda_0024__10_002D2(string k)
		{
			return !string.IsNullOrWhiteSpace(k);
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D1(CircuitElement e)
		{
			if (e.Ports.Any([SpecialName] (Port p) => p.Kind == PortKind.Pneumatic) && !(e is Junction))
			{
				return !(e is TextNote);
			}
			return false;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D2(Port p)
		{
			return p.Kind == PortKind.Pneumatic;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D3(CircuitElement x)
		{
			return IsAirSource(x);
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D4(CircuitElement e)
		{
			if (e.Ports.Any([SpecialName] (Port p) => p.Kind == PortKind.Hydraulic))
			{
				return !(e is Junction);
			}
			return false;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D5(Port p)
		{
			return p.Kind == PortKind.Hydraulic;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D6(CircuitElement e)
		{
			if (!(e is ElectricContact))
			{
				return e is ElectricCoil;
			}
			return true;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D7(PowerTerminal t)
		{
			return t.Polarity == Polarity.Plus24V;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D8(PowerTerminal t)
		{
			return t.Polarity == Polarity.Zero0V;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D9(Port p)
		{
			return p.ConnectionCount == 0;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D10(string a, string b)
		{
			return string.Equals(a?.Trim(), b?.Trim(), StringComparison.OrdinalIgnoreCase);
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D12(ElectricCoil x)
		{
			return x.Kind == CoilKind.Solenoid;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D17(ElectricCoil k)
		{
			return !string.IsNullOrWhiteSpace(k.Label);
		}

		[SpecialName]
		internal string _Lambda_0024__11_002D18(ElectricCoil k)
		{
			return k.Label.Trim().ToUpperInvariant();
		}

		[SpecialName]
		internal string _Lambda_0024__11_002D19(DirectionalValve v)
		{
			return Name(v);
		}

		[SpecialName]
		internal IEnumerable<(string, CylinderBase)> _Lambda_0024__11_002D20(CylinderBase cy)
		{
			return new(string, CylinderBase)[2]
			{
				(cy.RetractedMark, cy),
				(cy.ExtendedMark, cy)
			};
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D21((string Mark, CylinderBase Cyl) t)
		{
			return !string.IsNullOrWhiteSpace(t.Mark);
		}

		[SpecialName]
		internal string _Lambda_0024__11_002D22((string Mark, CylinderBase Cyl) t)
		{
			return t.Mark.Trim().ToUpperInvariant();
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D23(IGrouping<string, (string Mark, CylinderBase Cyl)> g)
		{
			return g.Count() > 1;
		}

		[SpecialName]
		internal string _Lambda_0024__11_002D24((string Mark, CylinderBase Cyl) t)
		{
			return Name(t.Cyl);
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D25(CircuitElement e)
		{
			if (e is CylinderBase || e is DirectionalValve || (e is ElectricContact && ((ElectricContact)e).IsManuallyOperated))
			{
				return !string.IsNullOrWhiteSpace(e.Label);
			}
			return false;
		}

		[SpecialName]
		internal string _Lambda_0024__11_002D26(CircuitElement e)
		{
			return e.Label.Trim().ToUpperInvariant();
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D27(IGrouping<string, CircuitElement> g)
		{
			return g.Count() > 1;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D28(PageConnector pc)
		{
			return !string.IsNullOrWhiteSpace(pc.Label);
		}

		[SpecialName]
		internal string _Lambda_0024__11_002D29(PageConnector pc)
		{
			return pc.Label.Trim().ToUpperInvariant() + "|" + pc.Medium;
		}

		[SpecialName]
		internal bool _Lambda_0024__11_002D30(CircuitElement e)
		{
			if (!(e is Junction))
			{
				return !(e is TextNote);
			}
			return false;
		}

		[SpecialName]
		internal string _Lambda_0024__14_002D0(CircuitMove m)
		{
			return m.ToString();
		}

		[SpecialName]
		internal int _Lambda_0024__17_002D0(CircuitElement e)
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
		}

		[SpecialName]
		internal bool _Lambda_0024__17_002D1(CircuitElement e)
		{
			if (e.IsManuallyOperated && !(e is HydraulicPump) && !(e is Compressor) && !(e is ShutOffValve) && !(e is SuctionCup) && !(e is ElectricCounter) && (!(e is ElectricContact) || ((ElectricContact)e).Operator != ContactOperator.EmergencyStop))
			{
				if (e is DirectionalValve)
				{
					return ((DirectionalValve)e).Actuator != ValveActuator.Solenoid;
				}
				return true;
			}
			return false;
		}

		[SpecialName]
		internal string _Lambda_0024__17_002D2(CircuitElement e)
		{
			return e.Label;
		}

		[SpecialName]
		internal bool _Lambda_0024__18_002D0(DirectionalValve x)
		{
			return x.IsMemoryValve;
		}

		[SpecialName]
		internal bool _Lambda_0024__18_002D1(CheckIssue x)
		{
			return x.Message.Contains("both signals");
		}

		[SpecialName]
		internal bool _Lambda_0024__20_002D1(SingleActingCylinder k)
		{
			return !(k is HydraulicSingleActingCylinder);
		}

		[SpecialName]
		internal bool _Lambda_0024__20_002D2(DoubleActingCylinder k)
		{
			if (!(k is HydraulicCylinder))
			{
				return !(k is SemiRotaryActuator);
			}
			return false;
		}

		[SpecialName]
		internal bool _Lambda_0024__20_002D3(ElectricCoil k)
		{
			if (k.Kind != 0 && k.Kind != CoilKind.OnDelayTimer)
			{
				return k.Kind == CoilKind.OffDelayTimer;
			}
			return true;
		}

		[SpecialName]
		internal bool _Lambda_0024__20_002D4(ElectricContact k)
		{
			if (k.Operator != ContactOperator.ProximitySensor)
			{
				return k.Operator == ContactOperator.LimitSwitch;
			}
			return true;
		}

		[SpecialName]
		internal string _Lambda_0024__20_002D5(CylinderBase k)
		{
			return k.Label;
		}

		[SpecialName]
		internal string _Lambda_0024__20_002D7(CircuitMove m)
		{
			return string.Format("{0}{1}", m.Cylinder.Label, m.Extend ? "+" : "-");
		}

		[SpecialName]
		internal DirectionalValve _Lambda_0024__22_002D1(DirectionalValve v)
		{
			return v;
		}

		[SpecialName]
		internal int _Lambda_0024__22_002D2(DirectionalValve v)
		{
			return v.State;
		}

		[SpecialName]
		internal ElectricCoil _Lambda_0024__22_002D3(ElectricCoil k)
		{
			return k;
		}

		[SpecialName]
		internal bool _Lambda_0024__22_002D4(ElectricCoil k)
		{
			return k.Active;
		}

		[SpecialName]
		internal CylinderBase _Lambda_0024__22_002D5(CylinderBase k)
		{
			return k;
		}

		[SpecialName]
		internal int _Lambda_0024__22_002D6(CylinderBase k)
		{
			return 0;
		}

		[SpecialName]
		internal CylinderBase _Lambda_0024__22_002D7(CylinderBase k)
		{
			return k;
		}

		[SpecialName]
		internal double _Lambda_0024__22_002D8(CylinderBase k)
		{
			return k.Position;
		}

		[SpecialName]
		internal int _Lambda_0024__22_002D10(DirectionalValve x)
		{
			return (!x.IsManuallyOperated) ? 1 : 0;
		}

		[SpecialName]
		internal bool _Lambda_0024__22_002D12(CylinderBase k)
		{
			return k.Position > 0.05;
		}

		[SpecialName]
		internal bool _Lambda_0024__22_002D13(CylinderBase k)
		{
			return k.Position <= 0.005;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__0_002D0
	{
		public Dictionary<Port, List<Port>> _0024VB_0024Local_map;

		public _Closure_0024__0_002D0(_Closure_0024__0_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_map = arg0._0024VB_0024Local_map;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(Port a, Port b)
		{
			if (!_0024VB_0024Local_map.ContainsKey(a))
			{
				_0024VB_0024Local_map[a] = new List<Port>();
			}
			_0024VB_0024Local_map[a].Add(b);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__10_002D0
	{
		public CircuitMove _0024VB_0024Local_m;

		public _Closure_0024__10_002D0(_Closure_0024__10_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_m = arg0._0024VB_0024Local_m;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(CircuitMove o)
		{
			if (o.Cylinder == _0024VB_0024Local_m.Cylinder)
			{
				return o.Extend != _0024VB_0024Local_m.Extend;
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__10_002D1
	{
		public List<string> _0024VB_0024Local_activeMarks;

		public _Closure_0024__10_002D1(_Closure_0024__10_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_activeMarks = arg0._0024VB_0024Local_activeMarks;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__3(string k)
		{
			_Closure_0024__10_002D2 arg = default(_Closure_0024__10_002D2);
			_Closure_0024__10_002D2 CS_0024_003C_003E8__locals0 = new _Closure_0024__10_002D2(arg)
			{
				_0024VB_0024Local_k = k
			};
			return _0024VB_0024Local_activeMarks.Any([SpecialName] (string a) => string.Equals(a, CS_0024_003C_003E8__locals0._0024VB_0024Local_k, StringComparison.OrdinalIgnoreCase));
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__10_002D2
	{
		public string _0024VB_0024Local_k;

		public _Closure_0024__10_002D2(_Closure_0024__10_002D2 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_k = arg0._0024VB_0024Local_k;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__4(string a)
		{
			return string.Equals(a, _0024VB_0024Local_k, StringComparison.OrdinalIgnoreCase);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__11_002D0
	{
		public List<CheckIssue> _0024VB_0024Local_issues;

		public VB_0024AnonymousDelegate_13<string, string, bool> _0024VB_0024Local_same;

		public _Closure_0024__11_002D0(_Closure_0024__11_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_issues = arg0._0024VB_0024Local_issues;
				_0024VB_0024Local_same = arg0._0024VB_0024Local_same;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(IssueSeverity sev, string msg, CircuitElement el)
		{
			_0024VB_0024Local_issues.Add(new CheckIssue
			{
				Severity = sev,
				Message = msg,
				Element = el
			});
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__11_002D1
	{
		public string _0024VB_0024Local_sol;

		public _Closure_0024__11_002D0 _0024VB_0024NonLocal__0024VB_0024Closure_2;

		public _Closure_0024__11_002D1(_Closure_0024__11_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_sol = arg0._0024VB_0024Local_sol;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__11(ElectricCoil k)
		{
			if (k.Kind == CoilKind.Solenoid)
			{
				return _0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_same(k.Label, _0024VB_0024Local_sol);
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__11_002D2
	{
		public ElectricCoil _0024VB_0024Local_k;

		public _Closure_0024__11_002D2(_Closure_0024__11_002D2 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_k = arg0._0024VB_0024Local_k;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__13(DirectionalValve v)
		{
			return v.UsesSolenoid(_0024VB_0024Local_k.Label);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__11_002D3
	{
		public ElectricContact _0024VB_0024Local_k;

		public _Closure_0024__11_002D0 _0024VB_0024NonLocal__0024VB_0024Closure_3;

		public _Closure_0024__11_002D3(_Closure_0024__11_002D3 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_k = arg0._0024VB_0024Local_k;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__14(ElectricCoil x)
		{
			if (x.Kind != CoilKind.Solenoid && x.Kind != CoilKind.Lamp)
			{
				return _0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_same(x.Label, _0024VB_0024Local_k.Reference);
			}
			return false;
		}

		[SpecialName]
		internal bool _Lambda_0024__15(ElectricCounter x)
		{
			return _0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_same(x.Label, _0024VB_0024Local_k.Reference);
		}

		[SpecialName]
		internal bool _Lambda_0024__16(PressureSwitch x)
		{
			return _0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_same(x.Label, _0024VB_0024Local_k.Reference);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__16_002D0
	{
		public int[] _0024VB_0024Local_parent;

		public Dictionary<Port, int> _0024VB_0024Local_index;

		public Func<int, int> _0024VB_0024Local_find;

		public _Closure_0024__16_002D0(_Closure_0024__16_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_parent = arg0._0024VB_0024Local_parent;
				_0024VB_0024Local_index = arg0._0024VB_0024Local_index;
				_0024VB_0024Local_find = arg0._0024VB_0024Local_find;
			}
		}

		[SpecialName]
		internal int _Lambda_0024__0(int i)
		{
			while (_0024VB_0024Local_parent[i] != i)
			{
				_0024VB_0024Local_parent[i] = _0024VB_0024Local_parent[_0024VB_0024Local_parent[i]];
				i = _0024VB_0024Local_parent[i];
			}
			return i;
		}

		[SpecialName]
		internal void _Lambda_0024__1(Port a, Port b)
		{
			if (a != null && b != null && _0024VB_0024Local_index.ContainsKey(a) && _0024VB_0024Local_index.ContainsKey(b))
			{
				int num = _0024VB_0024Local_find(_0024VB_0024Local_index[a]);
				int num2 = _0024VB_0024Local_find(_0024VB_0024Local_index[b]);
				if (num != num2)
				{
					_0024VB_0024Local_parent[num] = num2;
				}
			}
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__18_002D0
	{
		public DirectionalValve _0024VB_0024Local_v;

		public _Closure_0024__18_002D0(_Closure_0024__18_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_v = arg0._0024VB_0024Local_v;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__2(CircuitElement e)
		{
			if (Operators.CompareString(e.Label, _0024VB_0024Local_v.Label, TextCompare: false) == 0)
			{
				return Operators.CompareString(e.TypeName, _0024VB_0024Local_v.TypeName, TextCompare: false) == 0;
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__18_002D1
	{
		public HashSet<CylinderBase> _0024VB_0024Local_moved;

		public _Closure_0024__18_002D1(_Closure_0024__18_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_moved = arg0._0024VB_0024Local_moved;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__3(CylinderBase k)
		{
			return !_0024VB_0024Local_moved.Contains(k);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__18_002D2
	{
		public HashSet<string> _0024VB_0024Local_together;

		public Func<CylinderBase, bool> _0024I4;

		public _Closure_0024__18_002D2(_Closure_0024__18_002D2 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_together = arg0._0024VB_0024Local_together;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__4(CylinderBase k)
		{
			return !_0024VB_0024Local_together.Contains(Name(k));
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__20_002D0
	{
		public List<string> _0024VB_0024Local_parts;

		public _Closure_0024__20_002D0(_Closure_0024__20_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_parts = arg0._0024VB_0024Local_parts;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(int n, string singular, string plural)
		{
			if (n > 0)
			{
				_0024VB_0024Local_parts.Add($"{n} {((n == 1) ? singular : plural)}");
			}
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__20_002D1
	{
		public CylinderBase _0024VB_0024Local_cyl;

		public Func<CircuitMove, bool> _0024I6;

		public _Closure_0024__20_002D1(_Closure_0024__20_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_cyl = arg0._0024VB_0024Local_cyl;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__6(CircuitMove x)
		{
			return x.Cylinder == _0024VB_0024Local_cyl;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__22_002D0
	{
		public CircuitElement _0024VB_0024Local_operate;

		public Dictionary<CylinderBase, int> _0024VB_0024Local_cylDir;

		public Dictionary<CylinderBase, double> _0024VB_0024Local_cylPos;

		public Dictionary<ElectricCoil, bool> _0024VB_0024Local_coilState;

		public Dictionary<DirectionalValve, int> _0024VB_0024Local_valveState;

		public int _0024VB_0024Local_stepNo;

		public List<string> _0024VB_0024Local_lines;

		public Simulator _0024VB_0024Local_sim;

		public double _0024VB_0024Local_lastEvent;

		public Func<CylinderBase, bool> _0024I14;

		public _Closure_0024__22_002D0(_Closure_0024__22_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_operate = arg0._0024VB_0024Local_operate;
				_0024VB_0024Local_cylDir = arg0._0024VB_0024Local_cylDir;
				_0024VB_0024Local_cylPos = arg0._0024VB_0024Local_cylPos;
				_0024VB_0024Local_coilState = arg0._0024VB_0024Local_coilState;
				_0024VB_0024Local_valveState = arg0._0024VB_0024Local_valveState;
				_0024VB_0024Local_stepNo = arg0._0024VB_0024Local_stepNo;
				_0024VB_0024Local_lines = arg0._0024VB_0024Local_lines;
				_0024VB_0024Local_sim = arg0._0024VB_0024Local_sim;
				_0024VB_0024Local_lastEvent = arg0._0024VB_0024Local_lastEvent;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(CircuitElement e)
		{
			if (Operators.CompareString(e.TypeName, _0024VB_0024Local_operate.TypeName, TextCompare: false) == 0 && Operators.CompareString(e.Label, _0024VB_0024Local_operate.Label, TextCompare: false) == 0 && e.X == _0024VB_0024Local_operate.X)
			{
				return e.Y == _0024VB_0024Local_operate.Y;
			}
			return false;
		}

		[SpecialName]
		internal List<string> _Lambda_0024__9(bool withMotion)
		{
			List<string> list = new List<string>();
			List<string> list2 = new List<string>();
			List<string> list3 = new List<string>();
			foreach (CylinderBase item in withMotion ? _0024VB_0024Local_cylDir.Keys.ToList() : new List<CylinderBase>())
			{
				int num = Math.Sign(item.Position - _0024VB_0024Local_cylPos[item]);
				if (Math.Abs(item.Position - _0024VB_0024Local_cylPos[item]) < 1E-05)
				{
					num = 0;
				}
				if (_0024VB_0024Local_cylPos[item] < 0.995 && item.Position >= 0.995)
				{
					list2.Add(string.Format("cylinder {0} is fully extended{1}", Name(item), string.IsNullOrEmpty(item.ExtendedMark) ? "" : $" (operates {item.ExtendedMark})"));
				}
				if (_0024VB_0024Local_cylPos[item] > 0.005 && item.Position <= 0.005)
				{
					list2.Add(string.Format("cylinder {0} is fully retracted{1}", Name(item), string.IsNullOrEmpty(item.RetractedMark) ? "" : $" (operates {item.RetractedMark})"));
				}
				if (num != _0024VB_0024Local_cylDir[item])
				{
					if (num > 0)
					{
						list3.Add($"cylinder {Name(item)} starts to extend");
					}
					if (num < 0)
					{
						list3.Add($"cylinder {Name(item)} starts to retract");
					}
					if (num == 0 && item.Position > 0.005 && item.Position < 0.995)
					{
						list3.Add($"cylinder {Name(item)} stops at {item.Position * 100.0:0} % of its stroke");
					}
					_0024VB_0024Local_cylDir[item] = num;
				}
				_0024VB_0024Local_cylPos[item] = item.Position;
			}
			list.AddRange(list2);
			foreach (ElectricCoil item2 in _0024VB_0024Local_coilState.Keys.ToList())
			{
				if (item2.Active != _0024VB_0024Local_coilState[item2])
				{
					_0024VB_0024Local_coilState[item2] = item2.Active;
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
			foreach (DirectionalValve item3 in _0024VB_0024Local_valveState.Keys.OrderBy([SpecialName] (DirectionalValve x) => (!x.IsManuallyOperated) ? 1 : 0).ToList())
			{
				if (item3.State != _0024VB_0024Local_valveState[item3])
				{
					_0024VB_0024Local_valveState[item3] = item3.State;
					if (!item3.IsRollerOperated)
					{
						list.Add($"valve {Name(item3)} switches to {PositionName(item3)}");
					}
				}
			}
			list.AddRange(list3);
			return list;
		}

		[SpecialName]
		internal void _Lambda_0024__11(string prefix, List<string> ev)
		{
			checked
			{
				if (ev.Count != 0 || Operators.CompareString(prefix, "", TextCompare: false) != 0)
				{
					_0024VB_0024Local_stepNo++;
					string text = string.Join(" → ", ev);
					_0024VB_0024Local_lines.Add(string.Format("{0,2}. t = {1,5:0.00} s   {2}{3}{4}", _0024VB_0024Local_stepNo, _0024VB_0024Local_sim.Time, prefix, (Operators.CompareString(prefix, "", TextCompare: false) != 0 && Operators.CompareString(text, "", TextCompare: false) != 0) ? " → " : "", text));
					_0024VB_0024Local_lastEvent = _0024VB_0024Local_sim.Time;
				}
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__14(CylinderBase k)
		{
			return _0024VB_0024Local_cylDir[k] != 0;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__7_002D0
	{
		public Port _0024VB_0024Local_q;

		public _Closure_0024__7_002D0(_Closure_0024__7_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_q = arg0._0024VB_0024Local_q;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(Port o)
		{
			return o != _0024VB_0024Local_q;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__8_002D0
	{
		public DirectionalValve _0024VB_0024Local_v;

		public string _0024VB_0024Local_supplyName;

		public Port _0024VB_0024Local_vp;

		public Func<string[], bool> _0024I2;

		public VB_0024AnonymousDelegate_11<int, bool> _0024I1;

		public _Closure_0024__8_002D0(_Closure_0024__8_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_v = arg0._0024VB_0024Local_v;
				_0024VB_0024Local_supplyName = arg0._0024VB_0024Local_supplyName;
				_0024VB_0024Local_vp = arg0._0024VB_0024Local_vp;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__1(int pos)
		{
			return _0024VB_0024Local_v.PassagesIn(pos).Any([SpecialName] (string[] f) => Operators.CompareString(f[0], _0024VB_0024Local_supplyName, TextCompare: false) == 0 && Operators.CompareString(f[1], _0024VB_0024Local_vp.Name, TextCompare: false) == 0);
		}

		[SpecialName]
		internal bool _Lambda_0024__2(string[] f)
		{
			if (Operators.CompareString(f[0], _0024VB_0024Local_supplyName, TextCompare: false) == 0)
			{
				return Operators.CompareString(f[1], _0024VB_0024Local_vp.Name, TextCompare: false) == 0;
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__8_002D1
	{
		public string _0024VB_0024Local_sol;

		public _Closure_0024__8_002D1(_Closure_0024__8_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_sol = arg0._0024VB_0024Local_sol;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__3(ElectricCoil k)
		{
			if (k.Kind == CoilKind.Solenoid)
			{
				return string.Equals(k.Label, _0024VB_0024Local_sol, StringComparison.OrdinalIgnoreCase);
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__9_002D0
	{
		public List<CircuitMove> _0024VB_0024Local_result;

		public _Closure_0024__9_002D0(_Closure_0024__9_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_result = arg0._0024VB_0024Local_result;
			}
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__9_002D1
	{
		public string _0024VB_0024Local_mark;

		public _Closure_0024__9_002D0 _0024VB_0024NonLocal__0024VB_0024Closure_2;

		public Func<string, bool> _0024I3;

		public _Closure_0024__9_002D1(_Closure_0024__9_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_mark = arg0._0024VB_0024Local_mark;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__2(CircuitMove m)
		{
			if (!_0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_result.Contains(m))
			{
				return m.Trigger.Marks.Any([SpecialName] (string k) => string.Equals(k, _0024VB_0024Local_mark, StringComparison.OrdinalIgnoreCase));
			}
			return false;
		}

		[SpecialName]
		internal bool _Lambda_0024__3(string k)
		{
			return string.Equals(k, _0024VB_0024Local_mark, StringComparison.OrdinalIgnoreCase);
		}
	}

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

	public static List<CheckIssue> StaticChecks(Circuit c)
	{
		_Closure_0024__11_002D0 arg = default(_Closure_0024__11_002D0);
		_Closure_0024__11_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__11_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_issues = new List<CheckIssue>();
		VB_0024AnonymousDelegate_12<IssueSeverity, string, CircuitElement> vB_0024AnonymousDelegate_ = [SpecialName] (IssueSeverity sev, string msg, CircuitElement el) =>
		{
			CS_0024_003C_003E8__locals0._0024VB_0024Local_issues.Add(new CheckIssue
			{
				Severity = sev,
				Message = msg,
				Element = el
			});
		};
		if (c.Elements.Count == 0)
		{
			vB_0024AnonymousDelegate_(IssueSeverity.Info, "The circuit is empty. Place components from the library or load an example.", null);
			return CS_0024_003C_003E8__locals0._0024VB_0024Local_issues;
		}
		List<CircuitElement> elements = c.Elements;
		if (elements.Any([SpecialName] (CircuitElement e) => e.Ports.Any([SpecialName] (Port p) => p.Kind == PortKind.Pneumatic) && !(e is Junction) && !(e is TextNote)) && !elements.Any([SpecialName] (CircuitElement x) => IsAirSource(x)) && !elements.OfType<PageConnector>().Any())
		{
			vB_0024AnonymousDelegate_(IssueSeverity.Error, "There is no compressed air supply in the circuit.", null);
		}
		if (elements.Any([SpecialName] (CircuitElement e) => e.Ports.Any([SpecialName] (Port p) => p.Kind == PortKind.Hydraulic) && !(e is Junction)) && !elements.OfType<HydraulicPump>().Any())
		{
			vB_0024AnonymousDelegate_(IssueSeverity.Error, "There is no hydraulic pump in the circuit.", null);
		}
		if (elements.OfType<HydraulicPump>().Any() && !elements.OfType<ReliefValve>().Any())
		{
			vB_0024AnonymousDelegate_(IssueSeverity.Error, "Hydraulic circuit without a pressure relief valve: the pump pressure rises without limit when nothing moves.", elements.OfType<HydraulicPump>().First());
		}
		bool num = elements.Any([SpecialName] (CircuitElement e) => e is ElectricContact || e is ElectricCoil);
		if (num && !elements.OfType<PowerTerminal>().Any([SpecialName] (PowerTerminal t) => t.Polarity == Polarity.Plus24V))
		{
			vB_0024AnonymousDelegate_(IssueSeverity.Error, "The electrical circuit has no +24 V connection.", null);
		}
		if (num && !elements.OfType<PowerTerminal>().Any([SpecialName] (PowerTerminal t) => t.Polarity == Polarity.Zero0V))
		{
			vB_0024AnonymousDelegate_(IssueSeverity.Error, "The electrical circuit has no 0 V connection.", null);
		}
		foreach (CircuitElement item in elements)
		{
			if (item is Junction || item is TextNote)
			{
				continue;
			}
			List<Port> list = item.Ports.Where([SpecialName] (Port p) => p.ConnectionCount == 0).ToList();
			if (list.Count == 0)
			{
				continue;
			}
			if (item is DirectionalValve directionalValve)
			{
				foreach (Port item2 in list)
				{
					if (Operators.CompareString(item2.Name, "1", TextCompare: false) == 0 || Operators.CompareString(item2.Name, "P", TextCompare: false) == 0)
					{
						vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Valve {Name(directionalValve)}: supply port {item2.Name} is not connected, so the valve never delivers anything.", directionalValve);
					}
					else if (Operators.CompareString(item2.Name, directionalValve.PilotPortName(leftSide: true), TextCompare: false) == 0 || Operators.CompareString(item2.Name, directionalValve.PilotPortName(leftSide: false), TextCompare: false) == 0)
					{
						vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Valve {Name(directionalValve)}: pilot port {item2.Name} is not connected, so the valve can never switch that way.", directionalValve);
					}
					else if (item2.Kind == PortKind.Hydraulic)
					{
						vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Valve {Name(directionalValve)}: hydraulic port {item2.Name} is open; oil would leak out.", directionalValve);
					}
					else if (!item2.VentsWhenOpen)
					{
						vB_0024AnonymousDelegate_(IssueSeverity.Warning, $"Valve {Name(directionalValve)}: output {item2.Name} is not connected; air will blow out when it is switched on.", directionalValve);
					}
				}
			}
			else if (item is CylinderBase)
			{
				foreach (Port item3 in list)
				{
					vB_0024AnonymousDelegate_((item3.Kind != PortKind.Hydraulic) ? IssueSeverity.Warning : IssueSeverity.Error, string.Format("Cylinder {0}: port {1} is not connected{2}.", Name(item), item3.Name, (item3.Kind == PortKind.Hydraulic) ? " (oil would leak)" : "; it vents to atmosphere"), item);
				}
			}
			else if (item is ElectricCoil || item is ElectricContact || item is PowerTerminal)
			{
				vB_0024AnonymousDelegate_(IssueSeverity.Error, $"{item.DisplayName} {Name(item)}: terminal {list[0].Name} is not wired.", item);
			}
			else if (IsAirSource(item) || item is HydraulicPump)
			{
				vB_0024AnonymousDelegate_(IssueSeverity.Warning, $"{item.DisplayName} {Name(item)}: port {list[0].Name} is not connected.", item);
			}
			else if (!(item is Silencer) && !(item is QuickExhaustValve))
			{
				vB_0024AnonymousDelegate_(IssueSeverity.Warning, $"{item.DisplayName} {Name(item)}: port {list[0].Name} is not connected.", item);
			}
		}
		List<ElectricCoil> source = elements.OfType<ElectricCoil>().ToList();
		CS_0024_003C_003E8__locals0._0024VB_0024Local_same = [SpecialName] (string a, string b) => string.Equals(a?.Trim(), b?.Trim(), StringComparison.OrdinalIgnoreCase);
		checked
		{
			_Closure_0024__11_002D1 closure_0024__11_002D = default(_Closure_0024__11_002D1);
			foreach (DirectionalValve item4 in elements.OfType<DirectionalValve>())
			{
				string[] array = new string[2]
				{
					(item4.Actuator == ValveActuator.Solenoid) ? item4.SolenoidLabel : null,
					(item4.ReturnType == ValveReturn.Solenoid) ? item4.ReturnSolenoidLabel : null
				};
				for (int i = 0; i < array.Length; i++)
				{
					closure_0024__11_002D = new _Closure_0024__11_002D1(closure_0024__11_002D);
					closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2 = CS_0024_003C_003E8__locals0;
					closure_0024__11_002D._0024VB_0024Local_sol = array[i];
					if (closure_0024__11_002D._0024VB_0024Local_sol != null && !source.Any(closure_0024__11_002D._Lambda_0024__11))
					{
						vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Valve {Name(item4)} uses solenoid {closure_0024__11_002D._0024VB_0024Local_sol}, but there is no 'Valve solenoid' coil labelled {closure_0024__11_002D._0024VB_0024Local_sol} in the electrical circuit.", item4);
					}
				}
				if (item4.IsRollerOperated && !MarkExists(elements, item4.TriggerMark))
				{
					vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Roller valve {Name(item4)}: no cylinder has a position mark named '{item4.TriggerMark}', so the roller is never operated.", item4);
				}
			}
			using (IEnumerator<ElectricCoil> enumerator5 = source.Where([SpecialName] (ElectricCoil x) => x.Kind == CoilKind.Solenoid).GetEnumerator())
			{
				_Closure_0024__11_002D2 closure_0024__11_002D2 = default(_Closure_0024__11_002D2);
				while (enumerator5.MoveNext())
				{
					closure_0024__11_002D2 = new _Closure_0024__11_002D2(closure_0024__11_002D2);
					closure_0024__11_002D2._0024VB_0024Local_k = enumerator5.Current;
					if (!elements.OfType<DirectionalValve>().Any(closure_0024__11_002D2._Lambda_0024__13))
					{
						vB_0024AnonymousDelegate_(IssueSeverity.Warning, $"Solenoid coil {closure_0024__11_002D2._0024VB_0024Local_k.Label} does not operate any valve (no valve uses solenoid label {closure_0024__11_002D2._0024VB_0024Local_k.Label}).", closure_0024__11_002D2._0024VB_0024Local_k);
					}
				}
			}
			using (IEnumerator<ElectricContact> enumerator6 = elements.OfType<ElectricContact>().GetEnumerator())
			{
				_Closure_0024__11_002D3 closure_0024__11_002D3 = default(_Closure_0024__11_002D3);
				while (enumerator6.MoveNext())
				{
					closure_0024__11_002D3 = new _Closure_0024__11_002D3(closure_0024__11_002D3);
					closure_0024__11_002D3._0024VB_0024NonLocal__0024VB_0024Closure_3 = CS_0024_003C_003E8__locals0;
					closure_0024__11_002D3._0024VB_0024Local_k = enumerator6.Current;
					bool flag = !string.IsNullOrWhiteSpace(closure_0024__11_002D3._0024VB_0024Local_k.Label) && MarkExists(elements, closure_0024__11_002D3._0024VB_0024Local_k.Label.Trim());
					if (closure_0024__11_002D3._0024VB_0024Local_k.Operator == ContactOperator.Relay && !source.Any(closure_0024__11_002D3._Lambda_0024__14) && !elements.OfType<ElectricCounter>().Any(closure_0024__11_002D3._Lambda_0024__15))
					{
						if (flag)
						{
							vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Contact {Name(closure_0024__11_002D3._0024VB_0024Local_k)} is a relay contact, so it only closes when a relay coil switches it. To make the cylinder operate it, " + $"set 'Operated by' to Limit switch (or Proximity sensor) and 'Reference' to {closure_0024__11_002D3._0024VB_0024Local_k.Label.Trim()}.", closure_0024__11_002D3._0024VB_0024Local_k);
						}
						else if (string.IsNullOrWhiteSpace(closure_0024__11_002D3._0024VB_0024Local_k.Reference))
						{
							vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Relay contact {Name(closure_0024__11_002D3._0024VB_0024Local_k)} has no Reference, so it never switches. Enter the label of its relay coil (e.g. K1) in 'Reference'.", closure_0024__11_002D3._0024VB_0024Local_k);
						}
						else
						{
							vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Relay contact {Name(closure_0024__11_002D3._0024VB_0024Local_k)} refers to {closure_0024__11_002D3._0024VB_0024Local_k.Reference}, but there is no relay coil labelled {closure_0024__11_002D3._0024VB_0024Local_k.Reference}.", closure_0024__11_002D3._0024VB_0024Local_k);
						}
					}
					if (closure_0024__11_002D3._0024VB_0024Local_k.Operator == ContactOperator.PressureSwitch && !elements.OfType<PressureSwitch>().Any(closure_0024__11_002D3._Lambda_0024__16))
					{
						vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Pressure switch contact {Name(closure_0024__11_002D3._0024VB_0024Local_k)} refers to '{closure_0024__11_002D3._0024VB_0024Local_k.Reference}', but there is no pressure switch with that label.", closure_0024__11_002D3._0024VB_0024Local_k);
					}
					if ((closure_0024__11_002D3._0024VB_0024Local_k.Operator == ContactOperator.ProximitySensor || closure_0024__11_002D3._0024VB_0024Local_k.Operator == ContactOperator.LimitSwitch) && !MarkExists(elements, closure_0024__11_002D3._0024VB_0024Local_k.Reference))
					{
						if (string.IsNullOrWhiteSpace(closure_0024__11_002D3._0024VB_0024Local_k.Reference) && flag)
						{
							vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Sensor {Name(closure_0024__11_002D3._0024VB_0024Local_k)} has no Reference. Enter the cylinder position mark {closure_0024__11_002D3._0024VB_0024Local_k.Label.Trim()} in 'Reference' (the label alone does not link it).", closure_0024__11_002D3._0024VB_0024Local_k);
						}
						else
						{
							vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Sensor {Name(closure_0024__11_002D3._0024VB_0024Local_k)}: no cylinder has a position mark named '{closure_0024__11_002D3._0024VB_0024Local_k.Reference}'.", closure_0024__11_002D3._0024VB_0024Local_k);
						}
					}
				}
			}
			foreach (IGrouping<string, ElectricCoil> item5 in from k in source
				where !string.IsNullOrWhiteSpace(k.Label)
				group k by k.Label.Trim().ToUpperInvariant())
			{
				if (item5.Count() > 1)
				{
					vB_0024AnonymousDelegate_(IssueSeverity.Error, $"{item5.Count()} coils share the label {item5.First().Label}; each coil needs its own name.", item5.First());
				}
			}
			Dictionary<string, List<DirectionalValve>> dictionary = new Dictionary<string, List<DirectionalValve>>(StringComparer.OrdinalIgnoreCase);
			foreach (DirectionalValve item6 in elements.OfType<DirectionalValve>())
			{
				string[] array2 = new string[2]
				{
					(item6.Actuator == ValveActuator.Solenoid) ? item6.SolenoidLabel : null,
					(item6.ReturnType == ValveReturn.Solenoid) ? item6.ReturnSolenoidLabel : null
				};
				foreach (string text in array2)
				{
					if (!string.IsNullOrWhiteSpace(text))
					{
						List<DirectionalValve> value = null;
						if (!dictionary.TryGetValue(text.Trim(), out value))
						{
							value = new List<DirectionalValve>();
							dictionary[text.Trim()] = value;
						}
						value.Add(item6);
					}
				}
			}
			foreach (KeyValuePair<string, List<DirectionalValve>> item7 in dictionary)
			{
				if (item7.Value.Count > 1 && item7.Value.Distinct().Count() > 1)
				{
					vB_0024AnonymousDelegate_(IssueSeverity.Error, string.Format("Valves {0} all use solenoid {1}, so they always switch together. Give each valve its own solenoid label.", string.Join(" and ", from v in item7.Value.Distinct()
						select Name(v)), item7.Key), item7.Value[1]);
				}
				else if (item7.Value.Count > 1)
				{
					vB_0024AnonymousDelegate_(IssueSeverity.Error, $"Valve {Name(item7.Value[0])} uses solenoid {item7.Key} on both sides; the right solenoid needs its own label.", item7.Value[0]);
				}
			}
			IEnumerable<IGrouping<string, (string Mark, CylinderBase Cyl)>> source2 = from t in elements.OfType<CylinderBase>().SelectMany([SpecialName] (CylinderBase cy) => new (string Mark, CylinderBase Cyl)[2]
				{
					(cy.RetractedMark, cy),
					(cy.ExtendedMark, cy)
				})
				where !string.IsNullOrWhiteSpace(t.Mark)
				group t by t.Mark.Trim().ToUpperInvariant();
			foreach (IGrouping<string, (string Mark, CylinderBase Cyl)> item8 in source2.Where([SpecialName] (IGrouping<string, (string Mark, CylinderBase Cyl)> g) => g.Count() > 1))
			{
				vB_0024AnonymousDelegate_(IssueSeverity.Error, string.Format("The position mark {0} is used {1} times ({2}); each end position needs its own mark.", item8.First().Item1, item8.Count(), string.Join(", ", item8.Select<(string, CylinderBase), string>((_Closure_0024__._0024I11_002D24 == null) ? (_Closure_0024__._0024I11_002D24 = [SpecialName] ((string Mark, CylinderBase Cyl) t) => Name(t.Cyl)) : _Closure_0024__._0024I11_002D24).Distinct())), item8.First().Item2);
			}
			IEnumerable<CircuitElement> source3 = elements.Where([SpecialName] (CircuitElement e) => (e is CylinderBase || e is DirectionalValve || (e is ElectricContact && ((ElectricContact)e).IsManuallyOperated)) && !string.IsNullOrWhiteSpace(e.Label));
			foreach (IGrouping<string, CircuitElement> item9 in from e in source3
				group e by e.Label.Trim().ToUpperInvariant() into g
				where g.Count() > 1
				select g)
			{
				vB_0024AnonymousDelegate_(IssueSeverity.Warning, $"{item9.Count()} components are called {item9.First().Label}; give each its own name so the circuit can be read and explained.", item9.ElementAt(1));
			}
			foreach (IGrouping<string, PageConnector> item10 in from pc in elements.OfType<PageConnector>()
				where !string.IsNullOrWhiteSpace(pc.Label)
				group pc by pc.Label.Trim().ToUpperInvariant() + "|" + pc.Medium)
			{
				if (item10.Count() == 1)
				{
					vB_0024AnonymousDelegate_(IssueSeverity.Warning, $"Page connector {item10.First().Label} has no partner with the same name, so the line ends there.", item10.First());
				}
			}
			AddReachabilityIssues(c, CS_0024_003C_003E8__locals0._0024VB_0024Local_issues);
			List<CircuitElement> list2 = elements.Where([SpecialName] (CircuitElement e) => !(e is Junction) && !(e is TextNote)).ToList();
			int num2 = list2.Count - 1;
			for (int l = 0; l <= num2; l++)
			{
				int num3 = l + 1;
				int num4 = list2.Count - 1;
				for (int m = num3; m <= num4; m++)
				{
					RectangleF a2 = list2[l].WorldBounds();
					RectangleF rectangleF = list2[m].WorldBounds();
					a2.Inflate(-6f, -6f);
					rectangleF.Inflate(-6f, -6f);
					if (a2.IntersectsWith(rectangleF))
					{
						RectangleF rectangleF2 = RectangleF.Intersect(a2, rectangleF);
						if ((double)(rectangleF2.Width * rectangleF2.Height) > 0.3 * (double)Math.Min(a2.Width * a2.Height, rectangleF.Width * rectangleF.Height))
						{
							vB_0024AnonymousDelegate_(IssueSeverity.Info, $"{Name(list2[l])} and {Name(list2[m])} overlap on the drawing.", list2[m]);
						}
					}
				}
			}
			try
			{
				List<CircuitMove> moves = FindMoves(c);
				List<CircuitMove> list3 = InferSequence(moves);
				foreach (string item11 in FindOverlaps(moves, list3))
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_issues.Add(OverlapIssue(c, item11, list3));
				}
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				ProjectData.ClearProjectError();
			}
			return CS_0024_003C_003E8__locals0._0024VB_0024Local_issues;
		}
	}

	private static bool IsAirSource(CircuitElement e)
	{
		if (!(e is AirSupply) && !(e is Compressor))
		{
			return e is AirReceiver;
		}
		return true;
	}

	private static bool MarkExists(IEnumerable<CircuitElement> els, string mark)
	{
		if (string.IsNullOrWhiteSpace(mark))
		{
			return false;
		}
		return els.OfType<ISignalSource>().Any([SpecialName] (ISignalSource k) => k.SignalNames().Contains(mark.Trim(), StringComparer.OrdinalIgnoreCase));
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

	private static void AddReachabilityIssues(Circuit c, List<CheckIssue> issues)
	{
		_Closure_0024__16_002D0 arg = default(_Closure_0024__16_002D0);
		_Closure_0024__16_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__16_002D0(arg);
		List<Port> list = c.AllPorts().ToList();
		CS_0024_003C_003E8__locals0._0024VB_0024Local_index = new Dictionary<Port, int>();
		checked
		{
			int num = list.Count - 1;
			for (int j = 0; j <= num; j++)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_index[list[j]] = j;
			}
			CS_0024_003C_003E8__locals0._0024VB_0024Local_parent = Enumerable.Range(0, list.Count).ToArray();
			CS_0024_003C_003E8__locals0._0024VB_0024Local_find = [SpecialName] (int i) =>
			{
				while (CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] != i)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i]];
					i = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i];
				}
				return i;
			};
			VB_0024AnonymousDelegate_10<Port, Port> vB_0024AnonymousDelegate_ = [SpecialName] (Port a, Port b) =>
			{
				if (a != null && b != null && CS_0024_003C_003E8__locals0._0024VB_0024Local_index.ContainsKey(a) && CS_0024_003C_003E8__locals0._0024VB_0024Local_index.ContainsKey(b))
				{
					int num3 = CS_0024_003C_003E8__locals0._0024VB_0024Local_find(CS_0024_003C_003E8__locals0._0024VB_0024Local_index[a]);
					int num4 = CS_0024_003C_003E8__locals0._0024VB_0024Local_find(CS_0024_003C_003E8__locals0._0024VB_0024Local_index[b]);
					if (num3 != num4)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[num3] = num4;
					}
				}
			};
			foreach (Tube tube in c.Tubes)
			{
				vB_0024AnonymousDelegate_(tube.A, tube.B);
			}
			foreach (CircuitElement element in c.Elements)
			{
				if (element is DirectionalValve directionalValve)
				{
					int num2 = directionalValve.Positions - 1;
					for (int k = 0; k <= num2; k++)
					{
						string[][] array = directionalValve.PassagesIn(k);
						foreach (string[] array2 in array)
						{
							vB_0024AnonymousDelegate_(directionalValve.GetPort(array2[0]), directionalValve.GetPort(array2[1]));
						}
					}
				}
				else if (element is LogicValve || element is QuickExhaustValve || element is CounterbalanceValve)
				{
					Port a2 = element.Ports[0];
					foreach (Port item3 in element.Ports.Skip(1))
					{
						vB_0024AnonymousDelegate_(a2, item3);
					}
				}
				else if (element is FlowControlValve || element is CheckValve || element is PressureRegulator || element is CompensatedFlowControl || element is AirReceiver || element is FlowMeter || element is ShutOffValve)
				{
					vB_0024AnonymousDelegate_(element.Ports[0], element.Ports[1]);
				}
			}
			List<PageConnector> list2 = c.Elements.OfType<PageConnector>().ToList();
			foreach (PageConnector item4 in list2)
			{
				foreach (PageConnector item5 in list2)
				{
					if (item4.Matches(item5))
					{
						vB_0024AnonymousDelegate_(item4.Ports[0], item5.Ports[0]);
					}
				}
			}
			HashSet<int> hashSet = new HashSet<int>();
			HashSet<int> hashSet2 = new HashSet<int>();
			foreach (Port item6 in list)
			{
				if (IsAirSource(item6.Owner) || item6.Owner is HydraulicPump || item6.Owner is PageConnector)
				{
					hashSet.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_find(CS_0024_003C_003E8__locals0._0024VB_0024Local_index[item6]));
				}
				if ((item6.VentsWhenOpen && item6.ConnectionCount == 0) || item6.Owner is Silencer || item6.Owner is HydraulicTank || item6.Owner is PageConnector)
				{
					hashSet2.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_find(CS_0024_003C_003E8__locals0._0024VB_0024Local_index[item6]));
				}
			}
			foreach (CylinderBase item7 in c.Elements.OfType<CylinderBase>())
			{
				Port port = item7.Ports[0];
				if (port.ConnectionCount == 0)
				{
					continue;
				}
				int item = CS_0024_003C_003E8__locals0._0024VB_0024Local_find(CS_0024_003C_003E8__locals0._0024VB_0024Local_index[port]);
				if (!hashSet.Contains(item))
				{
					issues.Add(new CheckIssue
					{
						Severity = IssueSeverity.Error,
						Element = item7,
						Message = $"Cylinder {Name(item7)} can never extend: no valve position connects its cap side to the supply."
					});
				}
				else if (!hashSet2.Contains(item))
				{
					issues.Add(new CheckIssue
					{
						Severity = IssueSeverity.Error,
						Element = item7,
						Message = $"Cylinder {Name(item7)}: its cap side can never exhaust, so it cannot retract (missing exhaust)."
					});
				}
				if (item7.Ports.Count > 1 && item7.Ports[1].ConnectionCount > 0)
				{
					int item2 = CS_0024_003C_003E8__locals0._0024VB_0024Local_find(CS_0024_003C_003E8__locals0._0024VB_0024Local_index[item7.Ports[1]]);
					if (!hashSet2.Contains(item2))
					{
						issues.Add(new CheckIssue
						{
							Severity = IssueSeverity.Error,
							Element = item7,
							Message = $"Cylinder {Name(item7)}: its rod side can never exhaust, so it cannot extend (missing exhaust)."
						});
					}
				}
			}
		}
	}

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
