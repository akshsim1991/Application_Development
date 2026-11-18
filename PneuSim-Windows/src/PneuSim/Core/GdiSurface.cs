using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace PneuSim;

public class GdiSurface : DrawSurface, IDisposable
{
	private readonly Graphics _g;

	private readonly ColorScheme _scheme;

	private readonly Dictionary<string, Pen> _pens;

	private readonly Dictionary<int, SolidBrush> _brushes;

	public Graphics Graphics => _g;

	public GdiSurface(Graphics g, ColorScheme scheme = null)
	{
		_pens = new Dictionary<string, Pen>();
		_brushes = new Dictionary<int, SolidBrush>();
		_g = g;
		_scheme = scheme ?? ColorScheme.Light;
	}

	private Pen P(Pen pen)
	{
		if (!_scheme.IsDark)
		{
			return pen;
		}
		string key = $"{pen.Color.ToArgb()}|{pen.Width}|{(int)pen.DashStyle}|{(int)pen.LineJoin}";
		Pen value = null;
		if (!_pens.TryGetValue(key, out value))
		{
			value = new Pen(_scheme.Map(pen.Color), pen.Width)
			{
				DashStyle = pen.DashStyle,
				LineJoin = pen.LineJoin
			};
			_pens[key] = value;
		}
		return value;
	}

	private Brush B(Brush brush)
	{
		if (!_scheme.IsDark)
		{
			return brush;
		}
		Color c = DrawSurface.ColorOf(brush);
		SolidBrush value = null;
		if (!_brushes.TryGetValue(c.ToArgb(), out value))
		{
			value = new SolidBrush(_scheme.Map(c));
			_brushes[c.ToArgb()] = value;
		}
		return value;
	}

	public override void DrawLine(Pen pen, float x1, float y1, float x2, float y2)
	{
		_g.DrawLine(P(pen), x1, y1, x2, y2);
	}

	public override void DrawLines(Pen pen, PointF[] points)
	{
		if (points.Length >= 2)
		{
			_g.DrawLines(P(pen), points);
		}
	}

	public override void DrawPolygon(Pen pen, PointF[] points)
	{
		_g.DrawPolygon(P(pen), points);
	}

	public override void FillPolygon(Brush brush, PointF[] points)
	{
		_g.FillPolygon(B(brush), points);
	}

	public override void DrawRectangle(Pen pen, float x, float y, float w, float h)
	{
		_g.DrawRectangle(P(pen), x, y, w, h);
	}

	public override void FillRectangle(Brush brush, float x, float y, float w, float h)
	{
		_g.FillRectangle(B(brush), x, y, w, h);
	}

	public override void DrawEllipse(Pen pen, float x, float y, float w, float h)
	{
		_g.DrawEllipse(P(pen), x, y, w, h);
	}

	public override void FillEllipse(Brush brush, float x, float y, float w, float h)
	{
		_g.FillEllipse(B(brush), x, y, w, h);
	}

	public override void DrawArc(Pen pen, float x, float y, float w, float h, float startAngle, float sweepAngle)
	{
		_g.DrawArc(P(pen), x, y, w, h, startAngle, sweepAngle);
	}

	public override void DrawString(string text, Font font, Brush brush, float x, float y)
	{
		_g.DrawString(text, font, B(brush), x, y);
	}

	public override SizeF MeasureString(string text, Font font)
	{
		return _g.MeasureString(text, font);
	}

	public override object Save()
	{
		return _g.Save();
	}

	public override void Restore(object state)
	{
		_g.Restore((GraphicsState)state);
	}

	public override void MultiplyTransform(Matrix m)
	{
		_g.MultiplyTransform(m);
	}

	public override void TranslateTransform(float dx, float dy)
	{
		_g.TranslateTransform(dx, dy);
	}

	public override void ScaleTransform(float sx, float sy)
	{
		_g.ScaleTransform(sx, sy);
	}

	public void Dispose()
	{
		foreach (Pen value in _pens.Values)
		{
			value.Dispose();
		}
		foreach (SolidBrush value2 in _brushes.Values)
		{
			value2.Dispose();
		}
	}

	void IDisposable.Dispose()
	{
		//ILSpy generated this explicit interface implementation from .override directive in Dispose
		this.Dispose();
	}
}
