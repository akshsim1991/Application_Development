using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class CheckValve : CircuitElement
{
	public override string TypeName { get; }

	public override string DisplayName
	{
		get
		{
			if (!Hydraulic)
			{
				return "Check valve";
			}
			return "Hydraulic check valve";
		}
	}

	[Category("Medium")]
	[DisplayName("Hydraulic")]
	[Description("True: fitted in a hydraulic (oil) line. False: compressed air.")]
	public bool Hydraulic
	{
		get
		{
			return base.Ports[0].Kind == PortKind.Hydraulic;
		}
		set
		{
			foreach (Port port in base.Ports)
			{
				port.Kind = (value ? PortKind.Hydraulic : PortKind.Pneumatic);
			}
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 8f, 60f, 24f);

	public CheckValve()
	{
		TypeName = "CheckValve";
		AddPort("1", 0f, 20f, -1f, 0f);
		AddPort("2", 60f, 20f, 1f, 0f);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 0f, 20f, 24f, 20f);
		g.DrawLine(r.PenFor(base.Ports[1]), 36f, 20f, 60f, 20f);
		g.DrawLine(r.Line, 30f, 12f, 24f, 20f);
		g.DrawLine(r.Line, 24f, 20f, 30f, 28f);
		g.FillEllipse(r.BodyBrush, 26f, 14f, 12f, 12f);
		g.DrawEllipse(r.Line, 26f, 14f, 12f, 12f);
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[2]
		{
			FaultKind.Leak,
			FaultKind.Blocked
		};
	}

	public override string FaultDescription(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.Leak => "Seat damaged: it also lets flow through in the blocked direction", 
			FaultKind.Blocked => "Stuck closed: nothing flows through it", 
			_ => base.FaultDescription(kind), 
		};
	}

	public override void AddEdges(Simulator sim)
	{
		switch (base.Fault)
		{
		case FaultKind.Leak:
			sim.AddEdge(base.Ports[0], base.Ports[1], 1.0, 0.4);
			break;
		default:
			sim.AddEdge(base.Ports[0], base.Ports[1], 1.0, 0.0);
			break;
		case FaultKind.Blocked:
			break;
		}
	}
}
