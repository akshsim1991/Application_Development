using System.Drawing;
using System.Drawing.Drawing2D;

namespace PneuSim;

public abstract class DrawSurface
{
	protected static readonly Graphics Measurer = Graphics.FromImage(new Bitmap(1, 1));

	public abstract void DrawLine(Pen pen, float x1, float y1, float x2, float y2);

	public abstract void DrawLines(Pen pen, PointF[] points);

	public abstract void DrawPolygon(Pen pen, PointF[] points);

	public abstract void FillPolygon(Brush brush, PointF[] points);

	public abstract void DrawEllipse(Pen pen, float x, float y, float w, float h);

	public abstract void FillEllipse(Brush brush, float x, float y, float w, float h);

	public abstract void DrawArc(Pen pen, float x, float y, float w, float h, float startAngle, float sweepAngle);

	public abstract void DrawString(string text, Font font, Brush brush, float x, float y);

	public abstract SizeF MeasureString(string text, Font font);

	public abstract object Save();

	public abstract void Restore(object state);

	public abstract void MultiplyTransform(Matrix m);

	public abstract void TranslateTransform(float dx, float dy);

	public abstract void ScaleTransform(float sx, float sy);

	public void DrawLine(Pen pen, PointF a, PointF b)
	{
		DrawLine(pen, a.X, a.Y, b.X, b.Y);
	}

	public virtual void DrawRectangle(Pen pen, float x, float y, float w, float h)
	{
		DrawPolygon(pen, new PointF[4]
		{
			new PointF(x, y),
			new PointF(x + w, y),
			new PointF(x + w, y + h),
			new PointF(x, y + h)
		});
	}

	public virtual void FillRectangle(Brush brush, float x, float y, float w, float h)
	{
		FillPolygon(brush, new PointF[4]
		{
			new PointF(x, y),
			new PointF(x + w, y),
			new PointF(x + w, y + h),
			new PointF(x, y + h)
		});
	}

	public void FillRectangle(Brush brush, RectangleF r)
	{
		FillRectangle(brush, r.X, r.Y, r.Width, r.Height);
	}

	public void DrawString(string text, Font font, Brush brush, RectangleF r)
	{
		DrawString(text, font, brush, r.X, r.Y);
	}

	public void DrawRectangle(Pen pen, RectangleF r)
	{
		DrawRectangle(pen, r.X, r.Y, r.Width, r.Height);
	}

	public static SizeF Measure(string text, Font font)
	{
		lock (Measurer)
		{
			return Measurer.MeasureString(text, font);
		}
	}

	public static Color ColorOf(Brush brush)
	{
		if (brush is SolidBrush solidBrush)
		{
			return solidBrush.Color;
		}
		return Color.Black;
	}
}
