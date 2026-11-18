using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;

namespace PneuSim;

public class TroubleshootPanel : UserControl
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__9_002D0
	{
		public string _0024VB_0024Local_key;

		public TroubleshootPanel _0024VB_0024Me;

		[SpecialName]
		internal void _Lambda_0024__R1(object a0, EventArgs a1)
		{
			_Lambda_0024__0();
		}

		[SpecialName]
		internal void _Lambda_0024__0()
		{
			_0024VB_0024Me.Command?.Invoke(_0024VB_0024Me, _0024VB_0024Local_key);
		}
	}

	private readonly Label _status;

	private readonly TextBox _messages;

	private readonly ListBox _log;

	private readonly Dictionary<string, Button> _buttons;

	public event EventHandler<string> Command;

	public TroubleshootPanel()
	{
		_status = new Label
		{
			Dock = DockStyle.Top,
			Height = 24,
			Font = new Font("Segoe UI", 9f, FontStyle.Bold),
			Padding = new Padding(4, 4, 0, 0)
		};
		_messages = new TextBox
		{
			Dock = DockStyle.Fill,
			Multiline = true,
			ReadOnly = true,
			ScrollBars = ScrollBars.Vertical,
			WordWrap = true
		};
		_log = new ListBox
		{
			Dock = DockStyle.Right,
			Width = 360,
			IntegralHeight = false,
			HorizontalScrollbar = true
		};
		_buttons = new Dictionary<string, Button>();
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 32,
			WrapContents = false
		};
		AddButton(flowLayoutPanel, "start", "New fault-finding task", "Hides one random fault in the circuit. Start the simulation, operate and measure (right-click) to find it.");
		AddButton(flowLayoutPanel, "hint", "Hint", "Each hint tells you more: first the area, then the neighbours, then the cause. Hints cost points.");
		AddButton(flowLayoutPanel, "found", "I found it…", "Then click the component or tube you think is faulty.");
		AddButton(flowLayoutPanel, "giveup", "Show fault", "Ends the task and shows where the fault was.");
		AddButton(flowLayoutPanel, "report", "Report…", "Saves the diagnosis log and result as a text file.");
		flowLayoutPanel.Controls.Add(new Label
		{
			Text = "  |  ",
			AutoSize = true,
			Padding = new Padding(0, 7, 0, 0)
		});
		AddButton(flowLayoutPanel, "add", "Add fault…", "Puts a fault into the selected component or tube. Select a component or tube in edit mode first.");
		AddButton(flowLayoutPanel, "clear", "Repair all", "Removes every fault from the project.");
		base.Controls.Add(_messages);
		base.Controls.Add(_log);
		base.Controls.Add(_status);
		base.Controls.Add(flowLayoutPanel);
		ShowExercise(null);
		_messages.Text = "Fault finding:\r\n• 'New fault-finding task' hides a fault in the circuit. Run it, operate it, right-click parts to measure them, then say where the fault is.\r\n• To prepare a task for a colleague: select a part, 'Add fault…', tick 'Hidden' and save the file.\r\n• Right-clicking a part during the simulation counts as one check; few checks and no hints give the best result.";
	}

	private void AddButton(FlowLayoutPanel bar, string key, string text, string tip)
	{
		_Closure_0024__9_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__9_002D0();
		CS_0024_003C_003E8__locals0._0024VB_0024Me = this;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_key = key;
		Button button = new Button
		{
			Text = text,
			AutoSize = true,
			Height = 26
		};
		button.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			CS_0024_003C_003E8__locals0._Lambda_0024__0();
		};
		new ToolTip().SetToolTip(button, tip);
		bar.Controls.Add(button);
		_buttons[CS_0024_003C_003E8__locals0._0024VB_0024Local_key] = button;
	}

	public void ShowExercise(TroubleshootExercise ex)
	{
		_status.Text = ((ex == null) ? "No fault-finding task running." : ex.StatusText());
		bool enabled = ex != null && !ex.IsOver;
		_buttons["hint"].Enabled = enabled;
		_buttons["found"].Enabled = enabled;
		_buttons["giveup"].Enabled = enabled;
		_buttons["report"].Enabled = ex != null;
		if (ex == null)
		{
			_log.Items.Clear();
		}
		else
		{
			if (_log.Items.Count == ex.Log.Count)
			{
				return;
			}
			_log.BeginUpdate();
			_log.Items.Clear();
			foreach (string item in ex.Log)
			{
				_log.Items.Add(item);
			}
			_log.TopIndex = Math.Max(0, checked(_log.Items.Count - 1));
			_log.EndUpdate();
		}
	}

	public void AddMessage(string text)
	{
		_messages.AppendText(((_messages.TextLength > 0) ? "\r\n\r\n" : "") + text);
	}
}
