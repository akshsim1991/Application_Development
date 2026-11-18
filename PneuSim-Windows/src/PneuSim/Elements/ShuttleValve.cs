using System.Drawing;

namespace PneuSim;

public class ShuttleValve : LogicValve
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public ShuttleValve()
	{
		TypeName = "ShuttleValve";
		DisplayName = "Shuttle valve (OR)";
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawFrame(g, r);
		g.DrawLine(r.Line, 20f, 13f, 16f, 20f);
		g.DrawLine(r.Line, 16f, 20f, 20f, 27f);
		g.DrawLine(r.Line, 60f, 13f, 64f, 20f);
		g.DrawLine(r.Line, 64f, 20f, 60f, 27f);
		int num = ((Side == 0) ? 50 : 21);
		using (SolidBrush brush = new SolidBrush(Color.FromArgb(70, 70, 70)))
		{
			g.FillEllipse(brush, num, 14f, 10f, 12f);
		}
		g.DrawLine(r.PenFor(base.Ports[Side]), (Side == 0) ? 16 : 64, 20f, 40f, 20f);
		g.DrawLine(r.PenFor(base.Ports[Side]), 40f, 20f, 40f, 10f);
	}

	public override bool UpdateLogic(Simulator sim)
	{
		double pressure = base.Ports[0].Pressure;
		double pressure2 = base.Ports[1].Pressure;
		int num = ((!(pressure > pressure2 + 0.01)) ? ((pressure2 > pressure + 0.01) ? 1 : Side) : 0);
		if (num == Side)
		{
			return false;
		}
		Side = num;
		return true;
	}
}
