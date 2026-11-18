using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace PneuSim;

/// <summary>One step of a GRAFCET (sequential function chart): a number, an action and the movement it stands for.</summary>
public sealed class GrafcetStep
{
	public int Number;

	public bool Initial;

	public string Action = "";

	public CircuitMove Move;
}

/// <summary>A GRAFCET generated from a circuit: S0 (initial) then one step per cylinder movement, with the transition conditions between them.</summary>
public sealed class GrafcetModel
{
	public List<GrafcetStep> Steps = new List<GrafcetStep>();

	/// <summary>Condition of the transition that leaves step k (the last one returns to step 0).</summary>
	public List<string> Conditions = new List<string>();

	public string Note = "";

	public bool IsEmpty => Steps.Count < 2;

	/// <summary>Plain-text outline, for copying into a report.</summary>
	public string ToText()
	{
		StringBuilder stringBuilder = new StringBuilder();
		for (int i = 0; i < Steps.Count; i++)
		{
			GrafcetStep grafcetStep = Steps[i];
			stringBuilder.AppendLine("[S" + grafcetStep.Number + (grafcetStep.Initial ? "]  initial step: " : "]  ") + grafcetStep.Action);
			stringBuilder.AppendLine("   --|  " + ((i < Conditions.Count) ? Conditions[i] : "") + ((i == Steps.Count - 1) ? "   (back to S0)" : ""));
		}
		return stringBuilder.ToString();
	}
}

/// <summary>Builds the GRAFCET of a circuit from the movement sequence found by the circuit analysis (A+ B+ B- A- ...).</summary>
public static class GrafcetBuilder
{
	public static GrafcetModel Build(Circuit circuit)
	{
		GrafcetModel grafcetModel = new GrafcetModel();
		List<CircuitMove> list;
		try
		{
			List<CircuitMove> moves = CircuitAnalysis.FindMoves(circuit);
			list = CircuitAnalysis.InferSequence(moves);
			if (list == null || list.Count == 0)
			{
				list = moves.OrderBy((CircuitMove m) => m.Letter).ThenByDescending((CircuitMove m) => m.Extend).ToList();
				grafcetModel.Note = "No fixed sequence was found, so the movements are listed in cylinder order.";
			}
		}
		catch (Exception)
		{
			grafcetModel.Note = "The circuit could not be analysed.";
			return grafcetModel;
		}
		if (list.Count == 0)
		{
			grafcetModel.Note = "No cylinder movements were found. Connect valves to the cylinders first.";
			return grafcetModel;
		}
		grafcetModel.Steps.Add(new GrafcetStep
		{
			Number = 0,
			Initial = true,
			Action = "Initial position: all cylinders retracted"
		});
		for (int i = 0; i < list.Count; i++)
		{
			CircuitMove circuitMove = list[i];
			string text = (string.IsNullOrWhiteSpace(circuitMove.Cylinder.Label) ? ("cylinder " + circuitMove.Letter) : circuitMove.Cylinder.Label);
			grafcetModel.Steps.Add(new GrafcetStep
			{
				Number = i + 1,
				Action = circuitMove.Letter + (circuitMove.Extend ? "+" : "-") + "   (" + text + (circuitMove.Extend ? " extends)" : " retracts)"),
				Move = circuitMove
			});
		}
		string description = list[0].Trigger.Description;
		grafcetModel.Conditions.Add(string.IsNullOrWhiteSpace(description) ? "start" : description);
		for (int j = 0; j < list.Count; j++)
		{
			string endMark = list[j].EndMark;
			grafcetModel.Conditions.Add(string.IsNullOrWhiteSpace(endMark) ? (list[j].Letter + (list[j].Extend ? " extended" : " retracted")) : endMark);
		}
		return grafcetModel;
	}
}

/// <summary>Follows a running simulation and says which GRAFCET step is active; records when each step started (shown in the displacement-step diagram).</summary>
public sealed class GrafcetTracker
{
	private readonly GrafcetModel _model;

	public int Active { get; private set; }

	/// <summary>Times (simulation seconds) at which a step became active, with the step label.</summary>
	public List<(double Time, string Label)> Markers { get; } = new List<(double, string)>();

	public GrafcetTracker(GrafcetModel model)
	{
		_model = model;
	}

	private static bool Reached(CircuitMove m)
	{
		return m.Extend ? (m.Cylinder.Position > 0.97) : (m.Cylinder.Position < 0.03);
	}

	private static bool Started(CircuitMove m)
	{
		return m.Extend ? (m.Cylinder.Position > 0.03) : (m.Cylinder.Position < 0.97);
	}

	/// <summary>Updates the active step. Returns true if it changed.</summary>
	public bool Update(double time)
	{
		if (_model == null || _model.IsEmpty)
		{
			return false;
		}
		int active = Active;
		int num = _model.Steps.Count - 1;
		if (Active == 0)
		{
			if (Started(_model.Steps[1].Move))
			{
				Active = 1;
			}
		}
		else if (Reached(_model.Steps[Active].Move))
		{
			Active = ((Active >= num) ? 0 : (Active + 1));
		}
		if (Active != active)
		{
			Markers.Add((time, "S" + Active));
			if (Markers.Count > 60)
			{
				Markers.RemoveAt(0);
			}
			return true;
		}
		return false;
	}
}
