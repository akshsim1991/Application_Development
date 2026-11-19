using System;
using System.Globalization;

namespace PneuSim;

/// <summary>Formats pressures (stored internally in bar, gauge) in the unit chosen in Settings.</summary>
public static class Units
{
	public static string PressureUnitName => AppSettings.PressureUnit;

	/// <summary>Text like "6 bar", "0.6 MPa" or "600 kPa". barFormat is the number format used for bar.</summary>
	public static string Pressure(double bar, string barFormat)
	{
		switch (AppSettings.PressureUnit)
		{
		case "MPa":
			return (bar / 10.0).ToString("0.0##", CultureInfo.CurrentCulture) + " MPa";
		case "kPa":
			return (bar * 100.0).ToString("0", CultureInfo.CurrentCulture) + " kPa";
		default:
			return bar.ToString(barFormat, CultureInfo.CurrentCulture) + " bar";
		}
	}

	/// <summary>Factor from bar to the unit chosen in Settings (1 for bar, 0.1 for MPa, 100 for kPa).</summary>
	public static double PressureFactor => (AppSettings.PressureUnit == "MPa") ? 0.1 : ((AppSettings.PressureUnit == "kPa") ? 100.0 : 1.0);

	/// <summary>Replaces "(bar)" in a caption with the unit chosen in Settings.</summary>
	public static string RelabelPressure(string caption)
	{
		return caption.Replace("(bar)", "(" + AppSettings.PressureUnit + ")");
	}

	/// <summary>"Caption (unit): value" with a caption ending in (bar) shown in the unit chosen in Settings.</summary>
	public static string Shown(string caption, double value)
	{
		if (caption.Contains("(bar)") && AppSettings.PressureUnit != "bar")
		{
			double f = (AppSettings.PressureUnit == "MPa") ? 0.1 : 100.0;
			return caption.Replace("(bar)", "(" + AppSettings.PressureUnit + ")") + ": " + (value * f).ToString("0.##", CultureInfo.CurrentCulture);
		}
		return caption + ": " + value.ToString("0.##", CultureInfo.CurrentCulture);
	}

	/// <summary>Range text like "5.5–6.5 bar".</summary>
	public static string PressureRange(double lowBar, double highBar, string barFormat)
	{
		string text = Pressure(lowBar, barFormat);
		string text2 = Pressure(highBar, barFormat);
		int num = text.LastIndexOf(' ');
		return text.Substring(0, num) + "–" + text2;
	}
}
