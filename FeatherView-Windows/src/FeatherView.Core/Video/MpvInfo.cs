using System.Globalization;

namespace FeatherView.Core.Video;

/// <summary>Builds the video information list shown in the details panel.</summary>
public static class MpvInfo
{
    public static List<(string Label, string Value)> Read(MpvPlayer p)
    {
        var l = new List<(string, string)>();
        void Add(string label, string? v) { if (!string.IsNullOrWhiteSpace(v) && v != "no") l.Add((label, v!.Trim())); }

        Add("Container", p.GetString("file-format"));
        if (p.GetDouble("duration") is double d) Add("Duration", TimeCode.Format(d));
        int? w = p.GetInt("video-params/w"), h = p.GetInt("video-params/h");
        if (w > 0 && h > 0) Add("Resolution", $"{w} × {h}");
        Add("Aspect ratio", p.GetDouble("video-params/aspect") is double a && a > 0 ? a.ToString("0.###", CultureInfo.InvariantCulture) + " : 1" : null);
        double? fps = p.GetDouble("container-fps") ?? p.GetDouble("estimated-vf-fps");
        if (fps > 0) Add("Frame rate", $"{fps:0.###} fps");
        Add("Video codec", p.GetString("video-codec"));
        if (p.GetDouble("video-bitrate") is double vb && vb > 0) Add("Video bitrate", Bitrate(vb));
        Add("Pixel format", p.GetString("video-params/pixelformat"));
        Add("Colour space", p.GetString("video-params/colormatrix"));
        var gamma = p.GetString("video-params/gamma");
        Add("Transfer", gamma);
        if (gamma is "pq" or "hlg") Add("HDR", gamma == "pq" ? "HDR10 / PQ" : "HLG");
        Add("Hardware decoding", p.GetString("hwdec-current"));
        Add("Audio codec", p.GetString("audio-codec-name"));
        Add("Channels", p.GetString("audio-params/hr-channels"));
        if (p.GetInt("audio-params/samplerate") is int sr && sr > 0) Add("Sample rate", $"{sr} Hz");
        if (p.GetDouble("audio-bitrate") is double ab && ab > 0) Add("Audio bitrate", Bitrate(ab));
        var tracks = p.GetTracks();
        var subs = tracks.Where(t => t.Type == "sub").ToList();
        if (subs.Count > 0) Add("Subtitle tracks", string.Join(", ", subs.Select(s => s.Lang ?? s.Title ?? $"#{s.Id}")));
        int audio = tracks.Count(t => t.Type == "audio");
        if (audio > 1) Add("Audio tracks", audio.ToString(CultureInfo.InvariantCulture));
        int chapters = p.GetInt("chapter-list/count") ?? 0;
        if (chapters > 0) Add("Chapters", chapters.ToString(CultureInfo.InvariantCulture));
        return l;
    }

    private static string Bitrate(double bitsPerSecond) =>
        bitsPerSecond >= 1_000_000 ? $"{bitsPerSecond / 1_000_000:0.0} Mb/s" : $"{bitsPerSecond / 1000:0} kb/s";
}
