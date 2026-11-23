using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace FeatherView.Core.Video;

public sealed record SubtitleCue(double Start, double End, string Text);

/// <summary>A loaded subtitle file: timed lines of text, looked up by playback position.</summary>
public sealed class SubtitleTrack
{
    private readonly List<SubtitleCue> _cues;
    public SubtitleTrack(string name, List<SubtitleCue> cues) { Name = name; _cues = cues.OrderBy(c => c.Start).ToList(); }
    public string Name { get; }
    public int Count => _cues.Count;

    /// <summary>The text to show at <paramref name="seconds"/>, shifted by <paramref name="delay"/> (positive = subtitles later). Empty when none.</summary>
    public string TextAt(double seconds, double delay = 0)
    {
        double t = seconds - delay;
        // cues are sorted by start; overlapping cues are rare, so scan back a few entries from the binary-search position
        int lo = 0, hi = _cues.Count - 1, pos = -1;
        while (lo <= hi) { int mid = (lo + hi) / 2; if (_cues[mid].Start <= t) { pos = mid; lo = mid + 1; } else hi = mid - 1; }
        var parts = new List<string>();
        for (int i = pos; i >= 0 && i > pos - 6; i--)
            if (_cues[i].End > t) parts.Insert(0, _cues[i].Text);
        return string.Join("\n", parts);
    }
}

/// <summary>Reads SRT, WebVTT and ASS/SSA subtitle files (text only; positioning and fancy styling are ignored).</summary>
public static class SubtitleParser
{
    public static readonly IReadOnlySet<string> Extensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { ".srt", ".vtt", ".ass", ".ssa" };

    private static readonly Regex Time = new(@"(?:(\d+):)?(\d{1,2}):(\d{2})[,.](\d{1,3})", RegexOptions.Compiled);
    private static readonly Regex Tags = new("<[^>]+>|\\{\\\\[^}]*\\}", RegexOptions.Compiled);

    public static List<SubtitleCue> Parse(string content, string extension)
    {
        content = content.Replace("\r\n", "\n").Replace('\r', '\n').TrimStart('﻿');
        return extension.ToLowerInvariant() switch
        {
            ".ass" or ".ssa" => ParseAss(content),
            _ => ParseSrtOrVtt(content),
        };
    }

    public static SubtitleTrack Load(string path)
    {
        var bytes = File.ReadAllBytes(path);
        string text;
        try { text = new UTF8Encoding(false, true).GetString(bytes); }                // UTF-8 (also with BOM) when it is valid ...
        catch (DecoderFallbackException) { text = Encoding.Latin1.GetString(bytes); }   // ... otherwise old Western files
        return new SubtitleTrack(Path.GetFileName(path), Parse(text, Path.GetExtension(path)));
    }

    /// <summary>Subtitle files next to the video: "movie.srt", "movie.en.srt", "movie.eng.vtt" ...</summary>
    public static List<string> FindFor(string videoPath)
    {
        var result = new List<string>();
        try
        {
            var dir = Path.GetDirectoryName(videoPath); var baseName = Path.GetFileNameWithoutExtension(videoPath);
            if (dir == null) return result;
            foreach (var f in Directory.EnumerateFiles(dir))
            {
                var name = Path.GetFileName(f);
                if (!Extensions.Contains(Path.GetExtension(f))) continue;
                if (name.StartsWith(baseName + ".", StringComparison.OrdinalIgnoreCase) || string.Equals(Path.GetFileNameWithoutExtension(f), baseName, StringComparison.OrdinalIgnoreCase))
                    result.Add(f);
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        result.Sort(StringComparer.OrdinalIgnoreCase);
        return result;
    }

    private static double? ParseTime(string s)
    {
        var m = Time.Match(s);
        if (!m.Success) return null;
        double h = m.Groups[1].Success ? double.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture) : 0;
        double min = double.Parse(m.Groups[2].Value, CultureInfo.InvariantCulture), sec = double.Parse(m.Groups[3].Value, CultureInfo.InvariantCulture);
        string frac = m.Groups[4].Value.PadRight(3, '0');
        return h * 3600 + min * 60 + sec + double.Parse(frac, CultureInfo.InvariantCulture) / 1000;
    }

    private static string Clean(string text) =>
        System.Net.WebUtility.HtmlDecode(Tags.Replace(text, "")).Replace("\\N", "\n").Replace("\\n", "\n").Trim();

    private static List<SubtitleCue> ParseSrtOrVtt(string content)
    {
        var cues = new List<SubtitleCue>();
        foreach (var block in Regex.Split(content, "\n{2,}"))
        {
            var lines = block.Split('\n');
            int t = Array.FindIndex(lines, l => l.Contains("-->"));
            if (t < 0) continue;                                                     // WEBVTT header, NOTE, STYLE, index-only blocks
            var parts = lines[t].Split(new[] { "-->" }, StringSplitOptions.None);
            if (parts.Length < 2) continue;
            var start = ParseTime(parts[0]); var end = ParseTime(parts[1]);
            if (start is null || end is null) continue;
            var text = Clean(string.Join("\n", lines.Skip(t + 1)));
            if (text.Length > 0) cues.Add(new SubtitleCue(start.Value, end.Value, text));
        }
        return cues;
    }

    private static List<SubtitleCue> ParseAss(string content)
    {
        var cues = new List<SubtitleCue>();
        int iStart = 1, iEnd = 2, iText = 9, fields = 10;
        bool inEvents = false;
        foreach (var raw in content.Split('\n'))
        {
            var line = raw.Trim();
            if (line.StartsWith("[", StringComparison.Ordinal)) { inEvents = line.Equals("[Events]", StringComparison.OrdinalIgnoreCase); continue; }
            if (!inEvents) continue;
            if (line.StartsWith("Format:", StringComparison.OrdinalIgnoreCase))
            {
                var names = line.Substring(7).Split(',').Select(n => n.Trim().ToLowerInvariant()).ToList();
                iStart = names.IndexOf("start"); iEnd = names.IndexOf("end"); iText = names.IndexOf("text"); fields = names.Count;
            }
            else if (line.StartsWith("Dialogue:", StringComparison.OrdinalIgnoreCase) && iStart >= 0 && iEnd >= 0 && iText >= 0)
            {
                var f = line.Substring(9).TrimStart().Split(new[] { ',' }, fields);   // the text field may itself contain commas
                if (f.Length <= iText) continue;
                var start = ParseTime(f[iStart].Replace('.', ',')); var end = ParseTime(f[iEnd].Replace('.', ','));
                if (start is null || end is null) continue;
                var text = Clean(f[iText]);
                if (text.Length > 0) cues.Add(new SubtitleCue(start.Value, end.Value, text));
            }
        }
        return cues;
    }
}
