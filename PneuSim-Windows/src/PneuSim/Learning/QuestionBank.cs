using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class QuestionBank
{
	[Serializable]
	[CompilerGenerated]
	internal sealed class _Closure_0024__
	{
		public static readonly _Closure_0024__ _0024I;

		public static Func<LibraryPreset, bool> _0024I5_002D0;

		public static Func<LibraryPreset, string> _0024I5_002D2;

		public static Func<IGrouping<string, LibraryPreset>, LibraryPreset> _0024I5_002D3;

		public static Func<LibraryPreset, string> _0024I5_002D5;

		static _Closure_0024__()
		{
			_0024I = new _Closure_0024__();
		}

		[SpecialName]
		internal bool _Lambda_0024__5_002D0(LibraryPreset p)
		{
			if (Operators.CompareString(p.Category, "Drawing", TextCompare: false) != 0 && !p.Name.Contains("junction"))
			{
				return !p.Name.Contains("connector");
			}
			return false;
		}

		[SpecialName]
		internal string _Lambda_0024__5_002D2(LibraryPreset p)
		{
			return Signature(p);
		}

		[SpecialName]
		internal LibraryPreset _Lambda_0024__5_002D3(IGrouping<string, LibraryPreset> g)
		{
			return g.First();
		}

		[SpecialName]
		internal string _Lambda_0024__5_002D5(LibraryPreset p)
		{
			return p.Name;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__5_002D0
	{
		public LibraryPreset _0024VB_0024Local_preset;

		public _Closure_0024__5_002D0(_Closure_0024__5_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_preset = arg0._0024VB_0024Local_preset;
			}
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__5_002D1
	{
		public string _0024VB_0024Local_sig;

		public _Closure_0024__5_002D0 _0024VB_0024NonLocal__0024VB_0024Closure_2;

		public _Closure_0024__5_002D1(_Closure_0024__5_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_sig = arg0._0024VB_0024Local_sig;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__1(LibraryPreset p)
		{
			if (p != _0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_preset)
			{
				return Operators.CompareString(Signature(p), _0024VB_0024Local_sig, TextCompare: false) != 0;
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__5_002D2
	{
		public Random _0024VB_0024Local_rnd;

		public Func<LibraryPreset, int> _0024I4;

		public Func<string, int> _0024I6;

		public _Closure_0024__5_002D2(_Closure_0024__5_002D2 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_rnd = arg0._0024VB_0024Local_rnd;
			}
		}

		[SpecialName]
		internal int _Lambda_0024__4(LibraryPreset x)
		{
			return _0024VB_0024Local_rnd.Next();
		}

		[SpecialName]
		internal int _Lambda_0024__6(string x)
		{
			return _0024VB_0024Local_rnd.Next();
		}
	}

	private static readonly QuizQuestion[] Written = new QuizQuestion[39]
	{
		Q("Basics", "What is the usual working pressure of an industrial compressed air system?", new string[4] { "0.6 bar", "6 bar", "60 bar", "600 bar" }, 1, "Most pneumatic systems work at about 6 bar (gauge)."),
		Q("Basics", "Why is compressed air passed through a service unit before use?", new string[4] { "To heat it", "To filter it, remove water and set the pressure", "To increase the flow", "To make it quieter" }, 1, "The service unit filters dirt and water and regulates the pressure."),
		Q("Basics", "What does 'free air' (normal litres) mean?", new string[4] { "Air at 6 bar", "Air at atmospheric pressure", "Air without oil", "Air after the silencer" }, 1, "Air consumption is quoted as free air, i.e. the volume at atmospheric pressure."),
		Q("Actuators", "A double-acting cylinder has a 40 mm bore. Roughly what force does it extend with at 6 bar?", new string[4] { "75 N", "750 N", "7 500 N", "75 000 N" }, 1, "F = p × A = 600 000 Pa × 0.00126 m² ≈ 750 N."),
		Q("Actuators", "Why does a double-acting cylinder retract with less force than it extends?", new string[4] { "The spring opposes it", "The rod reduces the piston area on that side", "The exhaust is blocked", "The pressure is lower" }, 1, "On the rod side the piston rod takes up part of the area."),
		Q("Actuators", "What returns a single-acting cylinder?", new string[4] { "Compressed air", "A spring (or the load)", "A solenoid", "A flow control valve" }, 1, "A single-acting cylinder uses air for one direction only; a spring returns it."),
		Q("Valves", "In the name '5/2-way valve', what do 5 and 2 mean?", new string[4] { "5 bar, 2 cylinders", "5 ports, 2 positions", "5 positions, 2 ports", "5 mm, 2 mm" }, 1, "The first number is the number of ports, the second the number of switching positions."),
		Q("Valves", "Which port number is the supply (pressure) port of a pneumatic valve?", new string[4] { "1", "2", "3", "12" }, 0, "1 = supply, 2 and 4 = outputs, 3 and 5 = exhausts, 12/14 = pilot ports."),
		Q("Valves", "A 5/2 valve with pilots 14 and 12 and no spring is called…", new string[4] { "A shuttle valve", "A memory (impulse) valve", "A time delay valve", "A quick exhaust valve" }, 1, "It stays in the last position it was switched to, so it remembers the last signal."),
		Q("Valves", "What happens to a cylinder when a closed-centre 5/3 valve goes to its middle position?", new string[4] { "It extends", "It retracts", "It stops and holds its position", "It moves freely" }, 2, "All ports are blocked, so the air in both chambers is trapped."),
		Q("Valves", "What is the function of a shuttle valve?", new string[4] { "AND", "OR", "NOT", "Memory" }, 1, "The output has air if either input has air."),
		Q("Valves", "Which valve is used for a two-hand safety control?", new string[4] { "Shuttle valve", "Two-pressure valve", "Quick exhaust valve", "Check valve" }, 1, "The two-pressure (AND) valve passes air only when both inputs have air."),
		Q("Speed", "How is the speed of a pneumatic cylinder usually controlled?", new string[4] { "Throttling the supply air (meter-in)", "Throttling the exhaust air (meter-out)", "Lowering the supply pressure", "Using a bigger valve" }, 1, "Meter-out keeps both chambers under pressure and gives a smooth, load-independent speed."),
		Q("Speed", "What does a quick exhaust valve do?", new string[4] { "Slows the cylinder down", "Lets the cylinder exhaust directly, so it returns faster", "Stops the cylinder", "Filters the exhaust" }, 1, "The air escapes next to the cylinder instead of through the long tube and the valve."),
		Q("Speed", "In a one-way flow control valve, which way is the flow throttled?", new string[4] { "Both ways", "Only one way; the check valve gives free flow the other way", "Neither way", "Only when the pressure is high" }, 1, "The check valve bypasses the throttle in one direction."),
		Q("Sequences", "What is signal overlap in a sequence circuit?", new string[4] { "Two cylinders moving at once", "A memory valve getting both pilot signals at the same time", "A leak in a tube", "Too much pressure" }, 1, "The opposite signal is still present, so the valve cannot switch and the sequence stops."),
		Q("Sequences", "In the cascade method, how many cascade valves are needed for 3 groups?", new string[4] { "1", "2", "3", "4" }, 1, "Number of cascade valves = number of groups − 1."),
		Q("Sequences", "How is a sequence split into groups in the cascade method?", new string[4] { "One group per cylinder", "So that no cylinder appears twice in a group", "Every two movements", "By pressure" }, 1, "Each group may contain each cylinder only once."),
		Q("Sequences", "Which valve detects that a cylinder has reached its end position in a pneumatic sequence?", new string[4] { "Roller lever valve", "Shuttle valve", "Flow control valve", "Service unit" }, 0, "Roller lever (limit) valves are operated by the cylinder rod or a cam."),
		Q("Vacuum", "How does a vacuum generator (ejector) make vacuum?", new string[4] { "With an electric pump", "Compressed air rushing through a nozzle drags air out of the suction port", "By cooling the air", "With a check valve" }, 1, "The fast jet of air (venturi principle) sucks air out of the suction cup line."),
		Q("Vacuum", "Why is a vacuum switch fitted in a suction cup line?", new string[4] { "To make more vacuum", "To report that the part is really held before the arm moves", "To save air", "To release the part" }, 1, "Without a workpiece the cup is open and the vacuum stays weak, so the switch does not operate."),
		Q("Air supply", "What does the air receiver do?", new string[4] { "Filters the air", "Stores compressed air and evens out peaks of air use", "Lowers the pressure", "Dries the air" }, 1, "The receiver stores air so the compressor runs in calmer cycles and the pressure does not drop at every stroke."),
		Q("Sensors", "What is an idle-return roller valve used for?", new string[4] { "To slow a cylinder", "To give only a short signal in one direction, avoiding signal overlap", "To count cycles", "To hold a cylinder in position" }, 1, "Its hinged lever operates the valve only when the cam comes from one side."),
		Q("Troubleshooting", "A cylinder moves very slowly and air hisses from the valve exhaust even when the cylinder is not moving. What is the most likely fault?", new string[4] { "A blocked tube", "A worn piston seal", "A burnt solenoid", "Too much supply pressure" }, 1, "Air passes the worn seal from one chamber to the other and out through the valve exhaust."),
		Q("Troubleshooting", "A solenoid valve does not switch, but it switches when you press its manual override. What do you check first?", new string[4] { "The cylinder", "The supply pressure", "The solenoid and its 24 V signal", "The silencer" }, 2, "The valve itself works, so the electrical side is the problem: no signal, a broken wire or a burnt coil."),
		Q("Electro", "What is the usual control voltage in electro-pneumatics?", new string[4] { "5 V DC", "24 V DC", "230 V AC", "415 V AC" }, 1, "24 V DC is safe and standard for control circuits."),
		Q("Electro", "What does a 'normally closed' (NC) contact do?", new string[4] { "Conducts only when operated", "Conducts until it is operated", "Never conducts", "Conducts with a delay" }, 1, "It is closed at rest and opens when operated (stop buttons are NC)."),
		Q("Electro", "What is a self-holding (latching) circuit?", new string[4] { "A relay holding itself on through its own contact", "A valve with a detent", "A cylinder with a brake", "A timer" }, 0, "The relay's own NO contact in parallel with the start button keeps the coil energized."),
		Q("Electro", "An on-delay timer relay…", new string[4] { "Switches its contacts immediately and back after a delay", "Switches its contacts a set time after the coil is energized", "Never switches", "Counts pulses" }, 1, "The contacts change over after the set time has passed."),
		Q("Electro", "What switches a solenoid valve?", new string[4] { "Air pressure", "An electromagnet (solenoid coil)", "A roller", "A spring" }, 1, "Current through the coil pulls the armature and moves the spool."),
		Q("Electro", "Which sensor detects a cylinder position without contact?", new string[4] { "Limit switch", "Proximity (reed or inductive) sensor", "Push button", "Pressure gauge" }, 1, "Proximity sensors react to the magnet on the piston or a metal target."),
		Q("Hydraulics", "Why does every hydraulic circuit need a pressure relief valve?", new string[4] { "To filter the oil", "To limit the pressure when nothing can move", "To cool the oil", "To make the pump quieter" }, 1, "Oil cannot be compressed; without a relief valve the pressure would rise until something breaks."),
		Q("Hydraulics", "A pump delivers 12 l/min into a cylinder with 20 cm² piston area. What is the speed?", new string[4] { "0.01 m/s", "0.1 m/s", "1 m/s", "10 m/s" }, 1, "v = Q / A = 0.0002 m³/s ÷ 0.002 m² = 0.1 m/s."),
		Q("Hydraulics", "In a hydraulic system, what decides the working pressure while the cylinder moves?", new string[4] { "The pump size", "The load", "The tank size", "The oil colour" }, 1, "Pressure builds up only as far as needed to move the load (up to the relief setting)."),
		Q("Hydraulics", "What does a tandem-centre 4/3 valve do in its middle position?", new string[4] { "Blocks all ports", "Connects P to T so the pump runs unloaded, A and B blocked", "Connects A and B to the pump", "Lets the cylinder float" }, 1, "The pump flow returns to tank at low pressure, saving energy, while the cylinder is held."),
		Q("Hydraulics", "What is a counterbalance valve used for?", new string[4] { "To hold a hanging load and stop it running away", "To increase speed", "To filter oil", "To cool oil" }, 0, "It keeps back-pressure on the cylinder so a load cannot fall."),
		Q("Hydraulics", "What does an accumulator do?", new string[4] { "Stores oil under pressure", "Measures flow", "Cools the oil", "Removes air" }, 0, "Gas pressure stores energy that can move actuators when the pump cannot."),
		Q("Hydraulics", "How is hydraulic power calculated (kW)?", new string[4] { "p × Q / 600 with p in bar and Q in l/min", "p + Q", "Q / p", "p × A" }, 0, "P = p × Q / 600."),
		Q("Basics", "What does a pressure sequence valve do?", new string[4] { "Switches when the pressure reaches a set value", "Limits flow", "Stores air", "Filters air" }, 0, "It starts the next action only after the pressure (e.g. clamping force) has been reached.")
	};

	private static Dictionary<LibraryPreset, string> _signatures;

	public static int WrittenCount => Written.Length;

	private static QuizQuestion Q(string topic, string text, string[] options, int correct, string explanation)
	{
		return new QuizQuestion
		{
			Topic = topic,
			Text = text,
			Options = options,
			Correct = correct,
			Explanation = explanation
		};
	}

	private static string Signature(LibraryPreset p)
	{
		if (_signatures == null)
		{
			_signatures = new Dictionary<LibraryPreset, string>();
			LibraryPreset[] presets = Library.Presets;
			foreach (LibraryPreset libraryPreset in presets)
			{
				using Bitmap bitmap = Library.RenderThumbnail(libraryPreset.Factory(), 120, 50);
				using MemoryStream memoryStream = new MemoryStream();
				bitmap.Save(memoryStream, ImageFormat.Png);
				using MD5 mD = MD5.Create();
				_signatures[libraryPreset] = Convert.ToBase64String(mD.ComputeHash(memoryStream.ToArray()));
			}
		}
		return _signatures[p];
	}

	private static List<QuizQuestion> SymbolQuestions(Random rnd)
	{
		_Closure_0024__5_002D2 arg = default(_Closure_0024__5_002D2);
		_Closure_0024__5_002D2 CS_0024_003C_003E8__locals0 = new _Closure_0024__5_002D2(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_rnd = rnd;
		List<QuizQuestion> list = new List<QuizQuestion>();
		List<LibraryPreset> list2 = Library.Presets.Where([SpecialName] (LibraryPreset p) => Operators.CompareString(p.Category, "Drawing", TextCompare: false) != 0 && !p.Name.Contains("junction") && !p.Name.Contains("connector")).ToList();
		using List<LibraryPreset>.Enumerator enumerator = list2.GetEnumerator();
		_Closure_0024__5_002D0 closure_0024__5_002D = default(_Closure_0024__5_002D0);
		_Closure_0024__5_002D1 closure_0024__5_002D2 = default(_Closure_0024__5_002D1);
		while (enumerator.MoveNext())
		{
			closure_0024__5_002D = new _Closure_0024__5_002D0(closure_0024__5_002D);
			closure_0024__5_002D._0024VB_0024Local_preset = enumerator.Current;
			closure_0024__5_002D2 = new _Closure_0024__5_002D1(closure_0024__5_002D2);
			closure_0024__5_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2 = closure_0024__5_002D;
			closure_0024__5_002D2._0024VB_0024Local_sig = Signature(closure_0024__5_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_preset);
			string[] array = (from x in (from x in list2.Where(closure_0024__5_002D2._Lambda_0024__1).GroupBy((_Closure_0024__._0024I5_002D2 == null) ? (_Closure_0024__._0024I5_002D2 = [SpecialName] (LibraryPreset p) => Signature(p)) : _Closure_0024__._0024I5_002D2).Select((_Closure_0024__._0024I5_002D3 == null) ? (_Closure_0024__._0024I5_002D3 = [SpecialName] (IGrouping<string, LibraryPreset> g) => g.First()) : _Closure_0024__._0024I5_002D3)
					orderby CS_0024_003C_003E8__locals0._0024VB_0024Local_rnd.Next()
					select x).Take(3).Select((_Closure_0024__._0024I5_002D5 == null) ? (_Closure_0024__._0024I5_002D5 = [SpecialName] (LibraryPreset p) => p.Name) : _Closure_0024__._0024I5_002D5).ToList()
					.Concat(new string[1] { closure_0024__5_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_preset.Name })
				orderby CS_0024_003C_003E8__locals0._0024VB_0024Local_rnd.Next()
				select x).ToArray();
			list.Add(new QuizQuestion
			{
				Topic = "Symbols",
				Text = "What does this symbol show?",
				Options = array,
				Correct = Array.IndexOf(array, closure_0024__5_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_preset.Name),
				SymbolPreset = closure_0024__5_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_preset,
				Explanation = $"It is the symbol for: {closure_0024__5_002D2._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_preset.Name}."
			});
		}
		return list;
	}

	public static List<QuizQuestion> Pick(int count, int? seed = null)
	{
		Random random = (seed.HasValue ? new Random(seed.Value) : new Random());
		List<QuizQuestion> list = (from x in SymbolQuestions(random)
			orderby random.Next()
			select x).Take(Math.Max(1, count / 4)).ToList();
		return (from x in Written.OrderBy([SpecialName] (QuizQuestion x) => random.Next()).Take(checked(count - list.Count)).ToList()
				.Concat(list)
			orderby random.Next()
			select x into q
			select Shuffled(q, random)).ToList();
	}

	private static QuizQuestion Shuffled(QuizQuestion q, Random rnd)
	{
		int[] array = (from x in Enumerable.Range(0, q.Options.Length)
			orderby rnd.Next()
			select x).ToArray();
		return new QuizQuestion
		{
			Topic = q.Topic,
			Text = q.Text,
			Explanation = q.Explanation,
			SymbolPreset = q.SymbolPreset,
			Options = array.Select([SpecialName] (int i) => q.Options[i]).ToArray(),
			Correct = Array.IndexOf(array, q.Correct)
		};
	}
}
