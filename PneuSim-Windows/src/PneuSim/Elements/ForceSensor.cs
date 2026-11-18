using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;

namespace PneuSim;

public class ForceSensor : CircuitElement
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Force sensor")]
	[DisplayName("Cylinder")]
	[Description("Label of the cylinder whose rod force is measured, e.g. 1A.")]
	public string Cylinder { get; set; }

	[Browsable(false)]
	public double Force { get; set; }

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 70f, 40f);

	public override PointF LabelAnchor => new PointF(0f, 0f);

	public ForceSensor()
	{
		TypeName = "ForceSensor";
		DisplayName = "Force sensor";
		Cylinder = "1A";
	}

	public CylinderBase FindCylinder(IEnumerable<CircuitElement> elements)
	{
		return elements.OfType<CylinderBase>().FirstOrDefault([SpecialName] (CylinderBase c) => string.Equals(c.Label?.Trim(), Cylinder?.Trim(), StringComparison.OrdinalIgnoreCase));
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Force", string.Format("{0:0} N ({1})", Force, (Force >= 0.0) ? "pushing" : "pulling")));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.FillRectangle(r.BodyBrush, 0f, 6f, 70f, 30f);
		g.DrawRectangle(r.Line, 0f, 6f, 70f, 30f);
		g.DrawLine(r.Line, 8f, 21f, 22f, 21f);
		Symbols.Arrow(g, r.Thin, new PointF(15f, 10f), new PointF(15f, 19f), 4f);
		Symbols.Arrow(g, r.Thin, new PointF(15f, 32f), new PointF(15f, 23f), 4f);
		g.DrawString(r.Simulating ? $"{Force:0} N" : $"F {Cylinder}", r.Font, r.TextBrush, 26f, 13f);
	}

	public override void ResetSim()
	{
		base.ResetSim();
		Force = 0.0;
	}
}
