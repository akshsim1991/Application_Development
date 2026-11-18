using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>The ribbon's commands. Most are the existing menu items (kept as a hidden command list), so shortcuts, enabled and checked states stay in one place.</summary>
public partial class MainForm
{
	private readonly Dictionary<string, RCmd> _cmds = new Dictionary<string, RCmd>();

	private readonly Dictionary<Keys, ToolStripMenuItem> _shortcuts = new Dictionary<Keys, ToolStripMenuItem>();

	private BackstageView _backstage;

	private static string Clean(string t)
	{
		return (t ?? "").Replace("&", "").Trim();
	}

	private static IEnumerable<ToolStripMenuItem> AllItems(ToolStripItemCollection items)
	{
		foreach (ToolStripItem item in items)
		{
			if (item is ToolStripMenuItem mi)
			{
				yield return mi;
				foreach (ToolStripMenuItem sub in AllItems(mi.DropDownItems))
				{
					yield return sub;
				}
			}
		}
	}

	private ToolStripMenuItem MI(string prefix)
	{
		List<ToolStripMenuItem> list = AllItems(_menu.Items).ToList();
		return list.FirstOrDefault((ToolStripMenuItem i) => string.Equals(Clean(i.Text), prefix, StringComparison.OrdinalIgnoreCase)) ?? list.FirstOrDefault((ToolStripMenuItem i) => Clean(i.Text).StartsWith(prefix, StringComparison.OrdinalIgnoreCase));
	}

	private static string ShortcutHint(ToolStripMenuItem it)
	{
		if (!string.IsNullOrEmpty(it.ShortcutKeyDisplayString))
		{
			return "  (" + it.ShortcutKeyDisplayString + ")";
		}
		if (it.ShortcutKeys != Keys.None)
		{
			try
			{
				return "  (" + new KeysConverter().ConvertToString(it.ShortcutKeys) + ")";
			}
			catch (Exception)
			{
			}
		}
		return "";
	}

	private RCmd FromMenu(string prefix, string tip = null)
	{
		ToolStripMenuItem it = MI(prefix);
		if (it == null)
		{
			return null;
		}
		return new RCmd
		{
			Run = delegate
			{
				if (it.Enabled)
				{
					it.PerformClick();
				}
			},
			Enabled = () => it.Enabled,
			Checked = () => it.Checked,
			Tip = (tip ?? Clean(it.Text).TrimEnd('.', '…')) + ShortcutHint(it)
		};
	}

	private void Reg(string key, RCmd c)
	{
		if (c != null)
		{
			_cmds[key] = c;
		}
	}

	private void RegAction(string key, Action run, string tip, Func<bool> enabled = null, Func<bool> isChecked = null)
	{
		_cmds[key] = new RCmd
		{
			Run = run,
			Tip = tip,
			Enabled = enabled,
			Checked = isChecked
		};
	}

	private void BuildCommands()
	{
		_cmds.Clear();
		_shortcuts.Clear();
		foreach (ToolStripMenuItem item in AllItems(_menu.Items))
		{
			if (item.ShortcutKeys != Keys.None && !_shortcuts.ContainsKey(item.ShortcutKeys))
			{
				_shortcuts[item.ShortcutKeys] = item;
			}
		}
		string[,] array = new string[,]
		{
			{ "New", "New" },
			{ "Open", "Open" },
			{ "Save", "Save" },
			{ "SaveAs", "Save As" },
			{ "Close", "Close" },
			{ "CloseAll", "Close all" },
			{ "Info", "Project information" },
			{ "Pdf", "PDF report" },
			{ "Svg", "SVG drawing" },
			{ "Dxf", "DXF drawing" },
			{ "Png", "PNG image" },
			{ "Csv", "Parts list (CSV)" },
			{ "Undo", "Undo" },
			{ "Redo", "Redo" },
			{ "Cut", "Cut" },
			{ "Copy", "Copy" },
			{ "Paste", "Paste" },
			{ "Duplicate", "Duplicate" },
			{ "SelectAll", "Select All" },
			{ "Delete", "Delete" },
			{ "Rotate", "Rotate" },
			{ "AddPage", "Add page" },
			{ "RenamePage", "Rename page" },
			{ "DeletePage", "Delete page" },
			{ "NextPage", "Next page" },
			{ "PrevPage", "Previous page" },
			{ "ZoomIn", "Zoom In" },
			{ "ZoomOut", "Zoom Out" },
			{ "ActualSize", "Actual Size" },
			{ "Labels", "Live value labels" },
			{ "Grid", "Show grid" },
			{ "Dark", "Dark mode" },
			{ "BottomPanel", "Bottom panel" },
			{ "Start", "Start" },
			{ "Pause", "Pause" },
			{ "Stop", "Stop and Reset" },
			{ "Real", "Realistic physics" },
			{ "Record", "Record animated" },
			{ "StepBack", "Step back" },
			{ "StepFwd", "Step forward" },
			{ "PrevEvent", "Back to the previous event" },
			{ "NextEvent", "Forward to the next event" },
			{ "Generator", "Circuit Generator" },
			{ "Check", "Check my circuit" },
			{ "Explain", "Explain my circuit" },
			{ "Calculators", "Calculators" },
			{ "Sizing", "Sizing sheets" },
			{ "Parts", "Parts list and costs" },
			{ "Sweep", "Parameter sweep" },
			{ "Settings", "Settings" },
			{ "NewFault", "New fault-finding task" },
			{ "Hint", "Hint" },
			{ "Found", "I found" },
			{ "ShowFault", "Show the fault" },
			{ "AddFault", "Put a fault" },
			{ "Repair", "Repair all" },
			{ "Cutaway", "Cutaway view" },
			{ "CutawayAll", "Cutaway: whole workspace" },
			{ "Tutorial", "Getting started tutorials" },
			{ "Guide", "Quick Guide" },
			{ "Update", "Update to new version" },
			{ "About", "About PneuSim" }
		};
		for (int i = 0; i < array.GetLength(0); i++)
		{
			Reg(array[i, 0], FromMenu(array[i, 1]));
		}
		RegAction("Exit", delegate
		{
			Close();
		}, "Exit PneuSim");
		RegAction("Fit", FitView, "Zoom so that the whole circuit fills the workspace (Ctrl+9)");
		RegAction("Pan", delegate
		{
			_canvas.PanMode = !_canvas.PanMode;
		}, "Pan (hand tool): drag to move the view. Middle mouse button, Space + drag and touchpad swipes also pan.", null, () => _canvas.PanMode);
		RegAction("Collapse", delegate
		{
			_ribbon.Collapsed = !_ribbon.Collapsed;
		}, "Collapse or expand the ribbon for more room (Ctrl+F1, or double-click a tab)", null, () => _ribbon.Collapsed);
		string[] array2 = new string[6] { "0.1x", "0.25x", "0.5x", "1x", "2x", "4x" };
		for (int j = 0; j < 6; j++)
		{
			int index = j;
			RegAction("Speed" + j, delegate
			{
				_speedBox.SelectedIndex = index;
				UpdateUiState();
			}, "Simulation speed " + array2[j], null, () => _speedBox.SelectedIndex == index);
		}
		string[,] array3 = new string[,]
		{
			{ "PanelDiagram", "Displacement-step diagram" },
			{ "PanelGrafcet", "GRAFCET" },
			{ "PanelPlotter", "Plotter" },
			{ "PanelCheck", "Check circuit" },
			{ "PanelExplain", "Explain" },
			{ "PanelTutorial", "Getting started" },
			{ "PanelInspector", "Inspector" },
			{ "PanelFault", "Fault finding" }
		};
		for (int k = 0; k < array3.GetLength(0); k++)
		{
			string title = array3[k, 1];
			RegAction(array3[k, 0], delegate
			{
				ShowBottomTab(title);
			}, "Show the " + title + " panel below the workspace");
		}
	}

	private List<(string Text, Action Run)> ExamplesMenu()
	{
		return Examples.All.Select(((string Name, Func<Circuit> Build) e) => (e.Name, (Action)delegate
		{
			OpenExample(e.Build);
		})).ToList();
	}

	private Func<string, RCmd> CmdLookup => (string key) => _cmds.TryGetValue(key, out var value) ? value : null;

	private void RebuildRibbon(ColorScheme scheme)
	{
		string activeTabName = _ribbon.ActiveTabName;
		_ribbon.Reset();
		RibbonBuilder.Build(_ribbon, CmdLookup, ExamplesMenu, scheme);
		_ribbon.SetDark(scheme.IsDark);
		if (activeTabName != null)
		{
			_ribbon.SelectTab(activeTabName);
		}
	}

	private void ShowBackstage()
	{
		if (_backstage == null)
		{
			_backstage = new BackstageView(CmdLookup, ExamplesMenu, delegate(string path)
			{
				HideBackstage();
				OpenFile(path);
			}, HideBackstage);
			base.Controls.Add(_backstage);
		}
		_backstage.Rebuild(AppSettings.RecentFiles, AppSettings.DarkMode);
		LayoutBackstage();
		_backstage.Visible = true;
		_backstage.BringToFront();
	}

	private void HideBackstage()
	{
		if (_backstage != null)
		{
			_backstage.Visible = false;
		}
	}

	private void LayoutBackstage()
	{
		if (_backstage != null && _ribbon != null)
		{
			int num = _ribbon.Top + RibbonBar.HeaderHeight;
			int num2 = _status.Top;
			_backstage.SetBounds(0, num, base.ClientSize.Width, Math.Max(100, num2 - num));
		}
	}

	protected override void OnResize(EventArgs e)
	{
		base.OnResize(e);
		if (_backstage != null && _backstage.Visible)
		{
			LayoutBackstage();
		}
	}

	/// <summary>Runs shortcuts of the hidden command list when the classic menu bar is not shown.</summary>
	private bool TryShortcut(Keys keyData)
	{
		if (keyData == (Keys.F1 | Keys.Control))
		{
			_ribbon.Collapsed = !_ribbon.Collapsed;
			return true;
		}
		if (keyData == Keys.Escape && _backstage != null && _backstage.Visible)
		{
			HideBackstage();
			return true;
		}
		if (_menu != null && _menu.Parent == null && _shortcuts.TryGetValue(keyData, out var value) && value.Enabled && !IsEditingText())
		{
			value.PerformClick();
			return true;
		}
		return false;
	}
}
