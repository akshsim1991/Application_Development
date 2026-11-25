using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.Json;

namespace FeatherView.Core.Video;

/// <summary>How the files are joined: Auto = lossless when the files match, otherwise re-encode.</summary>
public enum MergeMode { Auto, Copy, Reencode }

public sealed record MediaProbe(string Path, double Duration, string? VideoCodec, int Width, int Height, double Fps, string? PixFmt,
                                string? AudioCodec, int SampleRate, int Channels)
{
    public bool HasVideo => VideoCodec != null;
    public bool HasAudio => AudioCodec != null;
}

public sealed record MergeResult(bool Success, string Output, string? Error, bool Reencoded, bool Cancelled = false);

public sealed class MergeException : Exception { public MergeException(string m) : base(m) { } }

/// <summary>
/// Joins several video files into one with ffmpeg (a separate program the user downloads on request).
/// Inputs are only read; the output is always a NEW file (never overwrites anything).
/// </summary>
public sealed class VideoMerger
{
    private readonly string _ffmpeg, _ffprobe;

    public VideoMerger(string ffmpegPath, string? ffprobePath = null)
    {
        _ffmpeg = ffmpegPath;
        _ffprobe = ffprobePath ?? FfmpegLocator.SiblingProbe(ffmpegPath);
    }

    // ---------- pure helpers (unit tested) ----------

    /// <summary>The list file for ffmpeg's concat demuxer. Paths use forward slashes and single quotes are escaped.</summary>
    public static string BuildConcatList(IEnumerable<string> inputs)
    {
        var sb = new StringBuilder();
        foreach (var p in inputs)
            sb.Append("file '").Append(p.Replace('\\', '/').Replace("'", "'\\''")).Append("'\n");
        return sb.ToString();
    }

    /// <summary>A file name in <paramref name="folder"/> that does not exist yet: "name.mp4", "name (2).mp4", ...</summary>
    public static string UniqueOutput(string folder, string baseName, string ext = ".mp4")
    {
        foreach (var c in System.IO.Path.GetInvalidFileNameChars()) baseName = baseName.Replace(c, '_');
        baseName = baseName.Trim().TrimEnd('.');
        if (baseName.Length == 0) baseName = "Merged video";
        if (!ext.StartsWith('.')) ext = "." + ext;
        var path = System.IO.Path.Combine(folder, baseName + ext);
        for (int i = 2; File.Exists(path); i++) path = System.IO.Path.Combine(folder, $"{baseName} ({i}){ext}");
        return path;
    }

    /// <summary>True when the files can be joined without re-encoding: same codecs, size, frame rate and audio format.</summary>
    public static bool CanCopy(IReadOnlyList<MediaProbe> p)
    {
        if (p.Count < 2) return true;
        var a = p[0];
        if (!a.HasVideo) return false;
        foreach (var b in p.Skip(1))
        {
            if (b.VideoCodec != a.VideoCodec || b.Width != a.Width || b.Height != a.Height || b.PixFmt != a.PixFmt) return false;
            if (Math.Abs(b.Fps - a.Fps) > 0.01) return false;
            if (b.HasAudio != a.HasAudio) return false;
            if (a.HasAudio && (b.AudioCodec != a.AudioCodec || b.SampleRate != a.SampleRate || b.Channels != a.Channels)) return false;
        }
        return true;
    }

    public static string[] CopyArgs(string listFile, string output) =>
        new[] { "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1", "-n", "-f", "concat", "-safe", "0", "-i", listFile,
                "-map", "0:v:0", "-map", "0:a:0?", "-c", "copy", "-movflags", "+faststart", output };

    public static string BuildFilterGraph(IReadOnlyList<MediaProbe> p, int width, int height, double fps)
    {
        var f = fps.ToString("0.###", CultureInfo.InvariantCulture);
        var sb = new StringBuilder();
        for (int i = 0; i < p.Count; i++)
        {
            sb.Append($"[{i}:v:0]scale={width}:{height}:force_original_aspect_ratio=decrease,pad={width}:{height}:(ow-iw)/2:(oh-ih)/2,setsar=1,fps={f},format=yuv420p[v{i}];");
            if (p[i].HasAudio) sb.Append($"[{i}:a:0]aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo[a{i}];");
            else sb.Append($"anullsrc=r=48000:cl=stereo:d={p[i].Duration.ToString("0.###", CultureInfo.InvariantCulture)}[a{i}];");
        }
        for (int i = 0; i < p.Count; i++) sb.Append($"[v{i}][a{i}]");
        sb.Append($"concat=n={p.Count}:v=1:a=1[v][a]");
        return sb.ToString();
    }

    public static string[] ReencodeArgs(IReadOnlyList<MediaProbe> p, string output, string videoEncoder)
    {
        var first = p[0];
        int w = Math.Max(2, first.Width / 2 * 2), h = Math.Max(2, first.Height / 2 * 2);
        double fps = first.Fps is > 1 and <= 120 ? first.Fps : 30;
        var args = new List<string> { "-hide_banner", "-loglevel", "error", "-nostats", "-progress", "pipe:1", "-n" };
        foreach (var x in p) { args.Add("-i"); args.Add(x.Path); }
        args.AddRange(new[] { "-filter_complex", BuildFilterGraph(p, w, h, fps), "-map", "[v]", "-map", "[a]" });
        switch (videoEncoder)
        {
            case "libx264": args.AddRange(new[] { "-c:v", "libx264", "-preset", "veryfast", "-crf", "20", "-pix_fmt", "yuv420p" }); break;
            case "h264_mf": args.AddRange(new[] { "-c:v", "h264_mf", "-b:v", "8M", "-pix_fmt", "yuv420p" }); break;
            default: args.AddRange(new[] { "-c:v", "mpeg4", "-q:v", "3", "-pix_fmt", "yuv420p" }); break;
        }
        args.AddRange(new[] { "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", output });
        return args.ToArray();
    }

    // ---------- running ffmpeg ----------

    public async Task<MediaProbe> ProbeAsync(string path, CancellationToken ct = default)
    {
        var (code, output, err) = await RunAsync(_ffprobe, new[] { "-v", "error", "-print_format", "json", "-show_format", "-show_streams", path }, null, ct).ConfigureAwait(false);
        if (code != 0) throw new MergeException($"\"{System.IO.Path.GetFileName(path)}\" could not be read: {Tail(err)}");
        using var doc = JsonDocument.Parse(output);
        string? vc = null, ac = null, pix = null; int w = 0, h = 0, sr = 0, ch = 0; double fps = 0, dur = 0;
        if (doc.RootElement.TryGetProperty("format", out var fmt) && fmt.TryGetProperty("duration", out var d)) double.TryParse(d.GetString(), NumberStyles.Float, CultureInfo.InvariantCulture, out dur);
        if (doc.RootElement.TryGetProperty("streams", out var streams))
            foreach (var s in streams.EnumerateArray())
            {
                var type = s.TryGetProperty("codec_type", out var t) ? t.GetString() : null;
                if (type == "video" && vc == null && !(s.TryGetProperty("disposition", out var dp) && dp.TryGetProperty("attached_pic", out var ap) && ap.GetInt32() == 1))
                {
                    vc = s.GetProperty("codec_name").GetString();
                    w = s.TryGetProperty("width", out var ww) ? ww.GetInt32() : 0; h = s.TryGetProperty("height", out var hh) ? hh.GetInt32() : 0;
                    pix = s.TryGetProperty("pix_fmt", out var pf) ? pf.GetString() : null;
                    fps = ParseRate(s.TryGetProperty("avg_frame_rate", out var afr) ? afr.GetString() : null);
                    if (fps <= 0) fps = ParseRate(s.TryGetProperty("r_frame_rate", out var rfr) ? rfr.GetString() : null);
                }
                else if (type == "audio" && ac == null)
                {
                    ac = s.GetProperty("codec_name").GetString();
                    sr = s.TryGetProperty("sample_rate", out var sx) && int.TryParse(sx.GetString(), out var srv) ? srv : 0;
                    ch = s.TryGetProperty("channels", out var cx) ? cx.GetInt32() : 0;
                }
                if (dur <= 0 && s.TryGetProperty("duration", out var sd)) double.TryParse(sd.GetString(), NumberStyles.Float, CultureInfo.InvariantCulture, out dur);
            }
        if (vc == null) throw new MergeException($"\"{System.IO.Path.GetFileName(path)}\" has no video picture.");
        return new MediaProbe(path, dur, vc, w, h, fps, pix, ac, sr, ch);
    }

    private static double ParseRate(string? r)
    {
        if (string.IsNullOrEmpty(r)) return 0;
        var parts = r.Split('/');
        if (parts.Length == 2 && double.TryParse(parts[0], NumberStyles.Float, CultureInfo.InvariantCulture, out var n) && double.TryParse(parts[1], NumberStyles.Float, CultureInfo.InvariantCulture, out var d) && d != 0) return n / d;
        return double.TryParse(r, NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : 0;
    }

    public async Task<string> ChooseEncoderAsync(CancellationToken ct = default)
    {
        var (code, output, _) = await RunAsync(_ffmpeg, new[] { "-hide_banner", "-encoders" }, null, ct).ConfigureAwait(false);
        if (code == 0)
        {
            if (output.Contains(" libx264 ")) return "libx264";
            if (output.Contains(" h264_mf ")) return "h264_mf";
        }
        return "mpeg4";
    }

    /// <summary>Joins <paramref name="inputs"/> (in the given order) into <paramref name="output"/>. Reports progress 0..1.</summary>
    public async Task<MergeResult> MergeAsync(IReadOnlyList<string> inputs, string output, MergeMode mode = MergeMode.Auto, IProgress<double>? progress = null, CancellationToken ct = default)
    {
        if (inputs.Count < 2) return new MergeResult(false, output, "Choose at least two videos.", false);
        if (File.Exists(output)) return new MergeResult(false, output, "The output file already exists.", false);
        if (inputs.Any(i => string.Equals(System.IO.Path.GetFullPath(i), System.IO.Path.GetFullPath(output), StringComparison.OrdinalIgnoreCase)))
            return new MergeResult(false, output, "The output file must be a new file.", false);

        var probes = new List<MediaProbe>();
        try { foreach (var i in inputs) probes.Add(await ProbeAsync(i, ct).ConfigureAwait(false)); }
        catch (MergeException ex) { return new MergeResult(false, output, ex.Message, false); }
        double total = Math.Max(0.1, probes.Sum(p => p.Duration));

        bool copyOk = CanCopy(probes);
        if (mode == MergeMode.Copy && !copyOk) return new MergeResult(false, output, "These videos differ (format, size or frame rate), so they cannot be joined without re-encoding.", false);

        string? list = null;
        try
        {
            if (mode != MergeMode.Reencode && copyOk)
            {
                list = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "fv-merge-" + Guid.NewGuid().ToString("N") + ".txt");
                await File.WriteAllTextAsync(list, BuildConcatList(inputs), new UTF8Encoding(false), ct).ConfigureAwait(false);
                var r = await RunFfmpegAsync(CopyArgs(list, output), total, progress, ct).ConfigureAwait(false);
                if (r.Ok) return new MergeResult(true, output, null, false);
                TryDelete(output);
                if (r.Cancelled) return new MergeResult(false, output, null, false, true);
                if (mode == MergeMode.Copy) return new MergeResult(false, output, r.Error, false);
                // Auto: the container could not take the streams as they are -> re-encode below
            }
            var enc = await ChooseEncoderAsync(ct).ConfigureAwait(false);
            var re = await RunFfmpegAsync(ReencodeArgs(probes, output, enc), total, progress, ct).ConfigureAwait(false);
            if (re.Ok) return new MergeResult(true, output, null, true);
            TryDelete(output);
            return re.Cancelled ? new MergeResult(false, output, null, true, true) : new MergeResult(false, output, re.Error, true);
        }
        finally { if (list != null) TryDelete(list); }
    }

    private async Task<(bool Ok, bool Cancelled, string? Error)> RunFfmpegAsync(string[] args, double totalSeconds, IProgress<double>? progress, CancellationToken ct)
    {
        try
        {
            var (code, _, err) = await RunAsync(_ffmpeg, args, line =>
            {
                // -progress prints "out_time_us=123456" (microseconds) about twice a second
                if (line.StartsWith("out_time_us=", StringComparison.Ordinal) && long.TryParse(line.AsSpan(12), out var us) && us > 0)
                    progress?.Report(Math.Clamp(us / 1_000_000.0 / totalSeconds, 0, 0.99));
            }, ct).ConfigureAwait(false);
            if (code == 0) { progress?.Report(1); return (true, false, null); }
            return (false, false, Tail(err));
        }
        catch (OperationCanceledException) { return (false, true, null); }
    }

    private static async Task<(int Code, string Output, string Error)> RunAsync(string exe, string[] args, Action<string>? onLine, CancellationToken ct)
    {
        var psi = new ProcessStartInfo(exe) { RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true, StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8 };
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = new Process { StartInfo = psi };
        var so = new StringBuilder(); var se = new StringBuilder();
        p.OutputDataReceived += (_, e) => { if (e.Data != null) { onLine?.Invoke(e.Data); if (onLine == null) so.AppendLine(e.Data); } };
        p.ErrorDataReceived += (_, e) => { if (e.Data != null) se.AppendLine(e.Data); };
        try { p.Start(); } catch (Exception ex) when (ex is System.ComponentModel.Win32Exception or InvalidOperationException) { throw new MergeException("The video tool could not be started: " + ex.Message); }
        p.BeginOutputReadLine(); p.BeginErrorReadLine();
        using var reg = ct.Register(() => { try { if (!p.HasExited) p.Kill(entireProcessTree: true); } catch { } });
        await p.WaitForExitAsync(CancellationToken.None).ConfigureAwait(false);
        ct.ThrowIfCancellationRequested();
        return (p.ExitCode, so.ToString(), se.ToString());
    }

    private static string Tail(string s) { s = s.Trim(); var lines = s.Split('\n'); return string.Join(" ", lines.TakeLast(3)).Trim(); }
    private static void TryDelete(string p) { try { if (File.Exists(p)) File.Delete(p); } catch { } }
}

/// <summary>Finds ffmpeg: next to the app, in the per-user tools folder, or on the PATH.</summary>
public static class FfmpegLocator
{
    public static string ToolsDir(string localAppData) => System.IO.Path.Combine(localAppData, "FeatherView", "tools");

    public static string SiblingProbe(string ffmpegPath)
    {
        var dir = System.IO.Path.GetDirectoryName(ffmpegPath) ?? "";
        var exe = OperatingSystem.IsWindows() ? "ffprobe.exe" : "ffprobe";
        var p = System.IO.Path.Combine(dir, exe);
        return File.Exists(p) ? p : "ffprobe";
    }

    public static string? Find(string appDirectory, string localAppData)
    {
        var exe = OperatingSystem.IsWindows() ? "ffmpeg.exe" : "ffmpeg";
        foreach (var dir in new[] { System.IO.Path.Combine(appDirectory, "tools"), ToolsDir(localAppData) })
        {
            var p = System.IO.Path.Combine(dir, exe);
            if (File.Exists(p)) return p;
        }
        foreach (var dir in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(System.IO.Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            try { var p = System.IO.Path.Combine(dir.Trim('"'), exe); if (File.Exists(p)) return p; } catch (ArgumentException) { }
        }
        return null;
    }
}
