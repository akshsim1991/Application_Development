using FeatherView.Core.Imaging;
using FeatherView.Core.Media;
using Xunit;

namespace FeatherView.Tests;

public class ShuffleOrderTests
{
    [Fact] public void ShowsEveryItemOnceBeforeRepeating()
    {
        var s = new ShuffleOrder(42); int current = 0; var shown = new List<int> { 0 };
        for (int i = 0; i < 9; i++) { current = s.Next(10, current)!.Value; shown.Add(current); }
        Assert.Equal(10, shown.Distinct().Count());            // a full round without repeats
        Assert.NotEqual(shown[9], s.Next(10, shown[9]));       // the next round starts but never repeats the current item
    }

    [Fact] public void RespectsAllowedAndHandlesTinyLists()
    {
        var s = new ShuffleOrder(1);
        for (int i = 0; i < 50; i++) Assert.True(s.Next(6, 0, x => x % 2 == 0) is 2 or 4);
        Assert.Null(new ShuffleOrder().Next(1, 0)); Assert.Null(new ShuffleOrder().Next(0, 0));
        Assert.Null(new ShuffleOrder().Next(3, 0, x => x == 0));
    }
}

public class GpsMathTests
{
    private static ulong R(uint n, uint d) => ((ulong)d << 32) | n;

    [Fact] public void ConvertsRationalsAndSigns()
    {
        var lat = GpsMath.FromRationals(new[] { R(48, 1), R(51, 1), R(30, 1) }, "N");
        Assert.Equal(48.858333, lat!.Value, 5);
        Assert.Equal(-48.858333, GpsMath.FromRationals(new[] { R(48, 1), R(51, 1), R(30, 1) }, "S")!.Value, 5);
        Assert.Equal(11.5, GpsMath.FromRationals(new[] { R(11, 1), R(300, 10) }, "E")!.Value, 5);
        Assert.Equal(-2.25, GpsMath.FromDegrees(new[] { 2.0, 15.0, 0.0 }, "W")!.Value, 5);
        Assert.Null(GpsMath.FromRationals(null, "N")); Assert.Null(GpsMath.FromDegrees(Array.Empty<double>(), "N"));
        Assert.Equal(0, GpsMath.FromRationals(new[] { R(5, 0) }, "N")!.Value);   // zero denominator must not divide by zero
    }

    [Fact] public void ValidatesAndFormats()
    {
        Assert.True(GpsMath.IsValid(46.5, 11.8)); Assert.False(GpsMath.IsValid(91, 0)); Assert.False(GpsMath.IsValid(0, 0)); Assert.False(GpsMath.IsValid(10, 200));
        Assert.Equal("46.5000° N, 11.7500° W", GpsMath.Format(46.5, -11.75));
        Assert.StartsWith("https://www.openstreetmap.org/", GpsMath.MapUrl(46.5, 11.75)); Assert.Contains("mlat=46.50000", GpsMath.MapUrl(46.5, 11.75));
    }
}

public class CameraSummaryTests
{
    [Fact] public void BuildsReadableLine() =>
        Assert.Equal("Sony A7R IV, 24mm f/5.6, 1/250s, ISO 100", CameraSummary.Build("Sony A7R IV", 24, 5.6, 1.0 / 250, 100));

    [Fact] public void SkipsMissingPartsAndFormatsLongExposures()
    {
        Assert.Equal("Canon EOS R6, 2.5s", CameraSummary.Build("Canon EOS R6", null, null, 2.5, null));
        Assert.Equal("f/1.8, ISO 400", CameraSummary.Build(null, 0, 1.8, null, 400));
        Assert.Equal("", CameraSummary.Build(" ", null, null, null, null));
    }
}
