using System.Diagnostics;
using FeatherView.Core.Video;
using Xunit;

namespace FeatherView.Tests;

public class MergeLogicTests
{
    private static MediaProbe P(string v = "h264", int w = 1280, int h = 720, double fps = 30, string? a = "aac", int sr = 48000, int ch = 2, double dur = 5) =>
        new("/x.mp4", dur, v, w, h, fps, "yuv420p", a, sr, ch);

    [Fact]
    public void ConcatListEscapesQuotesAndBackslashes()
    {
        var t = VideoMerger.BuildConcatList(new[] { @"C:\Videos\a b.mp4", "/tmp/it's.mp4" });
        Assert.Equal("file 'C:/Videos/a b.mp4'\nfile '/tmp/it'\\''s.mp4'\n", t);
    }

    [Fact]
    public void CanCopyOnlyWhenFilesMatch()
    {
        Assert.True(VideoMerger.CanCopy(new[] { P(), P() }));
        Assert.False(VideoMerger.CanCopy(new[] { P(), P(w: 1920, h: 1080) }));
        Assert.False(VideoMerger.CanCopy(new[] { P(), P(fps: 25) }));
        Assert.False(VideoMerger.CanCopy(new[] { P(), P(v: "hevc") }));
        Assert.False(VideoMerger.CanCopy(new[] { P(), P(a: null) }));
        Assert.False(VideoMerger.CanCopy(new[] { P(), P(sr: 44100) }));
        Assert.True(VideoMerger.CanCopy(new[] { P(a: null), P(a: null) }));
    }

    [Fact]
    public void UniqueOutputNeverPicksAnExistingFile()
    {
        var dir = Path.Combine(Path.GetTempPath(), "fv-merge-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(dir);
        try
        {
            Assert.Equal(Path.Combine(dir, "Merged.mp4"), VideoMerger.UniqueOutput(dir, "Merged"));
            File.WriteAllText(Path.Combine(dir, "Merged.mp4"), "x");
            Assert.Equal(Path.Combine(dir, "Merged (2).mp4"), VideoMerger.UniqueOutput(dir, "Merged"));
            Assert.EndsWith("Merged video.mp4", VideoMerger.UniqueOutput(dir, "   "));
            Assert.DoesNotContain('/', Path.GetFileName(VideoMerger.UniqueOutput(dir, "a/b:c")));
        }
        finally { Directory.Delete(dir, true); }
    }

    [Fact]
    public void FilterGraphJoinsEveryInputAndFillsMissingAudio()
    {
        var g = VideoMerger.BuildFilterGraph(new[] { P(), P(a: null, dur: 2.5) }, 1280, 720, 30);
        Assert.Contains("[0:a:0]aresample", g);
        Assert.Contains("anullsrc=r=48000:cl=stereo:d=2.5[a1]", g);
        Assert.EndsWith("[v0][a0][v1][a1]concat=n=2:v=1:a=1[v][a]", g);
    }
}

/// <summary>Real ffmpeg runs; skipped when ffmpeg is not installed. Set FV_FFMPEG to test a specific build.</summary>
public class MergeIntegrationTests
{
    private static string? Ffmpeg() => Environment.GetEnvironmentVariable("FV_FFMPEG") is { Length: > 0 } e && File.Exists(e) ? e : FfmpegLocator.Find(AppContext.BaseDirectory, Path.GetTempPath());

    private static void Make(string ffmpeg, string path, string video, string audio, int seconds = 2)
    {
        var args = new List<string> { "-y", "-loglevel", "error", "-f", "lavfi", "-i", video };
        if (audio != "") { args.AddRange(new[] { "-f", "lavfi", "-i", audio }); }
        args.AddRange(new[] { "-t", seconds.ToString(), "-c:v", "libx264", "-pix_fmt", "yuv420p" });
        if (audio != "") args.AddRange(new[] { "-c:a", "aac" });
        args.Add(path);
        var psi = new ProcessStartInfo(ffmpeg) { UseShellExecute = false, RedirectStandardError = true };
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = Process.Start(psi)!; var err = p.StandardError.ReadToEnd(); p.WaitForExit();
        Assert.True(p.ExitCode == 0, err);
    }

    [SkippableFact]
    public async Task JoinsMatchingClipsWithoutReencoding_AndMismatchedClipsWithReencoding()
    {
        var ff = Ffmpeg(); Skip.If(ff == null, "ffmpeg not installed");
        var dir = Path.Combine(Path.GetTempPath(), "fv-merge-it-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(dir);
        try
        {
            var a = Path.Combine(dir, "a.mp4"); var b = Path.Combine(dir, "b.mp4"); var c = Path.Combine(dir, "c.mp4");
            Make(ff!, a, "testsrc=size=320x240:rate=25", "sine=frequency=440:sample_rate=48000");
            Make(ff!, b, "testsrc=size=320x240:rate=25", "sine=frequency=880:sample_rate=48000");
            Make(ff!, c, "testsrc=size=640x360:rate=30", "");        // different size, rate, and no audio
            var m = new VideoMerger(ff!);

            var o1 = Path.Combine(dir, "ab.mp4");
            var r1 = await m.MergeAsync(new[] { a, b }, o1, MergeMode.Auto);
            Assert.True(r1.Success, r1.Error); Assert.False(r1.Reencoded);
            Assert.InRange((await m.ProbeAsync(o1)).Duration, 3.8, 4.3);

            var o2 = Path.Combine(dir, "abc.mp4"); double last = 0;
            var r2 = await m.MergeAsync(new[] { a, b, c }, o2, MergeMode.Auto, new Progress<double>(v => last = v));
            Assert.True(r2.Success, r2.Error); Assert.True(r2.Reencoded);
            var pr = await m.ProbeAsync(o2);
            Assert.InRange(pr.Duration, 5.5, 6.5); Assert.Equal(320, pr.Width); Assert.True(pr.HasAudio);

            var copyOnly = await m.MergeAsync(new[] { a, c }, Path.Combine(dir, "x.mp4"), MergeMode.Copy);
            Assert.False(copyOnly.Success);
            Assert.False(File.Exists(Path.Combine(dir, "x.mp4")));
            var again = await m.MergeAsync(new[] { a, b }, o1);          // never overwrites
            Assert.False(again.Success);
        }
        finally { Directory.Delete(dir, true); }
    }
}
