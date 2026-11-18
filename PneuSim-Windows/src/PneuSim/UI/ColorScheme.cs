using System;
using System.Drawing;

namespace PneuSim;

public class ColorScheme
{
	public static readonly ColorScheme Light = new ColorScheme(isDark: false);

	public static readonly ColorScheme Dark = new ColorScheme(isDark: true);

	public bool IsDark { get; }

	public Color Background
	{
		get
		{
			if (!IsDark)
			{
				return Color.White;
			}
			return Color.FromArgb(30, 32, 38);
		}
	}

	public Color GridDot
	{
		get
		{
			if (!IsDark)
			{
				return Color.FromArgb(205, 210, 220);
			}
			return Color.FromArgb(70, 74, 86);
		}
	}

	public ColorScheme(bool isDark)
	{
		IsDark = isDark;
	}

	public Color Map(Color c)
	{
		if (!IsDark)
		{
			return c;
		}
		double num = (0.299 * (double)(int)c.R + 0.587 * (double)(int)c.G + 0.114 * (double)(int)c.B) / 255.0;
		if ((double)(int)checked((byte)unchecked((uint)(Math.Max(c.R, Math.Max(c.G, c.B)) - Math.Min(c.R, Math.Min(c.G, c.B))))) / 255.0 < 0.12)
		{
			int num2 = checked((int)Math.Round(38.0 + (1.0 - num) * 187.0));
			return Color.FromArgb(c.A, num2, num2, num2);
		}
		double boost = ((num < 0.45) ? (0.45 - num) : 0.0);
		return Color.FromArgb(c.A, Lift(c.R, boost), Lift(c.G, boost), Lift(c.B, boost));
	}

	private static int Lift(byte v, double boost)
	{
		checked
		{
			return (int)Math.Round(Math.Min(255.0, (double)unchecked((int)v) + (double)(255 - v) * boost * 1.4));
		}
	}
}
