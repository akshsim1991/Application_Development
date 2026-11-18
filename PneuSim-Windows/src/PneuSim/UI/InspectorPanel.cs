using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class InspectorPanel : UserControl
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__23_002D0
	{
		public Label _0024VB_0024Local_caption;

		public LiveTuning.Knob _0024VB_0024Local_knob;

		public TrackBar _0024VB_0024Local_bar;

		public VB_0024AnonymousDelegate_22 _0024VB_0024Local_show;

		public _Closure_0024__23_002D1 _0024VB_0024NonLocal__0024VB_0024Closure_2;

		public _Closure_0024__23_002D0(_Closure_0024__23_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_caption = arg0._0024VB_0024Local_caption;
				_0024VB_0024Local_knob = arg0._0024VB_0024Local_knob;
				_0024VB_0024Local_bar = arg0._0024VB_0024Local_bar;
				_0024VB_0024Local_show = arg0._0024VB_0024Local_show;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0()
		{
			_0024VB_0024Local_caption.Text = $"{_0024VB_0024Local_knob.Caption}: {LiveTuning.GetValue(_0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_el, _0024VB_0024Local_knob):0.##}";
		}

		[SpecialName]
		internal void _Lambda_0024__R1(object a0, EventArgs a1)
		{
			_Lambda_0024__1();
		}

		[SpecialName]
		internal void _Lambda_0024__1()
		{
			if (!_0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Me._updating)
			{
				LiveTuning.SetValue(_0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_el, _0024VB_0024Local_knob, _0024VB_0024Local_knob.Min + (_0024VB_0024Local_knob.Max - _0024VB_0024Local_knob.Min) * (double)_0024VB_0024Local_bar.Value / 200.0);
				_0024VB_0024Local_show();
				_0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Me.SettingChanged?.Invoke(_0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Me, EventArgs.Empty);
			}
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__23_002D1
	{
		public CircuitElement _0024VB_0024Local_el;

		public InspectorPanel _0024VB_0024Me;

		public _Closure_0024__23_002D1(_Closure_0024__23_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_el = arg0._0024VB_0024Local_el;
			}
		}
	}

	private readonly Label _title;

	private readonly ListView _list;

	private readonly FlowLayoutPanel _knobs;

	private object _target;

	private bool _updating;

	public string Note { get; set; }

	public object Target
	{
		get
		{
			return _target;
		}
		set
		{
			_target = RuntimeHelpers.GetObjectValue(value);
			if (value == null)
			{
				ShowHint();
				return;
			}
			_title.Text = "Inspecting: " + TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(value)) + (string.IsNullOrEmpty(Note) ? "" : ("      —  " + Note));
			BuildKnobs();
			RefreshValues();
		}
	}

	public event EventHandler SettingChanged;

	public event EventHandler SettingCommitted;

	public InspectorPanel()
	{
		_title = new Label
		{
			Dock = DockStyle.Top,
			Height = 24,
			Font = new Font("Segoe UI", 9f, FontStyle.Bold),
			Padding = new Padding(4, 4, 0, 0)
		};
		_list = new ListView
		{
			Dock = DockStyle.Fill,
			View = View.Details,
			FullRowSelect = true,
			HeaderStyle = ColumnHeaderStyle.Nonclickable
		};
		_knobs = new FlowLayoutPanel
		{
			Dock = DockStyle.Right,
			Width = 320,
			FlowDirection = FlowDirection.TopDown,
			WrapContents = false,
			AutoScroll = true
		};
		Note = "";
		_list.Columns.Add("Quantity", 170);
		_list.Columns.Add("Value", 260);
		base.Controls.Add(_list);
		base.Controls.Add(_knobs);
		base.Controls.Add(_title);
		ShowHint();
	}

	private void ShowHint()
	{
		_title.Text = "State inspector";
		_list.Items.Clear();
		_list.Items.Add(new ListViewItem(new string[2] { "Right-click a component or tube", "during the simulation to see its live values here." }));
		_knobs.Controls.Clear();
	}

	public void RefreshValues()
	{
		if (_target == null)
		{
			return;
		}
		List<(string, string)> list = LiveTuning.Inspect(RuntimeHelpers.GetObjectValue(_target));
		_list.BeginUpdate();
		checked
		{
			if (_list.Items.Count != list.Count)
			{
				_list.Items.Clear();
				foreach (var item in list)
				{
					_list.Items.Add(new ListViewItem(new string[2] { item.Item1, item.Item2 }));
				}
			}
			else
			{
				int num = list.Count - 1;
				for (int i = 0; i <= num; i++)
				{
					_list.Items[i].SubItems[0].Text = list[i].Item1;
					if (Operators.CompareString(_list.Items[i].SubItems[1].Text, list[i].Item2, TextCompare: false) != 0)
					{
						_list.Items[i].SubItems[1].Text = list[i].Item2;
					}
				}
			}
			_list.EndUpdate();
		}
	}

	private void BuildKnobs()
	{
		_Closure_0024__23_002D1 arg = new _Closure_0024__23_002D1(null);
		arg._0024VB_0024Me = this;
		_knobs.Controls.Clear();
		arg._0024VB_0024Local_el = _target as CircuitElement;
		if (arg._0024VB_0024Local_el == null)
		{
			return;
		}
		List<LiveTuning.Knob> list = LiveTuning.KnobsFor(arg._0024VB_0024Local_el);
		if (list.Count == 0)
		{
			_knobs.Controls.Add(new Label
			{
				Text = "No settings to change while running.",
				AutoSize = true,
				Padding = new Padding(4, 8, 0, 0)
			});
			return;
		}
		_knobs.Controls.Add(new Label
		{
			Text = "Change while it runs:",
			AutoSize = true,
			Font = new Font("Segoe UI", 8.5f, FontStyle.Bold),
			Padding = new Padding(4, 6, 0, 0)
		});
		_Closure_0024__23_002D0 closure_0024__23_002D = default(_Closure_0024__23_002D0);
		foreach (LiveTuning.Knob item in list)
		{
			closure_0024__23_002D = new _Closure_0024__23_002D0(closure_0024__23_002D);
			closure_0024__23_002D._0024VB_0024NonLocal__0024VB_0024Closure_2 = arg;
			closure_0024__23_002D._0024VB_0024Local_caption = new Label
			{
				AutoSize = true,
				Padding = new Padding(4, 4, 0, 0)
			};
			closure_0024__23_002D._0024VB_0024Local_bar = new TrackBar
			{
				Minimum = 0,
				Maximum = 200,
				TickFrequency = 20,
				Width = 290,
				Height = 32
			};
			closure_0024__23_002D._0024VB_0024Local_knob = item;
			closure_0024__23_002D._0024VB_0024Local_show = closure_0024__23_002D._Lambda_0024__0;
			_updating = true;
			closure_0024__23_002D._0024VB_0024Local_bar.Value = checked((int)Math.Round(Math.Max(0.0, Math.Min(200.0, (LiveTuning.GetValue(closure_0024__23_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_el, closure_0024__23_002D._0024VB_0024Local_knob) - closure_0024__23_002D._0024VB_0024Local_knob.Min) / (closure_0024__23_002D._0024VB_0024Local_knob.Max - closure_0024__23_002D._0024VB_0024Local_knob.Min) * 200.0))));
			_updating = false;
			closure_0024__23_002D._0024VB_0024Local_show();
			closure_0024__23_002D._0024VB_0024Local_bar.Scroll += closure_0024__23_002D._Lambda_0024__R1;
			closure_0024__23_002D._0024VB_0024Local_bar.MouseUp += [SpecialName] (object a0, MouseEventArgs a1) =>
			{
				_Lambda_0024__23_002D2();
			};
			closure_0024__23_002D._0024VB_0024Local_bar.KeyUp += [SpecialName] (object a0, KeyEventArgs a1) =>
			{
				_Lambda_0024__23_002D3();
			};
			_knobs.Controls.Add(closure_0024__23_002D._0024VB_0024Local_caption);
			_knobs.Controls.Add(closure_0024__23_002D._0024VB_0024Local_bar);
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__23_002D2()
	{
		SettingCommitted?.Invoke(this, EventArgs.Empty);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__23_002D3()
	{
		SettingCommitted?.Invoke(this, EventArgs.Empty);
	}
}
