using System;
using System.Drawing;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>PneuSim settings: appearance, units, mode, sounds and updates. Values are applied by the caller.</summary>
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

	private readonly RadioButton _light = new RadioButton { Text = "Light", AutoSize = true };

	private readonly RadioButton _dark = new RadioButton { Text = "Dark", AutoSize = true };

	private readonly RadioButton _system = new RadioButton { Text = "Follow Windows", AutoSize = true };

	private readonly ComboBox _unit = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 90 };

	private readonly CheckBox _advanced = new CheckBox { Text = "Advanced mode (show engineering properties)", AutoSize = true };

	private readonly CheckBox _sounds = new CheckBox { Text = "Sounds on", AutoSize = true };

	private readonly TrackBar _volume = new TrackBar { Minimum = 0, Maximum = 100, TickFrequency = 10, Width = 220 };

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
		ClientSize = new Size(440, 640);
		AutoScaleMode = AutoScaleMode.Font;
		FlowLayoutPanel flow = new FlowLayoutPanel
		{
			Dock = DockStyle.Fill,
			AutoScroll = true,
			FlowDirection = FlowDirection.TopDown,
			WrapContents = false,
			Padding = new Padding(14, 10, 14, 10)
		};
		flow.Controls.Add(Heading("Appearance"));
		FlowLayoutPanel theme = Row();
		theme.Controls.AddRange(new Control[3] { _system, _light, _dark });
		flow.Controls.Add(theme);
		flow.Controls.Add(Heading("Pressure unit"));
		_unit.Items.AddRange(new object[3] { "bar", "MPa", "kPa" });
		flow.Controls.Add(_unit);
		flow.Controls.Add(Heading("Mode"));
		flow.Controls.Add(_advanced);
		flow.Controls.Add(Heading("Sounds"));
		flow.Controls.Add(_sounds);
		Label volumeLabel = new Label { Text = "Volume", AutoSize = true };
		flow.Controls.Add(volumeLabel);
		flow.Controls.Add(_volume);
		for (int i = 0; i < SoundList.Length; i++)
		{
			_soundBoxes[i] = new CheckBox { Text = SoundList[i].Label, AutoSize = true, Margin = new Padding(14, 1, 3, 1) };
			flow.Controls.Add(_soundBoxes[i]);
		}
		flow.Controls.Add(Heading("Updates"));
		flow.Controls.Add(_updates);
		Panel buttons = new Panel { Dock = DockStyle.Bottom, Height = 52 };
		Button ok = new Button { Text = "OK", DialogResult = DialogResult.OK, Location = new Point(260, 12), Size = new Size(80, 30), Anchor = AnchorStyles.Top | AnchorStyles.Right };
		Button cancel = new Button { Text = "Cancel", DialogResult = DialogResult.Cancel, Location = new Point(348, 12), Size = new Size(80, 30), Anchor = AnchorStyles.Top | AnchorStyles.Right };
		buttons.Controls.Add(ok);
		buttons.Controls.Add(cancel);
		base.Controls.Add(flow);
		base.Controls.Add(buttons);
		base.AcceptButton = ok;
		base.CancelButton = cancel;
		_sounds.CheckedChanged += delegate
		{
			UpdateSoundControls();
		};
		LoadFrom();
	}

	private static Label Heading(string text)
	{
		return new Label
		{
			Text = text,
			AutoSize = true,
			Font = new Font("Segoe UI", 9f, FontStyle.Bold),
			Margin = new Padding(0, 10, 0, 3)
		};
	}

	private static FlowLayoutPanel Row()
	{
		return new FlowLayoutPanel
		{
			AutoSize = true,
			FlowDirection = FlowDirection.LeftToRight,
			WrapContents = false
		};
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
		_system.Checked = !_light.Checked && !_dark.Checked;
		_unit.SelectedItem = AppSettings.PressureUnit;
		if (_unit.SelectedIndex < 0)
		{
			_unit.SelectedIndex = 0;
		}
		_advanced.Checked = AppSettings.AdvancedMode;
		_sounds.Checked = AppSettings.SoundsOn;
		_volume.Value = Math.Max(0, Math.Min(100, AppSettings.SoundVolume));
		for (int i = 0; i < SoundList.Length; i++)
		{
			_soundBoxes[i].Checked = !AppSettings.MutedSounds.Contains(SoundList[i].Key);
		}
		_updates.Checked = AppSettings.CheckForUpdates;
		UpdateSoundControls();
	}

	/// <summary>Copies the dialog values into AppSettings and saves them.</summary>
	public void ApplyTo()
	{
		AppSettings.ThemeMode = (_light.Checked ? "Light" : (_dark.Checked ? "Dark" : "System"));
		AppSettings.PressureUnit = (string)_unit.SelectedItem;
		AppSettings.AdvancedMode = _advanced.Checked;
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
		AppSettings.CheckForUpdates = _updates.Checked;
		AppSettings.Save();
	}
}
