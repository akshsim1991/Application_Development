using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;

namespace PneuSim;

public class TroubleshootExercise
{
	public DateTime Started { get; set; }

	public DateTime? Finished { get; set; }

	public int HintsUsed { get; set; }

	public int WrongGuesses { get; set; }

	public bool Solved { get; set; }

	public bool GaveUp { get; set; }

	public bool KindRight { get; set; }

	public List<string> Checks { get; }

	public List<string> Log { get; }

	public bool IsOver
	{
		get
		{
			if (!Solved)
			{
				return GaveUp;
			}
			return true;
		}
	}

	public TroubleshootExercise()
	{
		Checks = new List<string>();
		Log = new List<string>();
		Started = DateTime.Now;
	}

	private void Note(string text)
	{
		Log.Add($"{(DateTime.Now - Started).TotalSeconds,5:0}s  {text}");
	}

	public static object FindHidden(Project project)
	{
		foreach (ProjectPage page in project.Pages)
		{
			CircuitElement circuitElement = page.Circuit.Elements.FirstOrDefault([SpecialName] (CircuitElement e) => e.Fault != 0 && e.FaultHidden);
			if (circuitElement != null)
			{
				return circuitElement;
			}
			Tube tube = page.Circuit.Tubes.FirstOrDefault([SpecialName] (Tube x) => x.Fault != 0 && x.FaultHidden);
			if (tube != null)
			{
				return tube;
			}
		}
		return null;
	}

	public static FaultKind FaultOf(object target)
	{
		if (target is CircuitElement)
		{
			return ((CircuitElement)target).Fault;
		}
		if (target is Tube)
		{
			return ((Tube)target).Fault;
		}
		return FaultKind.None;
	}

	public static FaultKind[] PossibleFaultsOf(object target)
	{
		if (target is CircuitElement)
		{
			return ((CircuitElement)target).PossibleFaults();
		}
		if (target is Tube)
		{
			return ((Tube)target).PossibleFaults();
		}
		return Array.Empty<FaultKind>();
	}

	public static string DescribeFault(object target, FaultKind kind)
	{
		if (target is CircuitElement)
		{
			return ((CircuitElement)target).FaultDescription(kind);
		}
		if (target is Tube)
		{
			return ((Tube)target).FaultDescription(kind);
		}
		return Faults.Describe(kind);
	}

	public static void SetFault(object target, FaultKind kind, bool hidden)
	{
		if (target is CircuitElement)
		{
			CircuitElement obj = (CircuitElement)target;
			obj.Fault = kind;
			obj.FaultHidden = hidden && kind != FaultKind.None;
		}
		else if (target is Tube)
		{
			Tube obj2 = (Tube)target;
			obj2.Fault = kind;
			obj2.FaultHidden = hidden && kind != FaultKind.None;
		}
	}

	public static string NameOf_(object target)
	{
		if (target is CircuitElement)
		{
			CircuitElement circuitElement = (CircuitElement)target;
			return string.IsNullOrWhiteSpace(circuitElement.Label) ? circuitElement.DisplayName : $"{circuitElement.Label} ({circuitElement.DisplayName})";
		}
		if (target is Tube)
		{
			Tube tube = (Tube)target;
			string arg = (tube.IsElectric ? "wire" : ((tube.A.Kind == PortKind.Hydraulic) ? "hose" : "tube"));
			return $"{arg} {EndName(tube.A)} – {EndName(tube.B)}";
		}
		return "?";
	}

	private static string EndName(Port p)
	{
		string arg = (string.IsNullOrWhiteSpace(p.Owner.Label) ? p.Owner.DisplayName : p.Owner.Label);
		if (!(p.Owner is Junction))
		{
			return $"{arg}:{p.Name}";
		}
		return "junction";
	}

	public static void RemoveAllFaults(Project project)
	{
		foreach (ProjectPage page in project.Pages)
		{
			foreach (CircuitElement element in page.Circuit.Elements)
			{
				element.Fault = FaultKind.None;
				element.FaultHidden = false;
			}
			foreach (Tube tube in page.Circuit.Tubes)
			{
				tube.Fault = FaultKind.None;
				tube.FaultHidden = false;
			}
		}
	}

	public static List<object> Candidates(Project project)
	{
		List<object> list = new List<object>();
		foreach (ProjectPage page in project.Pages)
		{
			foreach (CircuitElement element in page.Circuit.Elements)
			{
				if (element.PossibleFaults().Length != 0 && (element.Ports.Count <= 0 || !element.Ports.All([SpecialName] (Port p) => p.ConnectionCount == 0)))
				{
					list.Add(element);
				}
			}
			foreach (Tube tube in page.Circuit.Tubes)
			{
				if (!(tube.A.Owner is PressureGauge) && !(tube.B.Owner is PressureGauge))
				{
					list.Add(tube);
				}
			}
		}
		return list;
	}

	public static TroubleshootExercise StartRandom(Project project, Random rnd)
	{
		List<object> list = Candidates(project);
		if (list.Count == 0)
		{
			return null;
		}
		RemoveAllFaults(project);
		object objectValue = RuntimeHelpers.GetObjectValue(list[rnd.Next(list.Count)]);
		FaultKind[] array = PossibleFaultsOf(RuntimeHelpers.GetObjectValue(objectValue));
		SetFault(RuntimeHelpers.GetObjectValue(objectValue), array[rnd.Next(array.Length)], hidden: true);
		TroubleshootExercise troubleshootExercise = new TroubleshootExercise();
		troubleshootExercise.Note("Fault-finding task started: one component or tube has a hidden fault.");
		return troubleshootExercise;
	}

	public bool RecordCheck(object target)
	{
		if (IsOver)
		{
			return false;
		}
		string text = NameOf_(RuntimeHelpers.GetObjectValue(target));
		if (Checks.Contains(text))
		{
			return false;
		}
		Checks.Add(text);
		Note($"Measured {text}");
		return true;
	}

	public string NextHint(Project project)
	{
		object objectValue = RuntimeHelpers.GetObjectValue(FindHidden(project));
		if (objectValue == null)
		{
			return "There is no hidden fault in this circuit.";
		}
		checked
		{
			HintsUsed++;
			string text;
			switch (HintsUsed)
			{
			case 1:
				text = AreaHint(project, RuntimeHelpers.GetObjectValue(objectValue));
				break;
			case 2:
				text = NeighbourHint(project, RuntimeHelpers.GetObjectValue(objectValue));
				break;
			default:
				text = $"The fault is in {NameOf_(RuntimeHelpers.GetObjectValue(objectValue))}: {DescribeFault(RuntimeHelpers.GetObjectValue(objectValue), FaultOf(RuntimeHelpers.GetObjectValue(objectValue))).ToLowerInvariant()}.";
				HintsUsed = Math.Min(HintsUsed, 3);
				break;
			}
			Note($"Hint {HintsUsed}: {text}");
			return text;
		}
	}

	private static string AreaHint(Project project, object target)
	{
		int num = 0;
		float x;
		PortKind portKind;
		if (target is CircuitElement)
		{
			CircuitElement circuitElement = (CircuitElement)target;
			num = project.Pages.FindIndex([SpecialName] (ProjectPage p) => p.Circuit.Elements.Contains(circuitElement));
			RectangleF rectangleF = circuitElement.WorldBounds();
			x = rectangleF.X + rectangleF.Width / 2f;
			portKind = ((circuitElement.Ports.Count != 0) ? circuitElement.Ports[0].Kind : PortKind.Pneumatic);
		}
		else
		{
			Tube tube = (Tube)target;
			num = project.Pages.FindIndex([SpecialName] (ProjectPage p) => p.Circuit.Tubes.Contains(tube));
			x = (tube.A.WorldPos().X + tube.B.WorldPos().X) / 2f;
			portKind = tube.A.Kind;
		}
		string arg = portKind switch
		{
			PortKind.Hydraulic => "the hydraulic circuit", 
			PortKind.Electric => "the electrical control circuit", 
			_ => "the pneumatic circuit", 
		};
		string arg2 = $"column {Project.ColumnAt(x)}" + ((project.Pages.Count > 1) ? $" of page {checked(num + 1)}" : "");
		return $"Look in {arg}, around {arg2}. Compare what should happen with what does happen there.";
	}

	private static string NeighbourHint(Project project, object target)
	{
		if (target is Tube)
		{
			Tube tube = (Tube)target;
			return string.Format("It is a {0} connected to {1}. Measure at both ends of each line there.", tube.IsElectric ? "wire" : "line", EndName(tube.A));
		}
		CircuitElement circuitElement = (CircuitElement)target;
		List<string> list = new List<string>();
		foreach (Port port in circuitElement.Ports)
		{
			foreach (CircuitElement item2 in TubeNeighbours(project, port))
			{
				string item = (string.IsNullOrWhiteSpace(item2.Label) ? item2.DisplayName : item2.Label);
				if (!list.Contains(item))
				{
					list.Add(item);
				}
			}
		}
		if (list.Count == 0)
		{
			return $"It is a {circuitElement.DisplayName.ToLowerInvariant()}.";
		}
		return string.Format("The faulty part is directly connected to {0}. Check the signals going in and out of it.", string.Join(", ", list));
	}

	private static List<CircuitElement> TubeNeighbours(Project project, Port start)
	{
		List<CircuitElement> list = new List<CircuitElement>();
		List<Tube> list2 = project.Pages.SelectMany([SpecialName] (ProjectPage pg) => pg.Circuit.Tubes).ToList();
		HashSet<Port> hashSet = new HashSet<Port> { start };
		Queue<Port> queue = new Queue<Port>();
		queue.Enqueue(start);
		while (queue.Count > 0)
		{
			Port port = queue.Dequeue();
			foreach (Tube item in list2)
			{
				Port port2 = ((item.A == port) ? item.B : ((item.B == port) ? item.A : null));
				if (port2 != null && hashSet.Add(port2))
				{
					if (port2.Owner is Junction || port2.Owner is PageConnector)
					{
						queue.Enqueue(port2);
					}
					else if (!list.Contains(port2.Owner))
					{
						list.Add(port2.Owner);
					}
				}
			}
		}
		return list;
	}

	public (bool Correct, string Message) Guess(Project project, object target, FaultKind kind)
	{
		object objectValue = RuntimeHelpers.GetObjectValue(FindHidden(project));
		checked
		{
			(bool, string) result;
			if (objectValue == null)
			{
				result = (false, "There is no hidden fault in this circuit.");
			}
			else if (target != objectValue)
			{
				WrongGuesses++;
				Note($"Wrong guess: {NameOf_(RuntimeHelpers.GetObjectValue(target))}");
				result = (false, $"No, {NameOf_(RuntimeHelpers.GetObjectValue(target))} is working correctly. Keep measuring.");
			}
			else
			{
				Solved = true;
				KindRight = kind == FaultOf(RuntimeHelpers.GetObjectValue(objectValue));
				Finished = DateTime.Now;
				Note($"Found it: {NameOf_(RuntimeHelpers.GetObjectValue(target))}");
				string arg = DescribeFault(RuntimeHelpers.GetObjectValue(objectValue), FaultOf(RuntimeHelpers.GetObjectValue(objectValue)));
				SetFault(RuntimeHelpers.GetObjectValue(objectValue), FaultOf(RuntimeHelpers.GetObjectValue(objectValue)), hidden: false);
				result = (true, KindRight ? $"Correct! {NameOf_(RuntimeHelpers.GetObjectValue(target))}: {arg}." : $"Right part! {NameOf_(RuntimeHelpers.GetObjectValue(target))} is faulty, but the fault is: {arg}.");
			}
			return result;
		}
	}

	public string GiveUp(Project project)
	{
		object objectValue = RuntimeHelpers.GetObjectValue(FindHidden(project));
		GaveUp = true;
		Finished = DateTime.Now;
		if (objectValue == null)
		{
			return "There is no hidden fault in this circuit.";
		}
		Note($"Gave up. The fault was in {NameOf_(RuntimeHelpers.GetObjectValue(objectValue))}");
		string result = $"The fault was in {NameOf_(RuntimeHelpers.GetObjectValue(objectValue))}: {DescribeFault(RuntimeHelpers.GetObjectValue(objectValue), FaultOf(RuntimeHelpers.GetObjectValue(objectValue)))}.";
		SetFault(RuntimeHelpers.GetObjectValue(objectValue), FaultOf(RuntimeHelpers.GetObjectValue(objectValue)), hidden: false);
		return result;
	}

	public int Score()
	{
		if (!Solved)
		{
			return 0;
		}
		int val = checked(100 - 15 * HintsUsed - 20 * WrongGuesses - 4 * Math.Max(0, Checks.Count - 4) - ((!KindRight) ? 10 : 0));
		return Math.Max(0, Math.Min(100, val));
	}

	public string StatusText()
	{
		DateTime? finished;
		DateTime? dateTime = (finished = Finished);
		TimeSpan timeSpan = (dateTime.HasValue ? finished.GetValueOrDefault() : DateTime.Now) - Started;
		string text = (Solved ? $"Solved in {timeSpan.TotalMinutes:0}:{timeSpan.Seconds:00} min — result {Score()} / 100" : ((!GaveUp) ? $"Find the fault!  Time {timeSpan.TotalMinutes:0}:{timeSpan.Seconds:00}" : "Task ended (fault shown)"));
		return $"{text}    Checks: {Checks.Count}   Hints: {HintsUsed}   Wrong guesses: {WrongGuesses}";
	}

	public string Report(string projectTitle)
	{
		StringBuilder stringBuilder = new StringBuilder();
		stringBuilder.AppendLine($"PneuSim troubleshooting report — {projectTitle}");
		stringBuilder.AppendLine($"Date: {Started:yyyy-MM-dd HH:mm}");
		stringBuilder.AppendLine(StatusText());
		stringBuilder.AppendLine();
		stringBuilder.AppendLine("Diagnosis log:");
		foreach (string item in Log)
		{
			stringBuilder.AppendLine("  " + item);
		}
		return stringBuilder.ToString();
	}
}
