using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class QuickExhaustValve : CircuitElement
{
	private bool _feeding;

	[Browsable(false)]
	public bool Feeding => _feeding;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 70f, 60f);

	public override PointF LabelAnchor => new PointF(56f, 22f);

	public QuickExhaustValve()
	{
		TypeName = "QuickExhaustValve";
		DisplayName = "Quick exhaust valve";
		AddPort("1", 0f, 30f, -1f, 0f);
		AddPort("2", 40f, 0f, 0f, -1f);
		AddPort("3", 40f, 60f, 0f, 1f, vents: true);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 0f, 30f, 20f, 30f);
		g.DrawLine(r.PenFor(base.Ports[1]), 40f, 0f, 40f, 18f);
		g.DrawLine(r.PenFor(base.Ports[2]), 40f, 42f, 40f, 60f);
		g.FillRectangle(r.BodyBrush, 20f, 18f, 34f, 24f);
		g.DrawRectangle(r.Line, 20f, 18f, 34f, 24f);
		using (SolidBrush brush = new SolidBrush(Color.FromArgb(70, 70, 70)))
		{
			if (_feeding)
			{
				g.FillRectangle(brush, 34f, 36f, 12f, 4f);
			}
			else
			{
				g.FillRectangle(brush, 22f, 24f, 4f, 12f);
			}
		}
		if (base.Ports[2].ConnectionCount == 0)
		{
			Symbols.Exhaust(g, r.Line, new PointF(40f, 60f), new PointF(0f, 1f));
		}
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_feeding = false;
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.Blocked };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.Blocked)
		{
			return base.FaultDescription(kind);
		}
		return "Exhaust port blocked: the air cannot get out here";
	}

	public override void AddEdges(Simulator sim)
	{
		if (_feeding)
		{
			sim.AddEdge(base.Ports[0], base.Ports[1], 1.0, 0.0);
		}
		else if (base.Fault != FaultKind.Blocked)
		{
			sim.AddEdge(base.Ports[1], base.Ports[2], 1.0, 0.0);
		}
	}

	public override bool UpdateLogic(Simulator sim)
	{
		bool flag = base.Ports[0].State == PortState.Pressurized;
		if (flag == _feeding)
		{
			return false;
		}
		_feeding = flag;
		return true;
	}
}
