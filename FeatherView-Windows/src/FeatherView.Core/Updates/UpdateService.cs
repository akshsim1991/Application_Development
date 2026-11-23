using System.Security.Cryptography;
using System.Text.Json.Serialization;
using FeatherView.Core.Infrastructure;

namespace FeatherView.Core.Updates;

public sealed class UpdateAsset
{
    [JsonPropertyName("url")] public string Url { get; set; } = "";
    [JsonPropertyName("size")] public long? Size { get; set; }
    [JsonPropertyName("sha256")] public string Sha256 { get; set; } = "";
}

/// <summary>latest.json published in the releases repository (same scheme as FeatherPDF / FileGlade).</summary>
public sealed class UpdateManifest
{
    [JsonPropertyName("version")] public string Version { get; set; } = "";
    [JsonPropertyName("date")] public string? Date { get; set; }
    [JsonPropertyName("notes")] public string? Notes { get; set; }
    [JsonPropertyName("win64")] public UpdateAsset? Win64 { get; set; }
}

[JsonSourceGenerationOptions(PropertyNameCaseInsensitive = true)]
[JsonSerializable(typeof(UpdateManifest))]
internal sealed partial class UpdateJsonContext : JsonSerializerContext { }

public static class VersionUtil
{
    /// <summary>True when <paramref name="a"/> is a higher dotted-numeric version than <paramref name="b"/>.</summary>
    public static bool IsNewer(string a, string b)
    {
        static int[] Parts(string v) => v.Trim().TrimStart('v', 'V').Split('.')
            .Select(p => int.TryParse(new string(p.TakeWhile(char.IsDigit).ToArray()), out var n) ? n : 0).ToArray();
        var x = Parts(a); var y = Parts(b);
        for (int i = 0; i < Math.Max(x.Length, y.Length); i++)
        {
            int l = i < x.Length ? x[i] : 0, r = i < y.Length ? y[i] : 0;
            if (l != r) return l > r;
        }
        return false;
    }
}

public sealed class UpdateException : Exception { public UpdateException(string m) : base(m) { } }

public sealed class UpdateService
{
    /// <summary>latest.json attached to the newest (non pre-release) GitHub Release of this repository.</summary>
    public static readonly Uri DefaultManifest =
        new("https://github.com/akshsim1991/Application_Development/releases/latest/download/latest.json");
    public static readonly IReadOnlySet<string> DefaultHosts = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
    { "raw.githubusercontent.com", "github.com", "objects.githubusercontent.com", "release-assets.githubusercontent.com" };

    private readonly HttpClient _http;
    private readonly Uri _manifest;
    private readonly IReadOnlySet<string> _hosts;
    private readonly ILog _log;

    public UpdateService(HttpClient http, Uri? manifest = null, IReadOnlySet<string>? allowedHosts = null, ILog? log = null)
    {
        _http = http; _manifest = manifest ?? DefaultManifest; _hosts = allowedHosts ?? DefaultHosts; _log = log ?? NullLog.Instance;
    }

    /// <summary>Returns the manifest when a newer version exists, otherwise null. Throws <see cref="UpdateException"/> on failure.</summary>
    public async Task<UpdateManifest?> CheckAsync(string currentVersion, CancellationToken ct = default)
    {
        EnsureAllowed(_manifest);
        try
        {
            using var req = new HttpRequestMessage(HttpMethod.Get, _manifest);
            req.Headers.CacheControl = new() { NoCache = true };
            using var resp = await _http.SendAsync(req, ct).ConfigureAwait(false);
            resp.EnsureSuccessStatusCode();
            EnsureAllowed(resp.RequestMessage?.RequestUri ?? _manifest);
            var json = await resp.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
            var m = System.Text.Json.JsonSerializer.Deserialize(json, UpdateJsonContext.Default.UpdateManifest)
                    ?? throw new UpdateException("The update information is empty.");
            if (string.IsNullOrWhiteSpace(m.Version)) throw new UpdateException("The update information is incomplete.");
            return VersionUtil.IsNewer(m.Version, currentVersion) ? m : null;
        }
        catch (UpdateException) { throw; }
        catch (OperationCanceledException) { throw; }
        catch (Exception ex) { _log.Warn($"Update check failed ({ex.GetType().Name})."); throw new UpdateException("The update server could not be reached."); }
    }

    /// <summary>Downloads the installer to a temp file and verifies size and SHA-256. Deletes it if verification fails.</summary>
    public async Task<string> DownloadAsync(UpdateManifest m, string directory, IProgress<double>? progress = null, CancellationToken ct = default)
    {
        var asset = m.Win64 ?? throw new UpdateException("No Windows download is listed for this version.");
        if (!Uri.TryCreate(asset.Url, UriKind.Absolute, out var uri)) throw new UpdateException("The download address is invalid.");
        if (string.IsNullOrWhiteSpace(asset.Sha256)) throw new UpdateException("The download has no fingerprint, so it cannot be verified.");
        EnsureAllowed(uri);
        Directory.CreateDirectory(directory);
        var dest = Path.Combine(directory, $"FeatherView-Setup-{SafeName(m.Version)}.exe");
        try
        {
            using var resp = await _http.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead, ct).ConfigureAwait(false);
            resp.EnsureSuccessStatusCode();
            EnsureAllowed(resp.RequestMessage?.RequestUri ?? uri);
            long total = resp.Content.Headers.ContentLength ?? asset.Size ?? -1;
            await using (var src = await resp.Content.ReadAsStreamAsync(ct).ConfigureAwait(false))
            await using (var dst = File.Create(dest))
            {
                var buf = new byte[81920]; long done = 0; int n;
                while ((n = await src.ReadAsync(buf, ct).ConfigureAwait(false)) > 0)
                {
                    await dst.WriteAsync(buf.AsMemory(0, n), ct).ConfigureAwait(false);
                    done += n; if (total > 0) progress?.Report((double)done / total);
                }
            }
            Verify(dest, asset);
            return dest;
        }
        catch { TryDelete(dest); throw; }
    }

    public static void Verify(string file, UpdateAsset asset)
    {
        var fi = new FileInfo(file);
        if (asset.Size is long sz && fi.Length != sz) throw new UpdateException("The downloaded file has the wrong size.");
        using var s = File.OpenRead(file);
        var hash = Convert.ToHexString(SHA256.HashData(s));
        if (!hash.Equals(asset.Sha256.Trim(), StringComparison.OrdinalIgnoreCase))
            throw new UpdateException("The downloaded file failed its integrity check and was discarded.");
    }

    private void EnsureAllowed(Uri uri)
    {
        if (uri.Scheme != Uri.UriSchemeHttps) throw new UpdateException("Updates must use a secure connection.");
        if (!_hosts.Contains(uri.Host)) throw new UpdateException("The update address is not on the trusted list.");
    }

    private static string SafeName(string v) => new(v.Where(c => char.IsLetterOrDigit(c) || c is '.' or '-').ToArray());
    private static void TryDelete(string p) { try { if (File.Exists(p)) File.Delete(p); } catch { } }
}
