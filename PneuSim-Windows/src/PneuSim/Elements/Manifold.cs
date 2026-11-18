using System;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

/// <summary>One block with an inlet and several outlets, all joined inside. Use it when one supply must feed many tubes or hoses.</summary>
public class Manifold : CircuitElement
{
	private int _outlets = 4;

	private PortKind _medium = PortKind.Pneumatic;

	public override string TypeName { get; }

	public override string DisplayName => (_medium == PortKind.Hydraulic) ? "Hydraulic manifold" : "Manifold (distribution block)";

	[Category("Manifold")]
	[DisplayName("Outlets")]
	[Description("Number of outlet connections (2 to 8). The inlet and all outlets are joined inside the block.")]
	public int Outlets
	{
		get
		{
			return _outlets;
		}
		set
		{
			int n = Math.Max(2, Math.Min(8, value));
			if (n != _outlets || Ports.Count == 0)
			{
				_outlets = n;
				Rebuild();
			}
		}
	}

	[Category("Manifold")]
	[DisplayName("Joins")]
	[Description("Air tubes or hydraulic hoses.")]
	public PortKind Medium
	{
		get
		{
			return _medium;
		}
		set
		{
			if (value == PortKind.Electric)
			{
				value = PortKind.Pneumatic;
			}
			_medium = value;
			foreach (Port p in Ports)
			{
				p.Kind = value;
			}
		}
	}

	private float Width => 20f * (float)_outlets;

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, Width, 40f);

	public override PointF LabelAnchor => new PointF(Width + 4f, 14f);

	public Manifold()
	{
		TypeName = "Manifold";
		Rebuild();
	}

	private void Rebuild()
	{
		Ports.Clear();
		float inletX = (float)(Math.Round(Width / 2f / 10f) * 10.0);
		AddPort("1", inletX, 0f, 0f, -1f, vents: false, _medium);
		for (int i = 0; i < _outlets; i++)
		{
			AddPort((i + 2).ToString(), 10f + 20f * (float)i, 40f, 0f, 1f, vents: false, _medium);
		}
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(Ports[0]), Ports[0].Local.X, 0f, Ports[0].Local.X, 14f);
		for (int i = 1; i < Ports.Count; i++)
		{
			g.DrawLine(r.PenFor(Ports[i]), Ports[i].Local.X, 26f, Ports[i].Local.X, 40f);
		}
		g.FillRectangle(r.BodyBrush, 0f, 14f, Width, 12f);
		g.DrawRectangle(r.Line, 0f, 14f, Width, 12f);
	}

	public override void AddEdges(Simulator sim)
	{
		for (int i = 1; i < Ports.Count; i++)
		{
			sim.AddEdge(Ports[0], Ports[i]);
		}
	}
}
