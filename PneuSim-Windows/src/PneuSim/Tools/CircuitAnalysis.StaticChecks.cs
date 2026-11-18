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

	/// <summary>Finds components lying on top of each other, lines running through components, and lines lying on top of each other.</summary>
	public static List<CheckIssue> OverlapChecks(Circuit c)
	{
		List<CheckIssue> res = new List<CheckIssue>();
		List<CircuitElement> els = c.Elements.Where((CircuitElement e) => !(e is TextNote)).ToList();
		for (int i = 0; i < els.Count; i++)
		{
			for (int j = i + 1; j < els.Count; j++)
			{
				RectangleF a = els[i].WorldBounds();
				RectangleF b = els[j].WorldBounds();
				a.Inflate(-3f, -3f);
				b.Inflate(-3f, -3f);
				if (a.Width > 0f && b.Width > 0f && a.IntersectsWith(b))
				{
					res.Add(new CheckIssue { Severity = IssueSeverity.Warning, Element = els[i], Message = "Components overlap in the Workspace: " + els[i].Label + " " + els[i].DisplayName + " and " + els[j].Label + " " + els[j].DisplayName + ". Move one of them." });
				}
			}
		}
		List<(Tube T, PointF[] R)> routes = c.Tubes.Select((Tube t) => (t, t.Route())).ToList();
		foreach (var item in routes)
		{
			bool hit = false;
			for (int k = 1; k < item.R.Length - 2 && !hit; k++)
			{
				RectangleF seg = RectangleF.FromLTRB(Math.Min(item.R[k].X, item.R[k + 1].X) - 0.5f, Math.Min(item.R[k].Y, item.R[k + 1].Y) - 0.5f, Math.Max(item.R[k].X, item.R[k + 1].X) + 0.5f, Math.Max(item.R[k].Y, item.R[k + 1].Y) + 0.5f);
				foreach (CircuitElement e in els)
				{
					if (e is Junction || e == item.T.A.Owner || e == item.T.B.Owner)
					{
						continue;
					}
					RectangleF bb = e.WorldBounds();
					bb.Inflate(-2f, -2f);
					if (bb.Width > 0f && bb.IntersectsWith(seg))
					{
						res.Add(new CheckIssue { Severity = IssueSeverity.Warning, Element = e, Message = "A line runs through " + e.Label + " " + e.DisplayName + ". Drag the line sideways or move the component (Rotate re-routes lines automatically)." });
						hit = true;
						break;
					}
				}
			}
		}
		for (int p = 0; p < routes.Count; p++)
		{
			for (int q = p + 1; q < routes.Count; q++)
			{
				if (routes[p].T.A.Owner == routes[q].T.A.Owner || routes[p].T.A.Owner == routes[q].T.B.Owner || routes[p].T.B.Owner == routes[q].T.A.Owner || routes[p].T.B.Owner == routes[q].T.B.Owner)
				{
					continue;
				}
				if (SegmentsOverlap(routes[p].R, routes[q].R))
				{
					res.Add(new CheckIssue { Severity = IssueSeverity.Warning, Element = routes[p].T.A.Owner, Message = "Two lines lie on top of each other near " + routes[p].T.A.Owner.Label + ". Drag one of them sideways so they can be told apart." });
				}
			}
		}
		return res;
	}

	private static bool SegmentsOverlap(PointF[] r1, PointF[] r2)
	{
		for (int i = 0; i < r1.Length - 1; i++)
		{
			for (int j = 0; j < r2.Length - 1; j++)
			{
				PointF a = r1[i], b = r1[i + 1], c = r2[j], d = r2[j + 1];
				if (Math.Abs(a.X - b.X) < 0.01f && Math.Abs(c.X - d.X) < 0.01f && Math.Abs(a.X - c.X) < 1f)
				{
					if (Math.Min(Math.Max(a.Y, b.Y), Math.Max(c.Y, d.Y)) - Math.Max(Math.Min(a.Y, b.Y), Math.Min(c.Y, d.Y)) > 4f)
					{
						return true;
					}
				}
				else if (Math.Abs(a.Y - b.Y) < 0.01f && Math.Abs(c.Y - d.Y) < 0.01f && Math.Abs(a.Y - c.Y) < 1f && Math.Min(Math.Max(a.X, b.X), Math.Max(c.X, d.X)) - Math.Max(Math.Min(a.X, b.X), Math.Min(c.X, d.X)) > 4f)
				{
					return true;
				}
			}
		}
		return false;
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
		foreach (CheckIssue overlap in OverlapChecks(c))
		{
			CS_0024_003C_003E8__locals0._0024VB_0024Local_issues.Add(overlap);
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
							vB_0024AnonymousDelegate_(IssueSeverity.Info, $"{Name(list2[l])} and {Name(list2[m])} overlap in the workspace.", list2[m]);
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
}
