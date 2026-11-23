using System.IO;
using System.Net.Http;
using System.Windows;
using System.Windows.Threading;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Media;
using FeatherView.Core.Mvvm;
using FeatherView.Core.Settings;
using FeatherView.Core.Updates;
using FeatherView.Core.Video;
using FeatherView.Services;
using FeatherView.Video;

namespace FeatherView.ViewModels;

/// <summary>Everything about playing one video: engine choice, transport state, tracks, resume, screenshots, A-B loop.</summary>
public sealed class VideoPlayerViewModel : ObservableObject, IDisposable
{
    private static readonly double[] SpeedSteps = { 0.25, 0.5, 0.75, 1, 1.25, 1.5, 2, 3, 4 };
    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromMinutes(10) };

    private readonly Func<AppSettings> _settings;
    private readonly IDialogService _dialogs;
    private readonly PlaybackHistory _history;
    private readonly ILog _log;
    private readonly string _localAppData;
    private readonly DispatcherTimer _timer = new() { Interval = TimeSpan.FromMilliseconds(250) };
    private readonly DispatcherTimer _toast = new() { Interval = TimeSpan.FromSeconds(3) };

    private IVideoEngine? _engine;
    private MediaFoundationEngine? _mf;
    private MpvEngine? _mpv;
    private bool _mpvBroken, _triedFallback;
    private MediaFile? _file;
    private string? _key;
    private int _ticks, _rotation, _speedIndex = 3;
    private double? _abA, _abB;
    private double _position, _duration, _volume = 80;
    private bool _isPlaying, _muted, _loop, _seeking;
    private string? _notice, _status;
    private bool _noticeCanInstall;
    private System.Windows.FrameworkElement? _surface;
    private double? _seekTarget, _scrubTarget;     // where the user just asked to go, until the player has really arrived
    private long _seekStamp;
    private readonly DispatcherTimer _scrubTimer = new() { Interval = TimeSpan.FromMilliseconds(220) };
    private bool _fullscreen, _controlsVisible = true;
    private int _idleTicks;

    public event Action<IReadOnlyList<(string Label, string Value)>>? InfoReady;
    public event Action? ToggleFullscreenRequested;

    public VideoPlayerViewModel(Func<AppSettings> settings, IDialogService dialogs, PlaybackHistory history, string localAppData, ILog log)
    {
        _settings = settings; _dialogs = dialogs; _history = history; _localAppData = localAppData; _log = log;
        _timer.Tick += (_, _) => Tick();
        _toast.Tick += (_, _) => { _toast.Stop(); StatusMessage = null; };
        _scrubTimer.Tick += (_, _) =>
        {
            // live preview while dragging (only on engines that can seek fast): at most ~4 seeks a second, never a flood
            if (_scrubTarget is double t) { _engine?.Seek(t, exact: false); _scrubTarget = null; } else _scrubTimer.Stop();
        };

        PlayPauseCommand = new RelayCommand(TogglePlay, () => Active);
        FrameBackCommand = new RelayCommand(() => _engine?.StepFrame(false), () => Active && CanFrameStep);
        FrameForwardCommand = new RelayCommand(() => _engine?.StepFrame(true), () => Active && CanFrameStep);
        SeekRelativeCommand = new RelayCommand(p => { if (!Active) return; if (p is string s && double.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var d)) SeekRelative(d); });
        SpeedUpCommand = new RelayCommand(() => ChangeSpeed(1), () => Active);
        SpeedDownCommand = new RelayCommand(() => ChangeSpeed(-1), () => Active);
        ResetSpeedCommand = new RelayCommand(() => SetSpeedIndex(3), () => Active);
        MuteCommand = new RelayCommand(() => IsMuted = !IsMuted, () => Active);
        VolumeUpCommand = new RelayCommand(() => Volume += 5, () => Active);
        VolumeDownCommand = new RelayCommand(() => Volume -= 5, () => Active);
        LoopCommand = new RelayCommand(() => LoopEnabled = !LoopEnabled, () => Active);
        AbLoopCommand = new RelayCommand(CycleAbLoop, () => Active && CanAbLoop);
        ScreenshotCommand = new RelayCommand(Screenshot, () => Active && CanScreenshot);
        LoadSubtitleCommand = new RelayCommand(LoadSubtitle, () => Active && CanExternalSubtitles);
        LoadAudioCommand = new RelayCommand(LoadAudio, () => Active && CanExternalAudio);
        GoToTimeCommand = new RelayCommand(GoToTime, () => Active);
        ChapterNextCommand = new RelayCommand(() => _engine?.ChapterStep(1), () => Active && CanChapters);
        ChapterPreviousCommand = new RelayCommand(() => _engine?.ChapterStep(-1), () => Active && CanChapters);
        RotateCommand = new RelayCommand(RotateVideo, () => Active && Has(VideoCaps.Rotation));
        InstallEngineCommand = new AsyncRelayCommand(InstallEngineAsync, ex => _log.Error("Engine install failed", ex));
    }

    // ---- commands ----
    public RelayCommand PlayPauseCommand { get; }
    public RelayCommand FrameBackCommand { get; }
    public RelayCommand FrameForwardCommand { get; }
    public RelayCommand SeekRelativeCommand { get; }
    public RelayCommand SpeedUpCommand { get; }
    public RelayCommand SpeedDownCommand { get; }
    public RelayCommand ResetSpeedCommand { get; }
    public RelayCommand MuteCommand { get; }
    public RelayCommand VolumeUpCommand { get; }
    public RelayCommand VolumeDownCommand { get; }
    public RelayCommand LoopCommand { get; }
    public RelayCommand AbLoopCommand { get; }
    public RelayCommand ScreenshotCommand { get; }
    public RelayCommand LoadSubtitleCommand { get; }
    public RelayCommand LoadAudioCommand { get; }
    public RelayCommand GoToTimeCommand { get; }
    public RelayCommand ChapterNextCommand { get; }
    public RelayCommand ChapterPreviousCommand { get; }
    public RelayCommand RotateCommand { get; }
    public AsyncRelayCommand InstallEngineCommand { get; }

    // ---- state for the view ----
    public System.Windows.FrameworkElement? Surface { get => _surface; private set => Set(ref _surface, value); }
    private bool Active => _file != null;
    /// <summary>The control bar. In full screen it hides after a few seconds of no activity while a video plays.</summary>
    private long _toggleStamp;
    public bool ControlsVisible { get => _controlsVisible; private set { if (Set(ref _controlsVisible, value)) { _toggleStamp = Environment.TickCount64; _engine?.SetCursorHidden(!value); } } }
    /// <summary>Full screen starts with the controls (and the mouse pointer) hidden; moving the mouse brings them back.</summary>
    public void SetFullscreen(bool on) { _fullscreen = on; _idleTicks = 0; ControlsVisible = !on; }
    public void RegisterActivity()
    {
        // hiding the bar changes the layout, and Windows can answer with a phantom mouse move: ignore moves in the first moments after hiding
        if (!_controlsVisible && _fullscreen && Environment.TickCount64 - _toggleStamp < 700) return;
        _idleTicks = 0;
        if (!_controlsVisible) ControlsVisible = true;
    }
    public bool HasMedia => _file != null;
    public bool IsCurrent(MediaFile f) => _file != null && string.Equals(_file.Path, f.Path, StringComparison.OrdinalIgnoreCase);
    public void SetBackground(bool background) => _engine?.SetVideoVisible(!background);
    public void RefreshInfo() { if (_engine != null && _file != null) InfoReady?.Invoke(_engine.GetInfo()); }

    private void RaiseCommands()
    {
        foreach (var c in new[] { PlayPauseCommand, FrameBackCommand, FrameForwardCommand, SeekRelativeCommand, SpeedUpCommand, SpeedDownCommand, ResetSpeedCommand, MuteCommand, VolumeUpCommand, VolumeDownCommand,
                                  LoopCommand, AbLoopCommand, ScreenshotCommand, LoadSubtitleCommand, LoadAudioCommand, GoToTimeCommand, ChapterNextCommand, ChapterPreviousCommand, RotateCommand })
            c.RaiseCanExecuteChanged();
    }
    public double Position { get => _position; private set { if (Set(ref _position, value)) OnPropertyChanged(nameof(PositionText)); } }
    public double Duration { get => _duration; private set { if (Set(ref _duration, value)) OnPropertyChanged(nameof(DurationText)); } }
    public string PositionText => TimeCode.Format(_position);
    public string DurationText => TimeCode.Format(_duration);
    public bool IsPlaying { get => _isPlaying; private set { if (Set(ref _isPlaying, value)) OnPropertyChanged(nameof(PlayIconIsPause)); } }
    public bool PlayIconIsPause => _isPlaying;
    public string? Notice { get => _notice; private set { Set(ref _notice, value); OnPropertyChanged(nameof(HasNotice)); } }
    public bool HasNotice => !string.IsNullOrEmpty(_notice);
    public bool NoticeCanInstall { get => _noticeCanInstall; private set => Set(ref _noticeCanInstall, value); }
    public string? StatusMessage { get => _status; private set { Set(ref _status, value); OnPropertyChanged(nameof(HasStatusMessage)); } }
    public bool HasStatusMessage => !string.IsNullOrEmpty(_status);
    public string EngineName => _engine?.Name ?? "";
    public string SpeedText => SpeedSteps[_speedIndex].ToString("0.##", System.Globalization.CultureInfo.InvariantCulture) + "×";
    public string AbText => _abA == null ? "" : _abB == null ? "A" : "A-B";
    public bool IsSeeking => _seeking;

    public double Volume
    {
        get => _volume;
        set
        {
            var max = Has(VideoCaps.Amplify) ? 200 : 100;
            var v = Math.Clamp(value, 0, max);
            if (!Set(ref _volume, v)) return;
            if (_engine != null) _engine.Volume = v;
            _settings().VideoVolume = (int)v;
            OnPropertyChanged(nameof(VolumeMax));
        }
    }
    public double VolumeMax => Has(VideoCaps.Amplify) ? 200 : 100;
    public bool IsMuted { get => _muted; set { if (Set(ref _muted, value)) { if (_engine != null) _engine.Muted = value; _settings().VideoMuted = value; } } }
    public bool LoopEnabled { get => _loop; set { if (Set(ref _loop, value)) { if (_engine != null) _engine.Loop = value; _settings().VideoLoop = value; } } }

    private bool Has(VideoCaps c) => _engine != null && (_engine.Capabilities & c) == c;
    public bool CanFrameStep => Has(VideoCaps.FrameStep);
    public bool CanAbLoop => Has(VideoCaps.AbLoop);
    public bool CanScreenshot => Has(VideoCaps.Screenshot);
    public bool CanExternalSubtitles => Has(VideoCaps.ExternalSubtitles);
    public bool CanExternalAudio => Has(VideoCaps.ExternalAudio);
    public bool CanChapters => Has(VideoCaps.Chapters);
    public bool CanSubtitleTracks => Has(VideoCaps.SubtitleTracks);
    public bool CanAudioTracks => Has(VideoCaps.AudioTracks);
    public bool CanDelays => Has(VideoCaps.Delays);
    public bool CanAspect => Has(VideoCaps.Aspect);
    public bool CanRotate => Has(VideoCaps.Rotation);

    public IReadOnlyList<TrackInfo> AudioTracks => _engine?.AudioTracks ?? Array.Empty<TrackInfo>();
    public IReadOnlyList<TrackInfo> SubtitleTracks => _engine?.SubtitleTracks ?? Array.Empty<TrackInfo>();
    public IReadOnlyList<(string Title, double Time)> Chapters => _engine?.Chapters ?? Array.Empty<(string, double)>();
    public double AudioDelay { get => _engine?.AudioDelay ?? 0; set { if (_engine != null) { _engine.AudioDelay = value; Toast($"Audio delay {value:+0.0;-0.0;0} s"); } } }
    public double SubtitleDelay { get => _engine?.SubtitleDelay ?? 0; set { if (_engine != null) { _engine.SubtitleDelay = value; Toast($"Subtitle delay {value:+0.0;-0.0;0} s"); } } }

    public void SelectAudio(int id) { _engine?.SelectAudio(id); }
    public void SelectSubtitle(int? id) { _engine?.SelectSubtitle(id); }
    public void SetAspect(string? ratio) { _engine?.SetAspect(ratio); }

    // ---- opening and engine choice ----
    private string? FindEngineLibrary() => EngineLocator.Find(AppContext.BaseDirectory, _localAppData);
    public bool EngineInstalled => FindEngineLibrary() != null;
    public string EngineStatusText
    {
        get
        {
            var p = FindEngineLibrary();
            return p == null ? "Not installed. Windows' built-in player is used. MKV, subtitles, audio tracks, frame stepping and many codecs need the extended engine."
                             : "Installed: " + p;
        }
    }

    public void Open(MediaFile file)
    {
        Stop();
        _file = file; _key = PlaybackHistory.MakeKey(file.Path, file.Length, file.LastWriteUtc);
        _triedFallback = false; _abA = _abB = null; _rotation = 0; _speedIndex = 3;
        OnPropertyChanged(nameof(HasMedia)); OnPropertyChanged(nameof(AbText)); OnPropertyChanged(nameof(SpeedText)); RaiseCommands();
        Position = 0; Duration = 0; IsPlaying = false; Notice = null; NoticeCanInstall = false;
        StartWith(ChooseEngine(preferMpv: null));
    }

    private IVideoEngine ChooseEngine(bool? preferMpv)
    {
        var s = _settings();
        bool wantMpv = preferMpv ?? s.VideoEngine switch { VideoEnginePreference.Mpv => true, VideoEnginePreference.MediaFoundation => false, _ => FindEngineLibrary() != null };
        if (wantMpv)
        {
            var lib = FindEngineLibrary();
            if (lib != null && !_mpvBroken)
            {
                try
                {
                    MpvPlayer.Configure(lib);
                    if (MpvPlayer.IsAvailable(out var err)) return _mpv ??= new MpvEngine(lib, s);
                    _log.Warn("libmpv could not be loaded: " + err);
                }
                catch (Exception ex) { _log.Warn($"libmpv start failed ({ex.GetType().Name})."); }
                _mpvBroken = true;
                Notice = "The extended video engine could not be loaded, so Windows' built-in player is used.";
            }
            else if (lib == null) { Notice = "The extended video engine is not installed, so Windows' built-in player is used."; NoticeCanInstall = true; }
        }
        return _mf ??= new MediaFoundationEngine();
    }

    private void StartWith(IVideoEngine engine)
    {
        if (!ReferenceEquals(_engine, engine))
        {
            if (_engine != null) { Unhook(_engine); _engine.Stop(); }
            _engine = engine; Hook(engine);
            Surface = engine.Surface;
        }
        var s = _settings();
        _volume = Math.Clamp(s.VideoVolume, 0, engine.Capabilities.HasFlag(VideoCaps.Amplify) ? 200 : 100);
        _muted = s.VideoMuted; _loop = s.VideoLoop;
        engine.Volume = _volume; engine.Muted = _muted; engine.Loop = _loop; engine.Speed = 1;
        engine.ApplySettings(s);
        OnPropertyChanged(nameof(Volume)); OnPropertyChanged(nameof(VolumeMax)); OnPropertyChanged(nameof(IsMuted)); OnPropertyChanged(nameof(LoopEnabled)); OnPropertyChanged(nameof(EngineName));
        RaiseCaps();
        double start = s.VideoResume && _key != null ? _history.ResumePosition(_key) ?? 0 : 0;
        _resumed = start > 0;
        engine.Open(_file!.Path, start, s.VideoAutoplay);
        _timer.Start();
    }

    private bool _resumed;

    private void Hook(IVideoEngine e)
    {
        e.Opened += OnOpened; e.Ended += OnEnded; e.Failed += OnFailed;
        e.SurfaceClicked += OnSurfaceClicked; e.SurfaceDoubleClicked += OnSurfaceDoubleClicked; e.SurfaceWheel += OnSurfaceWheel; e.SurfaceMoved += RegisterActivity;
    }
    private void Unhook(IVideoEngine e)
    {
        e.Opened -= OnOpened; e.Ended -= OnEnded; e.Failed -= OnFailed;
        e.SurfaceClicked -= OnSurfaceClicked; e.SurfaceDoubleClicked -= OnSurfaceDoubleClicked; e.SurfaceWheel -= OnSurfaceWheel; e.SurfaceMoved -= RegisterActivity;
    }

    private void OnSurfaceClicked() => TogglePlay();
    private void OnSurfaceDoubleClicked() => ToggleFullscreenRequested?.Invoke();
    private void OnSurfaceWheel(int delta) => Volume += delta > 0 ? 5 : -5;

    private void OnOpened()
    {
        if (_engine == null) return;
        Duration = _engine.Duration;
        if (_resumed) Toast("Resumed at " + TimeCode.Format(_engine.Position));
        RaiseCaps();
        InfoReady?.Invoke(_engine.GetInfo());
        IsPlaying = _engine.IsPlaying;
    }

    private void OnEnded()
    {
        IsPlaying = false;
        if (_file != null && _key != null) { _history.Record(_key, _file.Name, _file.Path, 0, _duration); _history.Save(); }
    }

    private void OnFailed(string message)
    {
        IsPlaying = false;
        // The built-in player cannot do MKV and many codecs: try the extended engine automatically when it is installed.
        if (_engine is MediaFoundationEngine && !_triedFallback && _file != null && FindEngineLibrary() != null && !_mpvBroken)
        {
            _triedFallback = true;
            var lib = FindEngineLibrary()!;
            try
            {
                MpvPlayer.Configure(lib);
                if (MpvPlayer.IsAvailable(out _)) { StartWith(_mpv ??= new MpvEngine(lib, _settings())); return; }
            }
            catch (Exception ex) { _log.Warn($"libmpv fallback failed ({ex.GetType().Name})."); _mpvBroken = true; }
        }
        bool canInstall = _engine is MediaFoundationEngine && FindEngineLibrary() == null;
        Notice = canInstall
            ? "Windows' built-in player cannot play this video. The extended video engine supports many more formats (MKV, HEVC, AV1 and others)."
            : "This video could not be played. It may be damaged or use an unsupported format.";
        NoticeCanInstall = canInstall;
        _log.Warn("Video failed: " + (message.Length > 200 ? message[..200] : message));
    }

    private void RaiseCaps()
    {
        foreach (var n in new[] { nameof(CanFrameStep), nameof(CanAbLoop), nameof(CanScreenshot), nameof(CanExternalSubtitles), nameof(CanExternalAudio), nameof(CanChapters),
                                  nameof(CanSubtitleTracks), nameof(CanAudioTracks), nameof(CanDelays), nameof(CanAspect), nameof(CanRotate), nameof(EngineName) })
            OnPropertyChanged(n);
        RaiseCommands();
    }

    // ---- transport ----
    private void Tick()
    {
        if (_engine == null) return;
        _engine.Poll();
        if (!_seeking)
        {
            var p = _engine.Position;
            // right after a seek the player still reports the OLD position for a moment: keep showing the target until it arrives
            if (_seekTarget is double target)
            {
                if (Math.Abs(p - target) < 2.0 || Environment.TickCount64 - _seekStamp > 6000) _seekTarget = null; else p = target;
            }
            Position = p;
        }
        if (_engine.Duration > 0) Duration = _engine.Duration;
        IsPlaying = _engine.IsPlaying;
        if (IsPlaying && ++_ticks % 60 == 0) SaveProgress(flush: false);
        if (_fullscreen) { if (++_idleTicks >= Math.Max(8, _settings().FullscreenControlsHideSeconds * 4) && _controlsVisible) ControlsVisible = false; }   // 4 ticks per second
        else { _idleTicks = 0; if (!_controlsVisible) ControlsVisible = true; }
    }

    public void TogglePlay() { if (_engine == null || _file == null) return; if (_engine.IsPlaying) { _engine.Pause(); SaveProgress(flush: true); } else _engine.Play(); IsPlaying = _engine.IsPlaying; }
    public void Pause() { if (_engine != null && _engine.IsPlaying) { _engine.Pause(); IsPlaying = false; SaveProgress(flush: true); } }
    private void HoldPosition(double target) { _seekTarget = target; _seekStamp = Environment.TickCount64; }

    public void SeekRelative(double seconds)
    {
        if (_engine == null) return;
        var from = _seekTarget ?? _engine.Position;      // repeated key presses add up even before the player has caught up
        var t = Math.Clamp(from + seconds, 0, Math.Max(0, _engine.Duration));
        _engine.Seek(t, exact: Has(VideoCaps.ExactSeek)); Position = t; HoldPosition(t);
    }

    public void BeginSeek() { _seeking = true; _scrubTarget = null; }

    /// <summary>While the thumb is dragged: only the time label moves (and, on fast engines, a throttled preview). The real seek happens once, on release.</summary>
    public void SeekTo(double seconds)
    {
        Position = seconds;
        if (!Has(VideoCaps.ExactSeek)) return;           // the built-in player seeks slowly on big files: wait for the release
        _scrubTarget = seconds;
        if (!_scrubTimer.IsEnabled) _scrubTimer.Start();
    }

    public void EndSeek(double seconds)
    {
        _seeking = false; _scrubTimer.Stop(); _scrubTarget = null;
        _engine?.Seek(seconds, exact: Has(VideoCaps.ExactSeek));
        Position = seconds; HoldPosition(seconds);
    }

    private void SetSpeedIndex(int i) { _speedIndex = Math.Clamp(i, 0, SpeedSteps.Length - 1); if (_engine != null) _engine.Speed = SpeedSteps[_speedIndex]; OnPropertyChanged(nameof(SpeedText)); }
    private void ChangeSpeed(int d) => SetSpeedIndex(_speedIndex + d);
    public void SetSpeed(double s) { var i = Array.IndexOf(SpeedSteps, s); if (i >= 0) SetSpeedIndex(i); }
    public IReadOnlyList<double> Speeds => SpeedSteps;

    private void CycleAbLoop()
    {
        if (_engine == null) return;
        if (_abA == null) { _abA = _engine.Position; Toast("Loop start set"); }
        else if (_abB == null) { if (_engine.Position > _abA) { _abB = _engine.Position; _engine.SetAbLoop(_abA, _abB); Toast("Looping A-B"); } else _abA = _engine.Position; }
        else { _abA = _abB = null; _engine.SetAbLoop(null, null); Toast("Loop cleared"); }
        OnPropertyChanged(nameof(AbText));
    }

    private void RotateVideo() { _rotation = (_rotation + 90) % 360; _engine?.SetRotation(_rotation); }

    private void GoToTime()
    {
        if (_engine == null) return;
        var text = _dialogs.Prompt("Go to time", "Enter a time such as 90, 1:30 or 1:02:03.", TimeCode.Format(_engine.Position));
        if (text == null) return;
        if (TimeCode.Parse(text) is double t) { var tt = Math.Min(t, Math.Max(0, _engine.Duration)); _engine.Seek(tt, exact: Has(VideoCaps.ExactSeek)); Position = tt; HoldPosition(tt); }
        else _dialogs.Error("Go to time", "That is not a valid time.");
    }

    // ---- screenshots, external files ----
    private void Screenshot()
    {
        if (_engine == null || _file == null) return;
        try
        {
            var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyPictures), "Feather View");
            var stamp = TimeCode.Format(_engine.Position).Replace(':', '-');
            var baseName = Path.GetFileNameWithoutExtension(_file.Name) + "_" + stamp;
            var path = Path.Combine(dir, baseName + ".png");
            for (int i = 2; File.Exists(path); i++) path = Path.Combine(dir, $"{baseName} ({i}).png");
            var saved = _engine.SaveScreenshot(path);
            Toast(saved != null ? "Screenshot saved to Pictures\\Feather View" : "The screenshot could not be saved");
        }
        catch (Exception ex) { _log.Warn($"Screenshot failed ({ex.GetType().Name})."); Toast("The screenshot could not be saved"); }
    }

    private void LoadSubtitle()
    {
        var p = _dialogs.PickFile("Load subtitles", "Subtitles|*.srt;*.ass;*.ssa;*.vtt;*.sub;*.idx;*.sup|All files|*.*");
        if (p == null || _engine == null) return;
        _engine.AddExternalSubtitle(p); Toast("Subtitles loaded");
        InfoReady?.Invoke(_engine.GetInfo());
    }

    private void LoadAudio()
    {
        var p = _dialogs.PickFile("Load audio track", "Audio|*.mp3;*.aac;*.m4a;*.ac3;*.dts;*.flac;*.mka;*.ogg;*.wav|All files|*.*");
        if (p == null || _engine == null) return;
        _engine.AddExternalAudio(p); Toast("Audio track loaded");
    }

    // ---- extended engine download ----
    private async Task InstallEngineAsync()
    {
        var dl = new EngineDownloader(Http, log: _log);
        var dir = EngineLocator.EngineDir(_localAppData);
        EngineManifest m;
        try { m = await dl.GetManifestAsync(); }
        catch (UpdateException ex)
        {
            _dialogs.Error("Extended video support", ex.Message + $"\n\nYou can also copy libmpv-2.dll into this folder yourself:\n{dir}");
            return;
        }
        var ok = _dialogs.Confirm("Extended video support",
            $"Download the extended video engine {m.Version}?\n\nIt is the open-source mpv/FFmpeg player library ({m.License ?? "LGPL"}) and adds MKV and many more formats, subtitles, audio tracks and frame stepping. " +
            "Some video formats are covered by patents in some countries; you are responsible for using them lawfully. The download is checked before it is installed.", "Download");
        if (!ok) return;
        var cts = new CancellationTokenSource();
        var progress = _dialogs.ShowDownloadProgress("Downloading extended video support", () => cts.Cancel());
        try { await dl.InstallAsync(m, dir, progress, cts.Token); }
        catch (OperationCanceledException) { _dialogs.CloseDownloadProgress(); return; }
        catch (UpdateException ex) { _dialogs.CloseDownloadProgress(); _dialogs.Error("Extended video support", ex.Message); return; }
        catch (Exception ex) { _log.Warn($"Engine download failed ({ex.GetType().Name})."); _dialogs.CloseDownloadProgress(); _dialogs.Error("Extended video support", "The download failed."); return; }
        _dialogs.CloseDownloadProgress();
        _mpvBroken = false;
        OnPropertyChanged(nameof(EngineStatusText)); OnPropertyChanged(nameof(EngineInstalled));
        _dialogs.Info("Extended video support", "Installed. The current video will now reopen with the extended engine.");
        if (_file != null) { var f = _file; Stop(); _file = f; _key = PlaybackHistory.MakeKey(f.Path, f.Length, f.LastWriteUtc); Notice = null; NoticeCanInstall = false; _triedFallback = true; StartWith(ChooseEngine(preferMpv: true)); }
    }

    // ---- lifecycle ----
    public void ApplySettings() => _engine?.ApplySettings(_settings());

    public void Stop()
    {
        _timer.Stop(); _scrubTimer.Stop(); _seekTarget = null; _scrubTarget = null; _seeking = false;
        if (_engine != null && _file != null) SaveProgress(flush: true);
        _engine?.Stop();
        _file = null; _key = null; IsPlaying = false;
        OnPropertyChanged(nameof(HasMedia)); RaiseCommands();
    }

    private void SaveProgress(bool flush)
    {
        if (_engine == null || _file == null || _key == null) return;
        var pos = _engine.Position; var dur = _engine.Duration;
        if (dur <= 0) return;
        _history.Record(_key, _file.Name, _file.Path, pos, dur);
        if (flush) _history.Save();
    }

    private void Toast(string message) { StatusMessage = message; _toast.Stop(); _toast.Start(); }

    public void Dispose()
    {
        Stop();
        _toast.Stop();
        _history.Save();
        if (_engine != null) Unhook(_engine);
        _mf?.Dispose(); _mpv?.Dispose();
    }
}
