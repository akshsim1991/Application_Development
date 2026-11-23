using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using FeatherView.Core.Imaging;

namespace FeatherView.Services;

public sealed record HistogramShapes(Geometry Luma, Geometry Red, Geometry Green, Geometry Blue);

/// <summary>Computes colour histograms from a small copy of the image (at most 256 px wide) so it costs a few milliseconds.</summary>
public static class HistogramService
{
    public static HistogramShapes? Compute(BitmapSource source)
    {
        try
        {
            double scale = Math.Min(1.0, 256.0 / Math.Max(source.PixelWidth, source.PixelHeight));
            BitmapSource small = scale < 1 ? new TransformedBitmap(source, new ScaleTransform(scale, scale)) : source;
            var bgra = new FormatConvertedBitmap(small, PixelFormats.Bgra32, null, 0);
            int w = bgra.PixelWidth, h = bgra.PixelHeight, stride = w * 4;
            var px = new byte[stride * h];
            bgra.CopyPixels(px, stride, 0);
            var data = Histogram.FromBgra(px, w, h, stride);
            return new HistogramShapes(Shape(data.Luma), Shape(data.Red), Shape(data.Green), Shape(data.Blue));
        }
        catch { return null; }
    }

    // 256 x 100 design space; the view stretches it to the panel width.
    private static Geometry Shape(int[] bins)
    {
        var n = HistogramData.Normalize(bins);
        var g = new StreamGeometry();
        using (var c = g.Open())
        {
            c.BeginFigure(new Point(0, 100), true, true);
            for (int i = 0; i < 256; i++) c.LineTo(new Point(i, 100 - n[i] * 98), true, false);
            c.LineTo(new Point(255, 100), true, false);
        }
        g.Freeze();
        return g;
    }
}
