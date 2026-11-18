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
					Message = "No problems found while building the circuit. Click 'Run full check' to test-run the circuit as well."
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
				_canvas.ScrollToElement(checkIssue.Element);
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
		using GeneratorDialog generatorDialog = new GeneratorDialog(CurrentCircuit);
		Theme.Apply(generatorDialog);
		if (generatorDialog.ShowDialog(this) == DialogResult.OK && generatorDialog.Result != null && generatorDialog.Result.MergedIntoWorkspace)
		{
			_project.Pages[_pageIndex].Circuit = generatorDialog.Result.Circuit;
			ShowPage(_pageIndex);
			FitView();
			OnCircuitModified();
			_explainText.Text = generatorDialog.Result.Explanation.Replace("\n", "\r\n").Replace("\r\r\n", "\r\n");
			ShowBottomTab("Explain");
			_statusMessage.Text = "Circuit built around the components in the workspace (you can undo this). Press Start (F9), then press the start button.";
		}
		else if (generatorDialog.DialogResult == DialogResult.OK && generatorDialog.Result != null && ConfirmDiscard())
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
				SimulatingSource = [SpecialName] () => _running,
				CircuitSource = [SpecialName] () => (_running && _simulator != null) ? _simulator.Circuit : CurrentCircuit
			};
		}
		_cutaway.ShowWholeWorkspace(whole: false);
		_cutaway.Show();
		_cutaway.BringToFront();
	}

	private void OnCutawayWhole(object sender, EventArgs e)
	{
		OnCutaway(sender, e);
		_cutaway.ShowWholeWorkspace(whole: true);
	}

	private void OnQuickGuide(object sender, EventArgs e)
	{
		try
		{
			string fileName = UserGuide.EnsureGuide();
			System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(fileName)
			{
				UseShellExecute = true
			});
		}
		catch (Exception ex)
		{
			ErrorLog.Write(ex, "Quick Guide");
			MessageBox.Show(this, "The user guide could not be opened: " + ex.Message, AppInfo.Name, MessageBoxButtons.OK, MessageBoxIcon.Information);
		}
	}

	private void OnAbout(object sender, EventArgs e)
	{
		using AboutDialog aboutDialog = new AboutDialog();
		Theme.Apply(aboutDialog);
		aboutDialog.ShowDialog(this);
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
