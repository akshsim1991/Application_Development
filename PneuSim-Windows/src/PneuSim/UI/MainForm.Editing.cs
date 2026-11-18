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

	private void RefreshPageTabs()
	{
		_switchingPages = true;
		_pageTabs.SetPages(_project.Pages.Select((ProjectPage p) => p.Name), Math.Min(_pageIndex, checked(_project.Pages.Count - 1)));
		_switchingPages = false;
	}

	private void ShowPage(int index, bool keepView = false)
	{
		if (index >= 0 && index < _project.Pages.Count)
		{
			_pageIndex = index;
			if (_pageTabs.SelectedIndex != index)
			{
				_switchingPages = true;
				_pageTabs.SelectedIndex = index;
				_switchingPages = false;
			}
			_project.UpdateCrossReferences();
			_canvas.Circuit = CurrentCircuit;
			_canvas.Simulator = _simulator;
			_properties.SelectedObject = null;
			if (!keepView)
			{
				_canvas.ScrollToCircuit();
			}
			_canvas.Invalidate();
		}
	}

	private void AddPage()
	{
		if (!_running)
		{
			_project.AddPage();
			RefreshPageTabs();
			ShowPage(checked(_project.Pages.Count - 1));
			OnCircuitModified();
		}
	}

	private void RenamePage()
	{
		if (!_running)
		{
			string text = Interaction.InputBox("Page name:", "PneuSim", _project.Pages[_pageIndex].Name);
			if (!string.IsNullOrWhiteSpace(text))
			{
				_project.Pages[_pageIndex].Name = text.Trim();
				RefreshPageTabs();
				OnCircuitModified();
			}
		}
	}

	private void DeletePage()
	{
		if (!_running && _project.Pages.Count > 1 && (CurrentCircuit.Elements.Count <= 0 || MessageBox.Show($"Delete page '{_project.Pages[_pageIndex].Name}' and everything on it?", "PneuSim", MessageBoxButtons.YesNo, MessageBoxIcon.Exclamation) == DialogResult.Yes))
		{
			_project.Pages.RemoveAt(_pageIndex);
			_pageIndex = Math.Max(0, checked(_pageIndex - 1));
			RefreshPageTabs();
			ShowPage(_pageIndex);
			OnCircuitModified();
		}
	}

	private void Undo()
	{
		checked
		{
			if (_running)
			{
				_statusMessage.Text = "Stop the simulation (F11) to undo changes.";
			}
			else if (_historyIndex > 0)
			{
				_historyIndex--;
				RestoreSnapshot();
			}
		}
	}

	private void Redo()
	{
		checked
		{
			if (_running)
			{
				_statusMessage.Text = "Stop the simulation (F11) to redo changes.";
			}
			else if (_historyIndex < _history.Count - 1)
			{
				_historyIndex++;
				RestoreSnapshot();
			}
		}
	}

	private void UpdateUndoButtons()
	{
		if (_btnUndo != null)
		{
			bool enabled = !_running && _historyIndex > 0;
			bool enabled2 = !_running && _historyIndex < checked(_history.Count - 1);
			_btnUndo.Enabled = enabled;
			_menuUndo.Enabled = enabled;
			_btnRedo.Enabled = enabled2;
			_menuRedo.Enabled = enabled2;
		}
	}

	private void MarkDirty()
	{
		_dirty = true;
		UpdateTitle();
	}

	private void UpdateTitle()
	{
		string text = ((_filePath == null) ? _project.Info.Title : Path.GetFileNameWithoutExtension(_filePath));
		Text = string.Format("{0}{1} - {2} {3}", text, _dirty ? " *" : "", "PneuSim", AppInfo.ShortVersion);
		RefreshDocTabs();
	}

	private CheckIssue SelectedIssue()
	{
		if (_checkList.SelectedItems.Count != 0)
		{
			return _checkList.SelectedItems[0].Tag as CheckIssue;
		}
		return null;
	}

	private bool IsEditingText()
	{
		Control control = this;
		while (control is ContainerControl && ((ContainerControl)control).ActiveControl != null)
		{
			control = ((ContainerControl)control).ActiveControl;
		}
		Control control2 = FocusedLeaf(control);
		if (!(control2 is TextBoxBase) && !(control2 is ComboBox))
		{
			return control2 is NumericUpDown;
		}
		return true;
	}

	private void OnLibraryActivate(object sender, EventArgs e)
	{
		if (!_running && _library.SelectedItems.Count != 0)
		{
			LibraryPreset libraryPreset = (LibraryPreset)_library.SelectedItems[0].Tag;
			_canvas.PlacingPreset = libraryPreset;
			_statusMessage.Text = $"Click in the workspace to place: {libraryPreset.Name}  (hold Shift to place several, Esc to cancel)";
			_canvas.Focus();
		}
	}

	private CircuitElement _hoverTipElement;

	private string _hoverTipText;

	/// <summary>The description of a component and, while simulating, its live values (force, pressure, speed, flow, current...).</summary>
	private string HoverTipFor(CircuitElement e)
	{
		if (e == null)
		{
			return null;
		}
		string text = ComponentHelp.HelpFor(e);
		if (!_running || !AppSettings.ShowHoverValues)
		{
			return text;
		}
		try
		{
			System.Collections.Generic.List<(string Name, string Value)> list = e.InspectValues();
			if (list.Count > 0)
			{
				System.Text.StringBuilder stringBuilder = new System.Text.StringBuilder(text);
				stringBuilder.Append("\r\n\r\nLive values");
				foreach (var item in list)
				{
					stringBuilder.Append("\r\n  " + item.Name + ": " + item.Value);
				}
				return stringBuilder.ToString();
			}
		}
		catch (Exception)
		{
		}
		return text;
	}

	private void OnHoverElement(object sender, CircuitElement e)
	{
		_hoverTipElement = e;
		_hoverTipText = HoverTipFor(e);
		_tooltip.SetToolTip(_canvas, _hoverTipText);
	}

	/// <summary>Keeps the live values in the hover tip current while the simulation runs.</summary>
	private void RefreshHoverTip()
	{
		if (_hoverTipElement == null)
		{
			return;
		}
		string text = HoverTipFor(_hoverTipElement);
		if (text != _hoverTipText)
		{
			_hoverTipText = text;
			_tooltip.SetToolTip(_canvas, text);
		}
	}

	private void SetZoom(float z)
	{
		_canvas.Zoom = z;
		UpdateUiState();
	}
}
