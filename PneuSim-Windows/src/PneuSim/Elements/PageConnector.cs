using System;
using System.ComponentModel;
using System.Drawing;
using System.Linq;

namespace PneuSim;

public class PageConnector : CircuitElement
{
	private PortKind _medium;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Connector")]
	[DisplayName("Carries")]
	[Description("What the connector joins: tubes, wires or hydraulic lines.")]
	public PortKind Medium
	{
		get
		{
			return _medium;
		}
		set
		{
			_medium = value;
			base.Ports[0].Kind = value;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 50f, 20f);

	public override PointF LabelAnchor => new PointF(14f, 17f);

	public PageConnector()
	{
		_medium = PortKind.Pneumatic;
		TypeName = "PageConnector";
		DisplayName = "Page connector";
		AddPort("1", 0f, 10f, -1f, 0f);
		base.Label = "X1";
	}

	public bool Matches(PageConnector other)
	{
		if (other.Medium == Medium && string.Equals(other.Label?.Trim(), base.Label?.Trim(), StringComparison.OrdinalIgnoreCase))
		{
			return !string.IsNullOrWhiteSpace(base.Label);
		}
		return false;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		PointF[] points = new PointF[5]
		{
			new PointF(0f, 10f),
			new PointF(10f, 0f),
			new PointF(50f, 0f),
			new PointF(50f, 20f),
			new PointF(10f, 20f)
		};
		g.FillPolygon(r.BodyBrush, points);
		g.DrawPolygon(r.PenFor(base.Ports[0]), points);
		if (!string.IsNullOrEmpty(base.CrossReference))
		{
			g.DrawString(base.CrossReference, r.SmallFont, r.MarkBrush, 12f, 22f);
		}
	}

	public override void AddEdges(Simulator sim)
	{
		foreach (PageConnector item in sim.Circuit.Elements.OfType<PageConnector>())
		{
			if (item != this && item.Matches(this))
			{
				sim.AddEdge(base.Ports[0], item.Ports[0]);
			}
		}
	}
}
