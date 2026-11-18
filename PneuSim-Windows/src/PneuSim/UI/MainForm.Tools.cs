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

	private void ScheduleCheck()
	{
		_checkTimer.Stop();
		_checkTimer.Start();
	}

	private void RunStaticChecks()
	{
		if (_project == null)
		{
			return;
		}
		try
		{
			List<CheckIssue> list = CircuitAnalysis.StaticChecks(_project.SimulationCircuit());
			if (list.Count == 0 && _project.AllElements().Any())
			{
				list.Add(new CheckIssue
				{
					Severity = IssueSeverity.Info,
					Message = "No problems found while drawing. Click 'Run full check' to test-run the circuit as well."
				});
			}
			ShowIssues(list);
			FillOperateList();
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ShowIssues(new List<CheckIssue>
			{
				new CheckIssue
				{
					Severity = IssueSeverity.Info,
					Message = "The checker could not analyse this circuit: " + ex2.Message
				}
			});
			ProjectData.ClearProjectError();
		}
	}

	private void OnFullCheck(object sender, EventArgs e)
	{
		Cursor = Cursors.WaitCursor;
		try
		{
			Circuit c = _project.SimulationCircuit();
			List<CheckIssue> list = CircuitAnalysis.StaticChecks(c);
			if (!list.Any([SpecialName] (CheckIssue i) => i.Severity == IssueSeverity.Error && i.Message.Contains("is empty")))
			{
				list.AddRange(CircuitAnalysis.DynamicCheck(c, _realPhysics));
			}
			if (list.Count == 0)
			{
				list.Add(new CheckIssue
				{
					Severity = IssueSeverity.Info,
					Message = "No problems found. The test run completed without warnings."
				});
			}
			ShowIssues(list);
			ShowBottomTab("Check circuit");
		}
		finally
		{
			Cursor = Cursors.Default;
		}
	}

	private void ShowIssues(List<CheckIssue> issues)
	{
		_issues = issues.OrderBy([SpecialName] (CheckIssue i) => i.Severity).ToList();
		_checkList.BeginUpdate();
		_checkList.Items.Clear();
		foreach (CheckIssue issue in _issues)
		{
			ListViewItem listViewItem = new ListViewItem(new string[2]
			{
				issue.Severity.ToString(),
				issue.Message
			})
			{
				Tag = issue
			};
			listViewItem.ForeColor = ((issue.Severity == IssueSeverity.Error) ? Color.Firebrick : ((issue.Severity == IssueSeverity.Warning) ? Color.DarkOrange : (AppSettings.DarkMode ? Color.Gainsboro : Color.DimGray)));
			_checkList.Items.Add(listViewItem);
		}
		_checkList.EndUpdate();
		int num = _issues.Where([SpecialName] (CheckIssue i) => i.Severity == IssueSeverity.Error).Count();
		foreach (TabPage tabPage in _bottomTabs.TabPages)
		{
			if (tabPage.Text.StartsWith("Check circuit"))
			{
				tabPage.Text = "Check circuit" + ((num > 0) ? $" ({num})" : "");
			}
		}
		OnIssueSelected(null, EventArgs.Empty);
	}

	private void OnIssueSelected(object sender, EventArgs e)
	{
		CheckIssue checkIssue = SelectedIssue();
		_fixCascade.Enabled = checkIssue != null && checkIssue.Fix != null && !_running;
		_fixElectro.Enabled = _fixCascade.Enabled;
		if (checkIssue != null && checkIssue.FixLabel != null)
		{
			_fixCascade.Text = "Fix: " + checkIssue.FixLabel.Replace("Redesign", "redesign");
		}
	}

	private void OnIssueActivated(object sender, EventArgs e)
	{
		CheckIssue checkIssue = SelectedIssue();
		if (checkIssue != null && checkIssue.Element != null && !_running)
		{
			int num = _project.Pages.FindIndex([SpecialName] (ProjectPage p) => p.Circuit.Elements.Contains(checkIssue.Element));
			if (num >= 0)
			{
				ShowPage(num);
				_canvas.SelectElements(new CircuitElement[1] { checkIssue.Element });
			}
		}
	}

	private void OnExplain(object sender, EventArgs e)
	{
		if (_explainOperate.SelectedItem == null)
		{
			FillOperateList();
		}
		Cursor = Cursors.WaitCursor;
		try
		{
			string text = CircuitAnalysis.Explain(_project.SimulationCircuit(), _explainOperate.SelectedItem as CircuitElement, _realPhysics);
			_explainText.Text = text.Replace("\n", "\r\n").Replace("\r\r\n", "\r\n");
			ShowBottomTab("Explain");
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			_explainText.Text = "Could not explain this circuit: " + ex2.Message;
			ProjectData.ClearProjectError();
		}
		finally
		{
			Cursor = Cursors.Default;
		}
	}

	private void OnGenerator(object sender, EventArgs e)
	{
		if (_running)
		{
			return;
		}
		using GeneratorDialog generatorDialog = new GeneratorDialog();
		Theme.Apply(generatorDialog);
		if (generatorDialog.ShowDialog(this) == DialogResult.OK && generatorDialog.Result != null && ConfirmDiscard())
		{
			NewProject(new Project(generatorDialog.Result.Circuit, "Generated circuit"));
			FitView();
			_dirty = true;
			UpdateTitle();
			_explainText.Text = generatorDialog.Result.Explanation.Replace("\n", "\r\n").Replace("\r\r\n", "\r\n");
			ShowBottomTab("Explain");
			_statusMessage.Text = "Circuit generated. Press Start (F9), then press the start button S1 / 1S0.";
		}
	}

	private void OnCalculators(object sender, EventArgs e)
	{
		using CalculatorsDialog calculatorsDialog = new CalculatorsDialog();
		Theme.Apply(calculatorsDialog);
		calculatorsDialog.ShowDialog(this);
	}

	private void OnPartsList(object sender, EventArgs e)
	{
		using PartsListDialog partsListDialog = new PartsListDialog(_project);
		Theme.Apply(partsListDialog);
		partsListDialog.ShowDialog(this);
		if (partsListDialog.PricesChanged)
		{
			OnCircuitModified();
		}
	}

	private void OnCutaway(object sender, EventArgs e)
	{
		if (_cutaway == null || _cutaway.IsDisposed)
		{
			_cutaway = new CutawayWindow
			{
				Owner = this,
				Location = checked(new Point(base.Right - 640, base.Top + 120)),
				ElementSource = [SpecialName] () => _canvas.SelectedElement ?? _canvas.LastHoveredElement,
				SimulatingSource = [SpecialName] () => _running
			};
		}
		_cutaway.Show();
		_cutaway.BringToFront();
	}

	private void OnQuickGuide(object sender, EventArgs e)
	{
		MessageBox.Show("BUILDING A CIRCUIT\r\n• Click a component in the library, then click on the drawing (or drag it there). Hover over a symbol for an explanation.\r\n• Drag from one port to another to connect them. Red circles are electrical terminals.\r\n• Drag a box to select several; R rotates, Del deletes, arrow keys nudge, Ctrl+Z / Ctrl+Y undo / redo.\r\n• Ctrl+C, Ctrl+X, Ctrl+V and Ctrl+D copy, cut, paste and duplicate. Drag a tube's middle segment to move it.\r\n• Use several pages (Page menu) and join lines across pages with page connectors of the same name.\r\n\r\nSIMULATING\r\n• Start (F9). Pressurized tubes turn blue, live wires red, hydraulic pressure lines orange.\r\n• Click push buttons, switches and pumps; clicking a solenoid valve operates its manual override.\r\n• Realistic physics: pressures build up, cylinders move by bore, load and friction; air consumption and cost are shown.\r\n• Right-click any component or tube to see its live values in the Inspector tab and change settings with sliders.\r\n• Replay: the step and event buttons (or drag the time slider) go back and forward in time; Start continues from there.\r\n• Plotter tab: tick any quantity, click for cursor A, right-click for cursor B (min / max / average between them);\r\n  set a trigger, zoom with the mouse wheel, and export CSV or PNG.\r\n• Record GIF saves an animation of the running circuit.\r\n\r\nFAULT FINDING\r\n• Tools > Fault finding > New fault-finding task hides a fault; find it with as few checks and hints as possible.\r\n• 'Add fault…' puts a chosen fault into the selected part or tube (tick Hidden to make a fault-finding file for a colleague).\r\n\r\nTOOLS\r\n• Check my circuit (F6) finds mistakes and signal overlap; Explain (F8) describes the circuit step by step.\r\n• Circuit Generator (Ctrl+G) designs a circuit from a sequence such as A+ B+ B- A-.\r\n• Calculators, parts list with costs, PDF report with title block, SVG and DXF export.\r\n• Parameter sweep: run the circuit with a setting changed step by step and compare cycle time, air use and speed.\r\n• Learn menu: getting-started tutorials with automatic checking, cutaway view of any component (F7).", "PneuSim Quick Guide", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
	}

	private void OnAbout(object sender, EventArgs e)
	{
		MessageBox.Show(string.Format("{0} {1}{2}Pneumatic, electro-pneumatic and hydraulic circuit design and simulation{3}{4}", "PneuSim", "3.2", "\r\n", "\r\n", "\r\n") + "(c) 2026 Akshaya Simha.\r\n\r\nDeveloped in VB.NET initially, and improved it with vibe coding using Claude Pro.\r\n\r\nDesign, simulate and check pneumatic and hydraulic circuits before you build them.\r\n\r\nSymbols based on ISO 1219 / IEC 60617.", "About PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
	}

	private void OnSweep(object sender, EventArgs e)
	{
		if (_project.AllElements().Count() == 0)
		{
			_statusMessage.Text = "Draw or load a circuit first.";
			return;
		}
		if (_running)
		{
			StopSimulation();
		}
		using SweepDialog sweepDialog = new SweepDialog(_project, _realPhysics);
		sweepDialog.ShowDialog(this);
	}
}
