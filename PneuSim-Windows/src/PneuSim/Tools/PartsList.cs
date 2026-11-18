using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class PartsList
{
	[Serializable]
	[CompilerGenerated]
	internal sealed class _Closure_0024__
	{
		public static readonly _Closure_0024__ _0024I;

		public static Func<Tube, bool> _0024I1_002D1;

		public static Func<Tube, bool> _0024I1_002D2;

		public static Func<PartLine, string> _0024I1_002D4;

		public static VB_0024AnonymousDelegate_19<string, string> _0024I2_002D0;

		public static Func<PartLine, double> _0024I2_002D1;

		static _Closure_0024__()
		{
			_0024I = new _Closure_0024__();
		}

		[SpecialName]
		internal bool _Lambda_0024__1_002D1(Tube t)
		{
			return t.A.Kind == PortKind.Pneumatic;
		}

		[SpecialName]
		internal bool _Lambda_0024__1_002D2(Tube t)
		{
			return t.A.Kind == PortKind.Hydraulic;
		}

		[SpecialName]
		internal string _Lambda_0024__1_002D4(PartLine l)
		{
			return l.Description;
		}

		[SpecialName]
		internal string _Lambda_0024__2_002D0(string s)
		{
			return "\"" + s.Replace("\"", "\"\"") + "\"";
		}

		[SpecialName]
		internal double _Lambda_0024__2_002D1(PartLine l)
		{
			return l.Total;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__1_002D0
	{
		public Dictionary<string, PartLine> _0024VB_0024Local_lines;

		public Project _0024VB_0024Local_project;

		public _Closure_0024__1_002D0(_Closure_0024__1_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_lines = arg0._0024VB_0024Local_lines;
				_0024VB_0024Local_project = arg0._0024VB_0024Local_project;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(string desc, string label)
		{
			PartLine value = null;
			if (!_0024VB_0024Local_lines.TryGetValue(desc, out value))
			{
				if (!_0024VB_0024Local_project.PartCosts.TryGetValue(desc, out var value2))
				{
					value2 = DefaultPrice(desc);
				}
				value = new PartLine
				{
					Description = desc,
					Labels = "",
					UnitPrice = value2
				};
				_0024VB_0024Local_lines[desc] = value;
			}
			checked
			{
				value.Quantity++;
				if (!string.IsNullOrWhiteSpace(label))
				{
					value.Labels = ((Operators.CompareString(value.Labels, "", TextCompare: false) == 0) ? label : (value.Labels + ", " + label));
				}
			}
		}

		[SpecialName]
		internal void _Lambda_0024__3(string desc, int qty)
		{
			if (qty > 0)
			{
				if (!_0024VB_0024Local_project.PartCosts.TryGetValue(desc, out var value))
				{
					value = DefaultPrice(desc);
				}
				_0024VB_0024Local_lines[desc] = new PartLine
				{
					Description = desc,
					Labels = "",
					Quantity = qty,
					UnitPrice = value
				};
			}
		}
	}

	public static double DefaultPrice(string description)
	{
		string text = description.ToLowerInvariant();
		if (text.Contains("compressor"))
		{
			return 35000.0;
		}
		if (text.Contains("air receiver"))
		{
			return 6000.0;
		}
		if (text.Contains("shut-off"))
		{
			return text.Contains("hydraulic") ? 1500 : 350;
		}
		if (text.Contains("flow meter"))
		{
			return 4500.0;
		}
		if (text.Contains("force sensor"))
		{
			return 8000.0;
		}
		if (text.Contains("vacuum switch"))
		{
			return 2000.0;
		}
		if (text.Contains("pressure switch"))
		{
			return 1800.0;
		}
		if (text.Contains("vacuum generator"))
		{
			return 2500.0;
		}
		if (text.Contains("suction cup"))
		{
			return 300.0;
		}
		if (text.Contains("gripper"))
		{
			return 9000.0;
		}
		if (text.Contains("preset counter"))
		{
			return 1800.0;
		}
		if (text.Contains("emergency stop"))
		{
			return 900.0;
		}
		if (text.Contains("hydraulic power unit"))
		{
			return 45000.0;
		}
		if (text.Contains("hydraulic single-acting"))
		{
			return 9000.0;
		}
		if (text.Contains("hydraulic check"))
		{
			return 1800.0;
		}
		if (text.Contains("hydraulic one-way") || text.Contains("hydraulic throttle"))
		{
			return 2500.0;
		}
		if (text.Contains("pressure reducing"))
		{
			return 4500.0;
		}
		if (text.Contains("hydraulic hose"))
		{
			return 900.0;
		}
		if (text.Contains("hydraulic cylinder"))
		{
			return 12000.0;
		}
		if (text.Contains("hydraulic motor"))
		{
			return 15000.0;
		}
		if (text.Contains("4/3-way hydraulic"))
		{
			return 9000.0;
		}
		if (text.Contains("4/2-way hydraulic"))
		{
			return 7000.0;
		}
		if (text.Contains("relief"))
		{
			return 3500.0;
		}
		if (text.Contains("accumulator"))
		{
			return 12000.0;
		}
		if (text.Contains("pressure-compensated"))
		{
			return 4000.0;
		}
		if (text.Contains("counterbalance"))
		{
			return 5000.0;
		}
		if (text.Contains("service unit"))
		{
			return 2500.0;
		}
		if (text.Contains("pressure regulator"))
		{
			return 1200.0;
		}
		if (text.Contains("gauge"))
		{
			return 400.0;
		}
		if (text.Contains("silencer"))
		{
			return 80.0;
		}
		if (text.Contains("t-fitting"))
		{
			return 40.0;
		}
		if (text.Contains("single-acting cylinder"))
		{
			return 2200.0;
		}
		if (text.Contains("double-acting cylinder"))
		{
			return 3000.0;
		}
		if (text.Contains("semi-rotary"))
		{
			return 6500.0;
		}
		if (text.Contains("air motor"))
		{
			return 9000.0;
		}
		int num = (text.Contains("solenoid") ? 900 : 0);
		checked
		{
			if (text.Contains("5/3-way"))
			{
				return 4200 + num;
			}
			if (text.Contains("5/2-way"))
			{
				return 2200 + num;
			}
			if (text.Contains("4/2-way"))
			{
				return 2000 + num;
			}
			if (text.Contains("3/2-way"))
			{
				return (text.Contains("roller") ? 1300 : (text.Contains("time delay") ? 2500 : 1100)) + num;
			}
			if (text.Contains("2/2-way"))
			{
				return 900 + num;
			}
			if (text.Contains("shuttle"))
			{
				return 600.0;
			}
			if (text.Contains("two-pressure"))
			{
				return 700.0;
			}
			if (text.Contains("quick exhaust"))
			{
				return 500.0;
			}
			if (text.Contains("check valve"))
			{
				return 300.0;
			}
			if (text.Contains("flow control"))
			{
				return 450.0;
			}
			if (text.Contains("timer relay"))
			{
				return 1500.0;
			}
			if (text.Contains("relay"))
			{
				return 450.0;
			}
			if (text.Contains("proximity sensor"))
			{
				return 1200.0;
			}
			if (text.Contains("limit switch"))
			{
				return 900.0;
			}
			if (text.Contains("push button") || text.Contains("selector switch"))
			{
				return 350.0;
			}
			if (text.Contains("indicator lamp"))
			{
				return 250.0;
			}
			if (text.Contains("tube fittings"))
			{
				return 25.0;
			}
			if (text.Contains("plastic tubing"))
			{
				return 40.0;
			}
			return 0.0;
		}
	}

	public static List<PartLine> Build(Project project)
	{
		_Closure_0024__1_002D0 arg = default(_Closure_0024__1_002D0);
		_Closure_0024__1_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__1_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_project = project;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_lines = new Dictionary<string, PartLine>();
		checked
		{
			VB_0024AnonymousDelegate_20<string, string> vB_0024AnonymousDelegate_ = [SpecialName] (string desc, string label) =>
			{
				PartLine value2 = null;
				if (!CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.TryGetValue(desc, out value2))
				{
					if (!CS_0024_003C_003E8__locals0._0024VB_0024Local_project.PartCosts.TryGetValue(desc, out var value3))
					{
						value3 = DefaultPrice(desc);
					}
					value2 = new PartLine
					{
						Description = desc,
						Labels = "",
						UnitPrice = value3
					};
					CS_0024_003C_003E8__locals0._0024VB_0024Local_lines[desc] = value2;
				}
				value2.Quantity++;
				if (!string.IsNullOrWhiteSpace(label))
				{
					value2.Labels = ((Operators.CompareString(value2.Labels, "", TextCompare: false) == 0) ? label : (value2.Labels + ", " + label));
				}
			};
			foreach (CircuitElement item in CS_0024_003C_003E8__locals0._0024VB_0024Local_project.AllElements())
			{
				bool flag = true;
				if (flag == item is TextNote || flag == item is PageConnector || flag == item is AirSupply || flag == item is PowerTerminal || flag == item is HydraulicTank)
				{
					continue;
				}
				if (flag == item is Junction)
				{
					if (item.Ports[0].ConnectionCount >= 3 && item.Ports[0].Kind != PortKind.Electric)
					{
						vB_0024AnonymousDelegate_("T-fitting", "");
					}
				}
				else if (flag != (item is ElectricContact && ((ElectricContact)item).Operator == ContactOperator.Relay) && flag != (item is ElectricContact && ((ElectricContact)item).Operator == ContactOperator.PressureSwitch) && flag != (item is ElectricCoil && ((ElectricCoil)item).Kind == CoilKind.Solenoid))
				{
					vB_0024AnonymousDelegate_(item.DisplayName, item.Label);
				}
			}
			int num = 0;
			int num2 = 0;
			foreach (ProjectPage page in CS_0024_003C_003E8__locals0._0024VB_0024Local_project.Pages)
			{
				num += page.Circuit.Tubes.Where((_Closure_0024__._0024I1_002D1 == null) ? (_Closure_0024__._0024I1_002D1 = [SpecialName] (Tube t) => t.A.Kind == PortKind.Pneumatic) : _Closure_0024__._0024I1_002D1).Count();
				num2 += page.Circuit.Tubes.Where((_Closure_0024__._0024I1_002D2 == null) ? (_Closure_0024__._0024I1_002D2 = [SpecialName] (Tube t) => t.A.Kind == PortKind.Hydraulic) : _Closure_0024__._0024I1_002D2).Count();
			}
			VB_0024AnonymousDelegate_21<string, int> obj = [SpecialName] (string desc, int qty) =>
			{
				if (qty > 0)
				{
					if (!CS_0024_003C_003E8__locals0._0024VB_0024Local_project.PartCosts.TryGetValue(desc, out var value))
					{
						value = DefaultPrice(desc);
					}
					CS_0024_003C_003E8__locals0._0024VB_0024Local_lines[desc] = new PartLine
					{
						Description = desc,
						Labels = "",
						Quantity = qty,
						UnitPrice = value
					};
				}
			};
			obj("Tube fittings (push-in)", num * 2);
			obj("Plastic tubing, metres (estimate: 1 m per connection)", num);
			obj("Hydraulic hose with fittings", num2);
			return CS_0024_003C_003E8__locals0._0024VB_0024Local_lines.Values.OrderBy([SpecialName] (PartLine l) => l.Description).ToList();
		}
	}

	public static string ToCsv(List<PartLine> lines)
	{
		StringBuilder stringBuilder = new StringBuilder();
		CultureInfo invariantCulture = CultureInfo.InvariantCulture;
		VB_0024AnonymousDelegate_19<string, string> vB_0024AnonymousDelegate_ = [SpecialName] (string s) => "\"" + s.Replace("\"", "\"\"") + "\"";
		stringBuilder.AppendLine("Qty,Description,Labels,Unit price (INR),Total (INR)");
		foreach (PartLine line in lines)
		{
			stringBuilder.AppendLine(string.Format("{0},{1},{2},{3},{4}", line.Quantity, vB_0024AnonymousDelegate_(line.Description), vB_0024AnonymousDelegate_(line.Labels), line.UnitPrice.ToString("0.00", invariantCulture), line.Total.ToString("0.00", invariantCulture)));
		}
		stringBuilder.AppendLine(string.Format(",{0},,,{1}", vB_0024AnonymousDelegate_("Total"), lines.Sum([SpecialName] (PartLine l) => l.Total).ToString("0.00", invariantCulture)));
		return stringBuilder.ToString();
	}
}
