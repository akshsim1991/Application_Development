using System.Windows;
using FeatherView.Core.Settings;

namespace FeatherView.Video;

[Flags]
public enum VideoCaps
{
    None = 0, ExactSeek = 1, FrameStep = 2, AbLoop = 4, SubtitleTracks = 8, AudioTracks = 16, Chapters = 32, Screenshot = 64,
    ExternalSubtitles = 128, ExternalAudio = 256, Delays = 512, DetailedInfo = 1024, SubtitleStyle = 2048, Rotation = 4096, Aspect = 8192, Amplify = 16384,
}

public sealed record TrackInfo(int Id, string Label, bool Selected);

/// <summary>
/// One playback back-end. Two exist: Media Foundation (built into Windows, no dependencies, basic features) and
/// libmpv (optional download, full feature set). The view-model only talks to this interface.
/// </summary>
public interface IVideoEngine : IDisposable
{
    string Name { get; }
    VideoCaps Capabilities { get; }
    FrameworkElement Surface { get; }

    event Action? Opened;
    event Action? Ended;
    event Action<string>? Failed;
    event Action? SurfaceClicked;
    event Action? SurfaceDoubleClicked;
    event Action<int>? SurfaceWheel;
    event Action? SurfaceMoved;
    void SetCursorHidden(bool hidden);
    /// <summary>False while the player is not on screen (another tab): the picture is not decoded, the sound keeps going.</summary>
    void SetVideoVisible(bool visible);

    void Open(string path, double startSeconds, bool autoplay);
    void Stop();
    void Poll();                       // called ~4x/second by the view-model while a video is shown

    bool IsPlaying { get; }
    void Play();
    void Pause();
    double Position { get; }
    double Duration { get; }
    void Seek(double seconds, bool exact);

    double Volume { get; set; }        // 0-200 (above 100 only where Amplify is supported)
    bool Muted { get; set; }
    double Speed { get; set; }
    bool Loop { get; set; }

    void StepFrame(bool forward);
    void SetAbLoop(double? a, double? b);

    IReadOnlyList<TrackInfo> AudioTracks { get; }
    IReadOnlyList<TrackInfo> SubtitleTracks { get; }
    void SelectAudio(int id);
    void SelectSubtitle(int? id);
    void AddExternalSubtitle(string path);
    void AddExternalAudio(string path);
    double AudioDelay { get; set; }    // seconds
    double SubtitleDelay { get; set; }
    IReadOnlyList<(string Title, double Time)> Chapters { get; }
    void ChapterStep(int delta);

    void SetRotation(int degrees);     // 0/90/180/270, display only
    void SetAspect(string? ratio);     // "16:9", "4:3" or null for the original
    string? SaveScreenshot(string path);
    IReadOnlyList<(string Label, string Value)> GetInfo();
    void ApplySettings(AppSettings settings);
}
