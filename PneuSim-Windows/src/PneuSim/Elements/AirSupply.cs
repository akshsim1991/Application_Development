using System;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class AirSupply : CircuitElement
{
	private double _pressure;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Supply")]
	[DisplayName("Pressure (bar)")]
	[Description("Operating pressure delivered by the supply.")]
	public double Pressure
	{
		get
		{
			return _pressure;
		}
		set
		{
			_pressure = Math.Max(0.5, Math.Min(16.0, value));
		}
	}

	public override RectangleF LocalBounds => new RectangleF(4f, 0f, 32f, 44f);

	public override PointF LabelAnchor => new PointF(36f, 24f);

	public AirSupply()
	{
		_pressure = 6.0;
		TypeName = "AirSupply";
		DisplayName = "Compressed air supply";
		AddPort("1", 20f, 0f, 0f, -1f);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Pen pen = r.PenFor(base.Ports[0]);
		g.DrawLine(pen, 20f, 0f, 20f, 18f);
		g.FillEllipse(r.BodyBrush, 8f, 18f, 24f, 24f);
		g.DrawEllipse(r.Line, 8f, 18f, 24f, 24f);
		Symbols.EnergyTriangle(g, r, new PointF[3]
		{
			new PointF(20f, 22f),
			new PointF(14f, 32f),
			new PointF(26f, 32f)
		}, hydraulic: false, r.Simulating);
		g.DrawString($"{Pressure:0.#} bar", r.SmallFont, r.TextBrush, 34f, 30f);
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.LowOutput };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.LowOutput)
		{
			return base.FaultDescription(kind);
		}
		return "Supply pressure too low (compressor problem or a big leak elsewhere in the plant)";
	}

	public override void AddTerminals(Simulator sim)
	{
		sim.AddSupply(base.Ports[0], (base.Fault == FaultKind.LowOutput) ? (Pressure * 0.4) : Pressure);
	}
}
