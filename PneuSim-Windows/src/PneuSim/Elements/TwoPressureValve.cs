using System.Drawing;

namespace PneuSim;

public class TwoPressureValve : LogicValve
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public TwoPressureValve()
	{
		TypeName = "TwoPressureValve";
		DisplayName = "Two-pressure valve (AND)";
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawFrame(g, r);
		int num = ((Side == 0) ? (-5) : 5);
		checked
		{
			g.DrawRectangle(r.Line, 20 + num, 14f, 6f, 12f);
			g.DrawRectangle(r.Line, 54 + num, 14f, 6f, 12f);
			g.DrawLine(r.Line, 26 + num, 20f, 54 + num, 20f);
			Pen pen = r.PenFor(base.Ports[2]);
			g.DrawLine(pen, 40f, 10f, 40f, 20f);
		}
	}

	public override bool UpdateLogic(Simulator sim)
	{
		double pressure = base.Ports[0].Pressure;
		double pressure2 = base.Ports[1].Pressure;
		int num = ((!(pressure < pressure2 - 0.01)) ? ((pressure2 < pressure - 0.01) ? 1 : Side) : 0);
		if (num == Side)
		{
			return false;
		}
		Side = num;
		return true;
	}
}
