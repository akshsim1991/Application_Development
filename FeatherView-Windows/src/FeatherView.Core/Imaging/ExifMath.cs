using System.Globalization;

namespace FeatherView.Core.Imaging;

public static class GpsMath
{
    private static double Rational(ulong v)
    {
        uint num = (uint)(v & 0xFFFFFFFF), den = (uint)(v >> 32);
        return den == 0 ? 0 : (double)num / den;
    }

    /// <summary>Degrees, minutes, seconds stored as three EXIF rationals (each packed in a ulong: low 32 bits numerator, high 32 bits denominator).</summary>
    public static double? FromRationals(ulong[]? dms, string? reference)
    {
        if (dms is not { Length: >= 1 }) return null;
        double d = Rational(dms[0]), m = dms.Length > 1 ? Rational(dms[1]) : 0, s = dms.Length > 2 ? Rational(dms[2]) : 0;
        return Sign(d + m / 60 + s / 3600, reference);
    }

    /// <summary>Degrees, minutes, seconds as plain numbers (the form Windows' property system returns).</summary>
    public static double? FromDegrees(double[]? dms, string? reference)
    {
        if (dms is not { Length: >= 1 }) return null;
        return Sign(dms[0] + (dms.Length > 1 ? dms[1] / 60 : 0) + (dms.Length > 2 ? dms[2] / 3600 : 0), reference);
    }

    private static double Sign(double v, string? reference) => reference is "S" or "W" or "s" or "w" ? -Math.Abs(v) : v;

    public static bool IsValid(double lat, double lon) => lat is >= -90 and <= 90 && lon is >= -180 and <= 180 && !(lat == 0 && lon == 0);

    public static string Format(double lat, double lon) =>
        $"{Math.Abs(lat).ToString("0.0000", CultureInfo.InvariantCulture)}° {(lat >= 0 ? "N" : "S")}, {Math.Abs(lon).ToString("0.0000", CultureInfo.InvariantCulture)}° {(lon >= 0 ? "E" : "W")}";

    /// <summary>Opened only when the user asks; nothing is ever sent automatically.</summary>
    public static string MapUrl(double lat, double lon)
    {
        var la = lat.ToString("0.00000", CultureInfo.InvariantCulture); var lo = lon.ToString("0.00000", CultureInfo.InvariantCulture);
        return $"https://www.openstreetmap.org/?mlat={la}&mlon={lo}#map=14/{la}/{lo}";
    }
}

public static class CameraSummary
{
    /// <summary>"Sony A7R IV, 24mm f/5.6, 1/250s, ISO 100" - any part that is missing is left out.</summary>
    public static string Build(string? camera, double? focalMm, double? fNumber, double? exposureSeconds, int? iso)
    {
        var parts = new List<string>();
        if (!string.IsNullOrWhiteSpace(camera)) parts.Add(camera.Trim());
        var lens = new List<string>();
        if (focalMm is > 0) lens.Add(focalMm.Value.ToString("0.#", CultureInfo.InvariantCulture) + "mm");
        if (fNumber is > 0) lens.Add("f/" + fNumber.Value.ToString("0.#", CultureInfo.InvariantCulture));
        if (lens.Count > 0) parts.Add(string.Join(' ', lens));
        if (exposureSeconds is > 0) parts.Add(exposureSeconds >= 1 ? exposureSeconds.Value.ToString("0.#", CultureInfo.InvariantCulture) + "s" : $"1/{Math.Round(1 / exposureSeconds.Value)}s");
        if (iso is > 0) parts.Add("ISO " + iso.Value.ToString(CultureInfo.InvariantCulture));
        return string.Join(", ", parts);
    }
}
