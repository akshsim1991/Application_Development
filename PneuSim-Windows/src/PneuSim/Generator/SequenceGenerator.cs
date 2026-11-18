using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class SequenceGenerator
{
	internal class PowerUnit
	{
		public DoubleActingCylinder Cylinder;

		public Valve52 Valve;
	}

	internal class LadderItem
	{
		public ContactOperator Op;

		public bool NormallyClosed;

		public string Label;

		public string Reference;

		public LadderItem()
		{
			Label = "";
			Reference = "";
		}

		public ElectricContact Build()
		{
			return Library.Contact(Op, NormallyClosed, Label, Reference);
		}
	}

	internal class Rung
	{
		public List<List<List<LadderItem>>> Sections;

		public CoilKind CoilKind;

		public string CoilLabel;

		public int Rows => checked(Sections.Sum([SpecialName] (List<List<LadderItem>> s) => s.Max([SpecialName] (List<LadderItem> b) => b.Count)) + 1);

		public int MaxBranches => Sections.Max([SpecialName] (List<List<LadderItem>> s) => s.Count);

		public Rung()
		{
			Sections = new List<List<List<LadderItem>>>();
		}
	}

	[Serializable]
	[CompilerGenerated]
	internal sealed class _Closure_0024__
	{
		public static readonly _Closure_0024__ _0024I;

		public static Func<LadderItem, LadderItem> _0024I13_002D0;

		public static Func<int, List<LadderItem>> _0024I13_002D1;

		public static Func<LadderItem, string> _0024I13_002D2;

		public static Func<LadderItem, string> _0024I13_002D3;

		public static Func<int, string> _0024I13_002D4;

		public static Func<Rung, int> _0024I14_002D0;

		public static Func<List<LadderItem>, int> _0024I14_002D1;

		public static Func<Valve52, Port> _0024I15_002D4;

		public static Func<Port, VB_0024AnonymousType_0<Port, float>> _0024I15_002D9;

		public static Func<VB_0024AnonymousType_0<Port, float>, float> _0024I15_002D10;

		static _Closure_0024__()
		{
			_0024I = new _Closure_0024__();
		}

		[SpecialName]
		internal LadderItem _Lambda_0024__13_002D0(LadderItem it)
		{
			return new LadderItem
			{
				Op = it.Op,
				NormallyClosed = it.NormallyClosed,
				Reference = it.Reference
			};
		}

		[SpecialName]
		internal List<LadderItem> _Lambda_0024__13_002D1(int k)
		{
			return new List<LadderItem> { RelayNO(k) };
		}

		[SpecialName]
		internal string _Lambda_0024__13_002D2(LadderItem s)
		{
			return s.Reference;
		}

		[SpecialName]
		internal string _Lambda_0024__13_002D3(LadderItem s)
		{
			return s.Reference;
		}

		[SpecialName]
		internal string _Lambda_0024__13_002D4(int k)
		{
			return $"K{k}";
		}

		[SpecialName]
		internal int _Lambda_0024__14_002D0(Rung r)
		{
			return r.Rows;
		}

		[SpecialName]
		internal int _Lambda_0024__14_002D1(List<LadderItem> b)
		{
			return b.Count;
		}

		[SpecialName]
		internal Port _Lambda_0024__15_002D4(Valve52 v)
		{
			return v.GetPort("14");
		}

		[SpecialName]
		internal VB_0024AnonymousType_0<Port, float> _Lambda_0024__15_002D9(Port p)
		{
			return new VB_0024AnonymousType_0<Port, float>(p, TapX(p));
		}

		[SpecialName]
		internal float _Lambda_0024__15_002D10(VB_0024AnonymousType_0<Port, float> t)
		{
			return t.X;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__15_002D0
	{
		public char _0024VB_0024Local_letter;

		public _Closure_0024__15_002D0(_Closure_0024__15_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_letter = arg0._0024VB_0024Local_letter;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(SequenceMove m)
		{
			return m.Letter == _0024VB_0024Local_letter;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__15_002D1
	{
		public List<SequenceMove> _0024VB_0024Local_moves;

		public Dictionary<char, PowerUnit> _0024VB_0024Local_units;

		public Func<int, SequenceMove> _0024I7;

		public _Closure_0024__15_002D1(_Closure_0024__15_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_moves = arg0._0024VB_0024Local_moves;
				_0024VB_0024Local_units = arg0._0024VB_0024Local_units;
			}
		}

		[SpecialName]
		internal Port _Lambda_0024__5(SequenceMove mv)
		{
			return _0024VB_0024Local_units[mv.Letter].Valve.GetPort(mv.Extend ? "14" : "12");
		}

		[SpecialName]
		internal string _Lambda_0024__6(List<int> gr, int gi)
		{
			return $"{RomanNumeral(checked(gi + 1))}: " + string.Join(" ", gr.Select([SpecialName] (int k) => _0024VB_0024Local_moves[k]));
		}

		[SpecialName]
		internal SequenceMove _Lambda_0024__7(int k)
		{
			return _0024VB_0024Local_moves[k];
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__15_002D2
	{
		public char _0024VB_0024Local_letter;

		public _Closure_0024__15_002D1 _0024VB_0024NonLocal__0024VB_0024Closure_2;

		public _Closure_0024__15_002D2(_Closure_0024__15_002D2 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_letter = arg0._0024VB_0024Local_letter;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__1(int k)
		{
			return _0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_moves[k].Letter == _0024VB_0024Local_letter;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__15_002D3
	{
		public List<int> _0024VB_0024Local_lastG;

		public _Closure_0024__15_002D1 _0024VB_0024NonLocal__0024VB_0024Closure_3;

		public _Closure_0024__15_002D3(_Closure_0024__15_002D3 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_lastG = arg0._0024VB_0024Local_lastG;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__2(int a)
		{
			_Closure_0024__15_002D4 arg = default(_Closure_0024__15_002D4);
			_Closure_0024__15_002D4 CS_0024_003C_003E8__locals0 = new _Closure_0024__15_002D4(arg)
			{
				_0024VB_0024NonLocal__0024VB_0024Closure_4 = this,
				_0024VB_0024Local_a = a
			};
			return _0024VB_0024Local_lastG.Any([SpecialName] (int b) => CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_4._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_moves[CS_0024_003C_003E8__locals0._0024VB_0024Local_a].Letter == CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_4._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_moves[b].Letter);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__15_002D4
	{
		public int _0024VB_0024Local_a;

		public _Closure_0024__15_002D3 _0024VB_0024NonLocal__0024VB_0024Closure_4;

		public _Closure_0024__15_002D4(_Closure_0024__15_002D4 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_a = arg0._0024VB_0024Local_a;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__3(int b)
		{
			return _0024VB_0024NonLocal__0024VB_0024Closure_4._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_moves[_0024VB_0024Local_a].Letter == _0024VB_0024NonLocal__0024VB_0024Closure_4._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_moves[b].Letter;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__15_002D5
	{
		public float _0024VB_0024Local_placeX;

		public _Closure_0024__15_002D5(_Closure_0024__15_002D5 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_placeX = arg0._0024VB_0024Local_placeX;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__8(float cxp)
		{
			if (cxp > _0024VB_0024Local_placeX - 12f)
			{
				return cxp < _0024VB_0024Local_placeX + 132f;
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__17_002D0
	{
		public Port _0024VB_0024Local_source;

		public Func<Port, float> _0024I0;

		public _Closure_0024__17_002D0(_Closure_0024__17_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_source = arg0._0024VB_0024Local_source;
			}
		}

		[SpecialName]
		internal float _Lambda_0024__0(Port p)
		{
			return Math.Abs(TapX(p) - _0024VB_0024Local_source.WorldPos().X);
		}
	}

	private const float CylPitch = 320f;

	private const float LeftMargin = 120f;

	private const float CylY = 70f;

	public static GeneratedCircuit Generate(MotionSequence seq, GeneratorMethod method, bool continuous)
	{
		if (method == GeneratorMethod.PneumaticCascade)
		{
			return GenerateCascade(seq, continuous);
		}
		return GenerateElectro(seq, continuous);
	}

	private static string MarkName(MotionSequence seq, SequenceMove mv, string prefix)
	{
		return $"{seq.Number(mv.Letter)}{prefix}{((!mv.Extend) ? 1 : 2)}";
	}

	private static Dictionary<char, PowerUnit> BuildPowerSection(Circuit c, MotionSequence seq, bool electric)
	{
		Dictionary<char, PowerUnit> dictionary = new Dictionary<char, PowerUnit>();
		string arg = (electric ? "B" : "S");
		checked
		{
			int num = seq.Letters.Count - 1;
			for (int i = 0; i <= num; i++)
			{
				int num2 = i + 1;
				float num3 = 120f + (float)i * 320f;
				DoubleActingCylinder doubleActingCylinder = c.Add(new DoubleActingCylinder
				{
					RetractedMark = $"{num2}{arg}1",
					ExtendedMark = $"{num2}{arg}2"
				}, num3, 70f, $"{num2}A");
				Valve52 valve;
				if (electric)
				{
					valve = Library.V52(ValveActuator.Solenoid, ValveReturn.Solenoid);
					valve.SolenoidLabel = $"{num2}M1";
					valve.ReturnSolenoidLabel = $"{num2}M2";
				}
				else
				{
					valve = Library.V52(ValveActuator.Pilot, ValveReturn.Pilot);
				}
				c.Add(valve, num3 - 90f, 200f, $"{num2}V1");
				AirSupply a = c.Add(new AirSupply(), num3, 300f);
				c.Connect(valve, "4", doubleActingCylinder, "1");
				c.Connect(valve, "2", doubleActingCylinder, "2");
				c.Connect(a, "1", valve, "1");
				dictionary[seq.Letters[i]] = new PowerUnit
				{
					Cylinder = doubleActingCylinder,
					Valve = valve
				};
			}
			return dictionary;
		}
	}

	private static void AddTitle(Circuit c, string text)
	{
		c.Add(new TextNote
		{
			Text = text,
			FontSize = 11f,
			Bold = true
		}, 20f, 10f);
	}

	private static LadderItem RelayNO(int k)
	{
		return new LadderItem
		{
			Op = ContactOperator.Relay,
			Reference = $"K{k}"
		};
	}

	private static LadderItem RelayNC(int k)
	{
		return new LadderItem
		{
			Op = ContactOperator.Relay,
			NormallyClosed = true,
			Reference = $"K{k}"
		};
	}

	private static List<LadderItem> Sensors(MotionSequence seq, int stepIndex)
	{
		return seq.Steps[stepIndex].Select([SpecialName] (SequenceMove mv) => new LadderItem
		{
			Op = ContactOperator.ProximitySensor,
			Reference = MarkName(seq, mv, "B")
		}).ToList();
	}

	private static GeneratedCircuit GenerateElectro(MotionSequence seq, bool continuous)
	{
		Circuit circuit = new Circuit();
		int count = seq.Steps.Count;
		AddTitle(circuit, $"Sequence {seq}   —   electro-pneumatic control with a relay step chain");
		BuildPowerSection(circuit, seq, electric: true);
		List<Rung> list = new List<Rung>();
		int num = count;
		checked
		{
			for (int i = 1; i <= num; i++)
			{
				Rung rung = new Rung
				{
					CoilKind = CoilKind.Relay,
					CoilLabel = $"K{i}"
				};
				List<List<LadderItem>> list2 = new List<List<LadderItem>>();
				if (i == 1)
				{
					List<LadderItem> list3 = Sensors(seq, count - 1);
					int num2 = count - 1;
					for (int j = 3; j <= num2; j++)
					{
						list3.Add(RelayNC(j));
					}
					List<LadderItem> list4 = new List<LadderItem>
					{
						new LadderItem
						{
							Op = ContactOperator.PushButton,
							Label = "S1"
						}
					};
					list4.AddRange(list3);
					list2.Add(list4);
					if (continuous)
					{
						List<LadderItem> list5 = new List<LadderItem>
						{
							new LadderItem
							{
								Op = ContactOperator.Selector,
								Label = "S2"
							}
						};
						list5.AddRange(list3.Select([SpecialName] (LadderItem it) => new LadderItem
						{
							Op = it.Op,
							NormallyClosed = it.NormallyClosed,
							Reference = it.Reference
						}));
						list2.Add(list5);
					}
				}
				else
				{
					List<LadderItem> list6 = new List<LadderItem> { RelayNO(i - 1) };
					list6.AddRange(Sensors(seq, i - 2));
					list2.Add(list6);
				}
				list2.Add(new List<LadderItem> { RelayNO(i) });
				rung.Sections.Add(list2);
				rung.Sections.Add(new List<List<LadderItem>>
				{
					new List<LadderItem> { RelayNC((i == count) ? 1 : (i + 1)) }
				});
				list.Add(rung);
			}
			SortedDictionary<string, List<int>> sortedDictionary = new SortedDictionary<string, List<int>>();
			int num3 = count - 1;
			for (int l = 0; l <= num3; l++)
			{
				foreach (SequenceMove item in seq.Steps[l])
				{
					string key = $"{seq.Number(item.Letter)}M{(item.Extend ? 1 : 2)}";
					if (!sortedDictionary.ContainsKey(key))
					{
						sortedDictionary[key] = new List<int>();
					}
					sortedDictionary[key].Add(l + 1);
				}
			}
			foreach (KeyValuePair<string, List<int>> item2 in sortedDictionary)
			{
				Rung rung2 = new Rung
				{
					CoilKind = CoilKind.Solenoid,
					CoilLabel = item2.Key
				};
				rung2.Sections.Add(item2.Value.Select((_Closure_0024__._0024I13_002D1 == null) ? (_Closure_0024__._0024I13_002D1 = [SpecialName] (int k) => new List<LadderItem> { RelayNO(k) }) : _Closure_0024__._0024I13_002D1).ToList());
				list.Add(rung2);
			}
			LayoutLadder(circuit, list, 400f);
			StringBuilder stringBuilder = new StringBuilder();
			stringBuilder.AppendLine($"Sequence: {seq}   ({count} steps, {seq.Letters.Count} cylinder(s))");
			stringBuilder.AppendLine();
			stringBuilder.AppendLine("Pneumatic part");
			foreach (char letter in seq.Letters)
			{
				int num4 = seq.Number(letter);
				stringBuilder.AppendLine($"  Cylinder {letter} = {num4}A, valve {num4}V1 (5/2 double solenoid): {num4}M1 extends, {num4}M2 retracts.");
				stringBuilder.AppendLine($"  Sensors: {num4}B1 = {letter} retracted, {num4}B2 = {letter} extended.");
			}
			stringBuilder.AppendLine();
			stringBuilder.AppendLine("Electrical part: relay step chain (one relay per step)");
			int num5 = count;
			for (int m = 1; m <= num5; m++)
			{
				string text = ((m != 1) ? string.Format("K{0} AND {1} (step {2} done)", m - 1, string.Join(" AND ", from s in Sensors(seq, m - 2)
					select s.Reference), m - 1) : string.Format("start S1{0} AND {1} (initial position)", continuous ? " (or continuous switch S2)" : "", string.Join(" AND ", from s in Sensors(seq, count - 1)
					select s.Reference)));
				stringBuilder.AppendLine(string.Format("  K{0} on when {1}; holds itself; off when K{2} comes on.  Step {3}: {4}", m, text, (m == count) ? 1 : (m + 1), m, string.Join(" + ", seq.Steps[m - 1])));
			}
			foreach (KeyValuePair<string, List<int>> item3 in sortedDictionary)
			{
				stringBuilder.AppendLine(string.Format("  Solenoid {0} = {1}", item3.Key, string.Join(" OR ", item3.Value.Select((_Closure_0024__._0024I13_002D4 == null) ? (_Closure_0024__._0024I13_002D4 = [SpecialName] (int k) => $"K{k}") : _Closure_0024__._0024I13_002D4))));
			}
			stringBuilder.AppendLine();
			stringBuilder.AppendLine("Only one step relay is on at a time, so a valve never gets opposing signals (no signal overlap).");
			stringBuilder.AppendLine("Press Start (F9), then press S1" + (continuous ? " or switch on S2 for continuous running." : "."));
			return new GeneratedCircuit
			{
				Circuit = circuit,
				Explanation = stringBuilder.ToString()
			};
		}
	}

	private static void LayoutLadder(Circuit c, List<Rung> rungs, float top)
	{
		int num = rungs.Max([SpecialName] (Rung r) => r.Rows);
		float num2 = top + 60f;
		checked
		{
			float num3 = num2 + (float)(num - 1) * 80f;
			float num4 = 20f;
			c.Add(new TextNote
			{
				Text = "Electrical control circuit (24 V DC)",
				FontSize = 9f,
				Bold = true
			}, num4, top - 10f);
			foreach (Rung rung in rungs)
			{
				Port a = c.Add(new PowerTerminal(), num4, num2 - 50f).GetPort("1");
				int num5 = 0;
				int num6 = rung.Sections.Count - 1;
				for (int i = 0; i <= num6; i++)
				{
					List<List<LadderItem>> list = rung.Sections[i];
					int num7 = list.Max([SpecialName] (List<LadderItem> b) => b.Count);
					List<ElectricContact> list2 = new List<ElectricContact>();
					List<ElectricContact> list3 = new List<ElectricContact>();
					int num8 = list.Count - 1;
					for (int j = 0; j <= num8; j++)
					{
						ElectricContact electricContact = null;
						int num9 = list[j].Count - 1;
						for (int k = 0; k <= num9; k++)
						{
							ElectricContact electricContact2 = c.Add(list[j][k].Build(), num4 + (float)j * 80f, num2 + (float)(num5 + k) * 80f);
							if (electricContact == null)
							{
								list2.Add(electricContact2);
							}
							else
							{
								c.Connect(electricContact, "2", electricContact2, "1");
							}
							electricContact = electricContact2;
						}
						list3.Add(electricContact);
					}
					bool num10 = i == rung.Sections.Count - 1;
					bool flag = !num10 && rung.Sections[i + 1].Count == 1;
					ElectricCoil electricCoil = null;
					Port port;
					if (!num10)
					{
						port = ((list.Count != 1 || !flag) ? c.Add(new Junction
						{
							IsElectric = true
						}, num4 + 20f, num2 + (float)(num5 + num7) * 80f - 10f).GetPort("1") : null);
					}
					else
					{
						electricCoil = c.Add(new ElectricCoil
						{
							Kind = rung.CoilKind,
							Label = rung.CoilLabel
						}, num4, num3);
						port = electricCoil.GetPort("A1");
					}
					foreach (ElectricContact item in list2)
					{
						c.Connect(a, item.GetPort("1"));
					}
					if (port != null)
					{
						float value = port.WorldPos().Y - (float)((!(port.Owner is Junction)) ? 10 : 0);
						foreach (ElectricContact item2 in list3)
						{
							Tube tube = c.Connect(item2.GetPort("2"), port);
							if (tube != null && Operators.CompareString(tube.MidAxis(), "Y", TextCompare: false) == 0)
							{
								tube.Mid = value;
							}
						}
						a = port;
					}
					else
					{
						a = list3[0].GetPort("2");
					}
					num5 += num7;
					if (electricCoil != null)
					{
						PowerTerminal b2 = c.Add(new PowerTerminal
						{
							Polarity = Polarity.Zero0V
						}, num4, num3 + 80f);
						c.Connect(electricCoil, "A2", b2, "1");
					}
				}
				num4 += (float)rung.MaxBranches * 80f + 30f;
			}
		}
	}

	private static GeneratedCircuit GenerateCascade(MotionSequence seq, bool continuous)
	{
		_Closure_0024__15_002D1 arg = default(_Closure_0024__15_002D1);
		_Closure_0024__15_002D1 CS_0024_003C_003E8__locals0 = new _Closure_0024__15_002D1(arg);
		if (seq.HasParallelSteps)
		{
			throw new FormatException("The cascade method needs one movement per step. Remove the brackets, or use the electro-pneumatic method.");
		}
		CS_0024_003C_003E8__locals0._0024VB_0024Local_moves = seq.AllMoves().ToList();
		using (List<char>.Enumerator enumerator = seq.Letters.GetEnumerator())
		{
			_Closure_0024__15_002D0 closure_0024__15_002D = default(_Closure_0024__15_002D0);
			while (enumerator.MoveNext())
			{
				closure_0024__15_002D = new _Closure_0024__15_002D0(closure_0024__15_002D);
				closure_0024__15_002D._0024VB_0024Local_letter = enumerator.Current;
				if (CS_0024_003C_003E8__locals0._0024VB_0024Local_moves.Where(closure_0024__15_002D._Lambda_0024__0).Count() != 2)
				{
					throw new FormatException($"The cascade method supports each cylinder extending and retracting once per cycle; {closure_0024__15_002D._0024VB_0024Local_letter} moves more often. Use the electro-pneumatic method.");
				}
			}
		}
		List<List<int>> list = new List<List<int>>();
		List<int> list2 = new List<int>();
		checked
		{
			int num = CS_0024_003C_003E8__locals0._0024VB_0024Local_moves.Count - 1;
			_Closure_0024__15_002D2 closure_0024__15_002D2 = default(_Closure_0024__15_002D2);
			for (int i = 0; i <= num; i++)
			{
				closure_0024__15_002D2 = new _Closure_0024__15_002D2(closure_0024__15_002D2);
				closure_0024__15_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2 = CS_0024_003C_003E8__locals0;
				closure_0024__15_002D2._0024VB_0024Local_letter = closure_0024__15_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_moves[i].Letter;
				if (list2.Any(closure_0024__15_002D2._Lambda_0024__1))
				{
					list.Add(list2);
					list2 = new List<int>();
				}
				list2.Add(i);
			}
			list.Add(list2);
			bool flag = false;
			if (list.Count > 2)
			{
				_Closure_0024__15_002D3 arg2 = default(_Closure_0024__15_002D3);
				_Closure_0024__15_002D3 CS_0024_003C_003E8__locals1 = new _Closure_0024__15_002D3(arg2);
				CS_0024_003C_003E8__locals1._0024VB_0024NonLocal__0024VB_0024Closure_3 = CS_0024_003C_003E8__locals0;
				List<int> list3 = list[0];
				CS_0024_003C_003E8__locals1._0024VB_0024Local_lastG = list[list.Count - 1];
				if (!list3.Any([SpecialName] (int a) =>
				{
					_Closure_0024__15_002D4 arg3 = default(_Closure_0024__15_002D4);
					_Closure_0024__15_002D4 CS_0024_003C_003E8__locals2 = new _Closure_0024__15_002D4(arg3);
					CS_0024_003C_003E8__locals2._0024VB_0024NonLocal__0024VB_0024Closure_4 = CS_0024_003C_003E8__locals1;
					CS_0024_003C_003E8__locals2._0024VB_0024Local_a = a;
					return CS_0024_003C_003E8__locals1._0024VB_0024Local_lastG.Any([SpecialName] (int b) => CS_0024_003C_003E8__locals2._0024VB_0024NonLocal__0024VB_0024Closure_4._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_moves[CS_0024_003C_003E8__locals2._0024VB_0024Local_a].Letter == CS_0024_003C_003E8__locals2._0024VB_0024NonLocal__0024VB_0024Closure_4._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_moves[b].Letter);
				}))
				{
					CS_0024_003C_003E8__locals1._0024VB_0024Local_lastG.AddRange(list3);
					list.RemoveAt(0);
					flag = true;
				}
			}
			int count = list.Count;
			int[] array = new int[CS_0024_003C_003E8__locals0._0024VB_0024Local_moves.Count - 1 + 1];
			int num2 = count - 1;
			for (int j = 0; j <= num2; j++)
			{
				foreach (int item in list[j])
				{
					array[item] = j + 1;
				}
			}
			Circuit circuit = new Circuit();
			AddTitle(circuit, $"Sequence {seq}   —   pneumatic cascade control, {count} groups");
			CS_0024_003C_003E8__locals0._0024VB_0024Local_units = BuildPowerSection(circuit, seq, electric: false);
			float num3 = 360f;
			float num4 = num3 + (float)(CS_0024_003C_003E8__locals0._0024VB_0024Local_moves.Count * 10) + 40f;
			float num5 = num4 + 120f;
			float num6 = 30f;
			float num7 = num5 + (float)count * num6 + 50f;
			Dictionary<int, List<Port>> dictionary = new Dictionary<int, List<Port>>();
			int num8 = count;
			for (int l = 1; l <= num8; l++)
			{
				dictionary[l] = new List<Port>();
			}
			List<Valve52> list4 = new List<Valve52>();
			float num9 = 30f;
			int num10 = count - 1;
			for (int m = 1; m <= num10; m++)
			{
				list4.Add(circuit.Add(Library.V52(ValveActuator.Pilot, ValveReturn.Pilot), num9 + (float)((m - 1) * 260), num7, $"0V{m}"));
			}
			AirSupply a2 = circuit.Add(new AirSupply(), list4[0].X + 90f, num7 + 100f, "0Z");
			circuit.Connect(a2, "1", list4[0], "1");
			int num11 = count - 1;
			for (int n = 2; n <= num11; n++)
			{
				circuit.Connect(list4[n - 2], "4", list4[n - 1], "1");
			}
			int num12 = count - 1;
			for (int num13 = 1; num13 <= num12; num13++)
			{
				dictionary[count - num13 + 1].Add(list4[num13 - 1].GetPort("2"));
			}
			dictionary[1].Add(list4[count - 2].GetPort("4"));
			Dictionary<int, List<Port>> dictionary2 = new Dictionary<int, List<Port>>();
			dictionary2[1] = list4.Select([SpecialName] (Valve52 v) => v.GetPort("14")).ToList();
			int num14 = count;
			for (int num15 = 2; num15 <= num14; num15++)
			{
				dictionary2[num15] = new List<Port> { list4[count - num15].GetPort("12") };
			}
			VB_0024AnonymousDelegate_9<SequenceMove, Port> vB_0024AnonymousDelegate_ = [SpecialName] (SequenceMove mv) => CS_0024_003C_003E8__locals0._0024VB_0024Local_units[mv.Letter].Valve.GetPort(mv.Extend ? "14" : "12");
			Valve32 valve = circuit.Add(Library.V32(continuous ? ValveActuator.Selector : ValveActuator.PushButton), 130f, num4 + 60f, "1S0");
			valve.Rotation = 2;
			float num16 = 160f;
			StringBuilder stringBuilder = new StringBuilder();
			stringBuilder.AppendLine($"Sequence: {seq}");
			stringBuilder.AppendLine(string.Format("Groups:   {0}", string.Join("  /  ", list.Select([SpecialName] (List<int> gr, int gi) => $"{RomanNumeral(gi + 1)}: " + string.Join(" ", gr.Select([SpecialName] (int k) => CS_0024_003C_003E8__locals0._0024VB_0024Local_moves[k]))))));
			if (flag)
			{
				stringBuilder.AppendLine("          (the last and first groups were combined, saving one cascade valve)");
			}
			stringBuilder.AppendLine($"Cascade valves: {count - 1} (0V1..0V{count - 1});  group line {RomanNumeral(count)} is active at rest.");
			stringBuilder.AppendLine();
			stringBuilder.AppendLine("Signals");
			List<float> list5 = new List<float>();
			int num17 = count;
			for (int num18 = 1; num18 <= num17; num18++)
			{
				list5.Add(TapX(vB_0024AnonymousDelegate_(CS_0024_003C_003E8__locals0._0024VB_0024Local_moves[list[num18 - 1][0]])));
			}
			foreach (Valve52 item2 in list4)
			{
				list5.Add(TapX(item2.GetPort("14")));
				list5.Add(TapX(item2.GetPort("12")));
			}
			float _0024VB_0024Local_placeX = num16;
			int num19 = CS_0024_003C_003E8__locals0._0024VB_0024Local_moves.Count - 1;
			_Closure_0024__15_002D5 closure_0024__15_002D3 = default(_Closure_0024__15_002D5);
			for (int num20 = 0; num20 <= num19; num20++)
			{
				closure_0024__15_002D3 = new _Closure_0024__15_002D5(closure_0024__15_002D3);
				SequenceMove sequenceMove = CS_0024_003C_003E8__locals0._0024VB_0024Local_moves[num20];
				int num21;
				string text;
				Valve32 valve2;
				unchecked
				{
					num21 = checked(num20 + 1) % CS_0024_003C_003E8__locals0._0024VB_0024Local_moves.Count;
					text = MarkName(seq, sequenceMove, "S");
					closure_0024__15_002D3._0024VB_0024Local_placeX = _0024VB_0024Local_placeX;
					while (list5.Any(closure_0024__15_002D3._Lambda_0024__8))
					{
						closure_0024__15_002D3._0024VB_0024Local_placeX += 10f;
					}
					_0024VB_0024Local_placeX = closure_0024__15_002D3._0024VB_0024Local_placeX + 170f;
					valve2 = circuit.Add(Library.V32(ValveActuator.RollerLever), closure_0024__15_002D3._0024VB_0024Local_placeX, num4, text);
					valve2.TriggerMark = text;
					dictionary[array[num20]].Add(valve2.GetPort("1"));
				}
				float laneY = num3 + (float)(num20 * 10);
				Port port = valve2.GetPort("2");
				bool flag2 = num21 == 0;
				List<Port> targets;
				string text2;
				if (array[num21] != array[num20])
				{
					targets = dictionary2[array[num21]];
					text2 = $"switches to group {RomanNumeral(array[num21])}";
				}
				else
				{
					targets = new List<Port> { vB_0024AnonymousDelegate_(CS_0024_003C_003E8__locals0._0024VB_0024Local_moves[num21]) };
					text2 = $"starts {CS_0024_003C_003E8__locals0._0024VB_0024Local_moves[num21]}";
				}
				if (flag2)
				{
					ConnectViaLane(circuit, port, new Port[1] { valve.GetPort("1") }, laneY);
					ConnectViaLane(circuit, valve.GetPort("2"), targets, num4 + 80f);
				}
				else
				{
					ConnectViaLane(circuit, port, targets, laneY);
				}
				stringBuilder.AppendLine(string.Format("  {0} ({1} done), fed from line {2}{3}: {4}", text, sequenceMove, RomanNumeral(array[num20]), flag2 ? " via start valve 1S0" : "", text2));
			}
			int num22 = count;
			for (int num23 = 1; num23 <= num22; num23++)
			{
				int index = list[num23 - 1][0];
				dictionary[num23].Add(vB_0024AnonymousDelegate_(CS_0024_003C_003E8__locals0._0024VB_0024Local_moves[index]));
				stringBuilder.AppendLine($"  Line {RomanNumeral(num23)} directly starts {CS_0024_003C_003E8__locals0._0024VB_0024Local_moves[index]}");
			}
			int num24 = count;
			for (int num25 = 1; num25 <= num24; num25++)
			{
				float num26 = num5 + (float)(num25 - 1) * num6;
				List<VB_0024AnonymousType_0<Port, float>> list6 = (from p in dictionary[num25]
					select new VB_0024AnonymousType_0<Port, float>(p, TapX(p)) into t
					orderby t.X
					select t).ToList();
				Junction junction = null;
				foreach (VB_0024AnonymousType_0<Port, float> item3 in list6)
				{
					Junction junction2 = circuit.Add(new Junction(), item3.X, num26);
					circuit.Connect(junction2.GetPort("1"), item3.Port);
					if (junction != null)
					{
						circuit.Connect(junction.GetPort("1"), junction2.GetPort("1"));
					}
					junction = junction2;
				}
				if (list6.Count > 0)
				{
					circuit.Add(new TextNote
					{
						Text = RomanNumeral(num25),
						FontSize = 8f,
						Bold = true
					}, list6[0].X - 30f, num26 - 8f);
				}
			}
			stringBuilder.AppendLine();
			stringBuilder.AppendLine("Each roller valve only receives air from its own group line, and only one line is pressurized at a time,");
			stringBuilder.AppendLine("so no valve ever receives opposing pilot signals (signal overlap is eliminated).");
			stringBuilder.AppendLine(continuous ? "Switch the selector valve 1S0 on to run continuously." : "Press Start (F9), then press the start valve 1S0.");
			return new GeneratedCircuit
			{
				Circuit = circuit,
				Explanation = stringBuilder.ToString()
			};
		}
	}

	private static float TapX(Port p)
	{
		PointF pointF = p.WorldDir();
		return p.WorldPos().X + ((Math.Abs(pointF.X) > 0.5f) ? (pointF.X * 20f) : 0f);
	}

	private static void ConnectViaLane(Circuit c, Port source, IEnumerable<Port> targets, float laneY)
	{
		_Closure_0024__17_002D0 arg = default(_Closure_0024__17_002D0);
		_Closure_0024__17_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__17_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_source = source;
		Port a = CS_0024_003C_003E8__locals0._0024VB_0024Local_source;
		foreach (Port item in targets.OrderBy([SpecialName] (Port p) => Math.Abs(TapX(p) - CS_0024_003C_003E8__locals0._0024VB_0024Local_source.WorldPos().X)))
		{
			Junction junction = c.Add(new Junction
			{
				IsElectric = (CS_0024_003C_003E8__locals0._0024VB_0024Local_source.Kind == PortKind.Electric)
			}, TapX(item), laneY);
			c.Connect(a, junction.GetPort("1"));
			c.Connect(junction.GetPort("1"), item);
			a = junction.GetPort("1");
		}
	}

	private static string RomanNumeral(int n)
	{
		string[] array = new string[16]
		{
			"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X",
			"XI", "XII", "XIII", "XIV", "XV", "XVI"
		};
		if (n < 1 || n > array.Length)
		{
			return n.ToString();
		}
		return array[checked(n - 1)];
	}
}
