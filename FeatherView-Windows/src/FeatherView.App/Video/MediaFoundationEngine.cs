using System.Globalization;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Effects;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using FeatherView.Core.Settings;
using FeatherView.Core.Video;

namespace FeatherView.Video;

/// <summary>
/// Built-in engine using Windows' own Media Foundation pipeline through WPF: no extra files.
/// Subtitles (SRT, VTT, ASS text) are drawn by the app itself, and aspect ratio / rotation are display transforms.
/// </summary>
public sealed class MediaFoundationEngine : IVideoEngine
{
    private readonly MediaElement _me = new()
    {
        LoadedBehavior = MediaState.Manual, UnloadedBehavior = MediaState.Manual, ScrubbingEnabled = false, Stretch = Stretch.Uniform,
        Focusable = false, Volume = 0.8
    };
    private readonly Grid _surface = new() { Background = Brushes.Black };
    private readonly TextBlock _subText = new() { TextAlignment = TextAlignment.Center, TextWrapping = TextWrapping.Wrap, IsHitTestVisible = false };
    private readonly Border _subBox = new() { HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Bottom, CornerRadius = new CornerRadius(6), Padding = new Thickness(12, 4, 12, 4), Visibility = Visibility.Collapsed, IsHitTestVisible = false };
    private readonly DispatcherTimer _subTimer = new() { Interval = TimeSpan.FromMilliseconds(100) };
    private readonly List<SubtitleTrack> _subs = new();
    private int _activeSub = -1;
    private double _subDelay;
    private AppSettings _settings = new();

    private double _lastX = -1, _lastY = -1;
    private bool _playing, _loop, _opened;
    private double _startAt;
    private bool _autoplay;
    private double _volume = 80;
    private bool _muted;
    private int _rotation;
    private double? _aspect;           // forced display aspect ratio (width / height), null = original
    private string? _path;

    public MediaFoundationEngine()
    {
        _subBox.Child = _subText;
        _surface.Children.Add(_me);
        _surface.Children.Add(_subBox);
        _surface.SizeChanged += (_, _) => UpdateSubtitleStyle();
        _subTimer.Tick += (_, _) => UpdateSubtitle();

        _me.MediaOpened += (_, _) =>
        {
            _opened = true;
            if (_startAt > 0) _me.Position = TimeSpan.FromSeconds(_startAt);
            if (_autoplay) { _me.Play(); _playing = true; } else { _me.Pause(); _playing = false; }
            ApplyTransform();
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
    public VideoCaps Capabilities => VideoCaps.Screenshot | VideoCaps.Rotation | VideoCaps.Aspect | VideoCaps.SubtitleTracks | VideoCaps.ExternalSubtitles | VideoCaps.SubtitleStyle;
    public FrameworkElement Surface => _surface;

    public event Action? Opened, Ended, SurfaceClicked, SurfaceDoubleClicked, SurfaceMoved;
    public event Action<string>? Failed;
    public event Action<int>? SurfaceWheel;
    public void SetCursorHidden(bool hidden) => _surface.Cursor = hidden ? Cursors.None : null;
    public void SetVideoVisible(bool visible) { }

    public void Open(string path, double startSeconds, bool autoplay)
    {
        _opened = false; _startAt = startSeconds; _autoplay = autoplay; _path = path;
        LoadSubtitlesFor(path);
        _me.Source = new Uri(path);
        _me.Play(); _me.Pause();              // initialise the pipeline; MediaOpened decides what happens next
        ApplyVolume();
    }

    public void Stop() { _playing = false; _opened = false; _subTimer.Stop(); try { _me.Stop(); _me.Close(); _me.Source = null; } catch { } }
    public void Poll() { }

    public bool IsPlaying => _playing;
    // Scrubbing (showing frames while paused/seeking) costs extra decoding, so it is only on while the video is paused.
    public void Play() { if (_opened) { _me.ScrubbingEnabled = false; _me.Play(); _playing = true; } }
    public void Pause() { if (_opened) { _me.Pause(); _me.ScrubbingEnabled = true; _playing = false; } }
    public double Position => _me.Position.TotalSeconds;
    public double Duration => _me.NaturalDuration.HasTimeSpan ? _me.NaturalDuration.TimeSpan.TotalSeconds : 0;
    public void Seek(double seconds, bool exact) { if (_opened) { _me.Position = TimeSpan.FromSeconds(Math.Max(0, seconds)); UpdateSubtitle(); } }

    public double Volume { get => _volume; set { _volume = Math.Clamp(value, 0, 100); ApplyVolume(); } }   // no amplification on this engine
    public bool Muted { get => _muted; set { _muted = value; ApplyVolume(); } }
    public double Speed { get => _me.SpeedRatio; set => _me.SpeedRatio = Math.Clamp(value, 0.25, 4); }
    public bool Loop { get => _loop; set => _loop = value; }
    private void ApplyVolume() { _me.IsMuted = _muted; _me.Volume = _volume / 100.0; }

    public void StepFrame(bool forward) { }
    public void SetAbLoop(double? a, double? b) { }
    public IReadOnlyList<TrackInfo> AudioTracks => Array.Empty<TrackInfo>();
    public void SelectAudio(int id) { }
    public void AddExternalAudio(string path) { }
    public double AudioDelay { get => 0; set { } }
    public IReadOnlyList<(string Title, double Time)> Chapters => Array.Empty<(string, double)>();
    public void ChapterStep(int delta) { }

    // ---- subtitles (drawn by the app) ----
    public IReadOnlyList<TrackInfo> SubtitleTracks => _subs.Select((t, i) => new TrackInfo(i + 1, t.Name, i == _activeSub)).ToList();

    private void LoadSubtitlesFor(string videoPath)
    {
        _subs.Clear(); _activeSub = -1; _subDelay = 0; _subText.Text = ""; _subBox.Visibility = Visibility.Collapsed;
        if (!_settings.AutoLoadSubtitles) return;
        foreach (var f in SubtitleParser.FindFor(videoPath))
            try { var t = SubtitleParser.Load(f); if (t.Count > 0) _subs.Add(t); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        if (_subs.Count > 0) { _activeSub = 0; _subTimer.Start(); }
    }

    public void AddExternalSubtitle(string path)
    {
        try
        {
            var t = SubtitleParser.Load(path);
            if (t.Count == 0) return;
            _subs.Add(t); _activeSub = _subs.Count - 1; _subTimer.Start(); UpdateSubtitle();
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
    }

    public void SelectSubtitle(int? id)
    {
        _activeSub = id is int i && i >= 1 && i <= _subs.Count ? i - 1 : -1;
        if (_activeSub >= 0) _subTimer.Start(); else _subTimer.Stop();
        UpdateSubtitle();
    }

    public double SubtitleDelay { get => _subDelay; set { _subDelay = value; UpdateSubtitle(); } }

    private void UpdateSubtitle()
    {
        if (_activeSub < 0 || _activeSub >= _subs.Count) { _subBox.Visibility = Visibility.Collapsed; return; }
        var text = _subs[_activeSub].TextAt(_me.Position.TotalSeconds, _subDelay);
        if (text.Length == 0) { _subBox.Visibility = Visibility.Collapsed; return; }
        if (_subText.Text != text) _subText.Text = text;
        _subBox.Visibility = Visibility.Visible;
    }

    /// <summary>Font size follows the picture height (like libmpv's 720p reference), colour, background and position come from Settings.</summary>
    private void UpdateSubtitleStyle()
    {
        double h = Math.Max(120, _surface.ActualHeight);
        _subText.FontSize = Math.Max(10, _settings.SubtitleSize / 720.0 * h * 0.62);
        _subText.FontWeight = FontWeights.SemiBold;
        try { _subText.Foreground = new SolidColorBrush((Color)ColorConverter.ConvertFromString(_settings.SubtitleColor)); } catch { _subText.Foreground = Brushes.White; }
        _subText.Effect = _settings.SubtitleBackgroundOpacity > 0 ? null : new DropShadowEffect { Color = Colors.Black, BlurRadius = 5, ShadowDepth = 1.5, Opacity = 0.95 };
        byte a = (byte)Math.Round(_settings.SubtitleBackgroundOpacity * 255 / 100.0);
        _subBox.Background = a == 0 ? Brushes.Transparent : new SolidColorBrush(Color.FromArgb(a, 0, 0, 0));
        _subBox.MaxWidth = Math.Max(200, _surface.ActualWidth * 0.9);
        // 100 = bottom edge, 0 = top: slide the box between the bottom margin and the top
        double room = Math.Max(0, h - _subText.FontSize * 3);
        _subBox.Margin = new Thickness(0, 0, 0, 18 + room * (100 - _settings.SubtitlePosition) / 100.0);
    }

    public void ApplySettings(AppSettings settings) { _settings = settings; UpdateSubtitleStyle(); }

    // ---- display transforms ----
    public void SetRotation(int degrees) { _rotation = ((degrees % 360) + 360) % 360; ApplyTransform(); }

    /// <summary>Forces a display aspect ratio ("16:9", "4:3"...) by stretching the picture; null restores the original.</summary>
    public void SetAspect(string? ratio)
    {
        _aspect = null;
        if (ratio != null)
        {
            var p = ratio.Split(':');
            if (p.Length == 2 && double.TryParse(p[0], NumberStyles.Float, CultureInfo.InvariantCulture, out var w) && double.TryParse(p[1], NumberStyles.Float, CultureInfo.InvariantCulture, out var h) && w > 0 && h > 0) _aspect = w / h;
        }
        ApplyTransform();
    }

    private void ApplyTransform()
    {
        var group = new TransformGroup();
        if (_aspect is double target && _me.NaturalVideoWidth > 0 && _me.NaturalVideoHeight > 0)
        {
            double natural = (double)_me.NaturalVideoWidth / _me.NaturalVideoHeight;
            group.Children.Add(new ScaleTransform(target / natural, 1));
        }
        if (_rotation != 0) group.Children.Add(new RotateTransform(_rotation));
        _me.LayoutTransform = group.Children.Count == 0 ? Transform.Identity : group;
    }

    public string? SaveScreenshot(string path)
    {
        try
        {
            int w = (int)_me.ActualWidth, h = (int)_me.ActualHeight;
            if (w < 2 || h < 2) return null;
            var rtb = new RenderTargetBitmap(w, h, 96, 96, PixelFormats.Pbgra32);
            rtb.Render(_me);                  // the video only: subtitles are not burned in
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
        if (_subs.Count > 0) l.Add(("Subtitles", string.Join(", ", _subs.Select(s => s.Name))));
        l.Add(("Engine", "Windows Media Foundation"));
        l.Add(("More details", "Install the extended video engine in Settings for codecs, bitrate, audio tracks and more formats."));
        return l;
    }

    public void Dispose() => Stop();
}
