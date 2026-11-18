using System;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class GeneratorDialog : Form
{
	private readonly ComboBox _sequence;

	private readonly RadioButton _electro;

	private readonly RadioButton _cascade;

	private readonly CheckBox _continuous;

	private readonly TextBox _explanation;

	private readonly Button _generate;

	private readonly CheckBox _useWorkspace;

	private readonly RadioButton _modeComplete;

	private readonly RadioButton _modeSequence;

	private readonly Circuit _workspace;

	public GeneratedCircuit Result { get; set; }

	private GeneratorMethod Method
	{
		get
		{
			if (!_cascade.Checked)
			{
				return GeneratorMethod.ElectroRelayChain;
			}
			return GeneratorMethod.PneumaticCascade;
		}
	}

	public GeneratorDialog(Circuit workspace = null)
	{
		_workspace = workspace;
		_modeComplete = new RadioButton
		{
			Text = "Complete my circuit from the components already in the workspace: PneuSim adds the missing valves, supply and wiring and connects everything",
			AutoSize = true,
			Checked = CircuitCompleter.CanComplete(workspace),
			Enabled = CircuitCompleter.CanComplete(workspace)
		};
		_modeSequence = new RadioButton
		{
			Text = "Generate a new circuit from a movement sequence (for example A+ B+ B- A-)",
			AutoSize = true,
			Checked = !CircuitCompleter.CanComplete(workspace)
		};
		int num = ((workspace == null) ? 0 : WorkspaceMerge.FindCylinders(workspace).Count);
		_useWorkspace = new CheckBox
		{
			Text = (num > 0) ? $"Build around the cylinders already in the workspace ({num} found): keep them and everything else there, and add the valves, sensors and wiring" : "Build around the cylinders already in the workspace (none found - place double-acting cylinders first)",
			AutoSize = true,
			Enabled = num > 0
		};
		_sequence = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDown,
			Width = 420,
			Font = new Font("Consolas", 11f)
		};
		_electro = new RadioButton
		{
			Text = "Electro-pneumatic: double-solenoid valves, proximity sensors, relay step chain",
			AutoSize = true,
			Checked = true
		};
		_cascade = new RadioButton
		{
			Text = "Pneumatic: double-pilot valves, roller valves, cascade (group) method",
			AutoSize = true
		};
		_continuous = new CheckBox
		{
			Text = "Add a switch for continuous cycling",
			AutoSize = true
		};
		_explanation = new TextBox
		{
			Multiline = true,
			ReadOnly = true,
			ScrollBars = ScrollBars.Both,
			WordWrap = false,
			Font = new Font("Consolas", 9f),
			BackColor = Color.White
		};
		_generate = new Button
		{
			Text = "Generate circuit",
			Width = 140,
			Height = 30
		};
		Text = "Circuit Generator";
		Font = new Font("Segoe UI", 9f);
		base.Size = new Size(820, 640);
		MinimumSize = new Size(640, 480);
		base.StartPosition = FormStartPosition.CenterParent;
		base.ShowInTaskbar = false;
		_sequence.Items.AddRange(new object[7] { "A+ B+ B- A-", "A+ B+ A- B-", "A+ B+ C+ C- B- A-", "A+ B+ B- C+ C- A-", "A+ (B+ C+) B- C- A-", "A+ A- B+ B-", "A+ B+ B- A- C+ C-" });
		_sequence.Text = "A+ B+ B- A-";
		Label value = new Label
		{
			AutoSize = false,
			Dock = DockStyle.Top,
			Height = 48,
			Padding = new Padding(10, 8, 10, 0),
			Text = "Type the order in which the cylinders move. A+ means cylinder A extends, A- means it retracts. Put movements that happen together in brackets, e.g. (B+ C+). All cylinders start and end retracted."
		};
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 222,
			FlowDirection = FlowDirection.TopDown,
			Padding = new Padding(10, 0, 10, 0),
			WrapContents = false
		};
		FlowLayoutPanel flowLayoutPanel2 = new FlowLayoutPanel
		{
			AutoSize = true,
			WrapContents = false
		};
		flowLayoutPanel2.Controls.Add(new Label
		{
			Text = "Sequence:",
			AutoSize = true,
			Padding = new Padding(0, 7, 0, 0)
		});
		flowLayoutPanel2.Controls.Add(_sequence);
		flowLayoutPanel.Controls.AddRange(new Control[7] { _modeComplete, _modeSequence, flowLayoutPanel2, _electro, _cascade, _continuous, _useWorkspace });
		FlowLayoutPanel flowLayoutPanel3 = new FlowLayoutPanel
		{
			Dock = DockStyle.Bottom,
			Height = 46,
			FlowDirection = FlowDirection.RightToLeft,
			Padding = new Padding(8)
		};
		Button button = new Button
		{
			Text = "Cancel",
			Width = 90,
			Height = 30,
			DialogResult = DialogResult.Cancel
		};
		flowLayoutPanel3.Controls.AddRange(new Control[2] { button, _generate });
		base.AcceptButton = _generate;
		base.CancelButton = button;
		Panel panel = new Panel
		{
			Dock = DockStyle.Fill,
			Padding = new Padding(10, 4, 10, 4)
		};
		_explanation.Dock = DockStyle.Fill;
		panel.Controls.Add(_explanation);
		panel.Controls.Add(new Label
		{
			Text = "How the circuit will work:",
			Dock = DockStyle.Top,
			Height = 20,
			Font = new Font("Segoe UI", 9f, FontStyle.Bold)
		});
		base.Controls.Add(panel);
		base.Controls.Add(flowLayoutPanel3);
		base.Controls.Add(flowLayoutPanel);
		base.Controls.Add(value);
		_sequence.TextChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			Preview();
		};
		_electro.CheckedChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			Preview();
		};
		_continuous.CheckedChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			Preview();
		};
		_useWorkspace.CheckedChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			Preview();
		};
		_modeComplete.CheckedChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			SetMode();
		};
		_generate.Click += OnGenerate;
		SetMode();
	}

	private GeneratedCircuit TryGenerate()
	{
		if (_modeComplete.Checked && _workspace != null)
		{
			return CircuitCompleter.Complete(_workspace);
		}
		GeneratedCircuit generatedCircuit = SequenceGenerator.Generate(MotionSequence.Parse(_sequence.Text), Method, _continuous.Checked);
		if (_useWorkspace.Checked && _workspace != null)
		{
			return WorkspaceMerge.Merge(_workspace, generatedCircuit);
		}
		return generatedCircuit;
	}

	private void SetMode()
	{
		bool flag = !_modeComplete.Checked;
		_sequence.Enabled = flag;
		_electro.Enabled = flag;
		_cascade.Enabled = flag;
		_continuous.Enabled = flag;
		_useWorkspace.Enabled = flag && _useWorkspace.Text.IndexOf("none found", StringComparison.Ordinal) < 0;
		_generate.Text = (flag ? "Generate circuit" : "Complete circuit");
		Preview();
	}

	private void Preview()
	{
		try
		{
			GeneratedCircuit generatedCircuit = TryGenerate();
			_explanation.Text = ((!_modeComplete.Checked && _useWorkspace.Checked && _workspace != null) ? WorkspaceMerge.Describe(_workspace, MotionSequence.Parse(_sequence.Text)).Replace("\n", "\r\n") : "") + generatedCircuit.Explanation.Replace("\n", "\r\n").Replace("\r\r\n", "\r\n");
			_explanation.ForeColor = Color.Black;
			_generate.Enabled = true;
		}
		catch (FormatException ex)
		{
			ProjectData.SetProjectError(ex);
			FormatException ex2 = ex;
			_explanation.Text = ex2.Message;
			_explanation.ForeColor = Color.DarkRed;
			_generate.Enabled = false;
			ProjectData.ClearProjectError();
		}
	}

	private void OnGenerate(object sender, EventArgs e)
	{
		try
		{
			Result = TryGenerate();
			base.DialogResult = DialogResult.OK;
			Close();
		}
		catch (FormatException ex)
		{
			ProjectData.SetProjectError(ex);
			FormatException ex2 = ex;
			MessageBox.Show(ex2.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
			ProjectData.ClearProjectError();
		}
	}
}
