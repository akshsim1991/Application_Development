using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class SweepDialog : Form
{
	private class Choice
	{
		public CircuitElement Element { get; }

		public int Index { get; }

		public Choice(CircuitElement e, int index)
		{
			Element = e;
			Index = index;
		}

		public override string ToString()
		{
			return Element.ToString();
		}
	}

	private class ChartBox : Control
	{
		[CompilerGenerated]
		internal sealed class _Closure_0024__5_002D0
		{
			public RectangleF _0024VB_0024Local_r;

			public float _0024VB_0024Local_x0;

			public float _0024VB_0024Local_x1;

			public float _0024VB_0024Local_y0;

			public float _0024VB_0024Local_y1;

			public _Closure_0024__5_002D0(_Closure_0024__5_002D0 arg0)
			{
				if (arg0 != null)
				{
					_0024VB_0024Local_r = arg0._0024VB_0024Local_r;
					_0024VB_0024Local_x0 = arg0._0024VB_0024Local_x0;
					_0024VB_0024Local_x1 = arg0._0024VB_0024Local_x1;
					_0024VB_0024Local_y0 = arg0._0024VB_0024Local_y0;
					_0024VB_0024Local_y1 = arg0._0024VB_0024Local_y1;
				}
			}

			[SpecialName]
			internal PointF _Lambda_0024__4(PointF p)
			{
				return new PointF(_0024VB_0024Local_r.Left + (p.X - _0024VB_0024Local_x0) / (_0024VB_0024Local_x1 - _0024VB_0024Local_x0) * _0024VB_0024Local_r.Width, _0024VB_0024Local_r.Bottom - (p.Y - _0024VB_0024Local_y0) / (_0024VB_0024Local_y1 - _0024VB_0024Local_y0) * _0024VB_0024Local_r.Height);
			}
		}

		private string _xName;

		private string _yName;

		private List<PointF> _points;

		public ChartBox()
		{
			_points = new List<PointF>();
			SetStyle(ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, value: true);
		}

		public void SetData(string xName, string yName, List<PointF> pts)
		{
			_xName = xName;
			_yName = yName;
			_points = pts.Where([SpecialName] (PointF p) => !float.IsNaN(p.Y)).ToList();
			Invalidate();
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			Graphics graphics = e.Graphics;
			graphics.SmoothingMode = SmoothingMode.AntiAlias;
			graphics.Clear(AppSettings.DarkMode ? Theme.DarkField : Color.White);
			Color color = (AppSettings.DarkMode ? Theme.DarkFore : Color.Black);
			checked
			{
				using Font font = new Font("Segoe UI", 8f);
				using SolidBrush brush = new SolidBrush(color);
				using Pen pen = new Pen(Color.Gray);
				_Closure_0024__5_002D0 arg = default(_Closure_0024__5_002D0);
				_Closure_0024__5_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__5_002D0(arg);
				if (_points.Count < 2)
				{
					graphics.DrawString("Run the sweep to see the chart.", font, brush, 10f, 10f);
					return;
				}
				CS_0024_003C_003E8__locals0._0024VB_0024Local_r = new RectangleF(50f, 26f, base.Width - 70, base.Height - 70);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_x0 = _points.Min([SpecialName] (PointF p) => p.X);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_x1 = _points.Max([SpecialName] (PointF p) => p.X);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_y0 = Math.Min(0f, _points.Min([SpecialName] (PointF p) => p.Y));
				CS_0024_003C_003E8__locals0._0024VB_0024Local_y1 = _points.Max([SpecialName] (PointF p) => p.Y);
				if ((double)(CS_0024_003C_003E8__locals0._0024VB_0024Local_x1 - CS_0024_003C_003E8__locals0._0024VB_0024Local_x0) < 1E-06)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_x1 = CS_0024_003C_003E8__locals0._0024VB_0024Local_x0 + 1f;
				}
				if ((double)(CS_0024_003C_003E8__locals0._0024VB_0024Local_y1 - CS_0024_003C_003E8__locals0._0024VB_0024Local_y0) < 1E-06)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_y1 = CS_0024_003C_003E8__locals0._0024VB_0024Local_y0 + 1f;
				}
				graphics.DrawLine(pen, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Left, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Right, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom);
				graphics.DrawLine(pen, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Left, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Top, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Left, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom);
				graphics.DrawString(_yName, font, brush, 4f, 4f);
				graphics.DrawString(_xName, font, brush, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Left, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom + 22f);
				graphics.DrawString($"{CS_0024_003C_003E8__locals0._0024VB_0024Local_y1:0.###}", font, brush, 2f, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Top - 6f);
				graphics.DrawString($"{CS_0024_003C_003E8__locals0._0024VB_0024Local_y0:0.###}", font, brush, 2f, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom - 8f);
				graphics.DrawString($"{CS_0024_003C_003E8__locals0._0024VB_0024Local_x0:0.##}", font, brush, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Left - 6f, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom + 4f);
				graphics.DrawString($"{CS_0024_003C_003E8__locals0._0024VB_0024Local_x1:0.##}", font, brush, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Right - 24f, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom + 4f);
				VB_0024AnonymousDelegate_6<PointF, PointF> vB_0024AnonymousDelegate_ = [SpecialName] (PointF p) => new PointF(CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Left + (p.X - CS_0024_003C_003E8__locals0._0024VB_0024Local_x0) / (CS_0024_003C_003E8__locals0._0024VB_0024Local_x1 - CS_0024_003C_003E8__locals0._0024VB_0024Local_x0) * CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Width, CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Bottom - (p.Y - CS_0024_003C_003E8__locals0._0024VB_0024Local_y0) / (CS_0024_003C_003E8__locals0._0024VB_0024Local_y1 - CS_0024_003C_003E8__locals0._0024VB_0024Local_y0) * CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Height);
				IOrderedEnumerable<PointF> source = _points.OrderBy([SpecialName] (PointF p) => p.X);
				VB_0024AnonymousDelegate_6<PointF, PointF> vB_0024AnonymousDelegate_2 = vB_0024AnonymousDelegate_;
				PointF[] array = source.Select((vB_0024AnonymousDelegate_2 == null) ? null : new Func<PointF, PointF>(vB_0024AnonymousDelegate_2.Invoke)).ToArray();
				using (Pen pen2 = new Pen(Color.FromArgb(0, 110, 210), 2f))
				{
					graphics.DrawLines(pen2, array);
				}
				using SolidBrush brush2 = new SolidBrush(Color.FromArgb(0, 110, 210));
				PointF[] array2 = array;
				for (int i = 0; i < array2.Length; i++)
				{
					PointF pointF = array2[i];
					graphics.FillEllipse(brush2, pointF.X - 3f, pointF.Y - 3f, 6f, 6f);
				}
			}
		}
	}

	[Serializable]
	[CompilerGenerated]
	internal sealed class _Closure_0024__
	{
		public static readonly _Closure_0024__ _0024I;

		public static Func<CircuitElement, bool> _0024I15_002D0;

		public static Func<CircuitElement, bool> _0024I15_002D1;

		public static VB_0024AnonymousDelegate_8<string, Label> _0024I15_002D3;

		public static Func<double, string> _0024I21_002D0;

		public static Func<double[], bool> _0024I21_002D1;

		static _Closure_0024__()
		{
			_0024I = new _Closure_0024__();
		}

		[SpecialName]
		internal bool _Lambda_0024__15_002D0(CircuitElement x)
		{
			return LiveTuning.KnobsFor(x).Count > 0;
		}

		[SpecialName]
		internal bool _Lambda_0024__15_002D1(CircuitElement x)
		{
			if (x.IsManuallyOperated && !(x is HydraulicPump))
			{
				return !(x is Compressor);
			}
			return false;
		}

		[SpecialName]
		internal Label _Lambda_0024__15_002D3(string t)
		{
			return new Label
			{
				Text = t,
				AutoSize = true,
				Padding = new Padding(0, 6, 0, 0)
			};
		}

		[SpecialName]
		internal string _Lambda_0024__21_002D0(double v)
		{
			if (!double.IsNaN(v))
			{
				return v.ToString("0.###");
			}
			return "–";
		}

		[SpecialName]
		internal bool _Lambda_0024__21_002D1(double[] r)
		{
			return !double.IsNaN(r[1]);
		}
	}

	private readonly Project _project;

	private readonly List<CircuitElement> _all;

	private readonly ComboBox _component;

	private readonly ComboBox _knob;

	private readonly NumericUpDown _from;

	private readonly NumericUpDown _to;

	private readonly NumericUpDown _steps;

	private readonly NumericUpDown _seconds;

	private readonly CheckedListBox _operate;

	private readonly CheckBox _real;

	private readonly DataGridView _grid;

	private readonly ChartBox _chart;

	private readonly ComboBox _plotColumn;

	private readonly Label _status;

	private SweepResult _result;

	public SweepDialog(Project project, bool realPhysics)
	{
		_component = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			Width = 260
		};
		_knob = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			Width = 260
		};
		_from = new NumericUpDown
		{
			DecimalPlaces = 2,
			Minimum = -100000m,
			Maximum = 100000m,
			Width = 80
		};
		_to = new NumericUpDown
		{
			DecimalPlaces = 2,
			Minimum = -100000m,
			Maximum = 100000m,
			Width = 80
		};
		_steps = new NumericUpDown
		{
			Minimum = 2m,
			Maximum = 50m,
			Value = 6m,
			Width = 60
		};
		_seconds = new NumericUpDown
		{
			Minimum = 1m,
			Maximum = 120m,
			Value = 10m,
			Width = 60
		};
		_operate = new CheckedListBox
		{
			CheckOnClick = true,
			IntegralHeight = false
		};
		_real = new CheckBox
		{
			Text = "Realistic physics",
			AutoSize = true
		};
		_grid = new DataGridView
		{
			Dock = DockStyle.Fill,
			ReadOnly = true,
			AllowUserToAddRows = false,
			RowHeadersVisible = false,
			AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.AllCells
		};
		_chart = new ChartBox
		{
			Dock = DockStyle.Right,
			Width = 360
		};
		_plotColumn = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			Width = 220
		};
		_status = new Label
		{
			AutoSize = true,
			Padding = new Padding(6, 7, 0, 0)
		};
		_project = project;
		_all = project.AllElements().ToList();
		Text = "Parameter sweep";
		base.Size = new Size(1100, 640);
		base.StartPosition = FormStartPosition.CenterParent;
		Font = new Font("Segoe UI", 9f);
		_real.Checked = realPhysics;
		foreach (CircuitElement item in _all.Where([SpecialName] (CircuitElement x) => LiveTuning.KnobsFor(x).Count > 0))
		{
			_component.Items.Add(new Choice(item, _all.IndexOf(item)));
		}
		foreach (CircuitElement item2 in _all.Where([SpecialName] (CircuitElement x) => x.IsManuallyOperated && !(x is HydraulicPump) && !(x is Compressor)))
		{
			_operate.Items.Add(new Choice(item2, _all.IndexOf(item2)));
		}
		_component.SelectedIndexChanged += OnComponentChanged;
		_knob.SelectedIndexChanged += OnKnobChanged;
		_plotColumn.SelectedIndexChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			UpdateChart();
		};
		TableLayoutPanel tableLayoutPanel = new TableLayoutPanel
		{
			Dock = DockStyle.Left,
			AutoSize = true,
			ColumnCount = 2,
			Padding = new Padding(6)
		};
		VB_0024AnonymousDelegate_8<string, Label> vB_0024AnonymousDelegate_ = [SpecialName] (string t) => new Label
		{
			Text = t,
			AutoSize = true,
			Padding = new Padding(0, 6, 0, 0)
		};
		tableLayoutPanel.Controls.Add(vB_0024AnonymousDelegate_("Component:"));
		tableLayoutPanel.Controls.Add(_component);
		tableLayoutPanel.Controls.Add(vB_0024AnonymousDelegate_("Setting:"));
		tableLayoutPanel.Controls.Add(_knob);
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			AutoSize = true,
			WrapContents = false
		};
		flowLayoutPanel.Controls.AddRange(new Control[7]
		{
			vB_0024AnonymousDelegate_("from"),
			_from,
			vB_0024AnonymousDelegate_("to"),
			_to,
			vB_0024AnonymousDelegate_("in"),
			_steps,
			vB_0024AnonymousDelegate_("steps")
		});
		tableLayoutPanel.Controls.Add(vB_0024AnonymousDelegate_("Values:"));
		tableLayoutPanel.Controls.Add(flowLayoutPanel);
		FlowLayoutPanel flowLayoutPanel2 = new FlowLayoutPanel
		{
			AutoSize = true,
			WrapContents = false
		};
		Button button = new Button
		{
			Text = "Run sweep",
			AutoSize = true
		};
		Button button2 = new Button
		{
			Text = "Export CSV…",
			AutoSize = true
		};
		button.Click += OnRun;
		button2.Click += OnExport;
		flowLayoutPanel2.Controls.AddRange(new Control[6]
		{
			vB_0024AnonymousDelegate_("Each run lasts"),
			_seconds,
			vB_0024AnonymousDelegate_("s"),
			_real,
			button,
			button2
		});
		tableLayoutPanel.Controls.Add(vB_0024AnonymousDelegate_("Run:"));
		tableLayoutPanel.Controls.Add(flowLayoutPanel2);
		GroupBox groupBox = new GroupBox
		{
			Text = "Operate during each run (held pressed / switched on)",
			Dock = DockStyle.Fill,
			Padding = new Padding(6)
		};
		_operate.Dock = DockStyle.Fill;
		groupBox.Controls.Add(_operate);
		Panel panel = new Panel
		{
			Dock = DockStyle.Top,
			Height = 150
		};
		panel.Controls.Add(groupBox);
		panel.Controls.Add(tableLayoutPanel);
		Panel panel2 = new Panel
		{
			Dock = DockStyle.Top,
			Height = 26
		};
		panel2.Controls.Add(_status);
		Panel panel3 = new Panel
		{
			Dock = DockStyle.Right,
			Width = 370
		};
		FlowLayoutPanel flowLayoutPanel3 = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 30
		};
		flowLayoutPanel3.Controls.AddRange(new Control[2]
		{
			new Label
			{
				Text = "Chart:",
				AutoSize = true,
				Padding = new Padding(0, 6, 0, 0)
			},
			_plotColumn
		});
		_chart.Dock = DockStyle.Fill;
		panel3.Controls.Add(_chart);
		panel3.Controls.Add(flowLayoutPanel3);
		base.Controls.Add(_grid);
		base.Controls.Add(panel3);
		base.Controls.Add(panel2);
		base.Controls.Add(panel);
		_status.Text = ((_component.Items.Count == 0) ? "This circuit has no settings to vary." : "Choose what to vary, then click Run sweep.");
		if (_component.Items.Count > 0)
		{
			_component.SelectedIndex = 0;
		}
		checked
		{
			int num = _operate.Items.Count - 1;
			for (int i = 0; i <= num; i++)
			{
				CircuitElement element = ((Choice)_operate.Items[i]).Element;
				if (element.Label != null && (element.Label.Contains("S1") || element.Label.Contains("S2")))
				{
					_operate.SetItemChecked(i, value: true);
				}
			}
			Theme.Apply(this);
			base.KeyDown += [SpecialName] (object s, KeyEventArgs e) =>
			{
				if (e.KeyCode == Keys.Escape)
				{
					Close();
				}
			};
			base.KeyPreview = true;
		}
	}

	private void OnComponentChanged(object sender, EventArgs e)
	{
		_knob.Items.Clear();
		if (!(_component.SelectedItem is Choice choice))
		{
			return;
		}
		foreach (LiveTuning.Knob item in LiveTuning.KnobsFor(choice.Element))
		{
			_knob.Items.Add(item.Caption);
		}
		if (_knob.Items.Count > 0)
		{
			_knob.SelectedIndex = 0;
		}
	}

	private LiveTuning.Knob SelectedKnob()
	{
		if (!(_component.SelectedItem is Choice choice) || _knob.SelectedIndex < 0)
		{
			return null;
		}
		return LiveTuning.KnobsFor(choice.Element)[_knob.SelectedIndex];
	}

	private void OnKnobChanged(object sender, EventArgs e)
	{
		LiveTuning.Knob knob = SelectedKnob();
		if (knob != null)
		{
			double value = LiveTuning.GetValue(((Choice)_component.SelectedItem).Element, knob);
			_from.Value = new decimal(Math.Max(knob.Min, (value > 0.0) ? (value * 0.5) : knob.Min));
			_to.Value = new decimal(Math.Min(knob.Max, (value > 0.0) ? (value * 1.5) : knob.Max));
		}
	}

	private void OnRun(object sender, EventArgs e)
	{
		LiveTuning.Knob knob = SelectedKnob();
		if (knob == null)
		{
			return;
		}
		SweepSettings sweepSettings = new SweepSettings
		{
			ElementIndex = ((Choice)_component.SelectedItem).Index,
			Knob = knob,
			FromValue = Convert.ToDouble(_from.Value),
			ToValue = Convert.ToDouble(_to.Value),
			Steps = Convert.ToInt32(_steps.Value),
			Seconds = Convert.ToDouble(_seconds.Value),
			RealPhysics = _real.Checked
		};
		foreach (object checkedItem in _operate.CheckedItems)
		{
			object objectValue = RuntimeHelpers.GetObjectValue(checkedItem);
			sweepSettings.Operate.Add(((Choice)objectValue).Index);
		}
		Cursor = Cursors.WaitCursor;
		try
		{
			_result = ParameterSweep.Run(_project, sweepSettings, [SpecialName] (int pct) =>
			{
				_status.Text = $"Running… {pct}%";
				_status.Refresh();
			});
			_status.Text = $"{_result.Rows.Count} runs done.";
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			_status.Text = "The sweep stopped: " + ex2.Message;
			ProjectData.ClearProjectError();
			return;
		}
		finally
		{
			Cursor = Cursors.Default;
		}
		ShowResult();
	}

	private void ShowResult()
	{
		_grid.Columns.Clear();
		_grid.Rows.Clear();
		foreach (string column in _result.Columns)
		{
			_grid.Columns.Add(column, column);
		}
		foreach (double[] row in _result.Rows)
		{
			_grid.Rows.Add(row.Select((_Closure_0024__._0024I21_002D0 == null) ? (_Closure_0024__._0024I21_002D0 = [SpecialName] (double v) => (!double.IsNaN(v)) ? v.ToString("0.###") : "–") : _Closure_0024__._0024I21_002D0).Cast<object>().ToArray());
		}
		_plotColumn.Items.Clear();
		checked
		{
			int num = _result.Columns.Count - 1;
			for (int i = 1; i <= num; i++)
			{
				_plotColumn.Items.Add(_result.Columns[i]);
			}
			int selectedIndex = ((!_result.Rows.Any([SpecialName] (double[] r) => !double.IsNaN(r[1]))) ? Math.Min(3, _plotColumn.Items.Count - 1) : 0);
			if (_plotColumn.Items.Count > 0)
			{
				_plotColumn.SelectedIndex = selectedIndex;
			}
		}
	}

	private void UpdateChart()
	{
		if (_result != null && _plotColumn.SelectedIndex >= 0)
		{
			int num = checked(_plotColumn.SelectedIndex + 1);
			_chart.SetData(_result.Columns[0], _result.Columns[num], _result.Rows.Select([SpecialName] (double[] r) => new PointF((float)r[0], (float)r[num])).ToList());
		}
	}

	private void OnExport(object sender, EventArgs e)
	{
		if (_result == null)
		{
			_status.Text = "Run the sweep first.";
			return;
		}
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "CSV file (*.csv)|*.csv",
			FileName = "sweep.csv"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			File.WriteAllText(saveFileDialog.FileName, _result.ToCsv(), Encoding.UTF8);
			_status.Text = "Saved " + saveFileDialog.FileName;
		}
	}
}
