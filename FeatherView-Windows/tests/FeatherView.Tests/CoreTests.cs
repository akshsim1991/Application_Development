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

public class FolderNavigationTests
{
    private static string MakeTree()
    {
        var root = Path.Combine(Path.GetTempPath(), "fv-tree-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(root, "Trips", "Italy")); Directory.CreateDirectory(Path.Combine(root, "Family")); Directory.CreateDirectory(Path.Combine(root, ".hidden"));
        File.SetAttributes(Path.Combine(root, ".hidden"), FileAttributes.Hidden | FileAttributes.Directory);
        File.WriteAllText(Path.Combine(root, "top.jpg"), "x"); File.WriteAllText(Path.Combine(root, "Trips", "a.png"), "x");
        File.WriteAllText(Path.Combine(root, "Trips", "Italy", "b.jpg"), "x"); File.WriteAllText(Path.Combine(root, "Family", "c.mp4"), "x");
        return root;
    }

    [Fact] public void ListsSubfoldersWithoutHiddenOnes()
    {
        var root = MakeTree();
        Assert.Equal(new[] { "Family", "Trips" }, MediaScanner.ListFolders(root));
        Assert.Empty(MediaScanner.ListFolders(Path.Combine(root, "nope")));
        Directory.Delete(root, true);
    }

    [Fact] public void FlatScanSeesOnlyTheFolderAndRecursiveScanSeesTheTree()
    {
        var root = MakeTree();
        Assert.Equal(new[] { "top.jpg" }, MediaScanner.Scan(root).Select(f => f.Name));
        var all = MediaScanner.ScanDetailed(root, recurse: true);
        Assert.Equal(new[] { "a.png", "b.jpg", "c.mp4", "top.jpg" }, all.Files.Select(f => f.Name).OrderBy(n => n, StringComparer.Ordinal));
        Assert.False(all.Truncated);
        Directory.Delete(root, true);
    }

    [Fact] public void RecursiveScanStopsAtTheLimit()
    {
        var root = MakeTree();
        var r = MediaScanner.ScanDetailed(root, recurse: true, maxFiles: 2);
        Assert.Equal(2, r.Files.Count); Assert.True(r.Truncated);
        Directory.Delete(root, true);
    }
}

public class InstallerChoiceTests
{
    private static UpdateManifest M(bool small) => new()
    {
        Version = "9", Win64 = new() { Url = "https://github.com/x/full.exe", Sha256 = "a", Size = 46_000_000 },
        Win64Small = small ? new() { Url = "https://github.com/x/small.exe", Sha256 = "b", Size = 1_500_000 } : null,
    };

    [Fact] public void SmallInstallsGetTheSmallDownloadWhenOneExists()
    {
        Assert.Equal(1_500_000, M(true).PickInstaller(frameworkDependentInstall: true)!.Size);
        Assert.Equal(46_000_000, M(true).PickInstaller(frameworkDependentInstall: false)!.Size);   // a self-contained copy must stay self-contained
        Assert.Equal(46_000_000, M(false).PickInstaller(frameworkDependentInstall: true)!.Size);   // no small file published: fall back to the full installer
    }

    [Fact] public void DetectsFlavourFromRuntimeConfig()
    {
        Assert.True(InstallFlavor.IsFrameworkDependent("{\"runtimeOptions\":{\"tfm\":\"net10.0\",\"framework\":{\"name\":\"Microsoft.WindowsDesktop.App\",\"version\":\"10.0.0\"}}}"));
        Assert.False(InstallFlavor.IsFrameworkDependent("{\"runtimeOptions\":{\"includedFrameworks\":[{\"name\":\"Microsoft.NETCore.App\"}]}}"));
        Assert.False(InstallFlavor.IsFrameworkDependent(null)); Assert.False(InstallFlavor.IsFrameworkDependent(""));
    }

    [Fact] public void ManifestWithSmallAssetParses()
    {
        var json = "{\"version\":\"2\",\"win64\":{\"url\":\"https://github.com/a\",\"sha256\":\"x\"},\"win64Small\":{\"url\":\"https://github.com/b\",\"sha256\":\"y\",\"size\":10}}";
        var h = new HttpClientHandlerStub(json);
        var m = new UpdateService(new HttpClient(h)).CheckAsync("1").GetAwaiter().GetResult();
        Assert.Equal("https://github.com/b", m!.Win64Small!.Url);
    }

    private sealed class HttpClientHandlerStub : HttpMessageHandler
    {
        private readonly string _json; public HttpClientHandlerStub(string json) { _json = json; }
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage r, CancellationToken ct) =>
            Task.FromResult(new HttpResponseMessage(System.Net.HttpStatusCode.OK) { Content = new StringContent(_json), RequestMessage = r });
    }
}

public class ScanControlTests
{
    [Fact] public void CancelledScanThrowsAndProgressIsReported()
    {
        var root = Path.Combine(Path.GetTempPath(), "fv-cancel-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(root);
        for (int i = 0; i < 700; i++) File.WriteAllText(Path.Combine(root, $"p{i:D4}.jpg"), "x");
        var reported = new List<int>();
        var r = MediaScanner.ScanDetailed(root, recurse: true, progress: new Progress<int>(reported.Add) is var p ? new SyncProgress(reported) : null);
        Assert.Equal(700, r.Files.Count); Assert.Contains(700, reported);
        using var cts = new CancellationTokenSource(); cts.Cancel();
        Assert.Throws<OperationCanceledException>(() => MediaScanner.ScanDetailed(root, recurse: true, ct: cts.Token));
        Directory.Delete(root, true);
    }

    [Fact] public void PicturesStayGroupedByFolderWhenSortedByName()
    {
        var root = Path.Combine(Path.GetTempPath(), "fv-group-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(root, "B")); Directory.CreateDirectory(Path.Combine(root, "A"));
        File.WriteAllText(Path.Combine(root, "B", "a.jpg"), "x"); File.WriteAllText(Path.Combine(root, "A", "z.jpg"), "x"); File.WriteAllText(Path.Combine(root, "A", "b.jpg"), "x");
        var names = MediaScanner.ScanDetailed(root, recurse: true).Files.Select(f => Path.GetFileName(Path.GetDirectoryName(f.Path)) + "/" + f.Name);
        Assert.Equal(new[] { "A/b.jpg", "A/z.jpg", "B/a.jpg" }, names);
        Directory.Delete(root, true);
    }

    private sealed class SyncProgress : IProgress<int> { private readonly List<int> _l; public SyncProgress(List<int> l) { _l = l; } public void Report(int v) => _l.Add(v); }
}
