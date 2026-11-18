using System.Collections.Generic;
using System.Drawing;

namespace PneuSim;

public class Gripper : DoubleActingCylinder
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override RectangleF LocalBounds => RectangleF.FromLTRB(0f, -16f, 150f, 40f);

	public Gripper()
	{
		TypeName = "Gripper";
		DisplayName = "Parallel gripper (double-acting)";
		base.StrokeLength = 10;
		base.StrokeTime = 0.25;
		base.BoreMm = 16.0;
		base.RodMm = 6.0;
		base.LoadMassKg = 0.1;
		base.FrictionN = 10.0;
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Insert(0, ("Jaws", (base.Position >= 0.995) ? "closed" : ((base.Position <= 0.005) ? "open" : "moving")));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.FillRectangle(r.BodyBrush, 0f, 0f, 120f, 30f);
		g.DrawRectangle(r.Line, 0f, 0f, 120f, 30f);
		g.DrawString("gripper", r.SmallFont, r.TextBrush, 40f, 9f);
		DrawPortStub(g, r, base.Ports[0]);
		DrawPortStub(g, r, base.Ports[1]);
		float num = (float)(22.0 * (1.0 - base.Position));
		using (SolidBrush brush = new SolidBrush(Color.FromArgb(70, 70, 70)))
		{
			g.FillRectangle(brush, 120f, 15f - num - 8f, 26f, 8f);
			g.FillRectangle(brush, 120f, 15f + num, 26f, 8f);
		}
		g.DrawLine(r.Thin, 140f, 15f - num, 140f, 15f + num);
		if (!string.IsNullOrWhiteSpace(base.RetractedMark))
		{
			g.DrawString(base.RetractedMark, r.SmallFont, r.MarkBrush, 2f, -14f);
		}
		if (!string.IsNullOrWhiteSpace(base.ExtendedMark))
		{
			g.DrawString(base.ExtendedMark, r.SmallFont, r.MarkBrush, 80f, -14f);
		}
		g.DrawString("1 close   2 open", r.SmallFont, r.TextBrush, 22f, 31f);
	}
}
