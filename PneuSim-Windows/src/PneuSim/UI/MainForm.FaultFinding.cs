using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;
using Microsoft.VisualBasic;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public partial class MainForm
{

	private void InitTroubleshooting()
	{
		_canvas.InspectRequested += OnInspect;
		_canvas.TargetPicked += OnTargetPicked;
		_inspector.SettingChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__117_002D0();
		};
		_inspector.SettingCommitted += [SpecialName] (object a0, EventArgs a1) =>
		{
			OnCircuitModified();
		};
		_trouble.Command += OnTroubleCommand;
	}

	private ToolStripMenuItem BuildTroubleshootMenu()
	{
		return new ToolStripMenuItem("&Fault finding")
		{
			DropDownItems = 
			{
				(ToolStripItem)Item("&New fault-finding task (hidden fault)", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D0();
				}),
				(ToolStripItem)Item("&Hint", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D1();
				}, Keys.H | Keys.Control),
				(ToolStripItem)Item("I &found the fault…", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D2();
				}),
				(ToolStripItem)Item("&Show the fault", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D3();
				}),
				(ToolStripItem)new ToolStripSeparator(),
				(ToolStripItem)Item("&Put a fault into the selection…", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D4();
				}),
				(ToolStripItem)Item("&Repair all faults", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D5();
				})
			}
		};
	}

	private void OnInspect(object sender, object target)
	{
		_inspector.Target = RuntimeHelpers.GetObjectValue(target);
		if (_exercise != null && !_exercise.IsOver && _exercise.RecordCheck(RuntimeHelpers.GetObjectValue(target)))
		{
			_trouble.ShowExercise(_exercise);
			_statusMessage.Text = $"Check {_exercise.Checks.Count}: {TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(target))} (see the Inspector tab).";
		}
		_inspector.Note = ((_exercise != null && !_exercise.IsOver) ? $"Fault finding: check {_exercise.Checks.Count}, {_exercise.HintsUsed} hints" : "");
		ShowBottomTab("Inspector");
	}

	private void ClearInspector()
	{
		_inspector.Target = null;
		_canvas.InspectedObject = null;
	}

	private void CheckForHiddenFault()
	{
		if (TroubleshootExercise.FindHidden(_project) == null)
		{
			_exercise = null;
		}
		else
		{
			_exercise = new TroubleshootExercise();
			_trouble.AddMessage("This circuit contains a hidden fault. Find it! Run the simulation, measure with right-clicks, then click 'I found it…'.");
			ShowBottomTab("Fault finding");
		}
		_trouble.ShowExercise(_exercise);
	}

	private void OnTargetPicked(object sender, object target)
	{
		if (_exercise == null || _exercise.IsOver)
		{
			return;
		}
		using (FaultDialog faultDialog = new FaultDialog(RuntimeHelpers.GetObjectValue(target), diagnosis: true))
		{
			if (faultDialog.ShowDialog(this) != DialogResult.OK)
			{
				_statusMessage.Text = "";
				return;
			}
			(bool, string) tuple = _exercise.Guess(_project, RuntimeHelpers.GetObjectValue(target), faultDialog.SelectedKind);
			_trouble.AddMessage(tuple.Item2);
			if (tuple.Item1)
			{
				_trouble.AddMessage($"Result: {_exercise.Score()} / 100 ({_exercise.Checks.Count} checks, {_exercise.HintsUsed} hints, {_exercise.WrongGuesses} wrong guesses). " + "Use 'Repair all' to fix the circuit, or start a new task.");
				MarkDirty();
			}
			_statusMessage.Text = tuple.Item2;
		}
		_trouble.ShowExercise(_exercise);
		_canvas.Invalidate();
	}

	private void AddFaultToSelection()
	{
		_Closure_0024__125_002D0 arg = default(_Closure_0024__125_002D0);
		_Closure_0024__125_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__125_002D0(arg);
		if (_running)
		{
			_statusMessage.Text = "Stop the simulation first (F11) to put in a fault.";
			return;
		}
		CS_0024_003C_003E8__locals0._0024VB_0024Local_target = RuntimeHelpers.GetObjectValue(((object)_canvas.SelectedElement) ?? ((object)_canvas.SelectedTube));
		if (CS_0024_003C_003E8__locals0._0024VB_0024Local_target == null)
		{
			_statusMessage.Text = "Select a component or a tube first, then click 'Add fault…'.";
			ShowBottomTab("Fault finding");
			return;
		}
		if (TroubleshootExercise.PossibleFaultsOf(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target)).Length == 0)
		{
			_statusMessage.Text = $"{TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target))} cannot have a fault.";
			return;
		}
		using (FaultDialog faultDialog = new FaultDialog(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target), diagnosis: false))
		{
			if (faultDialog.ShowDialog(this) != DialogResult.OK)
			{
				return;
			}
			TroubleshootExercise.SetFault(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target), faultDialog.SelectedKind, faultDialog.Hidden);
			if (faultDialog.Hidden && faultDialog.SelectedKind != 0)
			{
				foreach (ProjectPage page in _project.Pages)
				{
					foreach (CircuitElement item in page.Circuit.Elements.Where([SpecialName] (CircuitElement x) => x != CS_0024_003C_003E8__locals0._0024VB_0024Local_target))
					{
						item.FaultHidden = false;
					}
					foreach (Tube item2 in page.Circuit.Tubes.Where([SpecialName] (Tube x) => x != CS_0024_003C_003E8__locals0._0024VB_0024Local_target))
					{
						item2.FaultHidden = false;
					}
				}
				_trouble.AddMessage($"Hidden fault put into {TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target))}. Save the file and give it to a colleague: it opens as a fault-finding task.");
			}
			else if (faultDialog.SelectedKind != 0)
			{
				_trouble.AddMessage($"{TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target))}: {TroubleshootExercise.DescribeFault(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target), faultDialog.SelectedKind)}. Run the simulation to see what it does.");
			}
		}
		AfterFaultChange();
	}

	private void AfterFaultChange()
	{
		_canvas.Invalidate();
		OnCircuitModified();
	}
}
