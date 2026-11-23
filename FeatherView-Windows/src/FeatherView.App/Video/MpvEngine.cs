using System.Globalization;
using System.IO;
using System.Windows;
using FeatherView.Core.Settings;
using FeatherView.Core.Video;

namespace FeatherView.Video;

/// <summary>Full-featured engine on libmpv: exact seeking, frame stepping, A-B loop, subtitles (SRT/ASS/...), audio tracks, chapters, HDR, hardware decoding.</summary>
public sealed class MpvEngine : IVideoEngine
{
    private readonly MpvHost _host = new();
    private MpvPlayer? _player;
    private AppSettings _settings;
    private readonly string _libraryPath;
    private (string path, double start, bool autoplay)? _pending;
    private bool _endFired, _loop, _loaded;

    public MpvEngine(string libraryPath, AppSettings settings)
    {
        _libraryPath = libraryPath; _settings = settings;
        _host.HandleReady += OnHandleReady;
        _host.Clicked += () => SurfaceClicked?.Invoke();
        _host.DoubleClicked += () => SurfaceDoubleClicked?.Invoke();
        _host.Wheel += d => SurfaceWheel?.Invoke(d);
    }

    public string Name => "libmpv (extended engine)";
    public VideoCaps Capabilities => VideoCaps.ExactSeek | VideoCaps.FrameStep | VideoCaps.AbLoop | VideoCaps.SubtitleTracks | VideoCaps.AudioTracks | VideoCaps.Chapters |
        VideoCaps.Screenshot | VideoCaps.ExternalSubtitles | VideoCaps.ExternalAudio | VideoCaps.Delays | VideoCaps.DetailedInfo | VideoCaps.SubtitleStyle |
        VideoCaps.Rotation | VideoCaps.Aspect | VideoCaps.Amplify;
    public FrameworkElement Surface => _host;

    public event Action? Opened, Ended, SurfaceClicked, SurfaceDoubleClicked;
    public event Action<string>? Failed;
    public event Action<int>? SurfaceWheel;

    private void OnHandleReady(IntPtr hwnd)
    {
        try
        {
            MpvPlayer.Configure(_libraryPath);
            var o = new Dictionary<string, string>
            {
                ["wid"] = hwnd.ToInt64().ToString(CultureInfo.InvariantCulture),
                ["terminal"] = "no", ["config"] = "no", ["idle"] = "yes", ["osc"] = "no", ["osd-level"] = "0",
                ["input-default-bindings"] = "no", ["input-vo-keyboard"] = "no", ["input-cursor"] = "no", ["cursor-autohide"] = "no",
                ["keep-open"] = "yes", ["hr-seek"] = "yes", ["volume-max"] = "200", ["audio-display"] = "no",
                ["ytdl"] = "no",                                   // never try to reach websites
                ["load-scripts"] = "no", ["load-stats-overlay"] = "no", ["load-osd-console"] = "no", ["load-auto-profiles"] = "no",
                ["demuxer-max-bytes"] = "48MiB", ["demuxer-max-back-bytes"] = "16MiB",   // keep memory modest
                ["screenshot-format"] = "png",
                ["hwdec"] = _settings.HardwareDecoding ? "auto-safe" : "no",
                ["sub-auto"] = _settings.AutoLoadSubtitles ? "fuzzy" : "no",
            };
            if (!string.IsNullOrWhiteSpace(_settings.PreferredSubtitleLanguages)) o["slang"] = _settings.PreferredSubtitleLanguages;
            if (!string.IsNullOrWhiteSpace(_settings.PreferredAudioLanguages)) o["alang"] = _settings.PreferredAudioLanguages;
            _player = new MpvPlayer(o);
            ApplySettings(_settings);
            if (_pending is { } p) { _pending = null; Open(p.path, p.start, p.autoplay); }
        }
        catch (Exception ex) { Failed?.Invoke("The extended video engine could not start: " + ex.Message); }
    }

    public void Open(string path, double startSeconds, bool autoplay)
    {
        _endFired = false; _loaded = false;
        if (_player == null) { _pending = (path, startSeconds, autoplay); return; }
        _player.SetBool("pause", !autoplay);
        _player.SetString("start", startSeconds > 0 ? startSeconds.ToString("0.###", CultureInfo.InvariantCulture) : "none");
        _player.SetString("ab-loop-a", "no"); _player.SetString("ab-loop-b", "no");
        try { _player.LoadFile(path); }
        catch (MpvException ex) { Failed?.Invoke(ex.Message); }
    }

    public void Stop() { _pending = null; _loaded = false; _player?.Stop(); }

    public void Poll()
    {
        if (_player == null) return;
        foreach (var e in _player.DrainEvents())
        {
            switch (e.Kind)
            {
                case MpvEventKind.FileLoaded: _loaded = true; _host.PrepareChildren(); Opened?.Invoke(); break;
                case MpvEventKind.PlaybackRestart: _host.PrepareChildren(); break;
                case MpvEventKind.Error: _loaded = false; Failed?.Invoke(e.Detail ?? "This video could not be played."); break;
            }
        }
        if (_loaded && !_endFired && _player.GetBool("eof-reached")) { _endFired = true; Ended?.Invoke(); }
        else if (_endFired && !_player.GetBool("eof-reached")) _endFired = false;
    }

    public bool IsPlaying => _player != null && _loaded && !_player.GetBool("pause") && !_player.GetBool("eof-reached");
    public void Play() { if (_player == null) return; if (_player.GetBool("eof-reached")) _player.Command("seek", "0", "absolute"); _player.SetBool("pause", false); }
    public void Pause() => _player?.SetBool("pause", true);
    public double Position => _player?.GetDouble("time-pos") ?? 0;
    public double Duration => _player?.GetDouble("duration") ?? 0;
    public void Seek(double seconds, bool exact) =>
        _player?.TryCommand("seek", Math.Max(0, seconds).ToString("0.###", CultureInfo.InvariantCulture), exact ? "absolute+exact" : "absolute+keyframes");

    public double Volume { get => _player?.GetDouble("volume") ?? 80; set => _player?.SetDouble("volume", Math.Clamp(value, 0, 200)); }
    public bool Muted { get => _player?.GetBool("mute") ?? false; set => _player?.SetBool("mute", value); }
    public double Speed { get => _player?.GetDouble("speed") ?? 1; set => _player?.SetDouble("speed", Math.Clamp(value, 0.25, 4)); }
    public bool Loop { get => _loop; set { _loop = value; _player?.SetString("loop-file", value ? "inf" : "no"); } }

    public void StepFrame(bool forward)
    {
        if (_player == null) return;
        _player.SetBool("pause", true);
        _player.TryCommand(forward ? "frame-step" : "frame-back-step");
    }

    public void SetAbLoop(double? a, double? b)
    {
        if (_player == null) return;
        _player.SetString("ab-loop-a", a is double x ? x.ToString("0.###", CultureInfo.InvariantCulture) : "no");
        _player.SetString("ab-loop-b", b is double y ? y.ToString("0.###", CultureInfo.InvariantCulture) : "no");
    }

    private static string Label(MpvTrack t)
    {
        var parts = new List<string>();
        if (!string.IsNullOrWhiteSpace(t.Lang)) parts.Add(t.Lang!);
        if (!string.IsNullOrWhiteSpace(t.Title)) parts.Add(t.Title!);
        var s = parts.Count > 0 ? string.Join(" - ", parts) : $"Track {t.Id}";
        if (!string.IsNullOrWhiteSpace(t.Codec)) s += $" [{t.Codec}]";
        if (t.External) s += " (external)";
        return s;
    }

    public IReadOnlyList<TrackInfo> AudioTracks => Tracks("audio");
    public IReadOnlyList<TrackInfo> SubtitleTracks => Tracks("sub");
    private IReadOnlyList<TrackInfo> Tracks(string type) =>
        _player == null ? Array.Empty<TrackInfo>() : _player.GetTracks().Where(t => t.Type == type).Select(t => new TrackInfo(t.Id, Label(t), t.Selected)).ToList();

    public void SelectAudio(int id) => _player?.SetString("aid", id.ToString(CultureInfo.InvariantCulture));
    public void SelectSubtitle(int? id) => _player?.SetString("sid", id is int i ? i.ToString(CultureInfo.InvariantCulture) : "no");
    public void AddExternalSubtitle(string path) => _player?.TryCommand("sub-add", path, "select");
    public void AddExternalAudio(string path) => _player?.TryCommand("audio-add", path, "select");

    public double AudioDelay { get => _player?.GetDouble("audio-delay") ?? 0; set => _player?.SetDouble("audio-delay", value); }
    public double SubtitleDelay { get => _player?.GetDouble("sub-delay") ?? 0; set => _player?.SetDouble("sub-delay", value); }

    public IReadOnlyList<(string Title, double Time)> Chapters => _player == null ? Array.Empty<(string, double)>() : _player.GetChapters().Select(c => (c.Title, c.Time)).ToList();
    public void ChapterStep(int delta) => _player?.TryCommand("add", "chapter", delta.ToString(CultureInfo.InvariantCulture));

    public void SetRotation(int degrees) => _player?.SetString("video-rotate", (((degrees % 360) + 360) % 360).ToString(CultureInfo.InvariantCulture));
    public void SetAspect(string? ratio) => _player?.SetString("video-aspect-override", ratio ?? "-1");

    public string? SaveScreenshot(string path)
    {
        if (_player == null) return null;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        return _player.TryCommand("screenshot-to-file", path, "video") ? path : null;     // video only: no subtitles or overlays burned in
    }

    public IReadOnlyList<(string Label, string Value)> GetInfo() => _player == null ? Array.Empty<(string, string)>() : MpvInfo.Read(_player);

    public void ApplySettings(AppSettings s)
    {
        _settings = s;
        if (_player == null) return;
        _player.SetDouble("sub-font-size", s.SubtitleSize);
        _player.SetString("sub-color", s.SubtitleColor);
        _player.SetDouble("sub-pos", s.SubtitlePosition);
        if (s.SubtitleBackgroundOpacity > 0)
        {
            int a = (int)Math.Round(s.SubtitleBackgroundOpacity * 255 / 100.0);
            if (_player.SetString("sub-border-style", "background-box")) _player.SetString("sub-back-color", $"#{a:X2}000000");
            else _player.SetString("sub-ass-style-overrides", $"BorderStyle=3,Outline=3,OutlineColour=&H{255 - a:X2}000000");   // libmpv before 0.38: ASS box style (alpha is inverted in ASS)
        }
        else if (!_player.SetString("sub-border-style", "outline-and-shadow")) _player.SetString("sub-ass-style-overrides", "");
    }

    public void Dispose() { _pending = null; _player?.Dispose(); _player = null; }
}
