using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Lessons
{
	private class Probe
	{
		public Circuit Circuit;

		public Simulator Sim;

		public List<string> Log;

		public Probe(Circuit original, bool realPhysics = false)
		{
			Log = new List<string>();
			Circuit = Circuit.FromXml(original.ToXml());
			Sim = new Simulator(Circuit)
			{
				RealPhysics = realPhysics
			};
			Sim.Reset();
		}

		public CircuitElement Find(string label)
		{
			return Circuit.Elements.FirstOrDefault([SpecialName] (CircuitElement e) => string.Equals(e.Label?.Trim(), label, StringComparison.OrdinalIgnoreCase) && (e.IsManuallyOperated || e is CylinderBase || e is ElectricCoil));
		}

		public CylinderBase Cylinder()
		{
			return (Find("1A") as CylinderBase) ?? Circuit.Elements.OfType<CylinderBase>().FirstOrDefault();
		}

		public void Run(double seconds)
		{
			checked
			{
				int num = (int)Math.Round(seconds / 0.005);
				for (int i = 1; i <= num; i++)
				{
					Sim.Step(0.005);
				}
			}
		}

		public void Press(CircuitElement e)
		{
			e.OnSimMouseDown(PointF.Empty);
			Sim.RunLogic();
		}

		public void Release(CircuitElement e)
		{
			e.OnSimMouseUp();
			Sim.RunLogic();
		}

		public void Tap(CircuitElement e)
		{
			Press(e);
			Run(0.2);
			Release(e);
		}
	}

	public static readonly Lesson[] All = checked(new Lesson[10]
	{
		new Lesson
		{
			Title = "1. Direct control",
			Goal = "Make the single-acting cylinder 1A extend while push button valve S1 is held down, and retract when it is released.",
			Hint = "Supply → port 1 of the 3/2 valve; port 2 → the cylinder. Port 3 is the exhaust.",
			Start = [SpecialName] () => Parts((new SingleActingCylinder(), "1A"), (Library.V32(ValveActuator.PushButton), "S1"), (new AirSupply(), "")),
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe11 = new Probe(c);
				string text12 = Need(probe11, "S1", "1A");
				if (text12 != null)
				{
					return Fail(text12);
				}
				probe11.Run(0.5);
				if (probe11.Cylinder().Position > 0.05)
				{
					return Fail("The cylinder moves before S1 is pressed. Use a normally closed (NC) valve.");
				}
				probe11.Press(probe11.Find("S1"));
				probe11.Run(2.0);
				if (probe11.Cylinder().Position < 0.99)
				{
					return Fail("When S1 is held, 1A does not extend fully. Check the supply and the tube from port 2 to the cylinder.");
				}
				probe11.Release(probe11.Find("S1"));
				probe11.Run(2.0);
				return (probe11.Cylinder().Position > 0.01) ? Fail("1A does not retract after S1 is released. The cylinder air must exhaust through port 3.") : Pass("This is direct control: the operator's valve feeds the cylinder itself.");
			}
		},
		new Lesson
		{
			Title = "2. Indirect control",
			Goal = "Control the double-acting cylinder 1A with the 5/2 valve 1V1 (pneumatic pilot, spring return). Push button valve S1 sends the pilot signal: 1A extends while S1 is held and retracts when it is released.",
			Hint = "S1 port 2 → pilot 14 of 1V1. 1V1 port 4 → cylinder port 1, port 2 → cylinder port 2. Both valves need a supply.",
			Start = [SpecialName] () => Parts((new DoubleActingCylinder(), "1A"), (Library.V52(ValveActuator.Pilot, ValveReturn.Spring), "1V1"), (Library.V32(ValveActuator.PushButton), "S1"), (new AirSupply(), ""), (new AirSupply(), "")),
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe10 = new Probe(c);
				string text11 = Need(probe10, "S1", "1A");
				if (text11 != null)
				{
					return Fail(text11);
				}
				probe10.Press(probe10.Find("S1"));
				probe10.Run(2.0);
				if (probe10.Cylinder().Position < 0.99)
				{
					return Fail("1A does not extend when S1 is held.");
				}
				if (CircuitAnalysis.FindMoves(probe10.Circuit).Any([SpecialName] (CircuitMove m) => m.Extend && m.Valve.Actuator == ValveActuator.PushButton))
				{
					return Fail("1A is fed directly by the push button valve. In indirect control the push button only pilots the 5/2 valve.");
				}
				probe10.Release(probe10.Find("S1"));
				probe10.Run(2.0);
				return (probe10.Cylinder().Position > 0.01) ? Fail("1A does not retract after S1 is released.") : Pass("Indirect control lets a small push button valve switch a big power valve.");
			}
		},
		new Lesson
		{
			Title = "3. Speed control (meter-out)",
			Goal = "Build an indirect control (as in lesson 2) where 1A extends slowly (at least 2 s for a stroke) but retracts quickly (under 1.2 s). Stroke time of 1A is 1 s.",
			Hint = "Put a one-way flow control valve in the line to cylinder port 2 so that the air leaving the cylinder is throttled (port 2 of the flow control towards the cylinder).",
			Start = [SpecialName] () => Parts((new DoubleActingCylinder(), "1A"), (Library.V52(ValveActuator.Pilot, ValveReturn.Spring), "1V1"), (new FlowControlValve
			{
				OpeningPercent = 30
			}, "1V2"), (Library.V32(ValveActuator.PushButton), "S1"), (new AirSupply(), ""), (new AirSupply(), "")),
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe9 = new Probe(c);
				string text10 = Need(probe9, "S1", "1A");
				if (text10 != null)
				{
					return Fail(text10);
				}
				probe9.Press(probe9.Find("S1"));
				double num4 = 0.0;
				while (probe9.Cylinder().Position < 0.999 && num4 < 15.0)
				{
					probe9.Run(0.05);
					num4 += 0.05;
				}
				if (num4 >= 15.0)
				{
					return Fail("1A does not reach the end of its stroke when S1 is held.");
				}
				if (num4 < 2.0)
				{
					return Fail($"1A extends in {num4:0.0} s; it should take at least 2 s. Throttle the air leaving the rod side.");
				}
				probe9.Release(probe9.Find("S1"));
				double num5 = 0.0;
				while (probe9.Cylinder().Position > 0.001 && num5 < 15.0)
				{
					probe9.Run(0.05);
					num5 += 0.05;
				}
				return (num5 > 1.2) ? Fail($"1A retracts in {num5:0.0} s; it should be under 1.2 s. Use a one-way (check) flow control so the return is free.") : Pass($"Extends in {num4:0.0} s, retracts in {num5:0.0} s. Meter-out control gives smooth, load-independent speed.");
			}
		},
		new Lesson
		{
			Title = "4. OR: two places",
			Goal = "Cylinder 1A (single-acting) must extend when push button valve S1 OR push button valve S2 is pressed.",
			Hint = "Use a shuttle valve. Without it, the air from one button would escape through the other button's exhaust.",
			Start = [SpecialName] () => Parts((new SingleActingCylinder(), "1A"), (new ShuttleValve(), "1V1"), (Library.V32(ValveActuator.PushButton), "S1"), (Library.V32(ValveActuator.PushButton), "S2"), (new AirSupply(), ""), (new AirSupply(), "")),
			Check = [SpecialName] (Circuit c) =>
			{
				string[] array2 = new string[2] { "S1", "S2" };
				foreach (string text8 in array2)
				{
					Probe probe8 = new Probe(c);
					string text9 = Need(probe8, "S1", "S2", "1A");
					if (text9 != null)
					{
						return Fail(text9);
					}
					probe8.Press(probe8.Find(text8));
					probe8.Run(2.0);
					if (probe8.Cylinder().Position < 0.99)
					{
						return Fail($"1A does not extend when only {text8} is pressed.");
					}
					probe8.Release(probe8.Find(text8));
					probe8.Run(2.0);
					if (probe8.Cylinder().Position > 0.01)
					{
						return Fail($"1A does not retract after {text8} is released.");
					}
				}
				return Pass("The shuttle valve passes whichever input has air and blocks the other.");
			}
		},
		new Lesson
		{
			Title = "5. AND: two-hand safety",
			Goal = "A press may only close when the operator uses both hands: double-acting cylinder 1A extends only while S1 AND S2 are both pressed.",
			Hint = "A two-pressure valve (AND) passes air only when both inputs have air. Its output pilots a 5/2 valve.",
			Start = [SpecialName] () => Parts((new DoubleActingCylinder(), "1A"), (Library.V52(ValveActuator.Pilot, ValveReturn.Spring), "1V1"), (new TwoPressureValve(), "1V2"), (Library.V32(ValveActuator.PushButton), "S1"), (Library.V32(ValveActuator.PushButton), "S2"), (new AirSupply(), ""), (new AirSupply(), ""), (new AirSupply(), "")),
			Check = [SpecialName] (Circuit c) =>
			{
				string[] array = new string[2] { "S1", "S2" };
				foreach (string text6 in array)
				{
					Probe probe6 = new Probe(c);
					string text7 = Need(probe6, "S1", "S2", "1A");
					if (text7 != null)
					{
						return Fail(text7);
					}
					probe6.Press(probe6.Find(text6));
					probe6.Run(2.0);
					if (probe6.Cylinder().Position > 0.05)
					{
						return Fail($"Unsafe: 1A moves when only {text6} is pressed.");
					}
				}
				Probe probe7 = new Probe(c);
				probe7.Press(probe7.Find("S1"));
				probe7.Press(probe7.Find("S2"));
				probe7.Run(2.0);
				if (probe7.Cylinder().Position < 0.99)
				{
					return Fail("1A does not extend when both S1 and S2 are pressed.");
				}
				probe7.Release(probe7.Find("S1"));
				probe7.Run(2.0);
				return (probe7.Cylinder().Position > 0.01) ? Fail("1A should retract as soon as one hand lets go.") : Pass("Two-hand control keeps both hands away from the tool.");
			}
		},
		new Lesson
		{
			Title = "6. Memory valve",
			Goal = "A short press on S1 makes double-acting cylinder 1A extend and STAY extended. A short press on S2 makes it retract.",
			Hint = "Use a 5/2 double pilot (memory) valve: S1 → pilot 14, S2 → pilot 12.",
			Start = [SpecialName] () => Parts((new DoubleActingCylinder(), "1A"), (Library.V52(ValveActuator.Pilot, ValveReturn.Pilot), "1V1"), (Library.V32(ValveActuator.PushButton), "S1"), (Library.V32(ValveActuator.PushButton), "S2"), (new AirSupply(), ""), (new AirSupply(), ""), (new AirSupply(), "")),
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe5 = new Probe(c);
				string text5 = Need(probe5, "S1", "S2", "1A");
				if (text5 != null)
				{
					return Fail(text5);
				}
				probe5.Tap(probe5.Find("S1"));
				probe5.Run(2.0);
				if (probe5.Cylinder().Position < 0.99)
				{
					return Fail("After a short press on S1, 1A should extend and stay extended.");
				}
				probe5.Tap(probe5.Find("S2"));
				probe5.Run(2.0);
				return (probe5.Cylinder().Position > 0.01) ? Fail("After a short press on S2, 1A should retract.") : Pass("A double pilot valve remembers the last signal: it is a pneumatic memory.");
			}
		},
		new Lesson
		{
			Title = "7. Automatic return",
			Goal = "A short press on start button S1 makes cylinder 1A extend; when it reaches the end it returns by itself.",
			Hint = "Give 1A the extended mark 1S2 and use a roller lever valve with roller mark 1S2 to pilot 12 of a memory valve.",
			Start = [SpecialName] () => Parts((new DoubleActingCylinder
			{
				ExtendedMark = "1S2"
			}, "1A"), (Library.V52(ValveActuator.Pilot, ValveReturn.Pilot), "1V1"), (Library.V32(ValveActuator.PushButton), "S1"), (new Valve32
			{
				Actuator = ValveActuator.RollerLever,
				TriggerMark = "1S2"
			}, "1S2"), (new AirSupply(), ""), (new AirSupply(), ""), (new AirSupply(), "")),
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe4 = new Probe(c);
				string text4 = Need(probe4, "S1", "1A");
				if (text4 != null)
				{
					return Fail(text4);
				}
				probe4.Tap(probe4.Find("S1"));
				bool flag3 = false;
				int num3 = 1;
				do
				{
					probe4.Run(0.05);
					if (probe4.Cylinder().Position >= 0.999)
					{
						flag3 = true;
					}
					if (flag3 && probe4.Cylinder().Position <= 0.001)
					{
						return Pass("The limit valve detects the end position and starts the return automatically.");
					}
					num3++;
				}
				while (num3 <= 100);
				return Fail(flag3 ? "1A extends but does not come back by itself." : "1A does not extend after a short press on S1.");
			}
		},
		new Lesson
		{
			Title = "8. Electrical self-holding",
			Goal = "Electro-pneumatic: pressing S1 briefly switches relay K1 on and it stays on (self-holding); S0 (normally closed) switches it off. K1 operates solenoid 1M1 of a 5/2 single-solenoid valve that extends cylinder 1A.",
			Hint = "Rung 1: +24 V → S0 (NC) → (S1 in parallel with a K1 NO contact) → coil K1 → 0 V. Rung 2: +24 V → K1 NO contact → solenoid coil 1M1 → 0 V.",
			Start = null,
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe3 = new Probe(c);
				string text3 = Need(probe3, "S1", "S0", "1A");
				if (text3 != null)
				{
					return Fail(text3);
				}
				probe3.Tap(probe3.Find("S1"));
				probe3.Run(2.0);
				if (!probe3.Sim.IsCoilActive("K1"))
				{
					return Fail("Relay K1 does not stay on after S1 is released. Add the self-holding contact K1 in parallel with S1.");
				}
				if (probe3.Cylinder().Position < 0.99)
				{
					return Fail("K1 is on, but 1A does not extend. Check solenoid 1M1 and the valve's solenoid label.");
				}
				probe3.Tap(probe3.Find("S0"));
				probe3.Run(2.0);
				if (probe3.Sim.IsCoilActive("K1"))
				{
					return Fail("S0 does not switch K1 off. S0 must be a normally closed contact in series.");
				}
				return (probe3.Cylinder().Position > 0.01) ? Fail("1A does not retract after stopping.") : Pass("Self-holding (latching) is the basis of every start/stop circuit.");
			}
		},
		new Lesson
		{
			Title = "9. Hydraulics: relief valve",
			Goal = "Build a hydraulic circuit: pump, relief valve set to 60 bar, 4/3 valve with solenoids 1M1 / 1M2 (push buttons S1 / S2), and hydraulic cylinder 1A. 1A must extend with S1 and retract with S2, and the pressure must never exceed 60 bar.",
			Hint = "Pump → junction → relief valve P (relief T → tank) and valve P. Valve T → tank. Use hydraulic junctions for branches.",
			Start = null,
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe2 = new Probe(c);
				string text2 = Need(probe2, "S1", "S2", "1A");
				if (text2 != null)
				{
					return Fail(text2);
				}
				if (!probe2.Circuit.Elements.OfType<ReliefValve>().Any())
				{
					return Fail("There is no pressure relief valve.");
				}
				probe2.Press(probe2.Find("S1"));
				probe2.Run(4.0);
				if (probe2.Cylinder().Position < 0.99)
				{
					return Fail("1A does not extend while S1 is held.");
				}
				double num2 = (from x in probe2.Circuit.AllPorts()
					where x.Kind == PortKind.Hydraulic
					select x).Max([SpecialName] (Port x) => x.Pressure);
				if (num2 > 60.5)
				{
					return Fail($"The pressure rises to {num2:0} bar. Set the relief valve to 60 bar and connect it right after the pump.");
				}
				probe2.Release(probe2.Find("S1"));
				probe2.Press(probe2.Find("S2"));
				probe2.Run(4.0);
				return (probe2.Cylinder().Position > 0.01) ? Fail("1A does not retract while S2 is held.") : Pass("The relief valve protects the pump and everything else: at the end of the stroke the oil flows back to tank at 60 bar.");
			}
		},
		new Lesson
		{
			Title = "10. Sequence A+ B+ B- A-",
			Goal = "Two double-acting cylinders 1A and 2A. After a short press on start button S1 (electrical) or 1S0 (pneumatic), they move A+ B+ B- A- once.",
			Hint = "Tip: Tools > Circuit Generator builds this for you; study how it avoids signal overlap.",
			Start = null,
			Check = [SpecialName] (Circuit c) =>
			{
				Probe probe = new Probe(c);
				CircuitElement circuitElement = probe.Find("S1") ?? probe.Find("1S0");
				if (circuitElement == null)
				{
					return Fail("I cannot find a start button labelled S1 or 1S0.");
				}
				CylinderBase cylinderBase = probe.Find("1A") as CylinderBase;
				CylinderBase cylinderBase2 = probe.Find("2A") as CylinderBase;
				if (cylinderBase == null || cylinderBase2 == null)
				{
					return Fail("Label the cylinders 1A and 2A.");
				}
				probe.Tap(circuitElement);
				List<string> list = new List<string>();
				bool flag = false;
				bool flag2 = false;
				int num = 1;
				do
				{
					probe.Sim.Step(0.005);
					if (!flag && cylinderBase.Position >= 0.995)
					{
						flag = true;
						list.Add("A+");
					}
					if (flag && cylinderBase.Position <= 0.005)
					{
						flag = false;
						list.Add("A-");
					}
					if (!flag2 && cylinderBase2.Position >= 0.995)
					{
						flag2 = true;
						list.Add("B+");
					}
					if (flag2 && cylinderBase2.Position <= 0.005)
					{
						flag2 = false;
						list.Add("B-");
					}
					num++;
				}
				while (num <= 2000);
				string text = string.Join(" ", list);
				return text.StartsWith("A+ B+ B- A-") ? Pass("The sequence is correct.") : Fail(string.Format("The cylinders moved: {0}. Expected A+ B+ B- A-.", (Operators.CompareString(text, "", TextCompare: false) == 0) ? "(nothing)" : text));
			}
		}
	});

	private static LessonResult Pass(string msg)
	{
		return new LessonResult
		{
			Passed = true,
			Feedback = "Well done! " + msg
		};
	}

	private static LessonResult Fail(string msg)
	{
		return new LessonResult
		{
			Passed = false,
			Feedback = msg
		};
	}

	private static string Need(Probe p, params string[] labels)
	{
		List<string> list = labels.Where([SpecialName] (string l) => p.Find(l) == null).ToList();
		if (list.Count == 0)
		{
			return null;
		}
		return string.Format("I cannot find {0}. Give your components these labels in the Properties panel ", string.Join(", ", list)) + "(push buttons / switches, and the cylinder 1A).";
	}

	private static Circuit Parts(params (CircuitElement El, string Label)[] items)
	{
		Circuit circuit = new Circuit();
		float num = 40f;
		for (int i = 0; i < items.Length; i = checked(i + 1))
		{
			(CircuitElement, string) tuple = items[i];
			circuit.Add(tuple.Item1, num, 120f, tuple.Item2);
			num += tuple.Item1.LocalBounds.Width + 50f;
		}
		return circuit;
	}
}
