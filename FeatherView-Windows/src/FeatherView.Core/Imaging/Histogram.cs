namespace FeatherView.Core.Imaging;

public sealed class HistogramData
{
    public int[] Red { get; } = new int[256];
    public int[] Green { get; } = new int[256];
    public int[] Blue { get; } = new int[256];
    public int[] Luma { get; } = new int[256];
    public long Pixels { get; set; }

    /// <summary>Scales a channel to 0..1 using a high percentile as the ceiling so one spike does not flatten the graph.</summary>
    public static double[] Normalize(int[] bins)
    {
        var sorted = bins.OrderBy(x => x).ToArray();
        double ceiling = Math.Max(1, sorted[(int)(sorted.Length * 0.995)]);
        return bins.Select(v => Math.Min(1.0, v / ceiling)).ToArray();
    }
}

public static class Histogram
{
    /// <summary>Computes channel histograms from 32-bit BGRA pixels (the format WPF's Bgra32 produces). Fully transparent pixels are ignored.</summary>
    public static HistogramData FromBgra(byte[] pixels, int width, int height, int stride)
    {
        var h = new HistogramData();
        for (int y = 0; y < height; y++)
        {
            int row = y * stride;
            for (int x = 0; x < width; x++)
            {
                int i = row + x * 4;
                if (i + 3 >= pixels.Length) break;
                if (pixels[i + 3] == 0) continue;
                byte b = pixels[i], g = pixels[i + 1], r = pixels[i + 2];
                h.Blue[b]++; h.Green[g]++; h.Red[r]++;
                int luma = (int)(0.2126 * r + 0.7152 * g + 0.0722 * b + 0.5);
                h.Luma[luma > 255 ? 255 : luma]++;
                h.Pixels++;
            }
        }
        return h;
    }
}
