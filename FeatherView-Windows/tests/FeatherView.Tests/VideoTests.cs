using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using FeatherView.Core.Updates;
using FeatherView.Core.Video;
using Xunit;

namespace FeatherView.Tests;

public class TimeCodeTests
{
    [Theory]
    [InlineData(0, "0:00")] [InlineData(65, "1:05")] [InlineData(3661, "1:01:01")] [InlineData(-5, "0:00")] [InlineData(double.NaN, "0:00")]
    public void Formats(double s, string expected) => Assert.Equal(expected, TimeCode.Format(s));

    [Theory]
    [InlineData("90", 90)] [InlineData("1:30", 90)] [InlineData("1:01:01", 3661)] [InlineData("0:05.5", 5.5)] [InlineData(" 2:00 ", 120)]
    public void Parses(string text, double expected) => Assert.Equal(expected, TimeCode.Parse(text)!.Value, 3);

    [Theory]
    [InlineData("")] [InlineData("abc")] [InlineData("1:75")] [InlineData("1:2:3:4")] [InlineData("-5")] [InlineData("1.5:30")]
    public void RejectsInvalid(string text) => Assert.Null(TimeCode.Parse(text));
}

public class PlaybackHistoryTests
{
    private static string Tmp() => Path.Combine(Path.GetTempPath(), "fv-hist-" + Guid.NewGuid().ToString("N") + ".json");

    [Fact] public void ResumeRules()
    {
        var h = new PlaybackHistory(Tmp());
        h.Record("a", "a.mp4", "C:\\a.mp4", 120, 600);
        Assert.Equal(120, h.ResumePosition("a"));
        h.Record("a", "a.mp4", "C:\\a.mp4", 595, 600);       // nearly finished: restart next time
        Assert.Null(h.ResumePosition("a"));
        h.Record("b", "b.mp4", "C:\\b.mp4", 2, 600);         // barely started
        Assert.Null(h.ResumePosition("b"));
        Assert.Equal(2, h.Recent.Count);
    }

    [Fact] public void PersistsAndIsBounded()
    {
        var p = Tmp(); var h = new PlaybackHistory(p);
        for (int i = 0; i < PlaybackHistory.Capacity + 20; i++) h.Record("k" + i, "n", "p", 60, 600);
        h.Save();
        var h2 = new PlaybackHistory(p);
        Assert.Equal(PlaybackHistory.Capacity, h2.Recent.Count);
        Assert.Equal(60, h2.ResumePosition("k" + (PlaybackHistory.Capacity + 19)));
        File.Delete(p);
    }

    [Fact] public void CorruptFileIsIgnored()
    {
        var p = Tmp(); File.WriteAllText(p, "{{{"); Assert.Empty(new PlaybackHistory(p).Recent); File.Delete(p);
    }

    [Fact] public void KeyChangesWhenFileChanges() =>
        Assert.NotEqual(PlaybackHistory.MakeKey("C:\\A.mp4", 10, DateTime.UnixEpoch), PlaybackHistory.MakeKey("C:\\A.mp4", 11, DateTime.UnixEpoch));
}

public class EngineDownloaderTests
{
    private sealed class Stub : HttpMessageHandler
    {
        public Func<HttpRequestMessage, HttpResponseMessage> Handler = _ => new(HttpStatusCode.NotFound);
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage r, CancellationToken ct) => Task.FromResult(Handler(r));
    }

    private static byte[] MakeZip(params (string name, string content)[] files)
    {
        using var ms = new MemoryStream();
        using (var z = new ZipArchive(ms, ZipArchiveMode.Create, true))
            foreach (var (n, c) in files) { var e = z.CreateEntry(n); using var w = new StreamWriter(e.Open()); w.Write(c); }
        return ms.ToArray();
    }

    [Fact] public async Task InstallsOnlyTheNamedDllAndLicenses()
    {
        var zip = MakeZip(("pkg/libmpv-2.dll", "DLL"), ("pkg/LICENSE.LGPL", "lic"), ("../evil.exe", "x"), ("pkg/other.dll", "y"));
        var sha = Convert.ToHexString(SHA256.HashData(zip));
        var h = new Stub { Handler = r => new(HttpStatusCode.OK) { Content = new ByteArrayContent(zip), RequestMessage = r } };
        var dl = new EngineDownloader(new HttpClient(h));
        var m = new EngineManifest { Version = "1", Win64 = new() { Url = "https://github.com/x/e.zip", Sha256 = sha, Size = zip.Length, File = "libmpv-2.dll" } };
        var dir = Path.Combine(Path.GetTempPath(), "fv-eng-" + Guid.NewGuid().ToString("N"));
        var dll = await dl.InstallAsync(m, dir);
        Assert.Equal("DLL", File.ReadAllText(dll));
        Assert.Equal(new[] { "LICENSE.LGPL", "libmpv-2.dll" }, Directory.GetFiles(dir).Select(Path.GetFileName).OrderBy(x => x, StringComparer.Ordinal));
        Directory.Delete(dir, true);
    }

    [Fact] public async Task RejectsWrongHash()
    {
        var zip = MakeZip(("libmpv-2.dll", "DLL"));
        var h = new Stub { Handler = r => new(HttpStatusCode.OK) { Content = new ByteArrayContent(zip), RequestMessage = r } };
        var m = new EngineManifest { Version = "1", Win64 = new() { Url = "https://github.com/x/e.zip", Sha256 = new string('0', 64) } };
        await Assert.ThrowsAsync<UpdateException>(() => new EngineDownloader(new HttpClient(h)).InstallAsync(m, Path.Combine(Path.GetTempPath(), "fv-eng-" + Guid.NewGuid().ToString("N"))));
    }

    [Fact] public async Task RejectsUntrustedHost()
    {
        var m = new EngineManifest { Version = "1", Win64 = new() { Url = "https://evil.example/e.zip", Sha256 = "00" } };
        await Assert.ThrowsAsync<UpdateException>(() => new EngineDownloader(new HttpClient(new Stub())).InstallAsync(m, Path.GetTempPath()));
    }

    [Fact] public void LocatorFindsDll()
    {
        var root = Path.Combine(Path.GetTempPath(), "fv-loc-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(root, "local", "FeatherView", "engine")); Directory.CreateDirectory(Path.Combine(root, "app"));
        Assert.Null(EngineLocator.Find(Path.Combine(root, "app"), Path.Combine(root, "local")));
        File.WriteAllText(Path.Combine(root, "local", "FeatherView", "engine", "libmpv-2.dll"), "x");
        Assert.NotNull(EngineLocator.Find(Path.Combine(root, "app"), Path.Combine(root, "local")));
        Directory.Delete(root, true);
    }
}

/// <summary>Runs against a real libmpv when one is installed (CI Windows runners skip these; the dev container has libmpv).</summary>
public class MpvPlayerTests
{
    private static string? Media(string name)
    {
        var dir = Environment.GetEnvironmentVariable("FV_TEST_MEDIA");
        var p = dir == null ? null : Path.Combine(dir, name);
        return p != null && File.Exists(p) ? p : null;
    }

    private static MpvPlayer? Create()
    {
        MpvPlayer.Configure(null);
        if (!MpvPlayer.IsAvailable(out _)) return null;
        return new MpvPlayer(new Dictionary<string, string> { ["vo"] = "null", ["ao"] = "null", ["pause"] = "yes", ["osc"] = "no", ["terminal"] = "no", ["config"] = "no", ["idle"] = "yes" });
    }

    private static void WaitFor(MpvPlayer p, MpvEventKind kind)
    {
        var end = DateTime.UtcNow.AddSeconds(10);
        while (DateTime.UtcNow < end)
        {
            if (p.DrainEvents().Any(e => e.Kind == kind)) return;
            Thread.Sleep(20);
        }
        throw new TimeoutException($"No {kind} event");
    }

    [SkippableFact] public void LoadsFileAndReadsTracksAndInfo()
    {
        var f = Media("test.mkv"); using var p = Create();
        Skip.If(f == null || p == null, "libmpv or test media not available");
        p!.LoadFile(f!); WaitFor(p, MpvEventKind.FileLoaded);
        Assert.InRange(p.GetDouble("duration") ?? 0, 9.5, 10.5);
        var t = p.GetTracks();
        Assert.Equal(1, t.Count(x => x.Type == "video")); Assert.Equal(2, t.Count(x => x.Type == "audio")); Assert.Equal(1, t.Count(x => x.Type == "sub"));
        Assert.Contains(t, x => x.Type == "audio" && x.Lang == "spa");
        var info = MpvInfo.Read(p).ToDictionary(x => x.Label, x => x.Value);
        Assert.Equal("320 × 240", info["Resolution"]); Assert.Contains("Video codec", info.Keys); Assert.Contains("Subtitle tracks", info.Keys);
    }

    [SkippableFact] public void ExactSeekFrameStepSpeedAndAbLoop()
    {
        var f = Media("plain.mp4"); using var p = Create();
        Skip.If(f == null || p == null, "libmpv or test media not available");
        p!.LoadFile(f!); WaitFor(p, MpvEventKind.FileLoaded);
        p.Command("seek", "2.5", "absolute+exact"); WaitFor(p, MpvEventKind.PlaybackRestart);
        Assert.Equal(2.5, p.GetDouble("time-pos")!.Value, 1);
        p.Command("frame-step"); Thread.Sleep(300);
        Assert.Equal(2.5 + 1.0 / 30, p.GetDouble("time-pos")!.Value, 2);
        Assert.True(p.SetDouble("speed", 0.25)); Assert.Equal(0.25, p.GetDouble("speed")!.Value, 3);
        Assert.True(p.SetDouble("speed", 4)); Assert.Equal(4, p.GetDouble("speed")!.Value, 3);
        Assert.True(p.SetDouble("ab-loop-a", 1)); Assert.True(p.SetDouble("ab-loop-b", 2));
        Assert.Equal(1, p.GetDouble("ab-loop-a")!.Value, 3);
        Assert.True(p.SetString("ab-loop-a", "no")); Assert.True(p.SetString("ab-loop-b", "no"));
        Assert.True(p.SetDouble("volume", 150)); Assert.Equal(150, p.GetDouble("volume")!.Value, 1);
    }

    [SkippableFact] public void EnginePropertiesAreAcceptedByLibmpv()
    {
        var f = Media("test.mkv"); using var p = Create();
        Skip.If(f == null || p == null, "libmpv or test media not available");
        p!.LoadFile(f!); WaitFor(p, MpvEventKind.FileLoaded);
        // every property/command the extended engine uses must be accepted
        foreach (var (n, v) in new[] { ("sub-font-size", "50"), ("sub-color", "#FFFF00"), ("sub-pos", "90"),
                                       ("sub-back-color", "#80000000"), ("sub-delay", "0.5"), ("audio-delay", "-0.2"), ("video-aspect-override", "16:9"),
                                       ("video-rotate", "90"), ("loop-file", "inf"), ("mute", "yes"), ("volume", "180"), ("sid", "no"), ("aid", "2") })
            Assert.True(p.SetString(n, v), $"property {n}");
        Assert.True(p.SetString("sub-border-style", "background-box") || p.SetString("sub-ass-style-overrides", "BorderStyle=3,Outline=3,OutlineColour=&H40000000"), "subtitle background");
        Assert.Equal(2, p.GetInt("aid")); Assert.Equal(0.5, p.GetDouble("sub-delay")!.Value, 2); Assert.Equal(90, p.GetInt("video-rotate"));
        Assert.True(p.SetString("sid", "1")); Assert.Equal(1, p.GetInt("sid"));
        Assert.True(p.SetString("video-aspect-override", "-1"));
        Assert.True(p.TryCommand("seek", "3", "absolute+keyframes"));
        Assert.True(p.TryCommand("seek", "4", "absolute+exact"));
        Assert.True(p.TryCommand("sub-add", Media("test.mkv")!.Replace("test.mkv", "s.srt"), "select"));
        Assert.Contains(p.GetTracks(), t => t.Type == "sub" && t.External);
        Assert.False(p.TryCommand("no-such-command"));
        Assert.Empty(p.GetChapters());
    }

    [SkippableFact] public void ReportsEndOfFileAndErrors()
    {
        var f = Media("plain.mp4"); using var p = Create();
        Skip.If(f == null || p == null, "libmpv or test media not available");
        p!.LoadFile(f!); WaitFor(p, MpvEventKind.FileLoaded);
        p.SetBool("pause", false); p.Command("seek", "5.9", "absolute+exact");
        WaitFor(p, MpvEventKind.EndOfFile);
        p.LoadFile("/definitely/not/here.mp4");
        WaitFor(p, MpvEventKind.Error);
    }

    [SkippableFact] public void MissingLibraryIsReportedNotThrown()
    {
        // With a nonexistent explicit path the resolver falls back to default names; either result must not throw.
        MpvPlayer.Configure("/nonexistent/libmpv.so");
        var ok = MpvPlayer.IsAvailable(out var err);
        Assert.True(ok || err != null);
        MpvPlayer.Configure(null);
    }
}
