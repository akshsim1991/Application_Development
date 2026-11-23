using System.IO.Compression;
using System.Text.Json.Serialization;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Updates;

namespace FeatherView.Core.Video;

public sealed class EngineAsset
{
    [JsonPropertyName("url")] public string Url { get; set; } = "";
    [JsonPropertyName("size")] public long? Size { get; set; }
    [JsonPropertyName("sha256")] public string Sha256 { get; set; } = "";
    /// <summary>Name of the libmpv DLL inside the ZIP, for example "libmpv-2.dll".</summary>
    [JsonPropertyName("file")] public string File { get; set; } = "libmpv-2.dll";
}

public sealed class EngineManifest
{
    [JsonPropertyName("version")] public string Version { get; set; } = "";
    [JsonPropertyName("license")] public string? License { get; set; }
    [JsonPropertyName("notes")] public string? Notes { get; set; }
    [JsonPropertyName("win64")] public EngineAsset? Win64 { get; set; }
}

[JsonSourceGenerationOptions(PropertyNameCaseInsensitive = true)]
[JsonSerializable(typeof(EngineManifest))]
internal sealed partial class EngineJsonContext : JsonSerializerContext { }

public static class EngineLocator
{
    public static readonly string[] FileNames = { "libmpv-2.dll", "mpv-2.dll", "libmpv.dll" };

    public static string EngineDir(string localAppData) => System.IO.Path.Combine(localAppData, "FeatherView", "engine");

    /// <summary>Looks next to the app (portable use) and in the per-user engine folder.</summary>
    public static string? Find(string appDirectory, string localAppData)
    {
        foreach (var dir in new[] { System.IO.Path.Combine(appDirectory, "engine"), appDirectory, EngineDir(localAppData) })
            foreach (var n in FileNames)
            {
                var p = System.IO.Path.Combine(dir, n);
                if (File.Exists(p)) return p;
            }
        return null;
    }
}

/// <summary>
/// Optional "extended video support" download. Same safety model as app updates: HTTPS only, host allow-list,
/// size and SHA-256 verified, and only the named DLL and licence texts are extracted (never ZIP-supplied paths).
/// </summary>
public sealed class EngineDownloader
{
    public static readonly Uri DefaultManifest = new("https://raw.githubusercontent.com/akshsim1991/FeatherView-releases/main/engine.json");
    private readonly HttpClient _http;
    private readonly Uri _manifest;
    private readonly IReadOnlySet<string> _hosts;
    private readonly ILog _log;

    public EngineDownloader(HttpClient http, Uri? manifest = null, IReadOnlySet<string>? hosts = null, ILog? log = null)
    { _http = http; _manifest = manifest ?? DefaultManifest; _hosts = hosts ?? UpdateService.DefaultHosts; _log = log ?? NullLog.Instance; }

    public async Task<EngineManifest> GetManifestAsync(CancellationToken ct = default)
    {
        Ensure(_manifest);
        try
        {
            using var resp = await _http.GetAsync(_manifest, ct).ConfigureAwait(false);
            resp.EnsureSuccessStatusCode();
            var m = System.Text.Json.JsonSerializer.Deserialize(await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false), EngineJsonContext.Default.EngineManifest);
            if (m?.Win64 == null || string.IsNullOrWhiteSpace(m.Win64.Url)) throw new UpdateException("No extended video engine is published yet.");
            return m;
        }
        catch (UpdateException) { throw; }
        catch (OperationCanceledException) { throw; }
        catch (Exception ex) { _log.Warn($"Engine manifest failed ({ex.GetType().Name})."); throw new UpdateException("The extended video engine could not be found online."); }
    }

    /// <summary>Downloads, verifies and unpacks the engine; returns the DLL path.</summary>
    public async Task<string> InstallAsync(EngineManifest m, string destinationDir, IProgress<double>? progress = null, CancellationToken ct = default)
    {
        var a = m.Win64 ?? throw new UpdateException("No Windows engine is listed.");
        if (!Uri.TryCreate(a.Url, UriKind.Absolute, out var uri)) throw new UpdateException("The engine address is invalid.");
        if (string.IsNullOrWhiteSpace(a.Sha256)) throw new UpdateException("The engine download has no fingerprint, so it cannot be verified.");
        Ensure(uri);
        var tmp = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "fv-engine-" + Guid.NewGuid().ToString("N") + ".zip");
        try
        {
            using (var resp = await _http.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead, ct).ConfigureAwait(false))
            {
                resp.EnsureSuccessStatusCode();
                Ensure(resp.RequestMessage?.RequestUri ?? uri);
                long total = resp.Content.Headers.ContentLength ?? a.Size ?? -1;
                await using var src = await resp.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
                await using var dst = File.Create(tmp);
                var buf = new byte[81920]; long done = 0; int n;
                while ((n = await src.ReadAsync(buf, ct).ConfigureAwait(false)) > 0)
                {
                    await dst.WriteAsync(buf.AsMemory(0, n), ct).ConfigureAwait(false);
                    done += n; if (total > 0) progress?.Report((double)done / total);
                }
            }
            UpdateService.Verify(tmp, new UpdateAsset { Url = a.Url, Size = a.Size, Sha256 = a.Sha256 });
            return Extract(tmp, destinationDir, a.File);
        }
        finally { try { if (File.Exists(tmp)) File.Delete(tmp); } catch { } }
    }

    public static string Extract(string zipPath, string destinationDir, string dllName)
    {
        Directory.CreateDirectory(destinationDir);
        dllName = System.IO.Path.GetFileName(dllName);
        string? result = null;
        using var zip = ZipFile.OpenRead(zipPath);
        foreach (var e in zip.Entries)
        {
            var name = System.IO.Path.GetFileName(e.FullName);     // flatten: entry paths are never trusted
            if (string.IsNullOrEmpty(name)) continue;
            bool isDll = name.Equals(dllName, StringComparison.OrdinalIgnoreCase);
            bool isLicense = name.StartsWith("LICENSE", StringComparison.OrdinalIgnoreCase) || name.StartsWith("COPYING", StringComparison.OrdinalIgnoreCase) || name.StartsWith("README", StringComparison.OrdinalIgnoreCase);
            if (!isDll && !isLicense) continue;
            var target = System.IO.Path.Combine(destinationDir, name);
            e.ExtractToFile(target, overwrite: true);
            if (isDll) result = target;
        }
        return result ?? throw new UpdateException("The engine package does not contain the expected library.");
    }

    private void Ensure(Uri u)
    {
        if (u.Scheme != Uri.UriSchemeHttps) throw new UpdateException("Downloads must use a secure connection.");
        if (!_hosts.Contains(u.Host)) throw new UpdateException("The download address is not on the trusted list.");
    }
}
