using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Shows a GRAFCET (steps, actions, transitions) generated from the circuit, highlights the active step while simulating and can be copied as text.</summary>
public sealed class GrafcetPanel : Panel
{
	private GrafcetModel _model;

	private int _active = -1;

	private readonly Button _generate = new Button { Text = "Generate from the circuit", Width = 170, Height = 26 };

	private readonly Button _copy = new Button { Text = "Copy as text", Width = 110, Height = 26 };

	private readonly Label _note = new Label { AutoSize = true };

	private readonly Panel _canvas = new Panel { Dock = DockStyle.Fill, AutoScroll = true };

	private const int RowH = 92;

	public event Action GenerateRequested;

	public GrafcetModel Model
	{
		get
		{
			return _model;
		}
		set
		{
			_model = value;
			_note.Text = ((value == null) ? "Press Generate to draw the GRAFCET of this circuit." : value.Note);
			_canvas.AutoScrollMinSize = new Size(560, (value == null) ? 10 : (value.Steps.Count * 92 + 40));
			_canvas.Invalidate();
		}
	}

	/// <summary>Active step number while simulating (-1 = none).</summary>
	public int Active
	{
		get
		{
			return _active;
		}
		set
		{
			if (value != _active)
			{
				_active = value;
				_canvas.Invalidate();
			}
		}
	}

	public GrafcetPanel()
	{
		Panel panel = new Panel
		{
			Dock = DockStyle.Top,
			Height = 36
		};
		_generate.Location = new Point(6, 5);
		_copy.Location = new Point(182, 5);
		_note.Location = new Point(304, 10);
		panel.Controls.Add(_generate);
		panel.Controls.Add(_copy);
		panel.Controls.Add(_note);
		base.Controls.Add(_canvas);
		base.Controls.Add(panel);
		_generate.Click += delegate
		{
			this.GenerateRequested?.Invoke();
		};
		_copy.Click += delegate
		{
			if (_model != null && !_model.IsEmpty)
			{
				try
				{
					Clipboard.SetText(_model.ToText());
				}
				catch (Exception)
				{
				}
			}
		};
		_canvas.Paint += PaintChart;
		_canvas.Resize += delegate
		{
			_canvas.Invalidate();
		};
		SetStyleDoubleBuffered(_canvas);
		Model = null;
	}

	private static void SetStyleDoubleBuffered(Control c)
	{
		typeof(Control).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)?.SetValue(c, true);
	}

	private void PaintChart(object sender, PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		bool darkMode = AppSettings.DarkMode;
		Color color = (darkMode ? Theme.DarkField : Color.White);
		Color color2 = (darkMode ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46));
		Color color3 = Color.FromArgb(0, 118, 132);
		Color color4 = (darkMode ? Color.FromArgb(70, 200, 210) : color3);
		graphics.Clear(color);
		if (_model == null || _model.IsEmpty)
		{
			using (SolidBrush brush = new SolidBrush(color2))
			{
				using Font font = new Font("Segoe UI", 9.5f);
				graphics.DrawString((_model == null) ? "Press \"Generate from the circuit\" to draw the GRAFCET (sequence chart) of this circuit." : (string.IsNullOrEmpty(_model.Note) ? "No sequence found." : _model.Note), font, brush, 12f, 12f);
			}
			return;
		}
		graphics.TranslateTransform(_canvas.AutoScrollPosition.X, _canvas.AutoScrollPosition.Y);
		using Pen pen = new Pen(color2, 1.6f);
		using Pen pen2 = new Pen(color4, 2.4f);
		using Font font2 = new Font("Segoe UI", 10f, FontStyle.Bold);
		using Font font3 = new Font("Segoe UI", 9.5f);
		using SolidBrush brush2 = new SolidBrush(color2);
		int num = 190;
		int num2 = 24;
		int count = _model.Steps.Count;
		for (int i = 0; i < count; i++)
		{
			GrafcetStep grafcetStep = _model.Steps[i];
			int num3 = num2 + i * 92;
			Rectangle rect = new Rectangle(num - 24, num3, 48, 36);
			bool flag = i == _active;
			if (flag)
			{
				using SolidBrush brush3 = new SolidBrush(color4);
				graphics.FillRectangle(brush3, rect);
			}
			graphics.DrawRectangle(pen, rect);
			if (grafcetStep.Initial)
			{
				graphics.DrawRectangle(pen, Rectangle.Inflate(rect, -4, -4));
			}
			string text = grafcetStep.Number.ToString();
			SizeF sizeF = graphics.MeasureString(text, font2);
			using (SolidBrush brush4 = new SolidBrush(flag ? Color.White : color2))
			{
				graphics.DrawString(text, font2, brush4, (float)(rect.X + rect.Width / 2) - sizeF.Width / 2f, (float)(rect.Y + rect.Height / 2) - sizeF.Height / 2f);
			}
			graphics.DrawLine(pen, rect.Right, rect.Y + 18, rect.Right + 18, rect.Y + 18);
			Rectangle rect2 = new Rectangle(rect.Right + 18, rect.Y + 4, 300, 28);
			graphics.DrawRectangle(pen, rect2);
			graphics.DrawString(grafcetStep.Action, font3, brush2, rect2.X + 8, rect2.Y + 5);
			int num4 = num3 + 36;
			int num5 = num3 + 92;
			if (i < count - 1)
			{
				graphics.DrawLine(pen, num, num4, num, num5);
				graphics.DrawLine(pen, num - 10, num4 + 28, num + 10, num4 + 28);
				string s = ((i < _model.Conditions.Count) ? _model.Conditions[i] : "");
				graphics.DrawString(s, font3, brush2, num + 16, num4 + 18);
			}
			else
			{
				graphics.DrawLine(pen, num, num4, num, num4 + 20);
				graphics.DrawLine(pen, num - 10, num4 + 20, num + 10, num4 + 20);
				string s2 = ((i < _model.Conditions.Count) ? _model.Conditions[i] : "");
				graphics.DrawString(s2 + "     back to step 0", font3, brush2, num + 16, num4 + 10);
				int num6 = num - 120;
				graphics.DrawLine(pen, num, num4 + 20, num, num4 + 40);
				graphics.DrawLine(pen, num, num4 + 40, num6, num4 + 40);
				graphics.DrawLine(pen, num6, num4 + 40, num6, num2 - 14);
				graphics.DrawLine(pen, num6, num2 - 14, num, num2 - 14);
				graphics.DrawLine(pen, num, num2 - 14, num, num2);
				graphics.DrawLine(pen, num - 5, num2 - 8, num, num2);
				graphics.DrawLine(pen, num + 5, num2 - 8, num, num2);
			}
		}
	}
}
