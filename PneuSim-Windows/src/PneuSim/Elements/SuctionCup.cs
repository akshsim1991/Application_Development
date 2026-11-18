using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class SuctionCup : CircuitElement
{
	private bool _part;

	public const double HoldVacuum = -0.3;

	private double _diameter;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Suction cup")]
	[DisplayName("Diameter (mm)")]
	[Description("Cup diameter; sets the holding force.")]
	public double DiameterMm
	{
		get
		{
			return _diameter;
		}
		set
		{
			_diameter = Math.Max(5.0, Math.Min(500.0, value));
		}
	}

	[Category("Suction cup")]
	[DisplayName("Workpiece at start")]
	[Description("True: a workpiece lies under the cup when the simulation starts.")]
	public bool WorkpieceAtStart { get; set; }

	[Browsable(false)]
	public bool WorkpiecePresent => _part;

	[Browsable(false)]
	public bool Sealed
	{
		get
		{
			if (_part)
			{
				return base.Fault != FaultKind.Leak;
			}
			return false;
		}
	}

	[Browsable(false)]
	public bool Holding
	{
		get
		{
			if (Sealed)
			{
				return base.Ports[0].Pressure <= -0.3;
			}
			return false;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 44f, 54f);

	public override PointF LabelAnchor => new PointF(40f, 22f);

	[Browsable(false)]
	public override bool IsManuallyOperated => true;

	public SuctionCup()
	{
		_part = true;
		TypeName = "SuctionCup";
		DisplayName = "Suction cup";
		_diameter = 40.0;
		WorkpieceAtStart = true;
		AddPort("1", 20f, 0f, 0f, -1f);
	}

	public double HoldingForce()
	{
		return Math.Max(0.0, 0.0 - base.Ports[0].Pressure) * 100000.0 * Math.PI / 4.0 * Math.Pow(_diameter / 1000.0, 2.0);
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.Leak };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.Leak)
		{
			return base.FaultDescription(kind);
		}
		return "Cup lip torn: air leaks in, so it cannot hold";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Workpiece", (!_part) ? "none" : (Holding ? "held" : "lying under the cup, not held")));
		list.Add(("Holding force", $"{HoldingForce():0} N"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		using (Pen pen = new Pen((r.Simulating && base.Ports[0].Pressure < -0.05) ? RenderContext.VacuumColor : Color.Black, 1.6f))
		{
			g.DrawLine(pen, 20f, 0f, 20f, 18f);
		}
		g.FillPolygon(r.BodyBrush, new PointF[4]
		{
			new PointF(14f, 18f),
			new PointF(26f, 18f),
			new PointF(36f, 32f),
			new PointF(4f, 32f)
		});
		g.DrawPolygon(r.Line, new PointF[4]
		{
			new PointF(14f, 18f),
			new PointF(26f, 18f),
			new PointF(36f, 32f),
			new PointF(4f, 32f)
		});
		if (r.Simulating ? _part : WorkpieceAtStart)
		{
			bool flag = r.Simulating && Holding;
			float y = ((flag || !r.Simulating) ? 33f : 37f);
			using (SolidBrush brush = new SolidBrush(flag ? Color.FromArgb(120, 160, 90) : Color.FromArgb(170, 170, 170)))
			{
				g.FillRectangle(brush, 0f, y, 40f, 8f);
			}
			g.DrawRectangle(r.Thin, 0f, y, 40f, 8f);
		}
		if (r.Simulating)
		{
			g.DrawString(Holding ? "held" : (_part ? "not held" : "no part"), r.SmallFont, r.TextBrush, 0f, 44f);
		}
	}

	public override void OnSimMouseDown(PointF local)
	{
		_part = !_part;
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_part = WorkpieceAtStart;
	}
}
