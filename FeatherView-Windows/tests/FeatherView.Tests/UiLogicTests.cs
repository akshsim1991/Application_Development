using FeatherView.Core.Imaging;
using FeatherView.Core.Media;
using Xunit;

namespace FeatherView.Tests;

public class AspectRatioTests
{
    [Theory]
    [InlineData(3840, 2160, "16:9")] [InlineData(1920, 1080, "16:9")] [InlineData(1366, 768, "16:9")] [InlineData(4000, 3000, "4:3")]
    [InlineData(1000, 1000, "1:1")] [InlineData(3000, 2000, "3:2")] [InlineData(1080, 1920, "9:16")] [InlineData(2560, 1080, "2.37:1")]
    public void Formats(int w, int h, string expected) => Assert.Equal(expected, AspectRatio.Format(w, h));

    [Fact] public void InvalidIsEmpty() { Assert.Equal("", AspectRatio.Format(0, 10)); Assert.Equal("", AspectRatio.Format(10, -1)); }
}

public class MediaFilterTests
{
    private static MediaFile F(string n, MediaKind k = MediaKind.Image) => new("C:\\x\\" + n, n, 1, DateTime.UtcNow, k);

    [Fact] public void ClassifiesAndCounts()
    {
        var files = new[] { F("a.jpg"), F("b.JPEG"), F("c.png"), F("d.gif"), F("e.webp"), F("f.tiff"), F("g.mp4", MediaKind.Video) };
        var c = MediaFilter.Count(files);
        Assert.Equal(7, c[QuickFilter.All]); Assert.Equal(2, c[QuickFilter.Jpg]); Assert.Equal(1, c[QuickFilter.Png]);
        Assert.Equal(1, c[QuickFilter.Gif]); Assert.Equal(1, c[QuickFilter.Video]); Assert.Equal(2, c[QuickFilter.Other]);
        Assert.Equal(7, c.Where(kv => kv.Key != QuickFilter.All).Sum(kv => kv.Value));
    }

    [Fact] public void FiltersAndSearches()
    {
        Assert.True(MediaFilter.Matches(F("Holiday1.JPG"), QuickFilter.Jpg, "holi"));
        Assert.False(MediaFilter.Matches(F("Holiday1.JPG"), QuickFilter.Png, null));
        Assert.False(MediaFilter.Matches(F("Holiday1.JPG"), QuickFilter.All, "xyz"));
        Assert.True(MediaFilter.Matches(F("a.png"), QuickFilter.All, "  "));
        Assert.Equal(1, MediaFilter.Count(new[] { F("cat.jpg"), F("dog.jpg") }, "cat")[QuickFilter.All]);
    }
}

public class HistogramTests
{
    [Fact] public void CountsChannelsAndSkipsTransparent()
    {
        // 2x1 image: opaque pure red, and a fully transparent pixel
        var px = new byte[] { 0, 0, 255, 255, 10, 20, 30, 0 };
        var h = Histogram.FromBgra(px, 2, 1, 8);
        Assert.Equal(1, h.Pixels); Assert.Equal(1, h.Red[255]); Assert.Equal(1, h.Green[0]); Assert.Equal(1, h.Blue[0]);
        Assert.Equal(1, h.Luma[54]);   // 0.2126 * 255 = 54.2
    }

    [Fact] public void RespectsStride()
    {
        // 1 pixel wide, 2 rows, stride 8 (padding): red then blue
        var px = new byte[] { 0, 0, 255, 255, 0, 0, 0, 0, 255, 0, 0, 255, 0, 0, 0, 0 };
        var h = Histogram.FromBgra(px, 1, 2, 8);
        Assert.Equal(2, h.Pixels); Assert.Equal(1, h.Red[255]); Assert.Equal(1, h.Blue[255]);
    }

    [Fact] public void NormalizeIsBounded()
    {
        var bins = new int[256]; bins[10] = 1_000_000; for (int i = 20; i < 200; i++) bins[i] = 50;
        var n = HistogramData.Normalize(bins);
        Assert.All(n, v => Assert.InRange(v, 0, 1)); Assert.Equal(1, n[10]);
    }
}
