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

	/// <summary>Range text like "5.5–6.5 bar".</summary>
	public static string PressureRange(double lowBar, double highBar, string barFormat)
	{
		string text = Pressure(lowBar, barFormat);
		string text2 = Pressure(highBar, barFormat);
		int num = text.LastIndexOf(' ');
		return text.Substring(0, num) + "–" + text2;
	}
}
