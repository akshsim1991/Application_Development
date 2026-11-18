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

	private void OnCircuitModified()
	{
		checked
		{
			if (_historyIndex < _history.Count - 1)
			{
				_history.RemoveRange(_historyIndex + 1, _history.Count - _historyIndex - 1);
			}
			_history.Add((_project.ToXml(), _pageIndex));
			if (_history.Count > 100)
			{
				_history.RemoveAt(0);
			}
			_historyIndex = _history.Count - 1;
			_project.UpdateCrossReferences();
			MarkDirty();
			UpdateUndoButtons();
			ScheduleCheck();
		}
	}

	private void RestoreSnapshot()
	{
		(string, int) tuple = _history[_historyIndex];
		bool flag = tuple.Item2 == _pageIndex;
		Point autoScrollPosition = _canvas.AutoScrollPosition;
		_project = Project.FromXml(tuple.Item1);
		checked
		{
			_pageIndex = Math.Min(tuple.Item2, _project.Pages.Count - 1);
			RefreshPageTabs();
			ShowPage(_pageIndex, flag);
			if (flag)
			{
				_canvas.AutoScrollPosition = new Point(-autoScrollPosition.X, -autoScrollPosition.Y);
			}
			MarkDirty();
			UpdateUndoButtons();
			ScheduleCheck();
		}
	}

	private static string SafeName(string s)
	{
		char[] invalidFileNameChars = Path.GetInvalidFileNameChars();
		string text = new string((s ?? "circuit").Select([SpecialName] (char c) => (!invalidFileNameChars.Contains(c)) ? c : '_').ToArray()).Trim();
		if (Operators.CompareString(text, "", TextCompare: false) != 0)
		{
			return text;
		}
		return "circuit";
	}

	private string AskPath(string filter, string ext, string name)
	{
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = filter,
			DefaultExt = ext,
			FileName = SafeName(name) + "." + ext
		};
		return (saveFileDialog.ShowDialog(this) == DialogResult.OK) ? saveFileDialog.FileName : null;
	}

	public static Bitmap RenderCircuitImage(Circuit c, bool simulating)
	{
		RectangleF rectangleF = c.Bounds();
		rectangleF.Inflate(30f, 30f);
		rectangleF = RectangleF.FromLTRB(rectangleF.Left, rectangleF.Top - 30f, rectangleF.Right, rectangleF.Bottom);
		Bitmap bitmap = checked(new Bitmap(Math.Max(1, (int)Math.Round(rectangleF.Width)), Math.Max(1, (int)Math.Round(rectangleF.Height))));
		using Graphics graphics = Graphics.FromImage(bitmap);
		graphics.Clear(Color.White);
		graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
		graphics.TranslateTransform(0f - rectangleF.Left, 0f - rectangleF.Top);
		using CircuitCanvas circuitCanvas = new CircuitCanvas
		{
			Circuit = c,
			Simulating = simulating,
			ShowColumns = true
		};
		circuitCanvas.PaintTo(graphics);
		return bitmap;
	}

	protected override void OnFormClosing(FormClosingEventArgs e)
	{
		if (!ConfirmDiscard())
		{
			e.Cancel = true;
		}
		if (!e.Cancel)
		{
			_sounds.Stop();
			DeleteAutosave();
		}
		base.OnFormClosing(e);
	}

	public void OfferRecovery()
	{
		if (!File.Exists(AutosavePath))
		{
			return;
		}
		if (MessageBox.Show("PneuSim was not closed normally last time. Restore the unsaved work from " + $"{File.GetLastWriteTime(AutosavePath):dd-MM-yyyy HH:mm}?", "PneuSim", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
		{
			try
			{
				string text = (File.Exists(AutosavePath + ".source") ? File.ReadAllText(AutosavePath + ".source").Trim() : "");
				NewProject(Project.Load(AutosavePath), (Operators.CompareString(text, "", TextCompare: false) == 0) ? null : text);
				_dirty = true;
				UpdateTitle();
				_statusMessage.Text = "Recovered unsaved work. Save it to keep it.";
				return;
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				MessageBox.Show("The recovery file could not be read: " + ex2.Message, "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
				ProjectData.ClearProjectError();
			}
		}
		DeleteAutosave();
	}

	public void RecoverFromError()
	{
		try
		{
			StopSimulation();
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			_running = false;
			_timer.Stop();
			ProjectData.ClearProjectError();
		}
		_dirty = true;
		Autosave();
	}

	private void OnPropertyValueChanged(object s, PropertyValueChangedEventArgs e)
	{
		CurrentCircuit.CleanupTubes();
		_properties.Refresh();
		_canvas.Invalidate();
		OnCircuitModified();
	}

	private void OnToggleReal(object sender, EventArgs e)
	{
		_realPhysics = !_realPhysics;
		AppSettings.RealPhysics = _realPhysics;
		AppSettings.Save();
		if (_running)
		{
			StopSimulation();
			OnStart(null, EventArgs.Empty);
		}
		_statusMessage.Text = (_realPhysics ? "Realistic physics: pressures build up, cylinders move according to bore, load and friction, air consumption is measured." : "Ideal mode: pressures switch instantly and cylinders move at their set stroke time.");
		UpdateUiState();
	}

	private void OnTick(object sender, EventArgs e)
	{
		try
		{
			SimulationTick();
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			StopSimulation();
			_statusMessage.Text = "The simulation stopped because of an error: " + ex2.Message;
			_statusMessage.ForeColor = Color.DarkRed;
			ProjectData.ClearProjectError();
		}
	}

	private void OnElementOperated(object sender, EventArgs e)
	{
		if (_running)
		{
			_simulator.RunLogic();
			if (_paused)
			{
				_recorder?.Record(force: true);
				UpdateReplayUi();
			}
			_inspector.RefreshValues();
			ShowSimulationStatus();
			_canvas.Invalidate();
		}
	}

	private void OnToggleRecord(object sender, EventArgs e)
	{
		if (_gif != null)
		{
			FinishRecording();
		}
		else if (CurrentCircuit.Elements.Count != 0)
		{
			_gifPage = CurrentCircuit;
			_gifBounds = _gifPage.Bounds();
			_gifBounds.Inflate(30f, 30f);
			float num = Math.Min(1f, 900f / Math.Max(1f, _gifBounds.Width));
			_gif = checked(new GifRecorder((int)Math.Round(_gifBounds.Width * num), (int)Math.Round(_gifBounds.Height * num), 6));
			_statusMessage.Text = "Recording: start the simulation and operate the circuit. Click 'Record GIF' again to save.";
			if (!_running)
			{
				OnStart(null, EventArgs.Empty);
			}
			UpdateUiState();
		}
	}

	private void ApplyFix(Func<Circuit> fix)
	{
		if (fix != null && !_running && MessageBox.Show("Replace the circuit on this page with a corrected design? (You can undo this.)", "PneuSim", MessageBoxButtons.OKCancel, MessageBoxIcon.Question) == DialogResult.OK)
		{
			_project.Pages[_pageIndex].Circuit = fix();
			ShowPage(_pageIndex);
			FitView();
			OnCircuitModified();
			_statusMessage.Text = "Circuit redesigned without signal overlap. Press Start (F9) to try it.";
		}
	}

	private void FillOperateList()
	{
		CircuitElement circuitElement = _explainOperate.SelectedItem as CircuitElement;
		_explainOperate.Items.Clear();
		foreach (CircuitElement item in CircuitAnalysis.ManualElements(_project.SimulationCircuit()))
		{
			_explainOperate.Items.Add(item);
		}
		if (circuitElement != null && _explainOperate.Items.Contains(circuitElement))
		{
			_explainOperate.SelectedItem = circuitElement;
		}
		else if (_explainOperate.Items.Count > 0)
		{
			_explainOperate.SelectedIndex = 0;
		}
	}

	private void OnTroubleCommand(object sender, string cmd)
	{
		switch (cmd)
		{
		case "start":
			StartExercise();
			goto default;
		case "hint":
			if (_exercise != null && !_exercise.IsOver)
			{
				_trouble.AddMessage("Hint: " + _exercise.NextHint(_project));
				goto default;
			}
			break;
		case "found":
			if (_exercise != null && !_exercise.IsOver)
			{
				_canvas.PickingTarget = true;
				_canvas.Cursor = Cursors.Hand;
				_statusMessage.Text = "Click the component or tube you think is faulty (Esc to cancel).";
				ShowBottomTab("Fault finding");
				goto default;
			}
			break;
		case "giveup":
			if (_exercise != null && !_exercise.IsOver)
			{
				_trouble.AddMessage(_exercise.GiveUp(_project));
				MarkDirty();
				goto default;
			}
			break;
		case "report":
			SaveExerciseReport();
			goto default;
		case "add":
			AddFaultToSelection();
			goto default;
		case "clear":
			if (_project.Pages.Sum([SpecialName] (ProjectPage p) => p.Circuit.FaultCount()) == 0)
			{
				_statusMessage.Text = "There are no faults in the project.";
				break;
			}
			TroubleshootExercise.RemoveAllFaults(_project);
			_exercise = null;
			_trouble.AddMessage("All faults repaired.");
			AfterFaultChange();
			goto default;
		default:
			_trouble.ShowExercise(_exercise);
			_canvas.Invalidate();
			break;
		}
	}

	private void AddReplayButtons(ToolStrip bar)
	{
		_btnBack = Button("Step back", Icons.StepIcon(forward: false, toEvent: false), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D0();
		}, iconOnly: true);
		_btnStepFwd = Button("Step forward", Icons.StepIcon(forward: true, toEvent: false), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D1();
		}, iconOnly: true);
		_btnPrevEvent = Button("Back to the previous event", Icons.StepIcon(forward: false, toEvent: true), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D2();
		}, iconOnly: true);
		_btnNextEvent = Button("Forward to the next event (a valve, relay or cylinder changes)", Icons.StepIcon(forward: true, toEvent: true), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D3();
		}, iconOnly: true);
		_timeline = new TrackBar
		{
			Minimum = 0,
			Maximum = 0,
			TickStyle = TickStyle.None,
			AutoSize = false,
			Height = 22,
			Width = 150
		};
		_timelineHost = new ToolStripControlHost(_timeline)
		{
			ToolTipText = "Replay: drag to go back in time"
		};
		_timeline.Scroll += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D4();
		};
		bar.Items.AddRange(new ToolStripItem[5] { _btnPrevEvent, _btnBack, _btnStepFwd, _btnNextEvent, _timelineHost });
	}

	private void AdvanceSimulation(double seconds)
	{
		double num = 0.005;
		checked
		{
			int num2 = Math.Max(1, (int)Math.Round(seconds / num));
			for (int i = 1; i <= num2; i++)
			{
				_simulator.Step(num);
				_recorder.Record();
			}
		}
	}

	private void AfterReplayMove()
	{
		_canvas.Invalidate();
		_diagram.Invalidate();
		_plotter.RefreshPlot();
		_inspector.RefreshValues();
		UpdateUiState();
		UpdateReplayUi();
	}

	protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
	{
		if ((keyData & Keys.Control) == Keys.Control && (keyData & Keys.Alt) != Keys.Alt && !IsEditingText())
		{
			bool flag = (keyData & Keys.Shift) == Keys.Shift;
			switch (keyData & Keys.KeyCode)
			{
			case Keys.Z:
				if (flag)
				{
					Redo();
				}
				else
				{
					Undo();
				}
				return true;
			case Keys.Y:
				Redo();
				return true;
			case Keys.X:
				_canvas.CutSelection();
				return true;
			case Keys.C:
				_canvas.CopySelection();
				return true;
			case Keys.V:
				_canvas.Paste();
				return true;
			case Keys.D:
				_canvas.DuplicateSelection();
				return true;
			case Keys.A:
				_canvas.SelectAll();
				return true;
			}
		}
		return base.ProcessCmdKey(ref msg, keyData);
	}

	private static Control FocusedLeaf(Control c)
	{
		foreach (Control control in c.Controls)
		{
			if (control.ContainsFocus)
			{
				return FocusedLeaf(control);
			}
		}
		return c;
	}

	private void FitView()
	{
		_canvas.ZoomToFit();
		UpdateUiState();
	}
}
