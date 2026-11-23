using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using FeatherView.Core.Settings;
using FeatherView.Core.Video;

namespace FeatherView.Video;

/// <summary>Built-in engine using Windows' own Media Foundation pipeline through WPF. No extra files, basic features.</summary>
public sealed class MediaFoundationEngine : IVideoEngine
{
    private readonly MediaElement _me = new()
    {
        LoadedBehavior = MediaState.Manual, UnloadedBehavior = MediaState.Manual, ScrubbingEnabled = true, Stretch = Stretch.Uniform,
        Focusable = false, Volume = 0.8
    };
    private double _lastX = -1, _lastY = -1;
    private bool _playing, _loop, _opened;
    private double _startAt;
    private bool _autoplay;
    private double _volume = 80;
    private bool _muted;
    private int _rotation;

    public MediaFoundationEngine()
    {
        _me.MediaOpened += (_, _) =>
        {
            _opened = true;
            if (_startAt > 0) _me.Position = TimeSpan.FromSeconds(_startAt);
            if (_autoplay) { _me.Play(); _playing = true; } else { _me.Pause(); _playing = false; }
            Opened?.Invoke();
        };
        _me.MediaEnded += (_, _) =>
        {
            if (_loop) { _me.Position = TimeSpan.Zero; _me.Play(); _playing = true; return; }
            _playing = false; Ended?.Invoke();
        };
        _me.MediaFailed += (_, e) => { _opened = false; _playing = false; Failed?.Invoke(e.ErrorException?.Message ?? "The video could not be played."); };
        _me.MouseLeftButtonDown += (_, e) => { if (e.ClickCount >= 2) SurfaceDoubleClicked?.Invoke(); else SurfaceClicked?.Invoke(); };
        _me.MouseWheel += (_, e) => { SurfaceWheel?.Invoke(e.Delta); e.Handled = true; };
        _me.MouseMove += (_, e) =>
        {
            var p = e.GetPosition(_me);
            if (Math.Abs(p.X - _lastX) >= 1 || Math.Abs(p.Y - _lastY) >= 1) { _lastX = p.X; _lastY = p.Y; SurfaceMoved?.Invoke(); }
        };
    }

    public string Name => "Windows Media Foundation (built in)";
    public VideoCaps Capabilities => VideoCaps.Screenshot | VideoCaps.Rotation;
    public FrameworkElement Surface => _me;

    public event Action? Opened, Ended, SurfaceClicked, SurfaceDoubleClicked;
    public event Action<string>? Failed;
    public event Action<int>? SurfaceWheel;
    public event Action? SurfaceMoved;
    public void SetVideoVisible(bool visible) { }
    public void SetCursorHidden(bool hidden) => _me.Cursor = hidden ? Cursors.None : null;

    public void Open(string path, double startSeconds, bool autoplay)
    {
        _opened = false; _startAt = startSeconds; _autoplay = autoplay;
        _me.Source = new Uri(path);
        _me.Play(); _me.Pause();              // initialise the pipeline; MediaOpened decides what happens next
        ApplyVolume();
    }

    public void Stop() { _playing = false; _opened = false; try { _me.Stop(); _me.Close(); _me.Source = null; } catch { } }
    public void Poll() { }

    public bool IsPlaying => _playing;
    public void Play() { if (_opened) { _me.Play(); _playing = true; } }
    public void Pause() { if (_opened) { _me.Pause(); _playing = false; } }
    public double Position => _me.Position.TotalSeconds;
    public double Duration => _me.NaturalDuration.HasTimeSpan ? _me.NaturalDuration.TimeSpan.TotalSeconds : 0;
    public void Seek(double seconds, bool exact) { if (_opened) _me.Position = TimeSpan.FromSeconds(Math.Max(0, seconds)); }

    public double Volume { get => _volume; set { _volume = Math.Clamp(value, 0, 100); ApplyVolume(); } }   // no amplification on this engine
    public bool Muted { get => _muted; set { _muted = value; ApplyVolume(); } }
    public double Speed { get => _me.SpeedRatio; set => _me.SpeedRatio = Math.Clamp(value, 0.25, 4); }
    public bool Loop { get => _loop; set => _loop = value; }
    private void ApplyVolume() { _me.IsMuted = _muted; _me.Volume = _volume / 100.0; }

    public void StepFrame(bool forward) { }
    public void SetAbLoop(double? a, double? b) { }
    public IReadOnlyList<TrackInfo> AudioTracks => Array.Empty<TrackInfo>();
    public IReadOnlyList<TrackInfo> SubtitleTracks => Array.Empty<TrackInfo>();
    public void SelectAudio(int id) { }
    public void SelectSubtitle(int? id) { }
    public void AddExternalSubtitle(string path) { }
    public void AddExternalAudio(string path) { }
    public double AudioDelay { get => 0; set { } }
    public double SubtitleDelay { get => 0; set { } }
    public IReadOnlyList<(string Title, double Time)> Chapters => Array.Empty<(string, double)>();
    public void ChapterStep(int delta) { }

    public void SetRotation(int degrees)
    {
        _rotation = ((degrees % 360) + 360) % 360;
        _me.LayoutTransform = new RotateTransform(_rotation);
    }
    public void SetAspect(string? ratio) { }

    public string? SaveScreenshot(string path)
    {
        try
        {
            int w = (int)_me.ActualWidth, h = (int)_me.ActualHeight;
            if (w < 2 || h < 2) return null;
            var rtb = new RenderTargetBitmap(w, h, 96, 96, PixelFormats.Pbgra32);
            rtb.Render(_me);
            var enc = new PngBitmapEncoder(); enc.Frames.Add(BitmapFrame.Create(rtb));
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            using var fs = File.Create(path); enc.Save(fs);
            return path;
        }
        catch { return null; }
    }

    public IReadOnlyList<(string Label, string Value)> GetInfo()
    {
        var l = new List<(string, string)>();
        if (_me.NaturalVideoWidth > 0) l.Add(("Resolution", $"{_me.NaturalVideoWidth} × {_me.NaturalVideoHeight}"));
        if (Duration > 0) l.Add(("Duration", TimeCode.Format(Duration)));
        l.Add(("Engine", "Windows Media Foundation"));
        l.Add(("More details", "Install the extended video engine in Settings for codecs, bitrate, tracks and subtitles."));
        return l;
    }

    public void ApplySettings(AppSettings settings) { }
    public void Dispose() => Stop();
}
