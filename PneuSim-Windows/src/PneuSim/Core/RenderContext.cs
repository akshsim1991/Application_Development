using System;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace PneuSim;

public class RenderContext : IDisposable
{
	public static readonly Color PressureColor = Color.FromArgb(0, 84, 200);

	public static readonly Color IdleTubeColor = Color.FromArgb(120, 170, 230);

	public static readonly Color EnergizedColor = Color.FromArgb(215, 30, 30);

	public static readonly Color HydraulicColor = Color.FromArgb(200, 90, 0);

	public static readonly Color VacuumColor = Color.FromArgb(0, 150, 140);

	public static readonly Color FaultColor = Color.FromArgb(220, 0, 90);

	public readonly Pen Line;

	public readonly Pen Thin;

	public readonly Pen Dashed;

	public readonly Pen Pressure;

	public readonly Pen PressureDashed;

	public readonly Pen Energized;

	public readonly Pen HydraulicPressure;

	public readonly Pen Vacuum;

	public readonly Font Font;

	public readonly Font SmallFont;

	public readonly SolidBrush TextBrush;

	public readonly SolidBrush MarkBrush;

	public readonly SolidBrush BodyBrush;

	public bool Simulating { get; set; }

	public bool Interactive { get; set; }

	public RenderContext()
	{
		Line = new Pen(Color.Black, 1.6f);
		Thin = new Pen(Color.Black, 1f);
		Dashed = new Pen(Color.Black, 1.2f)
		{
			DashStyle = DashStyle.Dash
		};
		Pressure = new Pen(PressureColor, 2.4f);
		PressureDashed = new Pen(PressureColor, 1.8f)
		{
			DashStyle = DashStyle.Dash
		};
		Energized = new Pen(EnergizedColor, 2f);
		HydraulicPressure = new Pen(HydraulicColor, 2.4f);
		Vacuum = new Pen(VacuumColor, 2.2f)
		{
			DashStyle = DashStyle.Dot
		};
		Font = new Font("Segoe UI", 8f);
		SmallFont = new Font("Segoe UI", 7f);
		TextBrush = new SolidBrush(Color.Black);
		MarkBrush = new SolidBrush(Color.FromArgb(160, 40, 40));
		BodyBrush = new SolidBrush(Color.White);
	}

	public static Color ActiveColor(PortKind kind)
	{
		return kind switch
		{
			PortKind.Electric => EnergizedColor, 
			PortKind.Hydraulic => HydraulicColor, 
			_ => PressureColor, 
		};
	}

	public Pen PenFor(Port p)
	{
		if (Simulating && p != null && p.IsPressurized)
		{
			return p.Kind switch
			{
				PortKind.Electric => Energized, 
				PortKind.Hydraulic => HydraulicPressure, 
				_ => Pressure, 
			};
		}
		return Line;
	}

	public Pen DashedFor(Port p)
	{
		if (Simulating && p != null && p.IsPressurized)
		{
			return PressureDashed;
		}
		return Dashed;
	}

	public void Dispose()
	{
		Line.Dispose();
		Thin.Dispose();
		Dashed.Dispose();
		Pressure.Dispose();
		PressureDashed.Dispose();
		Energized.Dispose();
		HydraulicPressure.Dispose();
		Vacuum.Dispose();
		Font.Dispose();
		SmallFont.Dispose();
		TextBrush.Dispose();
		MarkBrush.Dispose();
		BodyBrush.Dispose();
	}

	void IDisposable.Dispose()
	{
		//ILSpy generated this explicit interface implementation from .override directive in Dispose
		this.Dispose();
	}
}
