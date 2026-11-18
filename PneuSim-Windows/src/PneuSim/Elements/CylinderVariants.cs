using System;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

/// <summary>Drawing and area helpers shared by the tandem, double-end and telescopic cylinders.</summary>
internal static class CylinderVariants
{
	/// <summary>Diameter (mm) of telescopic stage k: stage 0 is the largest (extends first), stage n the rod.</summary>
	public static double Diameter(double boreMm, double rodMm, int stages, int k)
	{
		double num = Math.Min(rodMm, boreMm * 0.8);
		return boreMm - (boreMm - num) * (double)k / (double)stages;
	}

	public static int Stage(double position, int stages)
	{
		return Math.Max(0, Math.Min(stages - 1, (int)(position * (double)stages)));
	}

	/// <summary>Area (m2) pushing the stage that is moving at this position when the cylinder extends.</summary>
	public static double TelescopicCapArea(double boreMm, double rodMm, int stages, double position)
	{
		double num = Diameter(boreMm, rodMm, stages, Stage(position, stages)) / 1000.0;
		return Math.PI / 4.0 * num * num;
	}

	/// <summary>Ring area (m2) pushing the moving stage back when the cylinder retracts.</summary>
	public static double TelescopicRodArea(double boreMm, double rodMm, int stages, double position)
	{
		int num = Stage(position, stages);
		double num2 = Diameter(boreMm, rodMm, stages, num) / 1000.0;
		double num3 = Diameter(boreMm, rodMm, stages, num + 1) / 1000.0;
		return Math.PI / 4.0 * Math.Max(1E-06, num2 * num2 - num3 * num3);
	}

	public static void DrawTelescopic(DrawSurface g, RenderContext r, double position, int stages)
	{
		float num = 92f / (float)stages;
		float num2 = 0f;
		for (int i = 0; i < stages; i++)
		{
			num2 += num * (float)Math.Max(0.0, Math.Min(1.0, position * (double)stages - (double)i));
			float num3 = 6f + (float)(stages - 1 - i) * 5f;
			float num4 = 120f + num2 + 8f;
			g.FillRectangle(r.BodyBrush, 118f, 15f - num3 / 2f, num4 - 118f, num3);
			g.DrawRectangle(r.Line, 118f, 15f - num3 / 2f, num4 - 118f, num3);
		}
	}

	public static void DrawSecondPiston(DrawSurface g, float pistonX, int extra)
	{
		using SolidBrush brush = new SolidBrush(Color.FromArgb(70, 70, 70));
		for (int i = 1; i <= extra; i++)
		{
			g.FillRectangle(brush, pistonX + 14f * (float)i, 1.5f, 8f, 27f);
		}
	}

	public static void DrawLeftRod(DrawSurface g, RenderContext r, float pistonX)
	{
		float num = pistonX - 100f;
		g.FillRectangle(r.BodyBrush, num, 12f, 0f - num + 1f, 6f);
		g.DrawRectangle(r.Line, num, 12f, 0f - num + 1f, 6f);
	}
}

/// <summary>Tandem cylinder: two or more pistons on one rod, so the force doubles for the same bore and pressure.</summary>
public class TandemCylinder : DoubleActingCylinder
{
	private int _pistons = 2;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Cylinder")]
	[DisplayName("Number of pistons")]
	[Description("Pistons on the common rod (2 to 4). Force and air use are multiplied by this number.")]
	public int Pistons
	{
		get
		{
			return _pistons;
		}
		set
		{
			_pistons = Math.Max(2, Math.Min(4, value));
		}
	}

	public TandemCylinder()
	{
		TypeName = "TandemCylinder";
		DisplayName = "Tandem cylinder";
	}

	protected override double CapArea()
	{
		return base.CapArea() * (double)_pistons;
	}

	protected override double RodArea()
	{
		return base.RodArea() * (double)_pistons;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		CylinderVariants.DrawSecondPiston(g, base.PistonX, _pistons - 1);
		DrawPortStub(g, r, base.Ports[0]);
		DrawPortStub(g, r, base.Ports[1]);
	}
}

/// <summary>Double-end (through-rod) double-acting cylinder: the rod comes out on both sides, so the force is the same in both directions.</summary>
public class ThroughRodCylinder : DoubleActingCylinder
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override RectangleF LocalBounds => RectangleF.FromLTRB(-100f, 0f, 220f, 40f);

	protected override double RodSideRatio => 1.0;

	public ThroughRodCylinder()
	{
		TypeName = "ThroughRodCylinder";
		DisplayName = "Double-end cylinder (through rod)";
	}

	protected override double CapArea()
	{
		return base.RodArea();
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		CylinderVariants.DrawLeftRod(g, r, base.PistonX);
		DrawPortStub(g, r, base.Ports[0]);
		DrawPortStub(g, r, base.Ports[1]);
	}
}

/// <summary>Hydraulic double-end (through-rod) cylinder.</summary>
public class HydraulicThroughRodCylinder : HydraulicCylinder
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	public override RectangleF LocalBounds => RectangleF.FromLTRB(-100f, 0f, 220f, 40f);

	protected override double RodSideRatio => 1.0;

	public HydraulicThroughRodCylinder()
	{
		TypeName = "HydraulicThroughRodCylinder";
		DisplayName = "Hydraulic double-end cylinder (through rod)";
	}

	protected override double CapArea()
	{
		return base.RodArea();
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		CylinderVariants.DrawLeftRod(g, r, base.PistonX);
		DrawPortStub(g, r, base.Ports[0]);
		DrawPortStub(g, r, base.Ports[1]);
	}
}

/// <summary>Telescopic double-acting cylinder (compressed air): nested stages, the largest extends first with the most force and least speed.</summary>
public class TelescopicCylinder : DoubleActingCylinder
{
	private int _stages = 3;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Cylinder")]
	[DisplayName("Number of stages")]
	[Description("Nested stages (2 to 4). The bore is the largest stage and the rod diameter the smallest; the stroke is shared equally. The largest stage extends first.")]
	public int Stages
	{
		get
		{
			return _stages;
		}
		set
		{
			_stages = Math.Max(2, Math.Min(4, value));
		}
	}

	public TelescopicCylinder()
	{
		TypeName = "TelescopicCylinder";
		DisplayName = "Telescopic cylinder";
		base.RodMm = 16.0;
		base.BoreMm = 50.0;
	}

	protected override double CapArea()
	{
		return CylinderVariants.TelescopicCapArea(BoreMm, RodMm, _stages, base.Position);
	}

	protected override double RodArea()
	{
		return CylinderVariants.TelescopicRodArea(BoreMm, RodMm, _stages, base.Position);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		CylinderVariants.DrawTelescopic(g, r, base.Position, _stages);
		DrawPortStub(g, r, base.Ports[0]);
		DrawPortStub(g, r, base.Ports[1]);
	}
}

/// <summary>Telescopic single-acting cylinder (compressed air, spring return).</summary>
public class TelescopicSingleActingCylinder : SingleActingCylinder
{
	private int _stages = 3;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Cylinder")]
	[DisplayName("Number of stages")]
	[Description("Nested stages (2 to 4). The largest stage extends first.")]
	public int Stages
	{
		get
		{
			return _stages;
		}
		set
		{
			_stages = Math.Max(2, Math.Min(4, value));
		}
	}

	public TelescopicSingleActingCylinder()
	{
		TypeName = "TelescopicSingleActingCylinder";
		DisplayName = "Telescopic cylinder, single-acting";
		base.RodMm = 16.0;
		base.BoreMm = 50.0;
	}

	protected override double CapArea()
	{
		return CylinderVariants.TelescopicCapArea(BoreMm, RodMm, _stages, base.Position);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		CylinderVariants.DrawTelescopic(g, r, base.Position, _stages);
		Symbols.Spring(g, r.Thin, base.PistonX + 10f, 118f, 15f, 10f);
		DrawPortStub(g, r, base.Ports[0]);
	}
}

/// <summary>Hydraulic telescopic double-acting cylinder.</summary>
public class HydraulicTelescopicCylinder : HydraulicCylinder
{
	private int _stages = 3;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Cylinder")]
	[DisplayName("Number of stages")]
	[Description("Nested stages (2 to 4). The largest stage extends first.")]
	public int Stages
	{
		get
		{
			return _stages;
		}
		set
		{
			_stages = Math.Max(2, Math.Min(4, value));
		}
	}

	public HydraulicTelescopicCylinder()
	{
		TypeName = "HydraulicTelescopicCylinder";
		DisplayName = "Hydraulic telescopic cylinder";
		base.BoreMm = 63.0;
		base.RodMm = 25.0;
	}

	protected override double CapArea()
	{
		return CylinderVariants.TelescopicCapArea(BoreMm, RodMm, _stages, base.Position);
	}

	protected override double RodArea()
	{
		return CylinderVariants.TelescopicRodArea(BoreMm, RodMm, _stages, base.Position);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		CylinderVariants.DrawTelescopic(g, r, base.Position, _stages);
		DrawPortStub(g, r, base.Ports[0]);
		DrawPortStub(g, r, base.Ports[1]);
	}
}

/// <summary>Hydraulic telescopic single-acting cylinder.</summary>
public class HydraulicTelescopicSingleActingCylinder : HydraulicSingleActingCylinder
{
	private int _stages = 3;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Cylinder")]
	[DisplayName("Number of stages")]
	[Description("Nested stages (2 to 4). The largest stage extends first.")]
	public int Stages
	{
		get
		{
			return _stages;
		}
		set
		{
			_stages = Math.Max(2, Math.Min(4, value));
		}
	}

	public HydraulicTelescopicSingleActingCylinder()
	{
		TypeName = "HydraulicTelescopicSingleActingCylinder";
		DisplayName = "Hydraulic telescopic cylinder, single-acting";
		base.BoreMm = 63.0;
		base.RodMm = 25.0;
	}

	protected override double CapArea()
	{
		return CylinderVariants.TelescopicCapArea(BoreMm, RodMm, _stages, base.Position);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		DrawBody(g, r);
		CylinderVariants.DrawTelescopic(g, r, base.Position, _stages);
		Symbols.Spring(g, r.Thin, base.PistonX + 10f, 118f, 15f, 10f);
		DrawPortStub(g, r, base.Ports[0]);
	}
}
