using System.Globalization;

namespace FeatherView.Core.Media;

public static class AspectRatio
{
    /// <summary>"16:9" for common exact ratios (after reducing), otherwise a decimal such as "1.43:1".</summary>
    public static string Format(int width, int height)
    {
        if (width <= 0 || height <= 0) return "";
        int g = Gcd(width, height), w = width / g, h = height / g;
        if (w <= 32 && h <= 32) return $"{w}:{h}";
        double r = (double)width / height;
        // near-matches to common ratios (e.g. 1366x768 is 683:384 reduced, but is "16:9" to a person)
        foreach (var (cw, ch) in new[] { (16, 9), (4, 3), (3, 2), (1, 1), (21, 9), (5, 4), (16, 10), (9, 16), (3, 4), (2, 3) })
            if (Math.Abs(r - (double)cw / ch) < 0.01) return $"{cw}:{ch}";
        return r.ToString("0.##", CultureInfo.InvariantCulture) + ":1";
    }

    private static int Gcd(int a, int b) { while (b != 0) { (a, b) = (b, a % b); } return a; }
}
