using System.Drawing;

namespace PneuSim;

public class Silencer : CircuitElement
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override RectangleF LocalBounds => new RectangleF(8f, 0f, 24f, 30f);

	public Silencer()
	{
		TypeName = "Silencer";
		DisplayName = "Silencer";
		AddPort("1", 20f, 0f, 0f, -1f);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.Line, 20f, 0f, 20f, 8f);
		g.DrawPolygon(r.Line, new PointF[3]
		{
			new PointF(20f, 8f),
			new PointF(10f, 22f),
			new PointF(30f, 22f)
		});
		g.DrawRectangle(r.Line, 10f, 22f, 20f, 6f);
		int num = 13;
		do
		{
			g.DrawLine(r.Thin, num, 22f, num, 28f);
			num = checked(num + 4);
		}
		while (num <= 27);
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
		return "Silencer clogged with oil and dirt: the air escapes very slowly";
	}

	public override void AddTerminals(Simulator sim)
	{
		sim.AddExhaust(base.Ports[0], (base.Fault == FaultKind.Blocked) ? 0.08 : 1.0);
	}
}
