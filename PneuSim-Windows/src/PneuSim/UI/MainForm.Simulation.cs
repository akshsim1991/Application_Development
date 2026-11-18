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

	private void OnStart(object sender, EventArgs e)
	{
		if (_running && !_paused)
		{
			return;
		}
		if (!_running)
		{
			if (_project.AllElements().Count() == 0)
			{
				_statusMessage.Text = "Place some components first, or load one of the examples.";
				return;
			}
			_running = true;
			_canvas.PlacingPreset = null;
			_canvas.ClearSelection();
			_project.UpdateCrossReferences();
			_simulator = new Simulator(_project.SimulationCircuit())
			{
				RealPhysics = _realPhysics
			};
			_canvas.Simulator = _simulator;
			_canvas.Simulating = true;
			_simulator.Reset();
			RefreshGrafcet();
			_grafcetTracker = new GrafcetTracker(_grafcetModel);
			_diagram.Markers = _grafcetTracker.Markers;
			_auto = ((AppSettings.AutoMode == "Off") ? null : new AutoOperator(_simulator.Circuit, AppSettings.AutoMode));
			_autoStatusShown = null;
			_recorder = new SimulationRecorder(_simulator);
			_recorder.Record(force: true);
			_diagram.Simulator = _simulator;
			_plotter.Simulator = _simulator;
			_statusMessage.Text = "Click push buttons and selector switches to operate the valves.";
		}
		_recorder?.TruncateAfterCurrent();
		_paused = false;
		_timer.Start();
		UpdateUiState();
		_canvas.Invalidate();
	}

	private void OnPause(object sender, EventArgs e)
	{
		if (_running)
		{
			_paused = !_paused;
			if (_paused)
			{
				_timer.Stop();
			}
			else
			{
				_timer.Start();
			}
			UpdateUiState();
		}
	}

	private void OnStop(object sender, EventArgs e)
	{
		StopSimulation();
	}

	private void StopSimulation()
	{
		_timer.Stop();
		if (_gif != null)
		{
			FinishRecording();
		}
		if (!_running)
		{
			return;
		}
		_running = false;
		_paused = false;
		_sounds.Stop();
		_auto?.Stop();
		_auto = null;
		_grafcetTracker = null;
		_grafcet.Active = -1;
		_canvas.AlarmElements = null;
		foreach (CircuitElement item in _project.AllElements())
		{
			item.ResetSim();
		}
		_canvas.Simulating = false;
		_canvas.PickingTarget = false;
		_recorder = null;
		ClearInspector();
		_canvas.Invalidate();
		_diagram.Invalidate();
		_plotter.RefreshPlot();
		_statusMessage.Text = "";
		_statusMessage.ForeColor = SystemColors.ControlText;
		UpdateUiState();
	}

	/// <summary>Builds the GRAFCET of the circuit (the running one while simulating) and shows it.</summary>
	private void RefreshGrafcet()
	{
		Circuit circuit = ((_running && _simulator != null) ? _simulator.Circuit : _project.SimulationCircuit());
		_grafcetModel = GrafcetBuilder.Build(circuit);
		_grafcet.Model = _grafcetModel;
	}

	private AutoOperator _auto;

	private string _autoStatusShown;

	private void SimulationTick()
	{
		double num = (new double[6] { 0.1, 0.25, 0.5, 1.0, 2.0, 4.0 })[Math.Max(0, _speedBox.SelectedIndex)];
		double dt = 0.02 * num / 4.0;
		int num2 = 1;
		checked
		{
			do
			{
				_simulator.Step(dt);
				num2++;
			}
			while (num2 <= 4);
			_recorder?.Record();
			if (_grafcetTracker != null && _grafcetTracker.Update(_simulator.Time))
			{
				_grafcet.Active = _grafcetTracker.Active;
			}
			if (_auto != null)
			{
				if (_auto.Update(_simulator.Time))
				{
					_simulator.RunLogic();
				}
				if (_auto.Status != null && _auto.Status != _autoStatusShown && !_alarmShown)
				{
					_autoStatusShown = _auto.Status;
					_statusMessage.Text = _auto.Status;
				}
			}
			_tickCount++;
			UpdateSoundsAndAlarm();
			ShowSimulationStatus();
			_canvas.Invalidate();
			_diagram.Invalidate();
		}
		if (_tickCount % 5 == 0)
		{
			_plotter.RefreshPlot();
			_inspector.RefreshValues();
			RefreshHoverTip();
			UpdateReplayUi();
		}
		if (_tickCount % 25 == 0 && _exercise != null)
		{
			_trouble.ShowExercise(_exercise);
		}
		if (_gif != null && _tickCount % 3 == 0)
		{
			CaptureFrame();
		}
	}

	private void UpdateSoundsAndAlarm()
	{
		_sounds.Update(_simulator.Circuit);
		string text = _sounds.WarningText();
		bool flag = text != null;
		_canvas.AlarmElements = (flag ? _sounds.OverRated.Select((KeyValuePair<CircuitElement, double> o) => o.Key).ToList() : null);
		_alarmShown = flag;
		if (flag)
		{
			_statusMessage.Text = text;
			_statusMessage.ForeColor = ((_tickCount / 12 % 2 == 0) ? Color.Firebrick : (AppSettings.DarkMode ? Color.Gainsboro : SystemColors.ControlText));
		}
		else if (_alarmWasShown)
		{
			_statusMessage.ForeColor = (AppSettings.DarkMode ? Color.Gainsboro : SystemColors.ControlText);
		}
		_alarmWasShown = flag;
	}

	private void ShowSimulationStatus()
	{
		_statusMode.Text = string.Format("Simulating{0}   t = {1:0.00} s{2}", _realPhysics ? " (realistic)" : "", _simulator.Time, _paused ? "  (paused)" : "") + ((_gif != null) ? $"   ● REC {_gif.FrameCount}" : "");
		if (_simulator.Warnings.Count > 0)
		{
			_statusMessage.Text = "Warning: " + _simulator.Warnings[0];
			_statusMessage.ForeColor = Color.DarkRed;
			return;
		}
		if (_statusMessage.ForeColor == Color.DarkRed)
		{
			_statusMessage.ForeColor = (AppSettings.DarkMode ? Color.Gainsboro : SystemColors.ControlText);
		}
		string text = _simulator.CostSummary(_project.Info);
		if (Operators.CompareString(text, "", TextCompare: false) != 0)
		{
			_statusMessage.Text = text;
		}
	}

	private void CaptureFrame()
	{
		using Bitmap bitmap = checked(new Bitmap((int)Math.Round(_gifBounds.Width), (int)Math.Round(_gifBounds.Height)));
		using (Graphics graphics = Graphics.FromImage(bitmap))
		{
			graphics.Clear(Color.White);
			graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
			graphics.TranslateTransform(0f - _gifBounds.Left, 0f - _gifBounds.Top);
			_gifCanvas.Circuit = _gifPage;
			_gifCanvas.Simulating = true;
			_gifCanvas.PaintTo(graphics);
		}
		if (!_gif.AddFrame(bitmap))
		{
			FinishRecording();
		}
	}

	private void FinishRecording()
	{
		GifRecorder gif = _gif;
		_gif = null;
		UpdateUiState();
		if (gif == null || gif.FrameCount == 0)
		{
			return;
		}
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "Animated GIF (*.gif)|*.gif",
			FileName = SafeName(_project.Info.Title) + ".gif"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			gif.Save(saveFileDialog.FileName);
			_statusMessage.Text = $"Saved {gif.FrameCount} frames to {saveFileDialog.FileName}";
		}
	}

	private void StartExercise()
	{
		if (_running)
		{
			StopSimulation();
		}
		TroubleshootExercise troubleshootExercise = TroubleshootExercise.StartRandom(_project, _rnd);
		if (troubleshootExercise == null)
		{
			_trouble.AddMessage("This circuit has nothing that can fail. Load an example or draw a circuit first.");
			return;
		}
		_exercise = troubleshootExercise;
		AfterFaultChange();
		_trouble.AddMessage("A fault is now hidden somewhere in the circuit. Start the simulation, operate the circuit and right-click components or tubes to measure them. When you know where the fault is, click 'I found it…'.");
		ShowBottomTab("Fault finding");
		_trouble.ShowExercise(_exercise);
	}

	private void PauseForReplay()
	{
		if (_running && !_paused)
		{
			_paused = true;
			_timer.Stop();
		}
	}

	private void ReplayStep(int direction)
	{
		checked
		{
			if (_running && _recorder != null)
			{
				PauseForReplay();
				if (direction < 0)
				{
					_recorder.Restore(_recorder.Index - 1);
				}
				else if (_recorder.IsRewound)
				{
					_recorder.Restore(_recorder.Index + 1);
				}
				else
				{
					AdvanceSimulation(0.05);
				}
				AfterReplayMove();
			}
		}
	}

	private void ReplayEvent(int direction)
	{
		if (!_running || _recorder == null)
		{
			return;
		}
		PauseForReplay();
		if (direction < 0)
		{
			int num = _recorder.PreviousEventFrame();
			_recorder.Restore((num >= 0) ? num : 0);
		}
		else
		{
			int num2 = _recorder.NextEventFrame();
			if (num2 >= 0)
			{
				_recorder.Restore(num2);
			}
			else
			{
				if (_recorder.IsRewound)
				{
					_recorder.Restore(checked(_recorder.Count - 1));
				}
				string right = _simulator.DiscreteState();
				double num3 = _simulator.Time + 30.0;
				do
				{
					AdvanceSimulation(0.05);
				}
				while (Operators.CompareString(_simulator.DiscreteState(), right, TextCompare: false) == 0 && _simulator.Time < num3);
				if (Operators.CompareString(_simulator.DiscreteState(), right, TextCompare: false) == 0)
				{
					_statusMessage.Text = "Nothing switched in the next 30 s.";
				}
			}
		}
		AfterReplayMove();
	}

	private void UpdateReplayUi()
	{
		checked
		{
			if (_timeline != null)
			{
				bool flag = _running && _recorder != null && _recorder.Count > 0;
				ToolStripButton[] array = new ToolStripButton[4] { _btnBack, _btnStepFwd, _btnPrevEvent, _btnNextEvent };
				for (int i = 0; i < array.Length; i++)
				{
					array[i].Enabled = _running;
				}
				_timelineHost.Enabled = flag;
				_updatingTimeline = true;
				if (flag)
				{
					_timeline.Maximum = Math.Max(0, _recorder.Count - 1);
					_timeline.Value = Math.Max(0, Math.Min(_timeline.Maximum, _recorder.Index));
				}
				else
				{
					_timeline.Maximum = 0;
				}
				_updatingTimeline = false;
			}
		}
	}

	private void UpdateUiState()
	{
		_btnStart.Enabled = !_running || _paused;
		_menuStart.Enabled = _btnStart.Enabled;
		_btnPause.Enabled = _running;
		_menuPause.Enabled = _running;
		_btnPause.Checked = _paused;
		_btnStop.Enabled = _running;
		_menuStop.Enabled = _running;
		_btnReal.Checked = _realPhysics;
		_menuReal.Checked = _realPhysics;
		_btnRecord.Checked = _gif != null;
		_menuRecord.Checked = _gif != null;
		_menuDark.Checked = AppSettings.DarkMode;
		_library.Enabled = !_running;
		_properties.Enabled = !_running;
		if (_running)
		{
			ShowSimulationStatus();
		}
		else
		{
			_statusMode.Text = "Edit mode" + (_realPhysics ? "  (realistic physics on)" : "");
		}
		_statusZoom.Text = $"Zoom {_canvas.Zoom * 100f:0}%";
		UpdateUndoButtons();
		UpdateReplayUi();
	}
}
