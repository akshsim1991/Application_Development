using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;

namespace PneuSim;

public class VectorSurface : DrawSurface
{
	private Matrix _matrix;

	public List<VectorShape> Shapes { get; }

	public VectorSurface()
	{
		_matrix = new Matrix();
		Shapes = new List<VectorShape>();
	}

	private PointF[] T(PointF[] pts)
	{
		PointF[] array = (PointF[])pts.Clone();
		_matrix.TransformPoints(array);
		return array;
	}

	private void AddPath(PointF[] pts, bool closed, Pen pen, Color fill)
	{
		if (pts.Length >= 2)
		{
			VectorShape vectorShape = new VectorShape
			{
				Points = T(pts),
				Closed = closed,
				Fill = fill
			};
			if (pen != null)
			{
				vectorShape.Stroke = pen.Color;
				vectorShape.StrokeWidth = pen.Width * ScaleFactor();
				vectorShape.Dashed = pen.DashStyle != DashStyle.Solid;
			}
			Shapes.Add(vectorShape);
		}
	}

	private float ScaleFactor()
	{
		PointF[] array = new PointF[1]
		{
			new PointF(1f, 0f)
		};
		_matrix.TransformVectors(array);
		return (float)Math.Sqrt(array[0].X * array[0].X + array[0].Y * array[0].Y);
	}

	private static PointF[] EllipsePoints(float x, float y, float w, float h, float start, float sweep, int n)
	{
		checked
		{
			PointF[] array = new PointF[n + 1];
			float num = x + w / 2f;
			float num2 = y + h / 2f;
			for (int i = 0; i <= n; i++)
			{
				double num3 = (double)(start + sweep * (float)i / (float)n) * Math.PI / 180.0;
				array[i] = new PointF((float)((double)num + (double)(w / 2f) * Math.Cos(num3)), (float)((double)num2 + (double)(h / 2f) * Math.Sin(num3)));
			}
			return array;
		}
	}

	public override void DrawLine(Pen pen, float x1, float y1, float x2, float y2)
	{
		AddPath(new PointF[2]
		{
			new PointF(x1, y1),
			new PointF(x2, y2)
		}, closed: false, pen, Color.Empty);
	}

	public override void DrawLines(Pen pen, PointF[] points)
	{
		AddPath(points, closed: false, pen, Color.Empty);
	}

	public override void DrawPolygon(Pen pen, PointF[] points)
	{
		AddPath(points, closed: true, pen, Color.Empty);
	}

	public override void FillPolygon(Brush brush, PointF[] points)
	{
		AddPath(points, closed: true, null, DrawSurface.ColorOf(brush));
	}

	public override void DrawEllipse(Pen pen, float x, float y, float w, float h)
	{
		AddPath(EllipsePoints(x, y, w, h, 0f, 360f, 36), closed: true, pen, Color.Empty);
	}

	public override void FillEllipse(Brush brush, float x, float y, float w, float h)
	{
		AddPath(EllipsePoints(x, y, w, h, 0f, 360f, 36), closed: true, null, DrawSurface.ColorOf(brush));
	}

	public override void DrawArc(Pen pen, float x, float y, float w, float h, float startAngle, float sweepAngle)
	{
		AddPath(EllipsePoints(x, y, w, h, startAngle, sweepAngle, 18), closed: false, pen, Color.Empty);
	}

	public override void DrawString(string text, Font font, Brush brush, float x, float y)
	{
		checked
		{
			if (!string.IsNullOrEmpty(text))
			{
				SizeF sizeF = DrawSurface.Measure("X", font);
				float num = sizeF.Height * 0.95f;
				string[] array = text.Replace("\r", "").Split('\n');
				float angleDeg = (float)(Math.Atan2(_matrix.Elements[1], _matrix.Elements[0]) * 180.0 / Math.PI);
				int num2 = array.Length - 1;
				for (int i = 0; i <= num2; i++)
				{
					PointF textPos = T(new PointF[1]
					{
						new PointF(x + font.Size * 0.12f, y + (float)i * num + sizeF.Height * 0.78f)
					})[0];
					Shapes.Add(new VectorShape
					{
						Text = array[i],
						TextPos = textPos,
						FontSize = font.SizeInPoints * 1.333f * ScaleFactor() * 0.98f,
						Bold = font.Bold,
						Stroke = DrawSurface.ColorOf(brush),
						AngleDeg = angleDeg
					});
				}
			}
		}
	}

	public override SizeF MeasureString(string text, Font font)
	{
		return DrawSurface.Measure(text, font);
	}

	public override object Save()
	{
		return _matrix.Clone();
	}

	public override void Restore(object state)
	{
		_matrix = ((Matrix)state).Clone();
	}

	public override void MultiplyTransform(Matrix m)
	{
		_matrix.Multiply(m, MatrixOrder.Prepend);
	}

	public override void TranslateTransform(float dx, float dy)
	{
		_matrix.Translate(dx, dy, MatrixOrder.Prepend);
	}

	public override void ScaleTransform(float sx, float sy)
	{
		_matrix.Scale(sx, sy, MatrixOrder.Prepend);
	}

	public RectangleF Bounds()
	{
		List<float> list = new List<float>();
		List<float> list2 = new List<float>();
		foreach (VectorShape shape in Shapes)
		{
			if (shape.IsText)
			{
				list.Add(shape.TextPos.X);
				list2.Add(shape.TextPos.Y);
				list.Add(shape.TextPos.X + shape.FontSize * 0.6f * (float)shape.Text.Length);
				list2.Add(shape.TextPos.Y - shape.FontSize);
				continue;
			}
			PointF[] points = shape.Points;
			for (int i = 0; i < points.Length; i = checked(i + 1))
			{
				PointF pointF = points[i];
				list.Add(pointF.X);
				list2.Add(pointF.Y);
			}
		}
		if (list.Count == 0)
		{
			return RectangleF.Empty;
		}
		return RectangleF.FromLTRB(list.Min(), list2.Min(), list.Max(), list2.Max());
	}
}
