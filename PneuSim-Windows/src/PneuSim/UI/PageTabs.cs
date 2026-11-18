using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Excel-style sheet tabs for the pages of a circuit: arrows, tabs with the selected one marked, and a "+" button to add a page.</summary>
public sealed class PageTabs : Control
{
	private readonly List<string> _names = new List<string>();

	private readonly List<Rectangle> _rects = new List<Rectangle>();

	private Rectangle _left;

	private Rectangle _right;

	private Rectangle _plus;

	private int _selected;

	private int _first;

	private int _hover = -1;

	public event Action<int> SelectedIndexChanged;

	public event Action AddRequested;

	public int SelectedIndex
	{
		get
		{
			return _selected;
		}
		set
		{
			if (value >= 0 && value < _names.Count && value != _selected)
			{
				_selected = value;
				EnsureVisible();
				Invalidate();
			}
		}
	}

	public PageTabs()
	{
		SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, value: true);
		Dock = DockStyle.Bottom;
		Height = 30;
		Font = new Font("Segoe UI", 9f);
	}

	public void SetPages(IEnumerable<string> names, int selected)
	{
		_names.Clear();
		_names.AddRange(names);
		_selected = Math.Max(0, Math.Min(_names.Count - 1, selected));
		_first = Math.Min(_first, Math.Max(0, _names.Count - 1));
		EnsureVisible();
		Invalidate();
	}

	private void EnsureVisible()
	{
		if (_selected < _first)
		{
			_first = _selected;
		}
	}

	private static bool Dark => AppSettings.DarkMode;

	private Color Back => Dark ? Theme.Dk(Color.FromArgb(34, 37, 43)) : Color.FromArgb(236, 239, 243);

	private Color TabBack => Dark ? Theme.Dk(Color.FromArgb(52, 56, 66)) : Color.White;

	private Color Text_ => Dark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46);

	private Color Faint => Dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(100, 110, 122);

	private Color Accent => Dark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132);

	protected override void OnPaint(PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		graphics.Clear(Back);
		using (Pen pen = new Pen(Dark ? Color.FromArgb(70, 76, 88) : Color.FromArgb(200, 205, 212)))
		{
			graphics.DrawLine(pen, 0, 0, base.Width, 0);
		}
		_left = new Rectangle(6, 5, 20, 20);
		_right = new Rectangle(28, 5, 20, 20);
		DrawArrow(graphics, _left, left: true, _first > 0);
		DrawArrow(graphics, _right, left: false, _first < _names.Count - 1);
		_rects.Clear();
		int num = 56;
		int num2 = base.Width - 50;
		for (int i = _first; i < _names.Count; i++)
		{
			Size size = TextRenderer.MeasureText(_names[i], Font);
			int num3 = Math.Max(70, size.Width + 28);
			if (num + num3 > num2 && i > _first)
			{
				break;
			}
			Rectangle rectangle = new Rectangle(num, 0, num3, base.Height);
			_rects.Add(rectangle);
			bool flag = i == _selected;
			if (flag)
			{
				using SolidBrush brush = new SolidBrush(TabBack);
				graphics.FillRectangle(brush, rectangle.X, rectangle.Y + 1, rectangle.Width, rectangle.Height - 1);
				using SolidBrush brush2 = new SolidBrush(Accent);
				graphics.FillRectangle(brush2, rectangle.X, rectangle.Bottom - 3, rectangle.Width, 3);
			}
			else if (i == _hover)
			{
				using SolidBrush brush3 = new SolidBrush(Color.FromArgb(40, Accent));
				graphics.FillRectangle(brush3, rectangle.X, rectangle.Y + 1, rectangle.Width, rectangle.Height - 1);
			}
			TextRenderer.DrawText(graphics, _names[i], new Font(Font, flag ? FontStyle.Bold : FontStyle.Regular), rectangle, flag ? Accent : Text_, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix);
			num += num3;
		}
		_plus = new Rectangle(num + 8, 4, 22, 22);
		using Pen pen2 = new Pen(Accent, 2f);
		graphics.DrawEllipse(pen2, _plus);
		graphics.DrawLine(pen2, _plus.X + 6, _plus.Y + 11, _plus.Right - 6, _plus.Y + 11);
		graphics.DrawLine(pen2, _plus.X + 11, _plus.Y + 6, _plus.X + 11, _plus.Bottom - 6);
	}

	private void DrawArrow(Graphics g, Rectangle r, bool left, bool enabled)
	{
		using Pen pen = new Pen(enabled ? Text_ : Faint, 1.8f);
		int num = r.X + 7;
		int num2 = r.Y + 5;
		if (left)
		{
			g.DrawLines(pen, new Point[3]
			{
				new Point(num + 5, num2),
				new Point(num, num2 + 5),
				new Point(num + 5, num2 + 10)
			});
		}
		else
		{
			g.DrawLines(pen, new Point[3]
			{
				new Point(num, num2),
				new Point(num + 5, num2 + 5),
				new Point(num, num2 + 10)
			});
		}
	}

	protected override void OnMouseMove(MouseEventArgs e)
	{
		base.OnMouseMove(e);
		int num = _rects.FindIndex((Rectangle r) => r.Contains(e.Location));
		int num2 = ((num >= 0) ? (_first + num) : (-1));
		if (num2 != _hover)
		{
			_hover = num2;
			Invalidate();
		}
	}

	protected override void OnMouseLeave(EventArgs e)
	{
		base.OnMouseLeave(e);
		_hover = -1;
		Invalidate();
	}

	protected override void OnMouseUp(MouseEventArgs e)
	{
		base.OnMouseUp(e);
		if (e.Button != MouseButtons.Left)
		{
			return;
		}
		if (_left.Contains(e.Location))
		{
			_first = Math.Max(0, _first - 1);
			Invalidate();
			return;
		}
		if (_right.Contains(e.Location))
		{
			_first = Math.Min(Math.Max(0, _names.Count - 1), _first + 1);
			Invalidate();
			return;
		}
		if (_plus.Contains(e.Location))
		{
			this.AddRequested?.Invoke();
			return;
		}
		int num = _rects.FindIndex((Rectangle r) => r.Contains(e.Location));
		if (num >= 0)
		{
			int num2 = _first + num;
			if (num2 != _selected)
			{
				_selected = num2;
				Invalidate();
				this.SelectedIndexChanged?.Invoke(num2);
			}
		}
	}

	protected override void OnMouseWheel(MouseEventArgs e)
	{
		base.OnMouseWheel(e);
		_first = Math.Max(0, Math.Min(Math.Max(0, _names.Count - 1), _first + ((e.Delta < 0) ? 1 : (-1))));
		Invalidate();
	}
}
