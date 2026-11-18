using System;
using System.Drawing;

namespace PneuSim;

public class SemiRotaryActuator : DoubleActingCylinder
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override RectangleF LocalBounds => new RectangleF(0f, -6f, 120f, 46f);

	public SemiRotaryActuator()
	{
		TypeName = "SemiRotaryActuator";
		DisplayName = "Semi-rotary actuator";
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.FillRectangle(r.BodyBrush, 30f, -4f, 60f, 34f);
		g.DrawRectangle(r.Line, 30f, -4f, 60f, 34f);
		g.DrawLine(r.PenFor(base.Ports[0]), 10f, 40f, 10f, 13f);
		g.DrawLine(r.PenFor(base.Ports[0]), 10f, 13f, 30f, 13f);
		g.DrawLine(r.PenFor(base.Ports[1]), 110f, 40f, 110f, 13f);
		g.DrawLine(r.PenFor(base.Ports[1]), 110f, 13f, 90f, 13f);
		g.DrawArc(r.Thin, 46f, -1f, 28f, 28f, 180f, 180f);
		double num = Math.PI * (1.0 - base.Position);
		PointF pointF = new PointF((float)(60.0 + 13.0 * Math.Cos(num)), (float)(13.0 - 13.0 * Math.Sin(num)));
		using (Pen pen = new Pen(Color.FromArgb(70, 70, 70), 3f))
		{
			g.DrawLine(pen, 60f, 13f, pointF.X, pointF.Y);
		}
		g.FillEllipse(Brushes.Black, 57f, 10f, 6f, 6f);
		g.DrawString($"{base.Position * 180.0:0}°", r.SmallFont, r.TextBrush, 50f, 16f);
		if (!string.IsNullOrWhiteSpace(base.RetractedMark))
		{
			g.DrawString(base.RetractedMark, r.SmallFont, r.MarkBrush, 31f, -16f);
		}
		if (!string.IsNullOrWhiteSpace(base.ExtendedMark))
		{
			g.DrawString(base.ExtendedMark, r.SmallFont, r.MarkBrush, 70f, -16f);
		}
	}
}
