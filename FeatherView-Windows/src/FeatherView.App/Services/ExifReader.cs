using System.Globalization;
using System.Windows.Media.Imaging;

namespace FeatherView.Services;

public sealed record InfoLine(string Label, string Value);

/// <summary>Reads a few camera details through WIC metadata queries. Location data is intentionally never read.</summary>
public static class ExifReader
{
    public static ushort ReadOrientation(BitmapFrame frame)
    {
        try
        {
            if (frame.Metadata is BitmapMetadata m && m.GetQuery("System.Photo.Orientation") is ushort o && o is >= 1 and <= 8) return o;
        }
        catch { /* format without metadata */ }
        return 1;
    }

    public static List<InfoLine> Read(BitmapFrame frame)
    {
        var l = new List<InfoLine>();
        if (frame.Metadata is not BitmapMetadata m) return l;
        void Add(string label, string? v) { if (!string.IsNullOrWhiteSpace(v)) l.Add(new InfoLine(label, v.Trim())); }
        object? Q(string q) { try { return m.GetQuery(q); } catch { return null; } }
        try { Add("Camera", string.Join(' ', new[] { m.CameraManufacturer, m.CameraModel }.Where(s => !string.IsNullOrWhiteSpace(s)))); } catch { }
        try { Add("Taken", m.DateTaken); } catch { }
        if (Q("System.Photo.FNumber") is double f) Add("Aperture", $"f/{f.ToString("0.#", CultureInfo.InvariantCulture)}");
        if (Q("System.Photo.ExposureTime") is double e && e > 0) Add("Exposure", e >= 1 ? $"{e:0.#} s" : $"1/{Math.Round(1 / e)} s");
        if (Q("System.Photo.ISOSpeed") is ushort iso) Add("ISO", iso.ToString(CultureInfo.InvariantCulture));
        if (Q("System.Photo.FocalLength") is double fl) Add("Focal length", $"{fl:0.#} mm");
        try { Add("Software", m.ApplicationName); } catch { }
        return l;
    }
}
