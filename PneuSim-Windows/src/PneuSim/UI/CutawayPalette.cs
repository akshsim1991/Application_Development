using System.Drawing;

namespace PneuSim;

public class CutawayPalette
{
	public Color Back { get; set; }

	public Color Text { get; set; }

	public Color SubText { get; set; }

	public Color Metal { get; set; }

	public Color MetalDark { get; set; }

	public Color Edge { get; set; }

	public Color Cavity { get; set; }

	public Color Vent { get; set; }

	public Color Air { get; set; }

	public Color Oil { get; set; }

	public Color Spring { get; set; }

	public static CutawayPalette Current()
	{
		if (AppSettings.DarkMode)
		{
			return new CutawayPalette
			{
				Back = Color.FromArgb(28, 30, 36),
				Text = Color.FromArgb(225, 228, 235),
				SubText = Color.FromArgb(160, 166, 178),
				Metal = Color.FromArgb(112, 118, 130),
				MetalDark = Color.FromArgb(70, 75, 86),
				Edge = Color.FromArgb(175, 180, 190),
				Cavity = Color.FromArgb(46, 49, 58),
				Vent = Color.FromArgb(68, 75, 92),
				Air = Color.FromArgb(70, 150, 255),
				Oil = Color.FromArgb(240, 140, 40),
				Spring = Color.FromArgb(205, 205, 205)
			};
		}
		return new CutawayPalette
		{
			Back = Color.White,
			Text = Color.Black,
			SubText = Color.FromArgb(105, 105, 105),
			Metal = Color.FromArgb(205, 210, 218),
			MetalDark = Color.FromArgb(95, 100, 110),
			Edge = Color.FromArgb(100, 100, 100),
			Cavity = Color.White,
			Vent = Color.FromArgb(225, 232, 242),
			Air = RenderContext.PressureColor,
			Oil = RenderContext.HydraulicColor,
			Spring = Color.FromArgb(90, 90, 90)
		};
	}
}
