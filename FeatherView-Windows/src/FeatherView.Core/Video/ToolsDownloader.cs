using System.IO.Compression;
using System.Text.Json.Serialization;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Updates;

namespace FeatherView.Core.Video;

public sealed class ToolsAsset
{
    [JsonPropertyName("url")] public string Url { get; set; } = "";
    [JsonPropertyName("size")] public long? Size { get; set; }
    [JsonPropertyName("sha256")] public string Sha256 { get; set; } = "";
}

public sealed class ToolsManifest
{
    [JsonPropertyName("version")] public string Version { get; set; } = "";
    [JsonPropertyName("license")] public string? License { get; set; }
    [JsonPropertyName("notes")] public string? Notes { get; set; }
    [JsonPropertyName("win64")] public ToolsAsset? Win64 { get; set; }
}

[JsonSourceGenerationOptions(PropertyNameCaseInsensitive = true)]
[JsonSerializable(typeof(ToolsManifest))]
internal sealed partial class ToolsJsonContext : JsonSerializerContext { }

/// <summary>
/// Optional download of ffmpeg (used to merge videos). Same safety model as updates and the video engine: HTTPS only, host allow-list,
/// size and SHA-256 verified, and only ffmpeg.exe, ffprobe.exe and licence texts are unpacked (ZIP entry paths are never trusted).
/// </summary>
public sealed class ToolsDownloader
{
    public static readonly Uri DefaultManifest = new("https://github.com/akshsim1991/Application_Development/releases/download/featherview-tools/tools.json");
    private static readonly string[] Wanted = { "ffmpeg.exe", "ffprobe.exe" };
    private readonly HttpClient _http;
    private readonly Uri _manifest;
    private readonly IReadOnlySet<string> _hosts;
    private readonly ILog _log;

    public ToolsDownloader(HttpClient http, Uri? manifest = null, IReadOnlySet<string>? hosts = null, ILog? log = null)
    { _http = http; _manifest = manifest ?? DefaultManifest; _hosts = hosts ?? UpdateService.DefaultHosts; _log = log ?? NullLog.Instance; }

    public async Task<ToolsManifest> GetManifestAsync(CancellationToken ct = default)
    {
        Ensure(_manifest);
        try
        {
            using var resp = await _http.GetAsync(_manifest, ct).ConfigureAwait(false);
            resp.EnsureSuccessStatusCode();
            var m = System.Text.Json.JsonSerializer.Deserialize(await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false), ToolsJsonContext.Default.ToolsManifest);
            if (m?.Win64 == null || string.IsNullOrWhiteSpace(m.Win64.Url)) throw new UpdateException("The video tool is not published yet.");
            return m;
        }
        catch (UpdateException) { throw; }
        catch (OperationCanceledException) { throw; }
        catch (Exception ex) { _log.Warn($"Tools manifest failed ({ex.GetType().Name})."); throw new UpdateException("The video tool could not be found online."); }
    }

    /// <summary>Downloads, verifies and unpacks ffmpeg; returns the path of ffmpeg.exe.</summary>
    public async Task<string> InstallAsync(ToolsManifest m, string destinationDir, IProgress<double>? progress = null, CancellationToken ct = default)
    {
        var a = m.Win64 ?? throw new UpdateException("No Windows download is listed.");
        if (!Uri.TryCreate(a.Url, UriKind.Absolute, out var uri)) throw new UpdateException("The download address is invalid.");
        if (string.IsNullOrWhiteSpace(a.Sha256)) throw new UpdateException("The download has no fingerprint, so it cannot be verified.");
        Ensure(uri);
        var tmp = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "fv-tools-" + Guid.NewGuid().ToString("N") + ".zip");
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
            return Extract(tmp, destinationDir);
        }
        finally { try { if (File.Exists(tmp)) File.Delete(tmp); } catch { } }
    }

    public static string Extract(string zipPath, string destinationDir)
    {
        Directory.CreateDirectory(destinationDir);
        string? result = null;
        using var zip = ZipFile.OpenRead(zipPath);
        foreach (var e in zip.Entries)
        {
            var name = System.IO.Path.GetFileName(e.FullName);     // flatten: entry paths are never trusted
            if (string.IsNullOrEmpty(name)) continue;
            bool wanted = Wanted.Any(w => name.Equals(w, StringComparison.OrdinalIgnoreCase));
            bool licence = name.StartsWith("LICENSE", StringComparison.OrdinalIgnoreCase) || name.StartsWith("COPYING", StringComparison.OrdinalIgnoreCase) || name.StartsWith("README", StringComparison.OrdinalIgnoreCase);
            if (!wanted && !licence) continue;
            var target = System.IO.Path.Combine(destinationDir, name);
            e.ExtractToFile(target, overwrite: true);
            if (name.Equals("ffmpeg.exe", StringComparison.OrdinalIgnoreCase)) result = target;
        }
        return result ?? throw new UpdateException("The download does not contain the expected program.");
    }

    private void Ensure(Uri u)
    {
        if (u.Scheme != Uri.UriSchemeHttps) throw new UpdateException("Downloads must use a secure connection.");
        if (!_hosts.Contains(u.Host)) throw new UpdateException("The download address is not on the trusted list.");
    }
}
