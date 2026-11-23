using System.Globalization;

namespace FeatherView.Core.Video;

/// <summary>Formats and parses playback times ("1:02:03", "83", "1:23.5").</summary>
public static class TimeCode
{
    public static string Format(double seconds)
    {
        if (double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0) seconds = 0;
        var t = TimeSpan.FromSeconds(Math.Floor(seconds));
        return t.TotalHours >= 1 ? $"{(int)t.TotalHours}:{t.Minutes:00}:{t.Seconds:00}" : $"{t.Minutes}:{t.Seconds:00}";
    }

    /// <summary>Accepts "ss", "mm:ss" or "hh:mm:ss" with optional decimals on the last part. Returns null if invalid.</summary>
    public static double? Parse(string? text)
    {
        if (string.IsNullOrWhiteSpace(text)) return null;
        var parts = text.Trim().Split(':');
        if (parts.Length is < 1 or > 3) return null;
        double total = 0;
        for (int i = 0; i < parts.Length; i++)
        {
            bool last = i == parts.Length - 1;
            var style = last ? NumberStyles.AllowDecimalPoint : NumberStyles.None;
            if (!double.TryParse(parts[i], style, CultureInfo.InvariantCulture, out var v) || v < 0) return null;
            if (!last && v != Math.Floor(v)) return null;
            if (i > 0 && v >= 60) return null;
            total = total * 60 + v;
        }
        return total;
    }
}
