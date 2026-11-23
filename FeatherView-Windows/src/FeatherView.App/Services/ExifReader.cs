using System.Globalization;
using FeatherView.Core.Imaging;
using System.Windows.Media.Imaging;

namespace FeatherView.Services;

public sealed record InfoLine(string Label, string Value);
public sealed record ImageMeta(string? Taken, string? Camera, string? Lens, double? FocalMm = null, double? FNumber = null,
                               double? ExposureSeconds = null, int? Iso = null, double? Latitude = null, double? Longitude = null);

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
        double? focal = null, fnum = null, exposure = null, lat = null, lon = null; int? iso = null;
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
                object? Q(string q) { try { return m.GetQuery(q); } catch { return null; } }
                static double? Num(object? o) => o switch { double d => d, float f => f, uint u => u, ushort us => us, int i => i, _ => null };
                focal = Num(Q("System.Photo.FocalLength")); fnum = Num(Q("System.Photo.FNumber")); exposure = Num(Q("System.Photo.ExposureTime"));
                iso = Num(Q("System.Photo.ISOSpeed")) is double isoValue ? (int)isoValue : null;
                // Location is read only to show it in the details panel; it never leaves the PC unless the user asks to open a map.
                lat = GpsMath.FromDegrees(Q("System.GPS.Latitude") as double[], Q("System.GPS.LatitudeRef") as string)
                      ?? GpsMath.FromRationals(Q("/app1/ifd/gps/{ushort=2}") as ulong[], Q("/app1/ifd/gps/{ushort=1}") as string);
                lon = GpsMath.FromDegrees(Q("System.GPS.Longitude") as double[], Q("System.GPS.LongitudeRef") as string)
                      ?? GpsMath.FromRationals(Q("/app1/ifd/gps/{ushort=4}") as ulong[], Q("/app1/ifd/gps/{ushort=3}") as string);
                if (lat is not double la || lon is not double lo || !GpsMath.IsValid(la, lo)) { lat = null; lon = null; }
            }
        }
        catch { }
        return new ImageMeta(taken, camera, lens?.Trim(), focal, fnum, exposure, iso, lat, lon);
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
