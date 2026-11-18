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

	private void OnSettings(object sender, EventArgs e)
	{
		using SettingsDialog settingsDialog = new SettingsDialog();
		Theme.Apply(settingsDialog);
		if (settingsDialog.ShowDialog(this) == DialogResult.OK)
		{
			settingsDialog.ApplyTo();
			ApplyTheme(AppSettings.ResolveDark());
			UpdateUiState();
			_statusMessage.Text = "Settings saved.";
		}
	}

	private void OnWindowsPreferenceChanged(object sender, Microsoft.Win32.UserPreferenceChangedEventArgs e)
	{
		if (AppSettings.ThemeMode == "System" && AppSettings.ResolveDark() != AppSettings.DarkMode)
		{
			BeginInvoke(new Action(delegate
			{
				ApplyTheme(AppSettings.ResolveDark());
			}));
		}
	}

	private void ApplyTheme(bool dark)
	{
		AppSettings.DarkMode = dark;
		AppSettings.Save();
		ColorScheme scheme = (dark ? ColorScheme.Dark : ColorScheme.Light);
		_canvas.Scheme = scheme;
		_diagram.Scheme = scheme;
		_plotter.Scheme = scheme;
		RenderLibraryImages(scheme);
		if (_ribbon != null)
		{
			string activeTabName = _ribbon.ActiveTabName;
			_ribbon.Reset();
			RibbonBuilder.Build(_ribbon, _ribbonCommands, scheme);
			_ribbon.SetDark(dark);
			if (activeTabName != null)
			{
				_ribbon.SelectTab(activeTabName);
			}
		}
		Color color = (dark ? Color.FromArgb(37, 39, 46) : SystemColors.Control);
		Color color2 = (dark ? Color.FromArgb(225, 228, 235) : SystemColors.ControlText);
		Color color3 = (dark ? Color.FromArgb(28, 30, 36) : SystemColors.Window);
		Color color4 = (dark ? Color.FromArgb(52, 56, 66) : Color.FromArgb(232, 236, 244));
		BackColor = color;
		ForeColor = color2;
		ThemeControls(base.Controls, color, color2, color3, color4);
		ToolStripProfessionalRenderer renderer = (dark ? new ToolStripProfessionalRenderer(new DarkColorTable()) : new ToolStripProfessionalRenderer());
		ToolStrip[] array = new ToolStrip[3] { _menu, _toolbar, _status };
		foreach (ToolStrip toolStrip in array)
		{
			toolStrip.Renderer = renderer;
			toolStrip.ForeColor = color2;
			foreach (ToolStripItem item in toolStrip.Items)
			{
				ThemeItem(item, color2, color, renderer);
			}
		}
		_properties.ViewBackColor = color3;
		_properties.ViewForeColor = color2;
		_properties.LineColor = color4;
		_properties.CategoryForeColor = color2;
		_properties.HelpBackColor = color;
		_properties.HelpForeColor = color2;
		if (dark)
		{
			try
			{
				SetSplitterColor(color4);
			}
			catch (MissingMethodException ex)
			{
				ProjectData.SetProjectError(ex);
				MissingMethodException ex2 = ex;
				ProjectData.ClearProjectError();
			}
		}
		_speedBox.ComboBox.BackColor = color3;
		_speedBox.ComboBox.ForeColor = color2;
		if (_menuDark != null)
		{
			_menuDark.Checked = dark;
		}
		_canvas.Invalidate();
		_diagram.Invalidate();
		_plotter.Invalidate();
	}

	[MethodImpl(MethodImplOptions.NoInlining)]
	private void SetSplitterColor(Color c)
	{
		_properties.CategorySplitterColor = c;
	}

	private void RenderLibraryImages(ColorScheme scheme)
	{
		ImageList.ImageCollection images = _library.LargeImageList.Images;
		List<Image> list = images.Cast<Image>().ToList();
		images.Clear();
		LibraryPreset[] presets = Library.Presets;
		foreach (LibraryPreset libraryPreset in presets)
		{
			images.Add(Library.RenderThumbnail(libraryPreset.Factory(), 72, 48, scheme));
		}
		foreach (Image item in list)
		{
			item.Dispose();
		}
		_library.Invalidate();
	}

	private void ThemeItem(ToolStripItem it, Color fore, Color back, ToolStripRenderer renderer)
	{
		it.ForeColor = fore;
		if (!(it is ToolStripDropDownItem toolStripDropDownItem))
		{
			return;
		}
		toolStripDropDownItem.DropDown.Renderer = renderer;
		toolStripDropDownItem.DropDown.BackColor = back;
		foreach (ToolStripItem dropDownItem in toolStripDropDownItem.DropDownItems)
		{
			ThemeItem(dropDownItem, fore, back, renderer);
		}
	}

	private void ThemeControls(Control.ControlCollection controls, Color back, Color fore, Color field, Color header)
	{
		foreach (Control control in controls)
		{
			if (!(control is CircuitCanvas) && !(control is ToolStrip))
			{
				if (control is Label && object.Equals(RuntimeHelpers.GetObjectValue(control.Tag), "header"))
				{
					control.BackColor = header;
					control.ForeColor = fore;
				}
				else if (control is TextBox || control is ListBox || control is ListView || control is ComboBox)
				{
					control.BackColor = field;
					control.ForeColor = fore;
				}
				else if (control is Button)
				{
					control.BackColor = back;
					control.ForeColor = fore;
					((Button)control).FlatStyle = ((!AppSettings.DarkMode) ? FlatStyle.Standard : FlatStyle.Flat);
				}
				else if (!(control is PropertyGrid))
				{
					control.BackColor = back;
					control.ForeColor = fore;
				}
				if (control.HasChildren)
				{
					ThemeControls(control.Controls, back, fore, field, header);
				}
			}
		}
	}
}
