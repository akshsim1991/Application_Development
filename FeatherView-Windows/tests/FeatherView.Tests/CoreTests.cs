using System.Net;
using System.Security.Cryptography;
using System.Text;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Media;
using FeatherView.Core.Settings;
using FeatherView.Core.Updates;
using Xunit;

namespace FeatherView.Tests;

public class NaturalComparerTests
{
    [Fact] public void NumbersCompareNumerically()
    {
        var l = new List<string> { "img10.jpg", "img2.jpg", "IMG1.jpg", "img02a.jpg" };
        l.Sort(NaturalComparer.Instance);
        Assert.Equal(new[] { "IMG1.jpg", "img2.jpg", "img02a.jpg", "img10.jpg" }, l);
    }
    [Fact] public void EqualIgnoresCase() => Assert.Equal(0, NaturalComparer.Instance.Compare("A.png", "a.PNG"));
}

public class VersionTests
{
    [Theory]
    [InlineData("0.2.0", "0.1.9", true)] [InlineData("1.0", "1.0.0", false)] [InlineData("v1.10", "1.9", true)]
    [InlineData("1.0.0", "1.0.1", false)] [InlineData("2.0.0-beta", "1.9.9", true)]
    public void IsNewer(string a, string b, bool expected) => Assert.Equal(expected, VersionUtil.IsNewer(a, b));
}

public class SettingsTests
{
    private static string Tmp() => Path.Combine(Path.GetTempPath(), "fv-test-" + Guid.NewGuid().ToString("N"));

    [Fact] public void RoundTrip()
    {
        var dir = Tmp(); var svc = new SettingsService(Path.Combine(dir, "s.json"));
        var s = new AppSettings { Theme = AppTheme.Dark, SlideshowSeconds = 9, ThumbnailSize = 200 };
        s.AddRecent("C:\\a.jpg");
        Assert.True(svc.Save(s));
        var r = svc.Load();
        Assert.Equal(AppTheme.Dark, r.Theme); Assert.Equal(9, r.SlideshowSeconds); Assert.Equal(200, r.ThumbnailSize);
        Assert.Single(r.RecentFiles);
        Directory.Delete(dir, true);
    }
    [Fact] public void CorruptFileGivesDefaults()
    {
        var dir = Tmp(); Directory.CreateDirectory(dir); var p = Path.Combine(dir, "s.json");
        File.WriteAllText(p, "{ not json");
        var r = new SettingsService(p).Load();
        Assert.Equal(AppTheme.System, r.Theme); Assert.True(File.Exists(p + ".corrupt"));
        Directory.Delete(dir, true);
    }
    [Fact] public void ValuesAreClamped()
    {
        var s = new AppSettings { ThumbnailSize = 5000, SlideshowSeconds = -3, ThumbnailParallelism = 99 };
        s.Normalize();
        Assert.Equal(AppSettings.MaxThumb, s.ThumbnailSize); Assert.Equal(1, s.SlideshowSeconds); Assert.Equal(8, s.ThumbnailParallelism);
    }
    [Fact] public void RecentIsDeduplicatedAndLimited()
    {
        var s = new AppSettings { MaxRecentFiles = 3 };
        foreach (var f in new[] { "a", "b", "c", "d", "B" }) s.AddRecent(f);
        Assert.Equal(new[] { "B", "d", "c" }, s.RecentFiles);
    }
}

public class FavoritesTests
{
    [Fact] public void ToggleAddsAndRemovesCaseInsensitively()
    {
        var s = new AppSettings();
        Assert.True(s.ToggleFavorite("C:\\Pics\\A.jpg")); Assert.True(s.IsFavorite("c:\\pics\\a.JPG"));
        Assert.False(s.ToggleFavorite("c:\\pics\\a.jpg")); Assert.Empty(s.Favorites);
    }
    [Fact] public void FavoritesSurviveSaveAndLoad()
    {
        var dir = Path.Combine(Path.GetTempPath(), "fv-fav-" + Guid.NewGuid().ToString("N")); var svc = new SettingsService(Path.Combine(dir, "s.json"));
        var s = new AppSettings(); s.ToggleFavorite("C:\\x.png"); s.ShowFilmstrip = false; svc.Save(s);
        var r = svc.Load(); Assert.True(r.IsFavorite("C:\\x.png")); Assert.False(r.ShowFilmstrip); Assert.True(r.ShowSidebar);
        Directory.Delete(dir, true);
    }
}

public class LruTests
{
    [Fact] public void EvictsLeastRecentlyUsed()
    {
        var c = new LruCache<int, string>(2);
        c.Set(1, "a"); c.Set(2, "b"); Assert.True(c.TryGet(1, out _)); c.Set(3, "c");
        Assert.False(c.TryGet(2, out _)); Assert.True(c.TryGet(1, out _)); Assert.True(c.TryGet(3, out _));
    }
}

public class ScannerTests
{
    [Fact] public void FindsAndSortsMedia()
    {
        var dir = Path.Combine(Path.GetTempPath(), "fv-scan-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(dir);
        foreach (var n in new[] { "b10.jpg", "b2.PNG", "a.txt", "c.mp4", "d.gif" }) File.WriteAllText(Path.Combine(dir, n), "x");
        var all = MediaScanner.Scan(dir);
        Assert.Equal(new[] { "b2.PNG", "b10.jpg", "c.mp4", "d.gif" }, all.Select(f => f.Name));
        Assert.Equal(3, MediaScanner.Scan(dir, includeVideo: false).Count);
        Assert.Equal("d.gif", MediaScanner.Scan(dir, descending: true).First().Name);
        Directory.Delete(dir, true);
    }
    [Fact] public void MissingFolderIsEmpty() => Assert.Empty(MediaScanner.Scan(Path.Combine(Path.GetTempPath(), "nope-" + Guid.NewGuid())));
}

public class LogTests
{
    [Fact] public void MasksUserProfile()
    {
        var p = Path.Combine(Path.GetTempPath(), "fv-log-" + Guid.NewGuid().ToString("N") + ".log");
        var log = new FileLog(p, userProfile: "C:\\Users\\Someone");
        log.Info("opened C:\\Users\\Someone\\Pictures\\x.jpg");
        var text = File.ReadAllText(p);
        Assert.DoesNotContain("Someone", text); Assert.Contains("%USERPROFILE%", text);
        File.Delete(p);
    }
}

public class UpdateTests
{
    private sealed class Stub : HttpMessageHandler
    {
        public Func<HttpRequestMessage, HttpResponseMessage> Handler = _ => new(HttpStatusCode.NotFound);
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage r, CancellationToken ct) => Task.FromResult(Handler(r));
    }
    private static HttpResponseMessage Json(string s, HttpRequestMessage r) => new(HttpStatusCode.OK) { Content = new StringContent(s, Encoding.UTF8, "application/json"), RequestMessage = r };

    [Fact] public async Task DetectsNewerVersion()
    {
        var h = new Stub { Handler = r => Json("{\"version\":\"1.2.0\",\"notes\":\"n\",\"win64\":{\"url\":\"https://github.com/x/y.exe\",\"sha256\":\"00\"}}", r) };
        var svc = new UpdateService(new HttpClient(h));
        Assert.NotNull(await svc.CheckAsync("1.1.0"));
        Assert.Null(await svc.CheckAsync("1.2.0"));
    }
    [Fact] public async Task RejectsUntrustedHostAndHttp()
    {
        var svc = new UpdateService(new HttpClient(new Stub()), new Uri("http://raw.githubusercontent.com/a.json"));
        await Assert.ThrowsAsync<UpdateException>(() => svc.CheckAsync("1.0"));
        var svc2 = new UpdateService(new HttpClient(new Stub()), new Uri("https://evil.example/a.json"));
        await Assert.ThrowsAsync<UpdateException>(() => svc2.CheckAsync("1.0"));
    }
    [Fact] public async Task DownloadVerifiesHash()
    {
        var payload = Encoding.UTF8.GetBytes("installer-bytes");
        var good = Convert.ToHexString(SHA256.HashData(payload));
        var h = new Stub { Handler = r => new(HttpStatusCode.OK) { Content = new ByteArrayContent(payload), RequestMessage = r } };
        var svc = new UpdateService(new HttpClient(h));
        var dir = Path.Combine(Path.GetTempPath(), "fv-up-" + Guid.NewGuid().ToString("N"));
        UpdateManifest M(string sha) => new() { Version = "9.9", Win64 = new() { Url = "https://github.com/a/b.exe", Sha256 = sha, Size = payload.Length } };
        var path = await svc.DownloadAsync(M(good), dir);
        Assert.True(File.Exists(path));
        await Assert.ThrowsAsync<UpdateException>(() => svc.DownloadAsync(M(new string('0', 64)), dir));
        Assert.Empty(Directory.GetFiles(dir));   // the failed download was deleted (same target name as the earlier good one)
        Directory.Delete(dir, true);
    }
}
