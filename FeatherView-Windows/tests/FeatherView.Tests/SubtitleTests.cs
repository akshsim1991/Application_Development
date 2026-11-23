using FeatherView.Core.Video;
using Xunit;

namespace FeatherView.Tests;

public class SubtitleParserTests
{
    [Fact] public void ParsesSrtWithTagsAndMultipleLines()
    {
        var srt = "1\r\n00:00:01,000 --> 00:00:03,500\r\n<i>Hello</i> &amp; welcome\r\nsecond line\r\n\r\n2\r\n00:01:02,5 --> 00:01:04,000\r\nLater\r\n";
        var c = SubtitleParser.Parse(srt, ".srt");
        Assert.Equal(2, c.Count);
        Assert.Equal(1.0, c[0].Start); Assert.Equal(3.5, c[0].End); Assert.Equal("Hello & welcome\nsecond line", c[0].Text);
        Assert.Equal(62.5, c[1].Start);
    }

    [Fact] public void ParsesVttWithoutHoursHeaderAndSettings()
    {
        var vtt = "WEBVTT\n\nNOTE a comment\n\nintro\n00:01.000 --> 00:02.000 align:start position:0%\nFirst\n\n01:00:00.250 --> 01:00:01.000\nHour in\n";
        var c = SubtitleParser.Parse(vtt, ".vtt");
        Assert.Equal(2, c.Count); Assert.Equal(1.0, c[0].Start); Assert.Equal("First", c[0].Text); Assert.Equal(3600.25, c[1].Start);
    }

    [Fact] public void ParsesAssDialogueStrippingOverrides()
    {
        var ass = "[Script Info]\nTitle: x\n\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\nDialogue: 0,0:00:05.00,0:00:07.50,Default,,0,0,0,,{\\i1}Hi, there{\\i0}\\Nnext\n";
        var c = SubtitleParser.Parse(ass, ".ass");
        Assert.Single(c); Assert.Equal(5.0, c[0].Start); Assert.Equal(7.5, c[0].End); Assert.Equal("Hi, there\nnext", c[0].Text);
    }

    [Fact] public void TextAtHonoursDelayAndEnds()
    {
        var t = new SubtitleTrack("x", SubtitleParser.Parse("1\n00:00:10,000 --> 00:00:12,000\nA\n\n2\n00:00:11,000 --> 00:00:14,000\nB\n", ".srt"));
        Assert.Equal("", t.TextAt(9.9)); Assert.Equal("A", t.TextAt(10)); Assert.Equal("A\nB", t.TextAt(11.5)); Assert.Equal("B", t.TextAt(12.5)); Assert.Equal("", t.TextAt(14.1));
        Assert.Equal("A", t.TextAt(12, delay: 1.5));       // subtitles 1.5 s later: A still visible at 12 s
        Assert.Equal("", t.TextAt(10, delay: 1));
    }

    [Fact] public void GarbageAndEmptyInputAreSafe()
    {
        Assert.Empty(SubtitleParser.Parse("", ".srt")); Assert.Empty(SubtitleParser.Parse("not a subtitle file\nat all", ".vtt")); Assert.Empty(SubtitleParser.Parse("[Events]\nDialogue: nonsense", ".ass"));
    }

    [Fact] public void LoadsUtf8AndLegacyEncodingsAndFindsNeighbours()
    {
        var dir = Path.Combine(Path.GetTempPath(), "fv-sub-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(dir);
        File.WriteAllBytes(Path.Combine(dir, "movie.srt"), new byte[] { 0xEF, 0xBB, 0xBF }.Concat(System.Text.Encoding.UTF8.GetBytes("1\n00:00:01,000 --> 00:00:02,000\nCafé\n")).ToArray());
        File.WriteAllBytes(Path.Combine(dir, "movie.fr.srt"), System.Text.Encoding.Latin1.GetBytes("1\n00:00:01,000 --> 00:00:02,000\nCafé\n"));   // not valid UTF-8
        File.WriteAllText(Path.Combine(dir, "other.srt"), "x"); File.WriteAllText(Path.Combine(dir, "movie.mp4"), "x");
        Assert.Equal(new[] { "movie.fr.srt", "movie.srt" }, SubtitleParser.FindFor(Path.Combine(dir, "movie.mp4")).Select(Path.GetFileName));
        Assert.Equal("Café", SubtitleParser.Load(Path.Combine(dir, "movie.srt")).TextAt(1.5));
        Assert.Equal("Café", SubtitleParser.Load(Path.Combine(dir, "movie.fr.srt")).TextAt(1.5));
        Directory.Delete(dir, true);
    }
}
