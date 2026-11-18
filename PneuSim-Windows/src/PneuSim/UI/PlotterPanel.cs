using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class PlotterPanel : UserControl
{
	private class PlotArea : Control
	{
		[CompilerGenerated]
		internal sealed class _Closure_0024__11_002D0
		{
			public (double Start, double End) _0024VB_0024Local_r;

			public int _0024VB_0024Local_plotW;

			public VB_0024AnonymousDelegate_8<double, float> _0024VB_0024Local_xOf;

			public Func<PointF, bool> _0024I1;

			public _Closure_0024__11_002D0(_Closure_0024__11_002D0 arg0)
			{
				if (arg0 != null)
				{
					_0024VB_0024Local_r = arg0._0024VB_0024Local_r;
					_0024VB_0024Local_plotW = arg0._0024VB_0024Local_plotW;
					_0024VB_0024Local_xOf = arg0._0024VB_0024Local_xOf;
				}
			}

			[SpecialName]
			internal float _Lambda_0024__0(double t)
			{
				return (float)(150.0 + (t - _0024VB_0024Local_r.Start) / (_0024VB_0024Local_r.End - _0024VB_0024Local_r.Start) * (double)_0024VB_0024Local_plotW);
			}

			[SpecialName]
			internal bool _Lambda_0024__1(PointF p)
			{
				if ((double)p.X >= _0024VB_0024Local_r.Start - 0.05)
				{
					return (double)p.X <= _0024VB_0024Local_r.End + 0.05;
				}
				return false;
			}
		}

		[CompilerGenerated]
		internal sealed class _Closure_0024__11_002D1
		{
			public int _0024VB_0024Local_bottom;

			public double _0024VB_0024Local_lo;

			public double _0024VB_0024Local_hi;

			public int _0024VB_0024Local_top;

			public VB_0024AnonymousDelegate_5<double, float> _0024VB_0024Local_yOf;

			public _Closure_0024__11_002D0 _0024VB_0024NonLocal__0024VB_0024Closure_2;

			public _Closure_0024__11_002D1(_Closure_0024__11_002D1 arg0)
			{
				if (arg0 != null)
				{
					_0024VB_0024Local_bottom = arg0._0024VB_0024Local_bottom;
					_0024VB_0024Local_lo = arg0._0024VB_0024Local_lo;
					_0024VB_0024Local_hi = arg0._0024VB_0024Local_hi;
					_0024VB_0024Local_top = arg0._0024VB_0024Local_top;
					_0024VB_0024Local_yOf = arg0._0024VB_0024Local_yOf;
				}
			}

			[SpecialName]
			internal float _Lambda_0024__4(double v)
			{
				return (float)((double)_0024VB_0024Local_bottom - (v - _0024VB_0024Local_lo) / (_0024VB_0024Local_hi - _0024VB_0024Local_lo) * (double)checked(_0024VB_0024Local_bottom - _0024VB_0024Local_top));
			}

			[SpecialName]
			internal PointF _Lambda_0024__5(PointF q)
			{
				return new PointF(Math.Max(150f, Math.Min(checked(150 + _0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_plotW), _0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_xOf(q.X))), _0024VB_0024Local_yOf(q.Y));
			}
		}

		private readonly PlotterPanel _owner;

		private const int LabelW = 150;

		public PlotArea(PlotterPanel owner)
		{
			_owner = owner;
			SetStyle(ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.Selectable | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, value: true);
			BackColor = Color.White;
		}

		private double TimeAtX(int x)
		{
			(double, double) tuple = _owner.TimeRange();
			return tuple.Item1 + checked((double)(x - 150) / Math.Max(1.0, base.Width - 150 - 10)) * (tuple.Item2 - tuple.Item1);
		}

		protected override void OnMouseEnter(EventArgs e)
		{
			base.OnMouseEnter(e);
			Focus();
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			base.OnMouseDown(e);
			Focus();
			if (e.X >= 150)
			{
				double num = TimeAtX(e.X);
				if (e.Button == MouseButtons.Right)
				{
					_owner.CursorB = num;
				}
				else
				{
					_owner.CursorA = num;
				}
				_owner.UpdateInfo();
				Invalidate();
			}
		}

		protected override void OnMouseWheel(MouseEventArgs e)
		{
			base.OnMouseWheel(e);
			_owner.ZoomAt(TimeAtX(Math.Max(150, e.X)), e.Delta, (Control.ModifierKeys & Keys.Shift) != 0);
		}

		private static double NiceCeiling(double v)
		{
			if (v <= 0.0)
			{
				return 1.0;
			}
			double num = Math.Pow(10.0, Math.Floor(Math.Log10(v)));
			double[] array = new double[11]
			{
				1.0, 1.2, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0,
				10.0
			};
			foreach (double num2 in array)
			{
				if (v <= num2 * num + 1E-06)
				{
					return num2 * num;
				}
			}
			return 10.0 * num;
		}

		private static double GridStep(double window)
		{
			double[] array = new double[9] { 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0 };
			foreach (double num in array)
			{
				if (window / num <= 12.0)
				{
					return num;
				}
			}
			return 30.0;
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			PaintOn(e.Graphics, base.Width, base.Height);
		}

		internal void PaintOn(Graphics gr, int w, int h)
		{
			gr.SmoothingMode = SmoothingMode.AntiAlias;
			gr.Clear(_owner.Scheme.Background);
			using GdiSurface g = new GdiSurface(gr, _owner.Scheme);
			PaintPlot(g, w, h);
		}

		private void PaintPlot(DrawSurface g, int w, int h)
		{
			Simulator sim = _owner.Sim;
			List<string> list = _owner.SelectedChannels();
			using Font font = new Font("Segoe UI", 8f);
			using Font font2 = new Font("Segoe UI", 8f, FontStyle.Bold);
			_Closure_0024__11_002D0 arg = default(_Closure_0024__11_002D0);
			_Closure_0024__11_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__11_002D0(arg);
			if (sim == null || list.Count == 0)
			{
				g.DrawString("Start the simulation and tick the quantities to plot on the left.", font, Brushes.Gray, 10f, 10f);
				return;
			}
			CS_0024_003C_003E8__locals0._0024VB_0024Local_r = _owner.TimeRange();
			CS_0024_003C_003E8__locals0._0024VB_0024Local_plotW = Math.Max(10, checked(w - 150 - 10));
			int num = Math.Max(30, checked(h - 24) / list.Count);
			CS_0024_003C_003E8__locals0._0024VB_0024Local_xOf = [SpecialName] (double t) => (float)(150.0 + (t - CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Start) / (CS_0024_003C_003E8__locals0._0024VB_0024Local_r.End - CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Start) * (double)CS_0024_003C_003E8__locals0._0024VB_0024Local_plotW);
			checked
			{
				using (Pen pen = new Pen(Color.FromArgb(230, 232, 238)))
				{
					double num2 = GridStep(CS_0024_003C_003E8__locals0._0024VB_0024Local_r.End - CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Start);
					double num3 = Math.Ceiling(CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Start / num2) * num2;
					double num4 = CS_0024_003C_003E8__locals0._0024VB_0024Local_r.End + 1E-06;
					double num5 = num2;
					bool flag = num5 >= 0.0;
					for (double num6 = num3; flag ? (num6 <= num4) : (num6 >= num4); num6 += num5)
					{
						float num7 = CS_0024_003C_003E8__locals0._0024VB_0024Local_xOf(num6);
						g.DrawLine(pen, num7, 0f, num7, h - 20);
						g.DrawString((num2 < 1.0) ? $"{num6:0.0#} s" : $"{num6:0} s", font, Brushes.Gray, num7 - 8f, h - 18);
					}
				}
				double num8 = _owner.TriggerTime();
				int num9 = list.Count - 1;
				_Closure_0024__11_002D1 closure_0024__11_002D = default(_Closure_0024__11_002D1);
				for (int i = 0; i <= num9; i++)
				{
					closure_0024__11_002D = new _Closure_0024__11_002D1(closure_0024__11_002D);
					closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2 = CS_0024_003C_003E8__locals0;
					string text = list[i];
					closure_0024__11_002D._0024VB_0024Local_top = i * num + 4;
					closure_0024__11_002D._0024VB_0024Local_bottom = (i + 1) * num - 6;
					Color color = _owner.ColorFor(text);
					List<PointF> value = null;
					sim.Channels.TryGetValue(text, out value);
					List<PointF> list2 = (value ?? new List<PointF>()).Where([SpecialName] (PointF p) => (double)p.X >= closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_r.Start - 0.05 && (double)p.X <= closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_r.End + 0.05).ToList();
					closure_0024__11_002D._0024VB_0024Local_lo = 0.0;
					closure_0024__11_002D._0024VB_0024Local_hi = 1.0;
					if (list2.Count > 0)
					{
						closure_0024__11_002D._0024VB_0024Local_lo = Math.Min(0f, list2.Min([SpecialName] (PointF p) => p.Y));
						closure_0024__11_002D._0024VB_0024Local_hi = Math.Max(list2.Max([SpecialName] (PointF p) => p.Y), closure_0024__11_002D._0024VB_0024Local_lo + 0.001);
						if (closure_0024__11_002D._0024VB_0024Local_hi - closure_0024__11_002D._0024VB_0024Local_lo < 1.0 && closure_0024__11_002D._0024VB_0024Local_hi <= 1.01 && closure_0024__11_002D._0024VB_0024Local_lo >= 0.0)
						{
							closure_0024__11_002D._0024VB_0024Local_hi = 1.0;
						}
						closure_0024__11_002D._0024VB_0024Local_hi = NiceCeiling(closure_0024__11_002D._0024VB_0024Local_hi);
						if (closure_0024__11_002D._0024VB_0024Local_lo < 0.0)
						{
							closure_0024__11_002D._0024VB_0024Local_lo = 0.0 - NiceCeiling(0.0 - closure_0024__11_002D._0024VB_0024Local_lo);
						}
					}
					using (SolidBrush brush = new SolidBrush(color))
					{
						g.DrawString(text, font2, brush, new RectangleF(4f, closure_0024__11_002D._0024VB_0024Local_top, 98f, num - 4));
					}
					g.DrawString($"{closure_0024__11_002D._0024VB_0024Local_hi:0.##}", font, Brushes.Gray, 110f, closure_0024__11_002D._0024VB_0024Local_top - 2);
					g.DrawString($"{closure_0024__11_002D._0024VB_0024Local_lo:0.##}", font, Brushes.Gray, 110f, closure_0024__11_002D._0024VB_0024Local_bottom - 12);
					g.DrawLine(Pens.Gainsboro, 150f, closure_0024__11_002D._0024VB_0024Local_bottom, 150 + closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_plotW, closure_0024__11_002D._0024VB_0024Local_bottom);
					closure_0024__11_002D._0024VB_0024Local_yOf = closure_0024__11_002D._Lambda_0024__4;
					if (list2.Count > 1)
					{
						using Pen pen2 = new Pen(color, 1.8f);
						g.DrawLines(pen2, list2.Select(closure_0024__11_002D._Lambda_0024__5).ToArray());
					}
					if (!double.IsNaN(num8) && Operators.CompareString(text, Conversions.ToString(_owner._trigChannel.SelectedItem), TextCompare: false) == 0)
					{
						using Pen pen3 = new Pen(Color.FromArgb(200, 120, 0), 1f)
						{
							DashStyle = DashStyle.Dot
						};
						float num10 = closure_0024__11_002D._0024VB_0024Local_yOf(Convert.ToDouble(_owner._trigLevel.Value));
						if (num10 >= (float)closure_0024__11_002D._0024VB_0024Local_top && num10 <= (float)closure_0024__11_002D._0024VB_0024Local_bottom)
						{
							g.DrawLine(pen3, 150f, num10, 150 + closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_plotW, num10);
						}
					}
					(double, string)[] array = new(double, string)[2]
					{
						(_owner.CursorA, "A"),
						(_owner.CursorB, "B")
					};
					for (int j = 0; j < array.Length; j++)
					{
						(double, string) tuple = array[j];
						if (!double.IsNaN(tuple.Item1) && !(tuple.Item1 < closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_r.Start) && !(tuple.Item1 > closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_r.End))
						{
							double num11 = _owner.ValueAt(text, tuple.Item1);
							if (!double.IsNaN(num11))
							{
								g.DrawString($"{tuple.Item2}: {num11:0.###}", font, Brushes.Black, closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_xOf(tuple.Item1) + 3f, closure_0024__11_002D._0024VB_0024Local_top + ((Operators.CompareString(tuple.Item2, "A", TextCompare: false) == 0) ? 10 : 22));
							}
						}
					}
					(double, double, double) tuple2 = _owner.StatsBetweenCursors(text);
					if (!double.IsNaN(tuple2.Item1))
					{
						string text2 = $"A–B:  min {tuple2.Item1:0.###}   max {tuple2.Item2:0.###}   avg {tuple2.Item3:0.###}";
						SizeF sizeF = g.MeasureString(text2, font);
						using (SolidBrush brush2 = new SolidBrush(Color.FromArgb(220, 255, 255, 255)))
						{
							g.FillRectangle(brush2, (float)(150 + closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_plotW) - sizeF.Width - 4f, closure_0024__11_002D._0024VB_0024Local_top, sizeF.Width + 2f, sizeF.Height);
						}
						g.DrawString(text2, font, Brushes.Black, (float)(150 + closure_0024__11_002D._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_plotW) - sizeF.Width - 3f, closure_0024__11_002D._0024VB_0024Local_top);
					}
				}
				if (!double.IsNaN(num8))
				{
					float num12 = CS_0024_003C_003E8__locals0._0024VB_0024Local_xOf(num8);
					using (SolidBrush brush3 = new SolidBrush(Color.FromArgb(200, 120, 0)))
					{
						g.FillPolygon(brush3, new PointF[3]
						{
							new PointF(num12 - 5f, 0f),
							new PointF(num12 + 5f, 0f),
							new PointF(num12, 8f)
						});
					}
					g.DrawString("T", font, Brushes.DarkOrange, num12 + 6f, 0f);
				}
				(double, Color)[] array2 = new(double, Color)[2]
				{
					(_owner.CursorA, Color.FromArgb(220, 60, 60)),
					(_owner.CursorB, Color.FromArgb(40, 140, 60))
				};
				for (int k = 0; k < array2.Length; k++)
				{
					(double, Color) tuple3 = array2[k];
					if (!double.IsNaN(tuple3.Item1) && !(tuple3.Item1 < CS_0024_003C_003E8__locals0._0024VB_0024Local_r.Start) && !(tuple3.Item1 > CS_0024_003C_003E8__locals0._0024VB_0024Local_r.End))
					{
						using Pen pen4 = new Pen(tuple3.Item2, 1.2f)
						{
							DashStyle = DashStyle.Dash
						};
						g.DrawLine(pen4, CS_0024_003C_003E8__locals0._0024VB_0024Local_xOf(tuple3.Item1), 0f, CS_0024_003C_003E8__locals0._0024VB_0024Local_xOf(tuple3.Item1), h - 20);
					}
				}
			}
		}
	}

	private static readonly Color[] Palette = new Color[8]
	{
		Color.FromArgb(0, 84, 200),
		Color.FromArgb(215, 60, 30),
		Color.FromArgb(0, 140, 70),
		Color.FromArgb(150, 60, 190),
		Color.FromArgb(200, 120, 0),
		Color.FromArgb(0, 150, 160),
		Color.FromArgb(180, 40, 110),
		Color.FromArgb(90, 90, 90)
	};

	private static readonly double[] WindowChoices = new double[7] { 1.0, 2.0, 5.0, 10.0, 30.0, 60.0, 120.0 };

	private readonly CheckedListBox _channels;

	private readonly PlotArea _plot;

	private readonly ComboBox _window;

	private readonly CheckBox _hold;

	private readonly ComboBox _trigChannel;

	private readonly ComboBox _trigEdge;

	private readonly NumericUpDown _trigLevel;

	private readonly CheckBox _trigSingle;

	private readonly Label _info;

	private Simulator _simulator;

	private double _windowSeconds;

	private double _holdEnd;

	private double _armTime;

	private double _singleTrigger;

	public double CursorA;

	public double CursorB;

	public ColorScheme Scheme { get; set; }

	public Simulator Simulator
	{
		get
		{
			return _simulator;
		}
		set
		{
			_simulator = value;
			CursorA = double.NaN;
			CursorB = double.NaN;
			_hold.Checked = false;
			Rearm();
			SyncChannels();
			_plot.Invalidate();
		}
	}

	internal double WindowSeconds => _windowSeconds;

	internal Simulator Sim => _simulator;

	public PlotterPanel()
	{
		_channels = new CheckedListBox
		{
			Dock = DockStyle.Left,
			Width = 230,
			CheckOnClick = true,
			IntegralHeight = false
		};
		_plot = new PlotArea(this)
		{
			Dock = DockStyle.Fill
		};
		_window = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			Width = 64
		};
		_hold = new CheckBox
		{
			Text = "Hold",
			AutoSize = true,
			Padding = new Padding(4, 4, 0, 0)
		};
		_trigChannel = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			Width = 200
		};
		_trigEdge = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			Width = 70
		};
		_trigLevel = new NumericUpDown
		{
			DecimalPlaces = 2,
			Minimum = -100000m,
			Maximum = 100000m,
			Width = 70,
			Increment = 0.5m
		};
		_trigSingle = new CheckBox
		{
			Text = "Single",
			AutoSize = true,
			Padding = new Padding(4, 4, 0, 0)
		};
		_info = new Label
		{
			AutoSize = true,
			Padding = new Padding(8, 6, 0, 0)
		};
		_windowSeconds = 10.0;
		_singleTrigger = double.NaN;
		CursorA = double.NaN;
		CursorB = double.NaN;
		Scheme = ColorScheme.Light;
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 62,
			WrapContents = true
		};
		VB_0024AnonymousDelegate_8<string, Label> vB_0024AnonymousDelegate_ = [SpecialName] (string t) => new Label
		{
			Text = t,
			AutoSize = true,
			Padding = new Padding(4, 7, 0, 0)
		};
		flowLayoutPanel.Controls.Add(vB_0024AnonymousDelegate_("Time window:"));
		double[] windowChoices = WindowChoices;
		foreach (double num in windowChoices)
		{
			_window.Items.Add($"{num:0} s");
		}
		_window.SelectedIndex = 3;
		flowLayoutPanel.Controls.Add(_window);
		flowLayoutPanel.Controls.Add(_hold);
		Button button = new Button
		{
			Text = "Clear cursors",
			AutoSize = true
		};
		button.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__19_002D1();
		};
		flowLayoutPanel.Controls.Add(button);
		Button button2 = new Button
		{
			Text = "Export CSV…",
			AutoSize = true
		};
		button2.Click += OnExportCsv;
		Button button3 = new Button
		{
			Text = "Export PNG…",
			AutoSize = true
		};
		button3.Click += OnExportPng;
		flowLayoutPanel.Controls.Add(button2);
		flowLayoutPanel.Controls.Add(button3);
		flowLayoutPanel.SetFlowBreak(button3, value: true);
		flowLayoutPanel.Controls.Add(vB_0024AnonymousDelegate_("Trigger:"));
		_trigChannel.Items.Add("off");
		_trigChannel.SelectedIndex = 0;
		flowLayoutPanel.Controls.Add(_trigChannel);
		_trigEdge.Items.AddRange(new object[2] { "rising", "falling" });
		_trigEdge.SelectedIndex = 0;
		flowLayoutPanel.Controls.Add(_trigEdge);
		flowLayoutPanel.Controls.Add(vB_0024AnonymousDelegate_("at"));
		flowLayoutPanel.Controls.Add(_trigLevel);
		flowLayoutPanel.Controls.Add(_trigSingle);
		Button button4 = new Button
		{
			Text = "Arm",
			AutoSize = true
		};
		button4.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			Rearm();
		};
		flowLayoutPanel.Controls.Add(button4);
		flowLayoutPanel.Controls.Add(_info);
		base.Controls.Add(_plot);
		base.Controls.Add(_channels);
		base.Controls.Add(flowLayoutPanel);
		_channels.ItemCheck += [SpecialName] (object a0, ItemCheckEventArgs a1) =>
		{
			_Lambda_0024__19_002D3();
		};
		_window.SelectedIndexChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__19_002D5();
		};
		_hold.CheckedChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__19_002D6();
		};
		ComboBox[] array = new ComboBox[2] { _trigChannel, _trigEdge };
		for (int j = 0; j < array.Length; j = checked(j + 1))
		{
			array[j].SelectedIndexChanged += [SpecialName] (object a0, EventArgs a1) =>
			{
				Rearm();
			};
		}
		_trigLevel.ValueChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			Rearm();
		};
		_trigSingle.CheckedChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			Rearm();
		};
		UpdateInfo();
	}

	internal List<string> SelectedChannels()
	{
		return _channels.CheckedItems.Cast<string>().ToList();
	}

	internal Color ColorFor(string name)
	{
		return Palette[Math.Max(0, _channels.Items.IndexOf(name)) % Palette.Length];
	}

	public void SyncChannels()
	{
		if (_simulator == null)
		{
			return;
		}
		List<string> list = _simulator.Channels.Keys.ToList();
		bool flag = _channels.Items.Count == 0;
		foreach (string item in list)
		{
			if (!_channels.Items.Contains(item))
			{
				int index = _channels.Items.Add(item);
				if (flag && (item.EndsWith("position (mm)") || item.Contains("pressure, cap side")) && _channels.CheckedItems.Count < 4)
				{
					_channels.SetItemChecked(index, value: true);
				}
			}
			if (!_trigChannel.Items.Contains(item))
			{
				_trigChannel.Items.Add(item);
			}
		}
	}

	public void RefreshPlot()
	{
		SyncChannels();
		UpdateInfo();
		_plot.Invalidate();
	}

	private double TimeRangeEndLive()
	{
		return Math.Max(_windowSeconds, (_simulator == null) ? 0.0 : _simulator.Time);
	}

	private bool TriggerOn()
	{
		return _trigChannel.SelectedIndex > 0;
	}

	public void SetTrigger(string channel, bool rising, double level, bool singleShot)
	{
		SyncChannels();
		_trigChannel.SelectedIndex = ((channel != null) ? Math.Max(0, _trigChannel.Items.IndexOf(channel)) : 0);
		_trigEdge.SelectedIndex = ((!rising) ? 1 : 0);
		_trigLevel.Value = new decimal(level);
		_trigSingle.Checked = singleShot;
		Rearm();
	}

	public void ShowChannel(string name)
	{
		SyncChannels();
		int num = _channels.Items.IndexOf(name);
		if (num >= 0)
		{
			_channels.SetItemChecked(num, value: true);
		}
	}

	public void Rearm()
	{
		_armTime = ((_simulator == null) ? 0.0 : _simulator.Time);
		_singleTrigger = double.NaN;
		_plot.Invalidate();
	}

	public List<double> TriggerTimes()
	{
		List<double> list = new List<double>();
		if (!TriggerOn() || _simulator == null)
		{
			return list;
		}
		List<PointF> value = null;
		if (!_simulator.Channels.TryGetValue(Conversions.ToString(_trigChannel.SelectedItem), out value))
		{
			return list;
		}
		double num = Convert.ToDouble(_trigLevel.Value);
		bool flag = _trigEdge.SelectedIndex == 0;
		checked
		{
			int num2 = value.Count - 1;
			for (int i = 1; i <= num2; i++)
			{
				float y = value[i - 1].Y;
				float y2 = value[i].Y;
				if ((flag && (double)y < num && (double)y2 >= num) || (!flag && (double)y > num && (double)y2 <= num))
				{
					list.Add(value[i].X);
				}
			}
			return list;
		}
	}

	public double TriggerTime()
	{
		List<double> list = TriggerTimes();
		if (list.Count == 0)
		{
			return double.NaN;
		}
		if (_trigSingle.Checked)
		{
			if (double.IsNaN(_singleTrigger))
			{
				List<double> list2 = list.Where([SpecialName] (double t) => t >= _armTime - 1E-06).ToList();
				if (list2.Count > 0)
				{
					_singleTrigger = list2[0];
				}
			}
			return _singleTrigger;
		}
		return list[checked(list.Count - 1)];
	}

	public (double Start, double End) TimeRange()
	{
		double num = TriggerTime();
		(double, double) result;
		if (!double.IsNaN(num) && !_hold.Checked)
		{
			double num2 = num - 0.2 * _windowSeconds;
			result = (num2, num2 + _windowSeconds);
		}
		else
		{
			double num3 = (_hold.Checked ? _holdEnd : TimeRangeEndLive());
			result = (num3 - _windowSeconds, num3);
		}
		return result;
	}

	internal void ZoomAt(double t, int delta, bool pan)
	{
		(double, double) tuple = TimeRange();
		if (pan)
		{
			if (!_hold.Checked)
			{
				_hold.Checked = true;
			}
			_holdEnd += ((delta > 0) ? (-0.2) : 0.2) * _windowSeconds;
			_holdEnd = Math.Max(_windowSeconds * 0.2, _holdEnd);
		}
		else
		{
			double num = ((delta > 0) ? 0.8 : 1.25);
			double num2 = Math.Max(0.2, Math.Min(120.0, _windowSeconds * num));
			double num3 = (t - tuple.Item1) / Math.Max(1E-06, tuple.Item2 - tuple.Item1);
			if (!_hold.Checked)
			{
				_hold.Checked = true;
			}
			_windowSeconds = num2;
			_holdEnd = t + (1.0 - num3) * num2;
		}
		UpdateInfo();
		_plot.Invalidate();
	}

	internal void UpdateInfo()
	{
		List<string> list = new List<string>();
		if (!double.IsNaN(CursorA))
		{
			list.Add($"A: {CursorA:0.00} s");
		}
		if (!double.IsNaN(CursorB))
		{
			list.Add($"B: {CursorB:0.00} s");
		}
		if (!double.IsNaN(CursorA) && !double.IsNaN(CursorB))
		{
			list.Add($"Δt = {Math.Abs(CursorB - CursorA):0.000} s");
		}
		if (_simulator != null && _simulator.LastCycleTime > 0.0)
		{
			list.Add($"Cycle time: {_simulator.LastCycleTime:0.00} s");
		}
		if (_hold.Checked || _windowSeconds != WindowChoices[Math.Max(0, _window.SelectedIndex)])
		{
			list.Add($"window {_windowSeconds:0.##} s");
		}
		if (TriggerOn())
		{
			list.Add(double.IsNaN(TriggerTime()) ? "waiting for trigger…" : $"triggered at {TriggerTime():0.00} s");
		}
		_info.Text = ((list.Count == 0) ? "Left click: cursor A, right click: cursor B, wheel: zoom, Shift+wheel: scroll" : string.Join("    ", list));
	}

	internal double ValueAt(string name, double t)
	{
		List<PointF> value = null;
		if (_simulator == null || !_simulator.Channels.TryGetValue(name, out value) || value.Count == 0)
		{
			return double.NaN;
		}
		PointF pointF = value[0];
		foreach (PointF item in value)
		{
			if (Math.Abs((double)item.X - t) < Math.Abs((double)pointF.X - t))
			{
				pointF = item;
			}
		}
		return pointF.Y;
	}

	public (double Min, double Max, double Avg) StatsBetweenCursors(string name)
	{
		(double, double, double) result = (double.NaN, double.NaN, double.NaN);
		if (double.IsNaN(CursorA) || double.IsNaN(CursorB) || _simulator == null)
		{
			return result;
		}
		List<PointF> value = null;
		if (!_simulator.Channels.TryGetValue(name, out value))
		{
			return result;
		}
		double num = Math.Min(CursorA, CursorB);
		double num2 = Math.Max(CursorA, CursorB);
		List<PointF> list = value.Where([SpecialName] (PointF p) => (double)p.X >= num - 1E-06 && (double)p.X <= num2 + 1E-06).ToList();
		if (list.Count == 0)
		{
			return result;
		}
		(double, double, double) result2 = (list.Min([SpecialName] (PointF p) => p.Y), list.Max([SpecialName] (PointF p) => p.Y), list.Average([SpecialName] (PointF p) => p.Y));
		return result2;
	}

	public string ToCsv()
	{
		if (_simulator == null)
		{
			return "";
		}
		List<string> list = SelectedChannels();
		if (list.Count == 0)
		{
			list = _simulator.Channels.Keys.ToList();
		}
		CultureInfo invariantCulture = CultureInfo.InvariantCulture;
		SortedDictionary<long, double?[]> sortedDictionary = new SortedDictionary<long, double?[]>();
		checked
		{
			int num = list.Count - 1;
			for (int i = 0; i <= num; i++)
			{
				List<PointF> value = null;
				if (!_simulator.Channels.TryGetValue(list[i], out value))
				{
					continue;
				}
				foreach (PointF item in value)
				{
					long key = (long)Math.Round(item.X * 1000f);
					double?[] value2 = null;
					if (!sortedDictionary.TryGetValue(key, out value2))
					{
						value2 = (sortedDictionary[key] = new double?[list.Count - 1 + 1]);
					}
					value2[i] = item.Y;
				}
			}
			VB_0024AnonymousDelegate_19<string, string> vB_0024AnonymousDelegate_ = [SpecialName] (string s) => "\"" + s.Replace("\"", "\"\"") + "\"";
			StringBuilder stringBuilder = new StringBuilder();
			string text = vB_0024AnonymousDelegate_("Time (s)");
			List<string> source = list;
			VB_0024AnonymousDelegate_19<string, string> vB_0024AnonymousDelegate_2 = vB_0024AnonymousDelegate_;
			stringBuilder.AppendLine(text + "," + string.Join(",", source.Select((vB_0024AnonymousDelegate_2 == null) ? null : new Func<string, string>(vB_0024AnonymousDelegate_2.Invoke))));
			foreach (KeyValuePair<long, double?[]> item2 in sortedDictionary)
			{
				stringBuilder.Append(((double)item2.Key / 1000.0).ToString("0.000", invariantCulture));
				double?[] value3 = item2.Value;
				for (int j = 0; j < value3.Length; j++)
				{
					double? num2 = value3[j];
					stringBuilder.Append(',');
					if (num2.HasValue)
					{
						stringBuilder.Append(num2.Value.ToString("0.#####", invariantCulture));
					}
				}
				stringBuilder.AppendLine();
			}
			return stringBuilder.ToString();
		}
	}

	public Bitmap ToImage()
	{
		Bitmap bitmap = new Bitmap(Math.Max(200, _plot.Width), Math.Max(100, _plot.Height));
		using Graphics gr = Graphics.FromImage(bitmap);
		_plot.PaintOn(gr, bitmap.Width, bitmap.Height);
		return bitmap;
	}

	private void OnExportCsv(object sender, EventArgs e)
	{
		if (_simulator == null || _simulator.Channels.Count == 0)
		{
			_info.Text = "Run the simulation first.";
			return;
		}
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "CSV file (*.csv)|*.csv",
			FileName = "plotter.csv"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			File.WriteAllText(saveFileDialog.FileName, ToCsv(), Encoding.UTF8);
			_info.Text = "Saved " + saveFileDialog.FileName;
		}
	}

	private void OnExportPng(object sender, EventArgs e)
	{
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "PNG image (*.png)|*.png",
			FileName = "plotter.png"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			using (Bitmap bitmap = ToImage())
			{
				bitmap.Save(saveFileDialog.FileName, ImageFormat.Png);
			}
			_info.Text = "Saved " + saveFileDialog.FileName;
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__19_002D1()
	{
		CursorA = double.NaN;
		CursorB = double.NaN;
		UpdateInfo();
		_plot.Invalidate();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__19_002D3()
	{
		if (base.IsHandleCreated)
		{
			BeginInvoke((Action)([SpecialName] () =>
			{
				_plot.Invalidate();
			}));
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__19_002D5()
	{
		_windowSeconds = WindowChoices[Math.Max(0, _window.SelectedIndex)];
		_plot.Invalidate();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__19_002D6()
	{
		_holdEnd = TimeRangeEndLive();
		_plot.Invalidate();
	}
}
