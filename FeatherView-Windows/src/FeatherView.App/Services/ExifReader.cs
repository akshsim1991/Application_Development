using System.Globalization;
using System.Windows.Media.Imaging;

namespace FeatherView.Services;

public sealed record InfoLine(string Label, string Value);
public sealed record ImageMeta(string? Taken, string? Camera, string? Lens);

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

    public static ImageMeta ReadMeta(BitmapFrame frame)
    {
        string? taken = null, camera = null, lens = null;
        try
        {
            if (frame.Metadata is BitmapMetadata m)
            {
                try { if (DateTime.TryParse(m.DateTaken, out var d)) taken = d.ToString("MMMM d, yyyy   h:mm tt", CultureInfo.CurrentCulture); } catch { }
                try
                {
                    var make = m.CameraManufacturer?.Trim(); var model = m.CameraModel?.Trim();
                    camera = string.IsNullOrWhiteSpace(model) ? make : (!string.IsNullOrWhiteSpace(make) && !model.StartsWith(make, StringComparison.OrdinalIgnoreCase) ? make + " " + model : model);
                    if (string.IsNullOrWhiteSpace(camera)) camera = null;
                }
                catch { }
                try { lens = m.GetQuery("System.Photo.LensModel") as string; if (string.IsNullOrWhiteSpace(lens)) lens = null; } catch { }
            }
        }
        catch { }
        return new ImageMeta(taken, camera, lens?.Trim());
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
