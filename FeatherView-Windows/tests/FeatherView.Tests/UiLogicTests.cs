using FeatherView.Core.Settings;
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

public class GroupingTests
{
    private static MediaFile F(string path, long len, DateTime utc, MediaKind k = MediaKind.Image) =>
        new(path, System.IO.Path.GetFileName(path), len, utc, k);

    [Fact]
    public void GroupsByTypeAndKeepsOrderInsideGroups()
    {
        var files = new List<MediaFile>
        {
            F("/p/a.jpg", 10, new DateTime(2024, 1, 1)), F("/p/b.png", 10, new DateTime(2024, 1, 1)),
            F("/p/c.jpg", 10, new DateTime(2024, 1, 1)), F("/p/d.mp4", 10, new DateTime(2024, 1, 1), MediaKind.Video),
        };
        var g = MediaGrouping.Apply(files, GroupMode.Type, false);
        Assert.Equal(new[] { "a.jpg", "c.jpg", "b.png", "d.mp4" }, g.Files.Select(f => f.Name));
        Assert.StartsWith("JPG", g.Labels![0]); Assert.StartsWith("JPG", g.Labels[1]); Assert.StartsWith("PNG", g.Labels[2]); Assert.StartsWith("MP4", g.Labels[3]);
    }

    [Fact]
    public void DateGroupsFollowDescendingChoice()
    {
        var files = new List<MediaFile> { F("/p/a.jpg", 1, new DateTime(2022, 6, 15, 12, 0, 0, DateTimeKind.Utc)), F("/p/b.jpg", 1, new DateTime(2024, 6, 15, 12, 0, 0, DateTimeKind.Utc)) };
        Assert.Equal("a.jpg", MediaGrouping.Apply(files, GroupMode.Year, false).Files[0].Name);
        Assert.Equal("b.jpg", MediaGrouping.Apply(files, GroupMode.Year, true).Files[0].Name);
    }

    [Fact]
    public void SizeBucketsAndNoGrouping()
    {
        var files = new List<MediaFile> { F("/p/a.jpg", 50L << 20, DateTime.UtcNow), F("/p/b.jpg", 10 * 1024, DateTime.UtcNow) };
        var g = MediaGrouping.Apply(files, GroupMode.Size, false);
        Assert.Equal("b.jpg", g.Files[0].Name);
        Assert.Null(MediaGrouping.Apply(files, GroupMode.None, false).Labels);
    }

    [Fact]
    public void FolderGroupsUseFolderName()
    {
        var files = new List<MediaFile> { F("/x/Holiday/a.jpg", 1, DateTime.UtcNow), F("/x/Art/b.jpg", 1, DateTime.UtcNow) };
        var g = MediaGrouping.Apply(files, GroupMode.Folder, false);
        Assert.Equal(new[] { "Art", "Holiday" }, g.Labels);
    }
}
