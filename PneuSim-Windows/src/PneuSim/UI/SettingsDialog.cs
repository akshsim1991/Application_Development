using System;
using System.Drawing;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>PneuSim settings, grouped on tabs. Values are copied into AppSettings by ApplyTo.</summary>
public sealed class SettingsDialog : Form
{
	private static readonly (string Key, string Label)[] SoundList = new (string, string)[6]
	{
		("valve", "Valve click"),
		("thud", "Cylinder end-stop thud"),
		("hiss", "Exhaust hiss"),
		("hum", "Compressor / pump hum"),
		("relief", "Relief-valve chatter"),
		("alarm", "Alarm (rated pressure exceeded, E-stop)")
	};

	private readonly RadioButton _system = new RadioButton { Text = "Follow Windows", AutoSize = true };

	private readonly RadioButton _light = new RadioButton { Text = "Light", AutoSize = true };

	private readonly RadioButton _dark = new RadioButton { Text = "Dark", AutoSize = true };

	private readonly RadioButton _amoled = new RadioButton { Text = "AMOLED (pure black)", AutoSize = true };

	private readonly ComboBox _unit = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 100 };

	private readonly ComboBox _currency = new ComboBox { DropDownStyle = ComboBoxStyle.DropDown, Width = 70 };

	private readonly CheckBox _advanced = new CheckBox { Text = "Advanced mode (show engineering properties)", AutoSize = true };

	private readonly CheckBox _startScreen = new CheckBox { Text = "Show the start screen when PneuSim starts", AutoSize = true };

	private readonly ComboBox _autoMode = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 330 };

	private readonly NumericUpDown _autoDelay = new NumericUpDown { DecimalPlaces = 1, Minimum = 0m, Maximum = 30m, Increment = 0.5m, Width = 80 };

	private readonly NumericUpDown _autoPause = new NumericUpDown { DecimalPlaces = 1, Minimum = 0m, Maximum = 120m, Increment = 0.5m, Width = 80 };

	private readonly NumericUpDown _autoHold = new NumericUpDown { DecimalPlaces = 1, Minimum = 0m, Maximum = 120m, Increment = 0.5m, Width = 80 };

	private readonly ComboBox _solenoids = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 330 };

	private readonly ComboBox _speed = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 100 };

	private readonly CheckBox _realistic = new CheckBox { Text = "Realistic physics (pressure build-up, loads, air use) on by default", AutoSize = true };

	private readonly CheckBox _valueLabels = new CheckBox { Text = "Show live value labels next to components", AutoSize = true };

	private readonly CheckBox _hoverValues = new CheckBox { Text = "Show live values when pointing at a component", AutoSize = true };

	private readonly CheckBox _snap = new CheckBox { Text = "Snap components to the grid", AutoSize = true };

	private readonly CheckBox _classic = new CheckBox { Text = "Show the classic menu bar and toolbar above the ribbon (takes effect after restart)", AutoSize = true };

	private readonly CheckBox _grid = new CheckBox { Text = "Show the grid dots in the workspace", AutoSize = true };

	private readonly NumericUpDown _autosave = new NumericUpDown { Minimum = 0m, Maximum = 60m, Width = 80 };

	private readonly CheckBox _sounds = new CheckBox { Text = "Sounds on", AutoSize = true };

	private readonly TrackBar _volume = new TrackBar { Minimum = 0, Maximum = 100, TickFrequency = 10, Width = 260 };

	private readonly CheckBox[] _soundBoxes = new CheckBox[SoundList.Length];

	private readonly CheckBox _updates = new CheckBox { Text = "Check for updates once a day", AutoSize = true };

	public SettingsDialog()
	{
		Text = "Settings";
		FormBorderStyle = FormBorderStyle.FixedDialog;
		StartPosition = FormStartPosition.CenterParent;
		MaximizeBox = false;
		MinimizeBox = false;
		ShowInTaskbar = false;
		Font = new Font("Segoe UI", 9f);
		ClientSize = new Size(520, 560);
		TabControl tabs = new TabControl
		{
			Dock = DockStyle.Fill,
			Padding = new Point(12, 4)
		};
		FlowLayoutPanel flowLayoutPanel = NewTab(tabs, "General");
		flowLayoutPanel.Controls.Add(Heading("Appearance"));
		FlowLayoutPanel flowLayoutPanel2 = Row();
		flowLayoutPanel2.Controls.AddRange(new Control[4] { _system, _light, _dark, _amoled });
		flowLayoutPanel.Controls.Add(flowLayoutPanel2);
		flowLayoutPanel.Controls.Add(Heading("Units and money"));
		_unit.Items.AddRange(new object[3] { "bar", "MPa", "kPa" });
		_currency.Items.AddRange(new object[8] { "₹", "$", "€", "£", "¥", "AED", "CHF", "R" });
		flowLayoutPanel.Controls.Add(Labelled("Pressure unit", _unit));
		flowLayoutPanel.Controls.Add(Labelled("Currency symbol for costs", _currency));
		flowLayoutPanel.Controls.Add(Heading("Mode"));
		flowLayoutPanel.Controls.Add(_advanced);
		flowLayoutPanel.Controls.Add(Heading("Start-up and files"));
		flowLayoutPanel.Controls.Add(_startScreen);
		flowLayoutPanel.Controls.Add(Labelled("Save a recovery copy every (minutes, 0 = never)", _autosave));
		flowLayoutPanel.Controls.Add(Heading("Updates"));
		flowLayoutPanel.Controls.Add(_updates);
		FlowLayoutPanel flowLayoutPanel3 = NewTab(tabs, "Simulation");
		flowLayoutPanel3.Controls.Add(Heading("Automatic operation"));
		Label value = new Label
		{
			Text = "PneuSim can press the start button for you, so a circuit runs by itself as soon as you press Start.",
			AutoSize = false,
			Width = 460,
			Height = 34
		};
		flowLayoutPanel3.Controls.Add(value);
		_autoMode.Items.AddRange(new object[3] { "Automatic: run one cycle by itself", "Automatic: keep repeating the cycle", "Manual: I operate the buttons myself" });
		flowLayoutPanel3.Controls.Add(Labelled("Operation", _autoMode));
		flowLayoutPanel3.Controls.Add(Labelled("Wait after Start before pressing (seconds)", _autoDelay));
		flowLayoutPanel3.Controls.Add(Labelled("Rest between repeated cycles (seconds)", _autoPause));
		flowLayoutPanel3.Controls.Add(Labelled("Hold a start button for (seconds, 0 = work it out)", _autoHold));
		_solenoids.Items.AddRange(new object[2] { "Automatic: PneuSim operates the solenoid for me", "Manual: I click the solenoid myself (manual override)" });
		flowLayoutPanel3.Controls.Add(Heading("Solenoid-operated valves"));
		flowLayoutPanel3.Controls.Add(new Label
		{
			Text = "For solenoid valves that have no coil in the circuit, PneuSim can operate the solenoid itself (and bring a double-solenoid valve back), or leave it to you.",
			AutoSize = false,
			Width = 460,
			Height = 34
		});
		flowLayoutPanel3.Controls.Add(Labelled("Solenoid valves", _solenoids));
		flowLayoutPanel3.Controls.Add(Heading("Speed and physics"));
		_speed.Items.AddRange(new object[6] { "0.1x", "0.25x", "0.5x", "1x", "2x", "4x" });
		flowLayoutPanel3.Controls.Add(Labelled("Simulation speed at start-up", _speed));
		flowLayoutPanel3.Controls.Add(_realistic);
		FlowLayoutPanel flowLayoutPanel4 = NewTab(tabs, "Workspace");
		flowLayoutPanel4.Controls.Add(Heading("Display"));
		flowLayoutPanel4.Controls.Add(_valueLabels);
		flowLayoutPanel4.Controls.Add(_hoverValues);
		flowLayoutPanel4.Controls.Add(_grid);
		flowLayoutPanel4.Controls.Add(_classic);
		flowLayoutPanel4.Controls.Add(Heading("Editing"));
		flowLayoutPanel4.Controls.Add(_snap);
		FlowLayoutPanel flowLayoutPanel5 = NewTab(tabs, "Sounds");
		flowLayoutPanel5.Controls.Add(_sounds);
		flowLayoutPanel5.Controls.Add(new Label { Text = "Volume", AutoSize = true });
		flowLayoutPanel5.Controls.Add(_volume);
		for (int i = 0; i < SoundList.Length; i++)
		{
			_soundBoxes[i] = new CheckBox { Text = SoundList[i].Label, AutoSize = true, Margin = new Padding(14, 2, 3, 2) };
			flowLayoutPanel5.Controls.Add(_soundBoxes[i]);
		}
		Button button = new Button { Text = "OK", DialogResult = DialogResult.OK, Size = new Size(90, 32), Margin = new Padding(6, 10, 10, 10) };
		Button button2 = new Button { Text = "Cancel", DialogResult = DialogResult.Cancel, Size = new Size(90, 32), Margin = new Padding(6, 10, 0, 10) };
		FlowLayoutPanel flowLayoutPanel6 = new FlowLayoutPanel
		{
			Dock = DockStyle.Fill,
			FlowDirection = FlowDirection.RightToLeft,
			WrapContents = false,
			Padding = new Padding(0, 0, 6, 0)
		};
		flowLayoutPanel6.Controls.Add(button);
		flowLayoutPanel6.Controls.Add(button2);
		TableLayoutPanel tableLayoutPanel = new TableLayoutPanel
		{
			Dock = DockStyle.Fill,
			ColumnCount = 1,
			RowCount = 2
		};
		tableLayoutPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));
		tableLayoutPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100f));
		tableLayoutPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 54f));
		tableLayoutPanel.Controls.Add(tabs, 0, 0);
		tableLayoutPanel.Controls.Add(flowLayoutPanel6, 0, 1);
		base.Controls.Add(tableLayoutPanel);
		base.AcceptButton = button;
		base.CancelButton = button2;
		_sounds.CheckedChanged += delegate
		{
			UpdateSoundControls();
		};
		_autoMode.SelectedIndexChanged += delegate
		{
			bool flag = _autoMode.SelectedIndex != 2;
			_autoDelay.Enabled = flag;
			_autoHold.Enabled = flag;
			_autoPause.Enabled = _autoMode.SelectedIndex == 1;
			_solenoids.Enabled = flag;
		};
		LoadFrom();
	}

	private static FlowLayoutPanel NewTab(TabControl tabs, string title)
	{
		TabPage tabPage = new TabPage(title) { Padding = new Padding(4) };
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Fill,
			AutoScroll = true,
			FlowDirection = FlowDirection.TopDown,
			WrapContents = false,
			Padding = new Padding(10, 6, 10, 6)
		};
		tabPage.Controls.Add(flowLayoutPanel);
		tabs.TabPages.Add(tabPage);
		return flowLayoutPanel;
	}

	private static Label Heading(string text)
	{
		return new Label
		{
			Text = text,
			AutoSize = true,
			Font = new Font("Segoe UI", 9f, FontStyle.Bold),
			Margin = new Padding(0, 12, 0, 4)
		};
	}

	private static FlowLayoutPanel Row()
	{
		return new FlowLayoutPanel
		{
			AutoSize = true,
			FlowDirection = FlowDirection.LeftToRight,
			WrapContents = true,
			Width = 470
		};
	}

	private static FlowLayoutPanel Labelled(string text, Control control)
	{
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			AutoSize = true,
			FlowDirection = FlowDirection.LeftToRight,
			WrapContents = false,
			Margin = new Padding(0, 2, 0, 2)
		};
		flowLayoutPanel.Controls.Add(new Label
		{
			Text = text,
			AutoSize = false,
			Width = 270,
			Height = 24,
			TextAlign = ContentAlignment.MiddleLeft
		});
		flowLayoutPanel.Controls.Add(control);
		return flowLayoutPanel;
	}

	private void UpdateSoundControls()
	{
		_volume.Enabled = _sounds.Checked;
		CheckBox[] soundBoxes = _soundBoxes;
		foreach (CheckBox checkBox in soundBoxes)
		{
			checkBox.Enabled = _sounds.Checked;
		}
	}

	private void LoadFrom()
	{
		_light.Checked = AppSettings.ThemeMode == "Light";
		_dark.Checked = AppSettings.ThemeMode == "Dark";
		_amoled.Checked = AppSettings.ThemeMode == "Amoled";
		_system.Checked = !_light.Checked && !_dark.Checked && !_amoled.Checked;
		_unit.SelectedItem = AppSettings.PressureUnit;
		if (_unit.SelectedIndex < 0)
		{
			_unit.SelectedIndex = 0;
		}
		_currency.Text = AppSettings.CurrencySymbol;
		_advanced.Checked = AppSettings.AdvancedMode;
		_startScreen.Checked = AppSettings.ShowStartScreen;
		_autosave.Value = Math.Max(0, Math.Min(60, AppSettings.AutosaveMinutes));
		_updates.Checked = AppSettings.CheckForUpdates;
		_autoMode.SelectedIndex = ((AppSettings.AutoMode == "Off") ? 2 : ((AppSettings.AutoMode == "Continuous") ? 1 : 0));
		_autoDelay.Value = (decimal)Math.Max(0.0, Math.Min(30.0, AppSettings.AutoStartDelay));
		_autoPause.Value = (decimal)Math.Max(0.0, Math.Min(120.0, AppSettings.AutoCyclePause));
		_autoHold.Value = (decimal)Math.Max(0.0, Math.Min(120.0, AppSettings.AutoHoldSeconds));
		_solenoids.SelectedIndex = (AppSettings.AutoSolenoids ? 0 : 1);
		_speed.SelectedIndex = Math.Max(0, Math.Min(5, AppSettings.DefaultSpeedIndex));
		_realistic.Checked = AppSettings.RealPhysics;
		_valueLabels.Checked = AppSettings.ShowValueLabels;
		_hoverValues.Checked = AppSettings.ShowHoverValues;
		_grid.Checked = AppSettings.ShowGrid;
		_classic.Checked = AppSettings.ClassicMenu;
		_snap.Checked = AppSettings.SnapToGrid;
		_sounds.Checked = AppSettings.SoundsOn;
		_volume.Value = Math.Max(0, Math.Min(100, AppSettings.SoundVolume));
		for (int i = 0; i < SoundList.Length; i++)
		{
			_soundBoxes[i].Checked = !AppSettings.MutedSounds.Contains(SoundList[i].Key);
		}
		UpdateSoundControls();
		_autoMode.SelectedIndex = _autoMode.SelectedIndex;
	}

	/// <summary>Copies the dialog values into AppSettings and saves them.</summary>
	public void ApplyTo()
	{
		AppSettings.ThemeMode = (_light.Checked ? "Light" : (_dark.Checked ? "Dark" : (_amoled.Checked ? "Amoled" : "System")));
		AppSettings.PressureUnit = (string)_unit.SelectedItem;
		string text = (_currency.Text ?? "").Trim();
		AppSettings.CurrencySymbol = ((text.Length == 0 || text.Length > 4) ? "₹" : text);
		AppSettings.AdvancedMode = _advanced.Checked;
		AppSettings.ShowStartScreen = _startScreen.Checked;
		AppSettings.AutosaveMinutes = (int)_autosave.Value;
		AppSettings.CheckForUpdates = _updates.Checked;
		AppSettings.AutoMode = ((_autoMode.SelectedIndex == 2) ? "Off" : ((_autoMode.SelectedIndex == 1) ? "Continuous" : "Once"));
		AppSettings.AutoStartDelay = (double)_autoDelay.Value;
		AppSettings.AutoCyclePause = (double)_autoPause.Value;
		AppSettings.AutoHoldSeconds = (double)_autoHold.Value;
		AppSettings.AutoSolenoids = _solenoids.SelectedIndex == 0;
		AppSettings.DefaultSpeedIndex = _speed.SelectedIndex;
		AppSettings.RealPhysics = _realistic.Checked;
		AppSettings.ShowValueLabels = _valueLabels.Checked;
		AppSettings.ShowHoverValues = _hoverValues.Checked;
		AppSettings.ShowGrid = _grid.Checked;
		AppSettings.ClassicMenu = _classic.Checked;
		AppSettings.SnapToGrid = _snap.Checked;
		AppSettings.SoundsOn = _sounds.Checked;
		AppSettings.SoundVolume = _volume.Value;
		AppSettings.MutedSounds.Clear();
		for (int i = 0; i < SoundList.Length; i++)
		{
			if (!_soundBoxes[i].Checked)
			{
				AppSettings.MutedSounds.Add(SoundList[i].Key);
			}
		}
		AppSettings.Save();
	}
}
