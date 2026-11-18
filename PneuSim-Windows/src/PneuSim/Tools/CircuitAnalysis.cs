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
public sealed partial class CircuitAnalysis
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
}
