using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;

namespace PneuSim;

public class FaultDialog : Form
{
	private readonly ListBox _list;

	private readonly CheckBox _hidden;

	private readonly List<FaultKind> _kinds;

	public FaultKind SelectedKind
	{
		get
		{
			if (_list.SelectedIndex >= 0)
			{
				return _kinds[_list.SelectedIndex];
			}
			return FaultKind.None;
		}
	}

	public bool Hidden => _hidden.Checked;

	public FaultDialog(object target, bool diagnosis)
	{
		_list = new ListBox
		{
			Dock = DockStyle.Fill,
			IntegralHeight = false
		};
		_hidden = new CheckBox
		{
			Text = "Hidden: a fault-finding task (the file does not show it)",
			AutoSize = true
		};
		_kinds = new List<FaultKind>();
		Text = (diagnosis ? "What is wrong with it?" : ("Put a fault into " + TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(target))));
		base.Size = new Size(520, 330);
		base.StartPosition = FormStartPosition.CenterParent;
		base.FormBorderStyle = FormBorderStyle.FixedDialog;
		base.MinimizeBox = false;
		base.MaximizeBox = false;
		Font = new Font("Segoe UI", 9f);
		Label value = new Label
		{
			Dock = DockStyle.Top,
			Height = 40,
			Padding = new Padding(6),
			Text = (diagnosis ? $"You think the fault is in {TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(target))}. What kind of fault is it?" : "Choose a fault. The circuit will behave as a real one with this fault.")
		};
		if (diagnosis)
		{
			_list.Items.Add("I am not sure what kind of fault (only the part counts)");
			_kinds.Add(FaultKind.None);
		}
		else
		{
			_list.Items.Add("No fault (repair it)");
			_kinds.Add(FaultKind.None);
		}
		FaultKind[] array = TroubleshootExercise.PossibleFaultsOf(RuntimeHelpers.GetObjectValue(target));
		foreach (FaultKind faultKind in array)
		{
			_list.Items.Add(TroubleshootExercise.DescribeFault(RuntimeHelpers.GetObjectValue(target), faultKind));
			_kinds.Add(faultKind);
		}
		FaultKind item = TroubleshootExercise.FaultOf(RuntimeHelpers.GetObjectValue(target));
		_list.SelectedIndex = ((!diagnosis) ? Math.Max(0, _kinds.IndexOf(item)) : 0);
		if (target is CircuitElement)
		{
			_hidden.Checked = ((CircuitElement)target).FaultHidden;
		}
		if (target is Tube)
		{
			_hidden.Checked = ((Tube)target).FaultHidden;
		}
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Bottom,
			Height = (diagnosis ? 40 : 70),
			FlowDirection = FlowDirection.LeftToRight,
			Padding = new Padding(6)
		};
		if (!diagnosis)
		{
			flowLayoutPanel.Controls.Add(_hidden);
			flowLayoutPanel.SetFlowBreak(_hidden, value: true);
		}
		Button button = new Button
		{
			Text = "OK",
			DialogResult = DialogResult.OK,
			Width = 90
		};
		Button button2 = new Button
		{
			Text = "Cancel",
			DialogResult = DialogResult.Cancel,
			Width = 90
		};
		flowLayoutPanel.Controls.Add(button);
		flowLayoutPanel.Controls.Add(button2);
		base.AcceptButton = button;
		base.CancelButton = button2;
		_list.DoubleClick += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__3_002D0();
		};
		base.Controls.Add(_list);
		base.Controls.Add(flowLayoutPanel);
		base.Controls.Add(value);
		Theme.Apply(this);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__3_002D0()
	{
		if (_list.SelectedIndex >= 0)
		{
			base.DialogResult = DialogResult.OK;
		}
	}
}
