using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Cutaways
{
	private static readonly Color VacuumTint = Color.FromArgb(120, 200, 190);

	public static Color Fluid(Port p, CutawayPalette pal)
	{
		if (p == null || !p.IsPressurized)
		{
			return pal.Vent;
		}
		return p.Kind switch
		{
			PortKind.Hydraulic => pal.Oil, 
			PortKind.Electric => Color.FromArgb(220, 40, 40), 
			_ => pal.Air, 
		};
	}

	public static void Zigzag(Graphics g, Pen pen, float x1, float x2, float cy, float amp, int turns = 8)
	{
		if (!(Math.Abs(x2 - x1) < 2f))
		{
			List<PointF> list = new List<PointF>();
			for (int i = 0; i <= turns; i = checked(i + 1))
			{
				list.Add(new PointF(x1 + (x2 - x1) * (float)i / (float)turns, cy + ((i == 0 || i == turns) ? 0f : ((i % 2 == 0) ? (0f - amp) : amp))));
			}
			g.DrawLines(pen, list.ToArray());
		}
	}

	private static void ZigzagV(Graphics g, Pen pen, float cx, float y1, float y2, float amp, int turns = 8)
	{
		if (!(Math.Abs(y2 - y1) < 2f))
		{
			List<PointF> list = new List<PointF>();
			for (int i = 0; i <= turns; i = checked(i + 1))
			{
				list.Add(new PointF(cx + ((i == 0 || i == turns) ? 0f : ((i % 2 == 0) ? (0f - amp) : amp)), y1 + (y2 - y1) * (float)i / (float)turns));
			}
			g.DrawLines(pen, list.ToArray());
		}
	}

	private static void Fit(Graphics g, RectangleF area, float w, float h)
	{
		float num = Math.Min(1.6f, Math.Min(area.Width / w, area.Height / h));
		g.TranslateTransform(area.X + (area.Width - w * num) / 2f, area.Y + (area.Height - h * num) / 2f);
		g.ScaleTransform(num, num);
	}

	private static void Fill(Graphics g, Color c, float x, float y, float w, float h)
	{
		using SolidBrush brush = new SolidBrush(c);
		g.FillRectangle(brush, x, y, w, h);
	}

	private static void Metal(Graphics g, CutawayPalette pal, float x, float y, float w, float h, bool dark = false)
	{
		Fill(g, dark ? pal.MetalDark : pal.Metal, x, y, w, h);
		using Pen pen = new Pen(pal.Edge);
		g.DrawRectangle(pen, x, y, w, h);
	}

	private static void Disc(Graphics g, Color c, Color edge, float cx, float cy, float r)
	{
		using SolidBrush brush = new SolidBrush(c);
		using Pen pen = new Pen(edge);
		g.FillEllipse(brush, cx - r, cy - r, 2f * r, 2f * r);
		g.DrawEllipse(pen, cx - r, cy - r, 2f * r, 2f * r);
	}

	private static void Txt(Graphics g, CutawayPalette pal, string s, float x, float y, bool bold = false, bool faint = false)
	{
		using Font font = new Font("Segoe UI", 8.5f, bold ? FontStyle.Bold : FontStyle.Regular);
		using SolidBrush brush = new SolidBrush(faint ? pal.SubText : pal.Text);
		g.DrawString(s, font, brush, x, y);
	}

	private static void FlowArrow(Graphics g, float x1, float y1, float x2, float y2)
	{
		using Pen pen = new Pen(Color.White, 2f)
		{
			EndCap = LineCap.ArrowAnchor
		};
		g.DrawLine(pen, x1, y1, x2, y2);
	}

	private static Color Lighter(Color c, Color back)
	{
		return Color.FromArgb(checked(c.R + back.R) / 2, checked(c.G + back.G) / 2, checked(c.B + back.B) / 2);
	}

	private static bool Flows(Port a, Port b)
	{
		if (a.State == PortState.Pressurized)
		{
			if (b.State == PortState.Pressurized)
			{
				return a.Pressure > b.Pressure + 0.05;
			}
			return true;
		}
		return false;
	}

	public static bool Supports(CircuitElement e)
	{
		if (!(e is PowerTerminal) && !(e is Junction) && !(e is TextNote))
		{
			return !(e is PageConnector);
		}
		return false;
	}

	public static string Draw(Graphics g, CircuitElement e, RectangleF area, CutawayPalette pal, double phase, bool simulating)
	{
		GraphicsState gstate = g.Save();
		try
		{
			bool flag = true;
			if (flag == e is SemiRotaryActuator)
			{
				return SemiRotary(g, (SemiRotaryActuator)e, area, pal);
			}
			if (flag == e is Gripper)
			{
				return GripperView(g, (Gripper)e, area, pal);
			}
			if (flag == e is PressureSwitch)
			{
				return PressureSwitchView(g, (PressureSwitch)e, area, pal);
			}
			if (flag == e is ShutOffValve)
			{
				return ShutOffView(g, (ShutOffValve)e, area, pal);
			}
			if (flag == e is VacuumGenerator)
			{
				return EjectorView(g, (VacuumGenerator)e, area, pal);
			}
			if (flag == e is SuctionCup)
			{
				return CupView(g, (SuctionCup)e, area, pal);
			}
			if (flag == e is Compressor)
			{
				return CompressorView(g, (Compressor)e, area, pal, phase);
			}
			if (flag == e is AirReceiver)
			{
				return ReceiverView(g, (AirReceiver)e, area, pal);
			}
			if (flag == e is FlowMeter)
			{
				return FlowMeterView(g, (FlowMeter)e, area, pal);
			}
			if (flag == e is ForceSensor)
			{
				return ForceSensorView(g, (ForceSensor)e, area, pal);
			}
			if (flag == e is ElectricCounter)
			{
				return CounterView(g, (ElectricCounter)e, area, pal, simulating);
			}
			if (flag == e is CylinderBase)
			{
				return Cylinder(g, (CylinderBase)e, area, pal);
			}
			if (flag == e is AirMotor)
			{
				return VaneMotor(g, e, ((AirMotor)e).Rpm, area, pal, phase);
			}
			if (flag == e is HydraulicMotor)
			{
				return VaneMotor(g, e, ((HydraulicMotor)e).Rpm, area, pal, phase);
			}
			if (flag == e is HydraulicPump)
			{
				return GearPump(g, (HydraulicPump)e, area, pal, phase, simulating);
			}
			if (flag == e is CheckValve)
			{
				return Check(g, (CheckValve)e, area, pal);
			}
			if (flag == e is ShuttleValve)
			{
				return Shuttle(g, (ShuttleValve)e, area, pal);
			}
			if (flag == e is TwoPressureValve)
			{
				return TwoPressure(g, (TwoPressureValve)e, area, pal);
			}
			if (flag == e is QuickExhaustValve)
			{
				return QuickExhaust(g, (QuickExhaustValve)e, area, pal);
			}
			if (flag == e is FlowControlValve)
			{
				return Throttle(g, (FlowControlValve)e, area, pal);
			}
			if (flag == e is CompensatedFlowControl)
			{
				return Compensated(g, (CompensatedFlowControl)e, area, pal);
			}
			if (flag == e is PressureRegulator)
			{
				return Regulator(g, (PressureRegulator)e, area, pal);
			}
			if (flag == e is ReliefValve)
			{
				return Relief(g, (ReliefValve)e, area, pal);
			}
			if (flag == e is CounterbalanceValve)
			{
				return Counterbalance(g, (CounterbalanceValve)e, area, pal);
			}
			if (flag == e is Accumulator)
			{
				return AccumulatorView(g, (Accumulator)e, area, pal);
			}
			if (flag == e is PressureGauge)
			{
				return Gauge(g, (PressureGauge)e, area, pal);
			}
			if (flag == e is Silencer)
			{
				return SilencerView(g, (Silencer)e, area, pal);
			}
			if (flag == e is AirSupply)
			{
				return Supply(g, (AirSupply)e, area, pal, simulating);
			}
			if (flag == e is HydraulicTank)
			{
				return Tank(g, (HydraulicTank)e, area, pal);
			}
			if (flag == e is ElectricCoil)
			{
				return Coil(g, (ElectricCoil)e, area, pal, simulating);
			}
			if (flag == e is ElectricContact)
			{
				return Contact(g, (ElectricContact)e, area, pal, simulating);
			}
			Txt(g, pal, "This is a wiring or drawing symbol; there is nothing inside to show.", area.X + 4f, area.Y + 10f, bold: false, faint: true);
			return "";
		}
		finally
		{
			g.Restore(gstate);
		}
	}

	private static string Cylinder(Graphics g, CylinderBase c, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 540f, 170f);
		bool flag = c.Ports.Count >= 2;
		bool flag2 = c.Ports[0].Kind == PortKind.Hydraulic;
		float num = (float)c.Position;
		Metal(g, pal, 30f, 50f, 232f, 70f);
		Fill(g, pal.Cavity, 42f, 60f, 208f, 50f);
		float num2 = 42f + num * 186f;
		Fill(g, Fluid(c.Ports[0], pal), 42f, 60f, num2 - 42f, 50f);
		if (flag)
		{
			Fill(g, Fluid(c.Ports[1], pal), num2 + 22f, 60f, 250f - num2 - 22f, 50f);
		}
		else
		{
			using Pen pen = new Pen(pal.Spring, 1.5f);
			Zigzag(g, pen, num2 + 22f, 248f, 85f, 18f, 10);
		}
		Metal(g, pal, num2, 60f, 22f, 50f, dark: true);
		Fill(g, Color.Black, num2 + 8f, 60f, 6f, 3f);
		Fill(g, Color.Black, num2 + 8f, 107f, 6f, 3f);
		float num3 = num2 + 22f + 220f;
		Metal(g, pal, num2 + 22f, 78f, num3 - num2 - 22f, 14f);
		Metal(g, pal, 250f, 74f, 12f, 22f, dark: true);
		Metal(g, pal, num3, 72f, 10f, 26f, dark: true);
		Fill(g, Fluid(c.Ports[0], pal), 44f, 26f, 12f, 34f);
		Txt(g, pal, flag2 ? "A" : "1", 44f, 8f, bold: true);
		if (flag)
		{
			Fill(g, Fluid(c.Ports[1], pal), 234f, 26f, 12f, 34f);
			Txt(g, pal, flag2 ? "B" : "2", 234f, 8f, bold: true);
		}
		else
		{
			Fill(g, pal.Vent, 236f, 46f, 8f, 14f);
			Txt(g, pal, "vent", 222f, 28f, bold: false, faint: true);
		}
		if (Math.Abs(c.LoadForceN) > 0.5)
		{
			using (Pen pen2 = new Pen(pal.Text, 2f)
			{
				EndCap = LineCap.ArrowAnchor
			})
			{
				if (c.LoadForceN > 0.0)
				{
					g.DrawLine(pen2, num3 + 50f, 85f, num3 + 14f, 85f);
				}
				else
				{
					g.DrawLine(pen2, num3 + 14f, 85f, num3 + 50f, 85f);
				}
			}
			Txt(g, pal, $"load {Math.Abs(c.LoadForceN):0} N", num3 + 4f, 100f, bold: false, faint: true);
		}
		string arg = string.Format("pressure side {0}: {1:0.0} bar", flag2 ? "A" : "1", c.CapPressure) + (flag ? string.Format(",  side {0}: {1:0.0} bar", flag2 ? "B" : "2", c.RodPressure) : "");
		return $"Piston at {num * (float)c.StrokeLength:0} of {c.StrokeLength} mm;  {arg}";
	}

	private static string SemiRotary(Graphics g, SemiRotaryActuator c, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 360f, 190f);
		float num = 180f;
		float num2 = 110f;
		Disc(g, pal.Metal, pal.Edge, num, num2, 75f);
		float num3 = 180f + (float)c.Position * 180f;
		using (SolidBrush brush = new SolidBrush(Fluid(c.Ports[0], pal)))
		{
			using SolidBrush brush2 = new SolidBrush(Fluid(c.Ports[1], pal));
			g.FillPie(brush, num - 62f, num2 - 62f, 124f, 124f, 180f, num3 - 180f);
			g.FillPie(brush2, num - 62f, num2 - 62f, 124f, 124f, num3, 360f - num3);
		}
		Metal(g, pal, num - 62f, num2, 124f, 10f, dark: true);
		double num4 = (double)num3 * Math.PI / 180.0;
		using (Pen pen = new Pen(pal.MetalDark, 9f))
		{
			g.DrawLine(pen, num, num2, (float)((double)num + 60.0 * Math.Cos(num4)), (float)((double)num2 + 60.0 * Math.Sin(num4)));
		}
		Disc(g, pal.MetalDark, pal.Edge, num, num2, 12f);
		Fill(g, Fluid(c.Ports[0], pal), num - 80f, num2 - 4f, 20f, 12f);
		Txt(g, pal, "1", num - 100f, num2 - 6f, bold: true);
		Fill(g, Fluid(c.Ports[1], pal), num + 60f, num2 - 4f, 20f, 12f);
		Txt(g, pal, "2", num + 86f, num2 - 6f, bold: true);
		return $"Vane turned to {c.Position * 180.0:0}° of 180°; air on 1 turns it clockwise, air on 2 turns it back.";
	}

	private static string VaneMotor(Graphics g, CircuitElement e, double rpm, RectangleF area, CutawayPalette pal, double phase)
	{
		Fit(g, area, 360f, 200f);
		float num = 180f;
		float num2 = 100f;
		Disc(g, pal.Metal, pal.Edge, num, num2, 80f);
		Port port = e.Ports[0];
		Port port2 = ((e.Ports.Count > 1) ? e.Ports[1] : null);
		using (SolidBrush brush = new SolidBrush(Fluid((rpm >= 0.0) ? port : port2, pal)))
		{
			using SolidBrush brush2 = new SolidBrush((port2 == null) ? pal.Vent : Fluid((rpm >= 0.0) ? port2 : port, pal));
			g.FillPie(brush, num - 66f, num2 - 66f, 132f, 132f, 90f, 180f);
			g.FillPie(brush2, num - 66f, num2 - 66f, 132f, 132f, 270f, 180f);
		}
		float num3 = num + 14f;
		int num4 = 0;
		checked
		{
			do
			{
				double num5 = (phase + (double)(num4 * 60)) * Math.PI / 180.0;
				double num6 = Math.Cos(num5);
				double num7 = Math.Sin(num5);
				float num8 = num3 - num;
				double num9 = (double)(0f - num8) * num6 + Math.Sqrt(Math.Pow((double)num8 * num6, 2.0) - (double)(num8 * num8 - 4356f));
				using (Pen pen = new Pen(pal.MetalDark, 4f))
				{
					g.DrawLine(pen, (float)((double)num3 + 30.0 * num6), (float)((double)num2 + 30.0 * num7), (float)((double)num3 + num9 * num6), (float)((double)num2 + num9 * num7));
				}
				num4++;
			}
			while (num4 <= 5);
			Disc(g, pal.MetalDark, pal.Edge, num3, num2, 42f);
			Disc(g, pal.Metal, pal.Edge, num3, num2, 8f);
			Fill(g, Fluid(port, pal), num - 120f, num2 - 7f, 56f, 14f);
			Txt(g, pal, (port2 == null) ? "in" : port.Name, num - 128f, num2 - 26f, bold: true);
			Fill(g, (port2 == null) ? pal.Vent : Fluid(port2, pal), num + 64f, num2 - 7f, 56f, 14f);
			Txt(g, pal, (port2 == null) ? "exhaust" : port2.Name, num + 100f, num2 - 26f, bold: true);
			if (!(Math.Abs(rpm) < 0.5))
			{
				return $"Turning at {Math.Abs(rpm):0} rpm: the flow pushes the vanes round.";
			}
			return "Stopped: no flow through the motor.";
		}
	}

	private static string GearPump(Graphics g, HydraulicPump p, RectangleF area, CutawayPalette pal, double phase, bool simulating)
	{
		Fit(g, area, 400f, 190f);
		Metal(g, pal, 110f, 40f, 180f, 110f);
		Disc(g, pal.Cavity, pal.Edge, 170f, 95f, 36f);
		Disc(g, pal.Cavity, pal.Edge, 230f, 95f, 36f);
		Color c = Lighter(pal.Oil, pal.Back);
		Fill(g, c, 30f, 85f, 104f, 20f);
		Fill(g, Fluid(p.Ports[0], pal), 266f, 85f, 104f, 20f);
		Gear(g, pal, 170f, 95f, (float)phase);
		Gear(g, pal, 230f, 95f, (float)(0.0 - phase + 18.0));
		Txt(g, pal, "from tank (suction)", 22f, 110f, bold: false, faint: true);
		Txt(g, pal, "P (pressure)", 290f, 110f, bold: true);
		Metal(g, pal, 180f, 150f, 40f, 30f, dark: true);
		Txt(g, pal, "M", 192f, 156f, bold: true);
		if (simulating && !p.Running)
		{
			return "Pump stopped (click it during simulation to start it).";
		}
		return $"The motor turns the gears; oil is carried round the outside and pushed out at P ({p.FlowLpm:0.#} l/min, {p.Ports[0].Pressure:0} bar).";
	}

	private static void Gear(Graphics g, CutawayPalette pal, float cx, float cy, float phaseDeg)
	{
		List<PointF> list = new List<PointF>();
		int num = 0;
		do
		{
			double num2 = (double)(phaseDeg + (float)checked(num * 9)) * Math.PI / 180.0;
			int num3 = ((num / 2 % 2 == 0) ? 34 : 27);
			list.Add(new PointF((float)((double)cx + (double)num3 * Math.Cos(num2)), (float)((double)cy + (double)num3 * Math.Sin(num2))));
			num = checked(num + 1);
		}
		while (num <= 39);
		using (SolidBrush brush = new SolidBrush(pal.MetalDark))
		{
			using Pen pen = new Pen(pal.Edge);
			g.FillPolygon(brush, list.ToArray());
			g.DrawPolygon(pen, list.ToArray());
		}
		Disc(g, pal.Metal, pal.Edge, cx, cy, 6f);
	}

	private static string Check(Graphics g, CheckValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 400f, 150f);
		Metal(g, pal, 20f, 40f, 360f, 70f);
		bool flag = Flows(v.Ports[0], v.Ports[1]);
		Fill(g, Fluid(v.Ports[0], pal), 20f, 60f, 170f, 30f);
		Fill(g, Fluid(v.Ports[1], pal), 190f, 60f, 190f, 30f);
		using (SolidBrush brush = new SolidBrush(pal.Metal))
		{
			using Pen pen = new Pen(pal.Edge);
			PointF[] points = new PointF[3]
			{
				new PointF(176f, 60f),
				new PointF(196f, 60f),
				new PointF(186f, 70f)
			};
			PointF[] points2 = new PointF[3]
			{
				new PointF(176f, 90f),
				new PointF(196f, 90f),
				new PointF(186f, 80f)
			};
			g.FillPolygon(brush, points);
			g.DrawPolygon(pen, points);
			g.FillPolygon(brush, points2);
			g.DrawPolygon(pen, points2);
		}
		float num = (flag ? 220f : 199f);
		using (Pen pen2 = new Pen(pal.Spring, 1.5f))
		{
			Zigzag(g, pen2, num + 13f, 300f, 75f, 7f);
		}
		Metal(g, pal, 300f, 62f, 8f, 26f, dark: true);
		Disc(g, pal.MetalDark, pal.Edge, num, 75f, 13f);
		if (flag)
		{
			FlowArrow(g, 40f, 75f, 160f, 75f);
		}
		Txt(g, pal, "1", 22f, 18f, bold: true);
		Txt(g, pal, "2", 364f, 18f, bold: true);
		if (!flag)
		{
			return "The ball sits on its seat: flow from 2 to 1 is blocked.";
		}
		return "Flow from 1 lifts the ball off its seat: the valve is open.";
	}

	private static string Shuttle(Graphics g, ShuttleValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 400f, 160f);
		Metal(g, pal, 20f, 50f, 360f, 70f);
		Fill(g, Fluid(v.Ports[0], pal), 20f, 70f, 110f, 30f);
		Fill(g, Fluid(v.Ports[1], pal), 270f, 70f, 110f, 30f);
		Fill(g, Fluid(v.Ports[2], pal), 130f, 70f, 140f, 30f);
		Fill(g, Fluid(v.Ports[2], pal), 192f, 10f, 16f, 60f);
		float[] array = new float[2] { 130f, 270f };
		foreach (float num in array)
		{
			Metal(g, pal, num - 4f, 70f, 8f, 8f);
			Metal(g, pal, num - 4f, 92f, 8f, 8f);
		}
		float cx = ((v.ConnectedInput == 0) ? 255f : 145f);
		Disc(g, pal.MetalDark, pal.Edge, cx, 85f, 14f);
		Txt(g, pal, "1 (left input)", 22f, 124f, bold: false, faint: true);
		Txt(g, pal, "1 (right input)", 296f, 124f, bold: false, faint: true);
		Txt(g, pal, "2 (output)", 214f, 12f, bold: true);
		return string.Format("OR: the ball closes the side without air; the output is joined to the {0} input.", (v.ConnectedInput == 0) ? "left" : "right");
	}

	private static string TwoPressure(Graphics g, TwoPressureValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 400f, 160f);
		Metal(g, pal, 20f, 50f, 360f, 70f);
		Fill(g, Fluid(v.Ports[0], pal), 20f, 70f, 110f, 30f);
		Fill(g, Fluid(v.Ports[1], pal), 270f, 70f, 110f, 30f);
		Fill(g, Fluid(v.Ports[2], pal), 130f, 70f, 140f, 30f);
		Fill(g, Fluid(v.Ports[2], pal), 192f, 10f, 16f, 60f);
		float[] array = new float[2] { 130f, 270f };
		foreach (float num in array)
		{
			Metal(g, pal, num - 4f, 70f, 8f, 8f);
			Metal(g, pal, num - 4f, 92f, 8f, 8f);
		}
		float num2 = ((v.ConnectedInput == 0) ? 8f : (-8f));
		Metal(g, pal, 150f + num2, 82f, 100f, 6f, dark: true);
		Metal(g, pal, 136f + num2, 72f, 10f, 26f, dark: true);
		Metal(g, pal, 254f + num2, 72f, 10f, 26f, dark: true);
		Txt(g, pal, "1 (left input)", 22f, 124f, bold: false, faint: true);
		Txt(g, pal, "1 (right input)", 296f, 124f, bold: false, faint: true);
		Txt(g, pal, "2 (output)", 214f, 12f, bold: true);
		if (!v.Ports[0].IsPressurized || !v.Ports[1].IsPressurized)
		{
			return "AND: the spool closes the input with air; the output gets air only when both inputs have it.";
		}
		return "AND: both inputs have air, so air reaches the output.";
	}

	private static string QuickExhaust(Graphics g, QuickExhaustValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 320f, 220f);
		Metal(g, pal, 80f, 50f, 160f, 120f);
		Fill(g, Fluid(v.Ports[0], pal), 0f, 98f, 120f, 24f);
		Fill(g, Fluid(v.Ports[1], pal), 120f, 80f, 80f, 60f);
		Fill(g, Fluid(v.Ports[1], pal), 148f, 0f, 24f, 80f);
		Fill(g, v.Feeding ? pal.Vent : Fluid(v.Ports[1], pal), 148f, 140f, 24f, 80f);
		if (v.Feeding)
		{
			Metal(g, pal, 132f, 136f, 56f, 8f, dark: true);
		}
		else
		{
			Metal(g, pal, 116f, 92f, 8f, 36f, dark: true);
		}
		Txt(g, pal, "1 (from valve)", 0f, 72f, bold: true);
		Txt(g, pal, "2 (to cylinder)", 178f, 4f, bold: true);
		Txt(g, pal, "3 (exhaust)", 178f, 196f, bold: true);
		if (!v.Feeding)
		{
			return "Exhausting: the disc closes 1, so the cylinder air escapes straight through 3.";
		}
		return "Feeding: air flows 1 → 2; the disc seals the exhaust 3.";
	}

	private static string Throttle(Graphics g, FlowControlValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 420f, 190f);
		Metal(g, pal, 20f, 60f, 380f, 60f);
		Fill(g, Fluid(v.Ports[0], pal), 20f, 78f, 190f, 24f);
		Fill(g, Fluid(v.Ports[1], pal), 210f, 78f, 190f, 24f);
		float num = 78f + 23f * (1f - (float)v.OpeningPercent / 100f);
		Metal(g, pal, 204f, 14f, 12f, 50f);
		Metal(g, pal, 190f, 6f, 40f, 10f, dark: true);
		using (SolidBrush brush = new SolidBrush(pal.MetalDark))
		{
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(198f, 60f),
				new PointF(222f, 60f),
				new PointF(210f, num)
			});
		}
		bool hasCheckValve = v.HasCheckValve;
		if (hasCheckValve)
		{
			Fill(g, Fluid(v.Ports[0], pal), 60f, 102f, 12f, 40f);
			Fill(g, Fluid(v.Ports[1], pal), 350f, 102f, 12f, 40f);
			Metal(g, pal, 20f, 120f, 380f, 40f);
			Fill(g, Fluid(v.Ports[0], pal), 60f, 130f, 140f, 16f);
			Fill(g, Fluid(v.Ports[1], pal), 200f, 130f, 162f, 16f);
			Disc(cx: Flows(v.Ports[0], v.Ports[1]) ? 214f : 203f, g: g, c: pal.MetalDark, edge: pal.Edge, cy: 138f, r: 8f);
			Metal(g, pal, 190f, 128f, 5f, 5f);
			Metal(g, pal, 190f, 143f, 5f, 5f);
			Txt(g, pal, "check valve (free flow 1 → 2)", 120f, 164f, bold: false, faint: true);
		}
		Txt(g, pal, "1", 22f, 40f, bold: true);
		Txt(g, pal, "2", 388f, 40f, bold: true);
		Txt(g, pal, $"{v.OpeningPercent}% open", 236f, 22f, bold: false, faint: true);
		if (!hasCheckValve)
		{
			return $"Both directions pass the needle, {v.OpeningPercent}% open.";
		}
		return $"Flow 2 → 1 must pass the needle ({v.OpeningPercent}% open); flow 1 → 2 lifts the check ball and passes freely.";
	}

	private static string Compensated(Graphics g, CompensatedFlowControl v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 420f, 180f);
		Metal(g, pal, 20f, 50f, 380f, 90f);
		Fill(g, Fluid(v.Ports[0], pal), 20f, 82f, 120f, 24f);
		Fill(g, Fluid(v.Ports[0], pal), 140f, 64f, 100f, 60f);
		Fill(g, Fluid(v.Ports[1], pal), 280f, 82f, 120f, 24f);
		Metal(g, pal, 150f, 70f, 40f, 48f, dark: true);
		using (Pen pen = new Pen(pal.Spring, 1.5f))
		{
			Zigzag(g, pen, 190f, 236f, 94f, 10f);
		}
		Metal(g, pal, 250f, 50f, 14f, 38f);
		Metal(g, pal, 250f, 100f, 14f, 40f);
		Fill(g, Fluid(v.Ports[1], pal), 264f, 82f, 16f, 24f);
		Txt(g, pal, "compensator", 140f, 30f, bold: false, faint: true);
		Txt(g, pal, "orifice", 244f, 30f, bold: false, faint: true);
		Txt(g, pal, "1", 22f, 30f, bold: true);
		Txt(g, pal, "2", 388f, 30f, bold: true);
		return $"The compensator spool keeps a constant pressure drop across the orifice, so {v.FlowLpm:0.#} l/min flows whatever the load.";
	}

	private static string Regulator(Graphics g, PressureRegulator v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 400f, 220f);
		Metal(g, pal, 40f, 30f, 320f, 100f);
		Fill(g, Fluid(v.Ports[0], pal), 40f, 60f, 140f, 22f);
		Fill(g, Fluid(v.Ports[1], pal), 220f, 60f, 140f, 22f);
		Fill(g, Fluid(v.Ports[1], pal), 180f, 82f, 40f, 34f);
		bool isPressurized = v.Ports[0].IsPressurized;
		bool flag = isPressurized && v.Ports[1].Pressure < v.Setting - 0.05;
		Fill(g, flag ? Fluid(v.Ports[0], pal) : pal.MetalDark, 190f, 56f, 20f, 30f);
		Metal(g, pal, 186f, flag ? 84f : 76f, 28f, 8f, dark: true);
		Metal(g, pal, 150f, 116f, 100f, 6f, dark: true);
		using (Pen pen = new Pen(pal.Spring, 1.5f))
		{
			ZigzagV(g, pen, 200f, 122f, 190f, 14f, 10);
		}
		Metal(g, pal, 186f, 190f, 28f, 10f, dark: true);
		Metal(g, pal, 196f, 200f, 8f, 18f);
		Txt(g, pal, "1 (in)", 42f, 40f, bold: true);
		Txt(g, pal, "2 (out)", 318f, 40f, bold: true);
		Txt(g, pal, $"set to {v.Setting:0.#} bar", 222f, 160f, bold: false, faint: true);
		if (!isPressurized)
		{
			return $"No supply at 1. When there is, the spring opens the poppet until the outlet reaches {v.Setting:0.#} bar.";
		}
		return $"Outlet {v.Ports[1].Pressure:0.0} bar (setting {v.Setting:0.#} bar): " + (flag ? "the spring holds the poppet open." : "the outlet pressure on the diaphragm closes the poppet.");
	}

	private static string Relief(Graphics g, ReliefValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 320f, 230f);
		Metal(g, pal, 90f, 20f, 140f, 170f);
		Port port = v.Ports[0];
		bool flag = port.IsPressurized && port.Pressure >= v.Setting - 0.5;
		Fill(g, Fluid(port, pal), 150f, 140f, 20f, 90f);
		Fill(g, flag ? Fluid(port, pal) : Lighter(pal.Oil, pal.Back), 170f, 106f, 150f, 20f);
		Fill(g, flag ? Fluid(port, pal) : pal.Cavity, 136f, 100f, 48f, 40f);
		float num = (flag ? 12f : 0f);
		using (SolidBrush brush = new SolidBrush(pal.MetalDark))
		{
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(144f, 126f - num),
				new PointF(176f, 126f - num),
				new PointF(160f, 142f - num)
			});
		}
		using (Pen pen = new Pen(pal.Spring, 1.5f))
		{
			ZigzagV(g, pen, 160f, 40f, 126f - num, 14f, 10);
		}
		Metal(g, pal, 140f, 30f, 40f, 10f, dark: true);
		Txt(g, pal, "P (from pump)", 176f, 206f, bold: true);
		Txt(g, pal, "T (to tank)", 236f, 84f, bold: true);
		Txt(g, pal, $"opens at {v.Setting:0} bar", 0f, 30f, bold: false, faint: true);
		if (!flag)
		{
			return $"Closed: {port.Pressure:0} bar is below the setting of {v.Setting:0} bar.";
		}
		return $"Open: the pressure ({port.Pressure:0} bar) lifts the poppet and the oil returns to tank.";
	}

	private static string Counterbalance(Graphics g, CounterbalanceValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 420f, 220f);
		Metal(g, pal, 40f, 50f, 340f, 120f);
		Fill(g, Fluid(v.Ports[0], pal), 40f, 120f, 130f, 20f);
		Fill(g, Fluid(v.Ports[1], pal), 250f, 120f, 130f, 20f);
		Fill(g, v.IsOpen ? Fluid(v.Ports[1], pal) : pal.Cavity, 170f, 116f, 80f, 28f);
		float num = (v.IsOpen ? 10f : 0f);
		Metal(g, pal, 200f, 104f - num, 20f, 18f, dark: true);
		using (Pen pen = new Pen(pal.Spring, 1.5f))
		{
			ZigzagV(g, pen, 210f, 60f, 104f - num, 10f);
		}
		Fill(g, Fluid(v.Ports[2], pal), 120f, 0f, 16f, 90f);
		Metal(g, pal, 112f, 86f, 32f, 8f, dark: true);
		Fill(g, Fluid(v.Ports[0], pal), 90f, 150f, 240f, 12f);
		Disc(g, pal.MetalDark, pal.Edge, Flows(v.Ports[0], v.Ports[1]) ? 226f : 214f, 156f, 6f);
		Txt(g, pal, "1 (valve)", 42f, 100f, bold: true);
		Txt(g, pal, "2 (cylinder)", 320f, 100f, bold: true);
		Txt(g, pal, "X (pilot)", 140f, 4f, bold: true);
		if (!v.IsOpen)
		{
			return "Closed: the poppet holds the oil in the cylinder, so the load cannot fall.";
		}
		return "Open: the load pressure or the pilot X has lifted the poppet; the load can move down under control.";
	}

	private static string AccumulatorView(Graphics g, Accumulator v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 260f, 250f);
		Metal(g, pal, 80f, 10f, 100f, 200f);
		Fill(g, pal.Cavity, 90f, 20f, 80f, 180f);
		float num = (float)Math.Min(1.0, v.StoredLitres / Math.Max(0.01, v.CapacityL));
		float num2 = 200f - 170f * num;
		Fill(g, Color.FromArgb(200, 205, 215), 90f, 20f, 80f, num2 - 20f);
		Fill(g, (num > 0f) ? pal.Oil : pal.Vent, 90f, num2, 80f, 200f - num2);
		Metal(g, pal, 90f, num2 - 8f, 80f, 8f, dark: true);
		Fill(g, Fluid(v.Ports[0], pal), 122f, 210f, 16f, 40f);
		Txt(g, pal, $"gas (N₂) {((v.StoredLitres > 0.001) ? v.GasPressure : v.PrechargeBar):0} bar", 186f, 30f, bold: false, faint: true);
		Txt(g, pal, $"oil {v.StoredLitres:0.00} l", 186f, Math.Min(190f, num2 + 6f), bold: false, faint: true);
		if (!(v.StoredLitres > 0.001))
		{
			return $"Empty: the gas is pre-charged to {v.PrechargeBar:0} bar; oil enters when the system pressure is higher.";
		}
		return $"{v.StoredLitres:0.00} l of oil stored against the gas cushion at {v.GasPressure:0} bar.";
	}

	private static string Gauge(Graphics g, PressureGauge v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 260f, 230f);
		float num = 130f;
		float num2 = 100f;
		Disc(g, pal.Cavity, pal.Edge, num, num2, 90f);
		int num3 = (v.Hydraulic ? 160 : 10);
		checked
		{
			using (Pen pen = new Pen(pal.Edge))
			{
				using Font font = new Font("Segoe UI", 7.5f);
				using SolidBrush brush = new SolidBrush(pal.SubText);
				int num4 = 0;
				do
				{
					double num5 = (double)(-225 + 27 * num4) * Math.PI / 180.0;
					g.DrawLine(pen, (float)((double)num + 80.0 * Math.Cos(num5)), (float)((double)num2 + 80.0 * Math.Sin(num5)), (float)((double)num + 88.0 * Math.Cos(num5)), (float)((double)num2 + 88.0 * Math.Sin(num5)));
					if (unchecked(num4 % 2) == 0)
					{
						g.DrawString(((double)(num3 * num4) / 10.0).ToString(), font, brush, (float)((double)num + 66.0 * Math.Cos(num5) - 8.0), (float)((double)num2 + 66.0 * Math.Sin(num5) - 6.0));
					}
					num4++;
				}
				while (num4 <= 10);
			}
			using (Pen pen2 = new Pen(Fluid(v.Ports[0], pal), 7f))
			{
				g.DrawArc(pen2, num - 45f, num2 - 45f, 90f, 90f, 150f, 240f);
			}
			double num6 = Math.Min(num3, Math.Max(0.0, v.Ports[0].Pressure));
			double num7 = (-225.0 + 270.0 * num6 / (double)num3) * Math.PI / 180.0;
			using (Pen pen3 = new Pen(Color.Firebrick, 3f))
			{
				g.DrawLine(pen3, num, num2, (float)((double)num + 72.0 * Math.Cos(num7)), (float)((double)num2 + 72.0 * Math.Sin(num7)));
			}
			Disc(g, pal.MetalDark, pal.Edge, num, num2, 6f);
			Fill(g, Fluid(v.Ports[0], pal), num - 6f, num2 + 90f, 12f, 40f);
			return $"{v.Ports[0].Pressure:0.0} bar: the pressure inside the curved Bourdon tube straightens it a little and turns the needle.";
		}
	}

	private static string SilencerView(Graphics g, Silencer v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 240f, 220f);
		Fill(g, Fluid(v.Ports[0], pal), 110f, 0f, 20f, 60f);
		Metal(g, pal, 70f, 60f, 100f, 130f);
		checked
		{
			using (SolidBrush brush = new SolidBrush(pal.MetalDark))
			{
				int num = 66;
				do
				{
					int num2 = 76;
					do
					{
						g.FillEllipse(brush, num2 + unchecked(num / 8 % 2) * 4 - 2, num - 2, 4, 4);
						num2 += 8;
					}
					while (num2 <= 164);
					num += 8;
				}
				while (num <= 184);
			}
			Txt(g, pal, "porous (sintered) element", 40f, 196f, bold: false, faint: true);
			return "Exhaust air spreads through the porous element, so it leaves slowly and quietly.";
		}
	}

	private static string Supply(Graphics g, AirSupply v, RectangleF area, CutawayPalette pal, bool simulating)
	{
		Fit(g, area, 420f, 180f);
		Metal(g, pal, 20f, 70f, 70f, 60f, dark: true);
		Txt(g, pal, "M", 46f, 90f, bold: true);
		Metal(g, pal, 90f, 80f, 60f, 40f);
		Txt(g, pal, "compressor", 82f, 130f, bold: false, faint: true);
		Color color = (simulating ? pal.Air : pal.Vent);
		using (SolidBrush brush = new SolidBrush(pal.Metal))
		{
			using Pen pen = new Pen(pal.Edge);
			g.FillEllipse(brush, 170, 50, 200, 100);
			g.DrawEllipse(pen, 170, 50, 200, 100);
		}
		using (SolidBrush brush2 = new SolidBrush(color))
		{
			g.FillEllipse(brush2, 178, 58, 184, 84);
		}
		Fill(g, color, 150f, 94f, 28f, 12f);
		Fill(g, color, 362f, 94f, 58f, 12f);
		Txt(g, pal, $"receiver {v.Pressure:0.#} bar", 222f, 92f, bold: true);
		return $"The compressor fills the receiver; the line is kept at {v.Pressure:0.#} bar.";
	}

	private static string Tank(Graphics g, HydraulicTank v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 360f, 210f);
		Metal(g, pal, 40f, 40f, 280f, 150f);
		Fill(g, pal.Cavity, 50f, 50f, 260f, 130f);
		Fill(g, Lighter(pal.Oil, pal.Back), 50f, 100f, 260f, 80f);
		Fill(g, Fluid(v.Ports[0], pal), 100f, 0f, 16f, 150f);
		Metal(g, pal, 230f, 20f, 40f, 20f, dark: true);
		Txt(g, pal, "breather", 228f, 0f, bold: false, faint: true);
		Txt(g, pal, "return line (T)", 122f, 0f, bold: true);
		return "Oil returns here, settles and cools; the pump draws it back out.";
	}

	private static string Coil(Graphics g, ElectricCoil k, RectangleF area, CutawayPalette pal, bool simulating)
	{
		Fit(g, area, 420f, 190f);
		bool flag = simulating && k.Active;
		Color color = Color.FromArgb(220, 40, 40);
		if (k.Kind == CoilKind.Lamp)
		{
			using (SolidBrush brush = new SolidBrush(flag ? Color.FromArgb(255, 220, 60) : pal.Cavity))
			{
				using Pen pen = new Pen(pal.Edge, 2f);
				g.FillEllipse(brush, 160, 20, 100, 100);
				g.DrawEllipse(pen, 160, 20, 100, 100);
			}
			using (Pen pen2 = new Pen(flag ? color : pal.Edge, 2f))
			{
				g.DrawBezier(pen2, 190f, 110f, 195f, 60f, 225f, 60f, 230f, 110f);
			}
			Metal(g, pal, 180f, 118f, 60f, 40f, dark: true);
			return flag ? "Current heats the filament until it glows." : "No current: the lamp is off.";
		}
		checked
		{
			using (SolidBrush brush2 = new SolidBrush(flag ? Color.FromArgb(200, 120, 60) : Color.FromArgb(170, 120, 80)))
			{
				using Pen pen3 = new Pen(pal.Edge);
				g.FillRectangle(brush2, 60, 60, 120, 70);
				g.DrawRectangle(pen3, 60, 60, 120, 70);
				int num = 66;
				do
				{
					g.DrawLine(pen3, num, 60, num + 3, 130);
					num += 6;
				}
				while (num <= 174);
			}
			if (flag)
			{
				Txt(g, pal, "current flows: magnetic field", 54f, 140f, bold: false, faint: true);
			}
			if (k.Kind == CoilKind.Solenoid)
			{
				float num2 = (flag ? 40f : 0f);
				Metal(g, pal, 150f - num2, 82f, 150f, 26f, dark: true);
				Metal(g, pal, 300f - num2, 70f, 12f, 50f);
				Txt(g, pal, "plunger → valve spool", 230f, 130f, bold: false, faint: true);
				return flag ? $"Energized: the plunger is pulled in and switches the valve that uses {k.Label}." : "Not energized: the valve spring pushes the plunger out.";
			}
			Metal(g, pal, 40f, 60f, 20f, 90f, dark: true);
			Metal(g, pal, 40f, 140f, 160f, 12f, dark: true);
			float num3 = (flag ? 10f : (-12f));
			using (Pen pen4 = new Pen(pal.MetalDark, 8f))
			{
				g.DrawLine(pen4, 70f, 48f, 280f, 48f + num3 * 2f);
			}
			float num4 = 48f + num3 * 2f;
			using (Pen pen5 = new Pen(flag ? color : pal.Edge, 3f))
			{
				g.DrawLine(pen5, 280f, num4, 330f, num4);
			}
			Metal(g, pal, 326f, 64f, 30f, 6f);
			Txt(g, pal, "NO", 360f, 58f, bold: false, faint: true);
			Metal(g, pal, 326f, 14f, 30f, 6f);
			Txt(g, pal, "NC", 360f, 8f, bold: false, faint: true);
			string arg = ((k.Kind == CoilKind.Relay) ? "relay" : "timer relay");
			return flag ? $"Energized: the core pulls the armature, so every contact of {arg} {k.Label} changes over." : $"Not energized: the spring holds the armature up; the NC contacts of {k.Label} are closed.";
		}
	}

	private static string Contact(Graphics g, ElectricContact k, RectangleF area, CutawayPalette pal, bool simulating)
	{
		Fit(g, area, 360f, 200f);
		bool flag = (simulating ? k.IsClosed : k.NormallyClosed);
		bool flag2 = flag ^ k.NormallyClosed;
		Color color = Color.FromArgb(220, 40, 40);
		Color color2 = ((simulating && flag && (k.Ports[0].IsPressurized || k.Ports[1].IsPressurized)) ? color : pal.Edge);
		using (Pen pen = new Pen(pal.Edge))
		{
			g.DrawRectangle(pen, 60, 60, 240, 110);
		}
		Metal(g, pal, 70f, 130f, 60f, 10f);
		Metal(g, pal, 230f, 130f, 60f, 10f);
		using (Pen pen2 = new Pen(color2, 3f))
		{
			g.DrawLine(pen2, 70, 135, 20, 135);
			g.DrawLine(pen2, 290, 135, 340, 135);
		}
		float num = (flag ? 120f : (k.NormallyClosed ? 120f : 100f));
		if (k.NormallyClosed && !flag)
		{
			num = 150f;
		}
		Metal(g, pal, 110f, num, 140f, 10f, dark: true);
		Metal(g, pal, 176f, 40f, 8f, num - 40f);
		switch (k.Operator)
		{
		case ContactOperator.PushButton:
		case ContactOperator.Selector:
			Metal(g, pal, 150f, flag2 ? 28f : 16f, 60f, 14f, dark: true);
			Txt(g, pal, (k.Operator == ContactOperator.Selector) ? "selector (stays put)" : "push button", 214f, 16f, bold: false, faint: true);
			break;
		case ContactOperator.LimitSwitch:
			Disc(g, pal.MetalDark, pal.Edge, 180f, flag2 ? 34f : 24f, 12f);
			Txt(g, pal, $"roller, cylinder mark {k.Reference}", 200f, 14f, bold: false, faint: true);
			break;
		case ContactOperator.ProximitySensor:
			Fill(g, flag2 ? Color.FromArgb(200, 60, 60) : pal.MetalDark, 150f, 20f, 60f, 16f);
			Txt(g, pal, $"magnet / target at mark {k.Reference}", 214f, 18f, bold: false, faint: true);
			break;
		case ContactOperator.Relay:
			Txt(g, pal, $"moved by relay {k.Reference}", 194f, 18f, bold: false, faint: true);
			break;
		case ContactOperator.PressureSwitch:
			Txt(g, pal, $"moved by pressure switch {k.Reference}", 194f, 18f, bold: false, faint: true);
			break;
		case ContactOperator.EmergencyStop:
		{
			using (SolidBrush brush = new SolidBrush(Color.FromArgb(215, 30, 30)))
			{
				g.FillEllipse(brush, 140f, flag2 ? 2f : (-10f), 80f, 40f);
			}
			Txt(g, pal, flag2 ? "pressed and latched: turn to release" : "emergency stop", 224f, 16f, bold: false, faint: true);
			break;
		}
		}
		Txt(g, pal, k.NormallyClosed ? "NC contact" : "NO contact", 64f, 176f, bold: false, faint: true);
		if (!flag)
		{
			return "The bridge is lifted off the fixed contacts: the circuit is open.";
		}
		return "The bridge touches both fixed contacts: current can flow.";
	}

	private static string GripperView(Graphics g, Gripper c, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 420f, 220f);
		Metal(g, pal, 20f, 60f, 220f, 100f);
		float num = (float)c.Position;
		float num2 = 50f + num * 110f;
		Fill(g, Fluid(c.Ports[0], pal), 30f, 75f, num2 - 30f, 70f);
		Fill(g, Fluid(c.Ports[1], pal), num2 + 16f, 75f, 220f - num2, 70f);
		Metal(g, pal, num2, 75f, 16f, 70f, dark: true);
		Metal(g, pal, num2 + 16f, 104f, 120f - num * 110f, 12f);
		float num3 = 50f * (1f - num);
		Metal(g, pal, 250f, 110f - num3 - 40f, 60f, 30f, dark: true);
		Metal(g, pal, 250f, 110f + num3 + 10f, 60f, 30f, dark: true);
		Metal(g, pal, 240f, 60f, 10f, 100f);
		Fill(g, Fluid(c.Ports[0], pal), 40f, 160f, 14f, 40f);
		Txt(g, pal, "1 close", 30f, 200f, bold: true);
		Fill(g, Fluid(c.Ports[1], pal), 200f, 160f, 14f, 40f);
		Txt(g, pal, "2 open", 190f, 200f, bold: true);
		if (!((double)num >= 0.995))
		{
			if (!((double)num <= 0.005))
			{
				return "The piston moves and the jaws follow it.";
			}
			return "Air at port 2 holds the piston back: the jaws are open.";
		}
		return "Air at port 1 pushed the piston over: the jaws are closed and grip the part.";
	}

	private static string PressureSwitchView(Graphics g, PressureSwitch v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 300f, 240f);
		Metal(g, pal, 60f, 20f, 180f, 170f);
		Fill(g, pal.Cavity, 80f, 40f, 140f, 70f);
		float num = (v.IsOn ? 14f : 0f);
		Color c = ((v.Ports[0].Pressure < -0.05) ? VacuumTint : Fluid(v.Ports[0], pal));
		Fill(g, c, 80f, 130f - num, 140f, 40f + num);
		Fill(g, c, 140f, 170f, 20f, 60f);
		using (Pen pen = new Pen(Color.FromArgb(40, 40, 40), 3f))
		{
			g.DrawLine(pen, 80f, 130f - num, 220f, 130f - num);
		}
		Metal(g, pal, 145f, 90f - num, 10f, 40f, dark: true);
		using (Pen pen2 = new Pen(pal.Spring, 1.5f))
		{
			ZigzagV(g, pen2, 150f, 40f, 90f - num, 10f);
		}
		using (Pen pen3 = new Pen(v.IsOn ? Color.FromArgb(220, 40, 40) : pal.Edge, 3f))
		{
			g.DrawLine(pen3, 90, 60, 135, v.IsOn ? 60 : 50);
		}
		Disc(g, pal.MetalDark, pal.Edge, 138f, 60f, 3f);
		Txt(g, pal, $"switches at {v.Setting:0.0#} bar", 70f, 0f, bold: false, faint: true);
		Txt(g, pal, "1", 166f, 210f, bold: true);
		if (v.Setting < 0.0)
		{
			return v.IsOn ? $"{v.Ports[0].Pressure:0.00} bar: the vacuum has pulled the diaphragm and the contact has switched." : $"{v.Ports[0].Pressure:0.00} bar is not enough vacuum: the contact is at rest.";
		}
		return v.IsOn ? $"{v.Ports[0].Pressure:0.0} bar: the diaphragm has pushed the plunger up against the spring and the contact has switched." : $"{v.Ports[0].Pressure:0.0} bar is not enough to overcome the spring: the contact is at rest.";
	}

	private static string ShutOffView(Graphics g, ShutOffValve v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 400f, 200f);
		Metal(g, pal, 20f, 60f, 360f, 80f);
		bool isOpen = v.IsOpen;
		Fill(g, Fluid(v.Ports[0], pal), 20f, 85f, 140f, 30f);
		Fill(g, Fluid(v.Ports[1], pal), 240f, 85f, 140f, 30f);
		Disc(g, pal.Metal, pal.Edge, 200f, 100f, 45f);
		if (isOpen)
		{
			Fill(g, Fluid(v.Ports[0], pal), 155f, 85f, 90f, 30f);
			FlowArrow(g, 60f, 100f, 340f, 100f);
		}
		else
		{
			Fill(g, pal.Cavity, 185f, 55f, 30f, 90f);
		}
		Metal(g, pal, 195f, 10f, 10f, 45f, dark: true);
		Metal(g, pal, isOpen ? 200f : 195f, isOpen ? 6f : 0f, isOpen ? 120f : 10f, isOpen ? 10f : 10f, dark: true);
		Txt(g, pal, "1", 22f, 40f, bold: true);
		Txt(g, pal, "2", 364f, 40f, bold: true);
		if (!isOpen)
		{
			return "The ball is turned a quarter turn: its solid side blocks the pipe.";
		}
		return "The hole through the ball lines up with the pipe: air flows through.";
	}

	private static string EjectorView(Graphics g, VacuumGenerator v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 440f, 220f);
		Metal(g, pal, 20f, 70f, 400f, 80f);
		bool working = v.Working;
		Color color = (working ? pal.Air : pal.Vent);
		using (SolidBrush brush = new SolidBrush(color))
		{
			g.FillPolygon(brush, new PointF[4]
			{
				new PointF(20f, 90f),
				new PointF(150f, 104f),
				new PointF(150f, 116f),
				new PointF(20f, 130f)
			});
			g.FillPolygon(brush, new PointF[4]
			{
				new PointF(200f, 104f),
				new PointF(420f, 90f),
				new PointF(420f, 130f),
				new PointF(200f, 116f)
			});
		}
		using (SolidBrush brush2 = new SolidBrush(working ? VacuumTint : pal.Cavity))
		{
			g.FillRectangle(brush2, 150, 80, 50, 60);
			g.FillRectangle(brush2, 160, 10, 30, 70);
		}
		if (working)
		{
			FlowArrow(g, 175f, 20f, 175f, 75f);
			FlowArrow(g, 220f, 110f, 400f, 110f);
		}
		Txt(g, pal, "1 (compressed air)", 20f, 160f, bold: true);
		Txt(g, pal, "V (vacuum)", 200f, 10f, bold: true);
		Txt(g, pal, "to silencer", 340f, 160f, bold: false, faint: true);
		if (!working)
		{
			return "No compressed air at port 1: no jet, no vacuum.";
		}
		return $"The fast jet of air drags the air out of the suction chamber: {v.Ports[1].Pressure:0.00} bar at V.";
	}

	private static string CupView(Graphics g, SuctionCup v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 300f, 230f);
		bool flag = v.Ports[0].Pressure < -0.05;
		Fill(g, flag ? VacuumTint : pal.Vent, 140f, 0f, 20f, 80f);
		using (SolidBrush brush = new SolidBrush(Color.FromArgb(60, 60, 60)))
		{
			using SolidBrush brush2 = new SolidBrush((flag && v.Sealed) ? VacuumTint : pal.Cavity);
			PointF[] points = new PointF[4]
			{
				new PointF(120f, 80f),
				new PointF(180f, 80f),
				new PointF(250f, 150f),
				new PointF(50f, 150f)
			};
			g.FillPolygon(brush, points);
			g.FillPolygon(brush2, new PointF[4]
			{
				new PointF(135f, 92f),
				new PointF(165f, 92f),
				new PointF(225f, 148f),
				new PointF(75f, 148f)
			});
		}
		if (v.WorkpiecePresent)
		{
			float y = (v.Holding ? 150f : 175f);
			using (SolidBrush brush3 = new SolidBrush(v.Holding ? Color.FromArgb(120, 160, 90) : Color.FromArgb(170, 170, 170)))
			{
				g.FillRectangle(brush3, 30f, y, 240f, 30f);
			}
			if (v.Fault == FaultKind.Leak)
			{
				Txt(g, pal, "torn lip: air leaks in", 60f, 210f, bold: false, faint: true);
			}
		}
		if (!v.Holding)
		{
			if (!v.WorkpiecePresent)
			{
				return "No workpiece: the cup is open to the air.";
			}
			return "Not enough vacuum in the cup: the part is not held.";
		}
		return $"The outside air pressure presses the part against the cup: {v.HoldingForce():0} N holding force.";
	}

	private static string CompressorView(Graphics g, Compressor v, RectangleF area, CutawayPalette pal, double phase)
	{
		Fit(g, area, 320f, 300f);
		Metal(g, pal, 90f, 30f, 140f, 150f);
		double num = (v.Running ? (phase * 2.0 * Math.PI) : 0.0);
		double num2 = 160.0 + 30.0 * Math.Sin(num);
		double num3 = 230.0 + 30.0 * Math.Cos(num);
		float num4 = (float)(num3 - 120.0);
		Fill(g, v.Running ? pal.Air : pal.Vent, 105f, 45f, 110f, num4 - 45f);
		Metal(g, pal, 105f, num4, 110f, 26f, dark: true);
		using (Pen pen = new Pen(pal.MetalDark, 8f))
		{
			g.DrawLine(pen, 160f, num4 + 13f, (float)num2, (float)num3);
		}
		Disc(g, pal.Metal, pal.Edge, 160f, 230f, 40f);
		Disc(g, pal.MetalDark, pal.Edge, (float)num2, (float)num3, 8f);
		Fill(g, Fluid(v.Ports[0], pal), 215f, 50f, 90f, 16f);
		Txt(g, pal, "1 → receiver", 220f, 30f, bold: true);
		if (!v.Running)
		{
			return "The pressure switch has stopped the motor: the receiver is full (or the main switch is off).";
		}
		return $"The crank drives the piston up and down; air is drawn in, compressed and pushed out ({v.EffectiveDelivery:0} NL/min).";
	}

	private static string ReceiverView(Graphics g, AirReceiver v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 420f, 200f);
		using (SolidBrush brush = new SolidBrush(pal.Metal))
		{
			using Pen pen = new Pen(pal.Edge, 2f);
			g.FillEllipse(brush, 40, 30, 340, 140);
			g.DrawEllipse(pen, 40, 30, 340, 140);
		}
		checked
		{
			int num = (int)Math.Round(Math.Min(1.0, v.Pressure / 10.0) * 200.0);
			using (SolidBrush brush2 = new SolidBrush(Color.FromArgb(40 + unchecked(num / 4), pal.Air)))
			{
				g.FillEllipse(brush2, 50, 40, 320, 120);
			}
			Fill(g, Fluid(v.Ports[0], pal), 0f, 92f, 50f, 16f);
			Fill(g, Fluid(v.Ports[1], pal), 370f, 92f, 50f, 16f);
			Txt(g, pal, $"{v.Pressure:0.0} bar   {(v.Pressure + 1.013) / 1.013 * v.VolumeLitres:0} NL stored", 120f, 90f, bold: true);
			Metal(g, pal, 205f, 170f, 10f, 20f, dark: true);
			Txt(g, pal, "drain", 220f, 175f, bold: false, faint: true);
			return "The receiver stores compressed air: it covers peaks of air use and lets the compressor run in longer, calmer cycles.";
		}
	}

	private static string FlowMeterView(Graphics g, FlowMeter v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 260f, 300f);
		using (SolidBrush brush = new SolidBrush(pal.Cavity))
		{
			using Pen pen = new Pen(pal.Edge, 2f);
			g.FillPolygon(brush, new PointF[4]
			{
				new PointF(110f, 30f),
				new PointF(150f, 30f),
				new PointF(140f, 250f),
				new PointF(120f, 250f)
			});
			g.DrawPolygon(pen, new PointF[4]
			{
				new PointF(110f, 30f),
				new PointF(150f, 30f),
				new PointF(140f, 250f),
				new PointF(120f, 250f)
			});
		}
		double num = (v.Hydraulic ? 40.0 : 200.0);
		float num2 = (float)Math.Min(1.0, Math.Abs(v.Flow) / num);
		float num3 = 235f - num2 * 190f;
		using (SolidBrush brush2 = new SolidBrush(Color.FromArgb(200, 60, 40)))
		{
			g.FillPolygon(brush2, new PointF[3]
			{
				new PointF(118f, num3),
				new PointF(142f, num3),
				new PointF(130f, num3 + 16f)
			});
		}
		using (Pen pen2 = new Pen(pal.SubText))
		{
			int num4 = 0;
			do
			{
				float num5 = 235f - (float)num4 * 47.5f;
				g.DrawLine(pen2, 152f, num5, 162f, num5);
				Txt(g, pal, $"{num * (double)num4 / 4.0:0}", 166f, num5 - 8f, bold: false, faint: true);
				num4 = checked(num4 + 1);
			}
			while (num4 <= 4);
		}
		Fill(g, Fluid(v.Ports[0], pal), 40f, 250f, 180f, 14f);
		Txt(g, pal, $"{Math.Abs(v.Flow):0.0} {v.Unit}", 60f, 270f, bold: true);
		return "The flow lifts the float in the tapered tube; the higher it floats, the more flow.";
	}

	private static string ForceSensorView(Graphics g, ForceSensor v, RectangleF area, CutawayPalette pal)
	{
		Fit(g, area, 360f, 200f);
		float num = (float)(Math.Max(-1.0, Math.Min(1.0, v.Force / 2000.0)) * 14.0);
		using (SolidBrush brush = new SolidBrush(pal.Metal))
		{
			using Pen pen = new Pen(pal.Edge);
			PointF[] points = new PointF[4]
			{
				new PointF(40f, 80f),
				new PointF(320f, 80f + num),
				new PointF(320f, 110f + num),
				new PointF(40f, 110f)
			};
			g.FillPolygon(brush, points);
			g.DrawPolygon(pen, points);
		}
		Metal(g, pal, 20f, 60f, 20f, 70f, dark: true);
		Fill(g, Color.FromArgb(200, 160, 40), 120f, 74f + num * 0.3f, 50f, 6f);
		Fill(g, Color.FromArgb(200, 160, 40), 120f, 110f + num * 0.3f, 50f, 6f);
		using (Pen pen2 = new Pen(Color.Firebrick, 3f)
		{
			EndCap = LineCap.ArrowAnchor
		})
		{
			g.DrawLine(pen2, 300f, 30f + num, 300f, 76f + num);
		}
		Txt(g, pal, $"{v.Force:0} N on {v.Cylinder}", 120f, 150f, bold: true);
		return "The force bends the beam very slightly; the strain gauges glued on it change their resistance, which is measured.";
	}

	private static string CounterView(Graphics g, ElectricCounter v, RectangleF area, CutawayPalette pal, bool simulating)
	{
		Fit(g, area, 300f, 180f);
		Metal(g, pal, 20f, 20f, 260f, 140f);
		Fill(g, Color.FromArgb(20, 30, 20), 50f, 45f, 200f, 60f);
		using (Font font = new Font("Consolas", 26f, FontStyle.Bold))
		{
			using SolidBrush brush = new SolidBrush(Color.FromArgb(120, 255, 120));
			g.DrawString($"{(simulating ? v.Count : 0),4}", font, brush, 60f, 50f);
		}
		Txt(g, pal, $"preset {v.Preset}", 60f, 115f, bold: true);
		Fill(g, (v.Active && simulating) ? Color.FromArgb(220, 40, 40) : pal.Vent, 210f, 115f, 30f, 18f);
		if (!simulating || !v.Active)
		{
			return "Each pulse on A1/A2 adds one; at the preset the contacts switch.";
		}
		return "The preset has been reached: the output contacts have switched.";
	}
}
