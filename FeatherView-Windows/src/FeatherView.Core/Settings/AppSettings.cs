using System.Text.Json.Serialization;

namespace FeatherView.Core.Settings;

[JsonConverter(typeof(JsonStringEnumConverter<AppTheme>))]
public enum AppTheme { System, Light, Dark }

[JsonConverter(typeof(JsonStringEnumConverter<VideoEnginePreference>))]
public enum VideoEnginePreference { Auto, MediaFoundation, Mpv }

[JsonConverter(typeof(JsonStringEnumConverter<SortMode>))]
public enum SortMode { Name, Date, Size, Type }

/// <summary>User settings persisted as JSON. Plain data only; validated by <see cref="Normalize"/>.</summary>
public sealed class AppSettings
{
    public int SchemaVersion { get; set; } = 1;

    // Appearance
    public AppTheme Theme { get; set; } = AppTheme.System;

    // General
    public bool RememberWindow { get; set; } = true;
    public double WindowLeft { get; set; } = double.NaN;
    public double WindowTop { get; set; } = double.NaN;
    public double WindowWidth { get; set; } = 1100;
    public double WindowHeight { get; set; } = 720;
    public bool WindowMaximized { get; set; }
    public bool RestoreSession { get; set; } = true;     // reopen the previous tabs at start-up
    public int MaxRecentFiles { get; set; } = 12;
    public List<string> RecentFiles { get; set; } = new();

    // Browsing
    public SortMode Sort { get; set; } = SortMode.Name;
    public bool SortDescending { get; set; }
    public int ThumbnailSize { get; set; } = 160;

    // Photo viewing
    public bool FitOnOpen { get; set; } = true;
    public bool ShowInfoPanelOnOpen { get; set; } = true;
    public bool ShowFilmstrip { get; set; } = true;
    public bool ShowSidebar { get; set; } = true;
    public List<string> Favorites { get; set; } = new();

    // Slideshow
    public int SlideshowSeconds { get; set; } = 4;
    public bool SlideshowLoop { get; set; } = true;
    public bool SlideshowFullscreen { get; set; } = true;

    // Video
    public VideoEnginePreference VideoEngine { get; set; } = VideoEnginePreference.Auto;
    public bool VideoAutoplay { get; set; } = true;
    public bool VideoResume { get; set; } = true;
    public bool VideoLoop { get; set; }
    public int VideoVolume { get; set; } = 80;            // 0-200 (above 100 amplifies)
    public bool VideoMuted { get; set; }
    public bool HardwareDecoding { get; set; } = true;
    public bool AutoLoadSubtitles { get; set; } = true;
    public string PreferredSubtitleLanguages { get; set; } = "";   // e.g. "en,eng"
    public string PreferredAudioLanguages { get; set; } = "";
    public int SubtitleSize { get; set; } = 42;           // mpv sub-font-size
    public string SubtitleColor { get; set; } = "#FFFFFF";
    public int SubtitleBackgroundOpacity { get; set; }    // 0-100 %
    public int SubtitlePosition { get; set; } = 100;      // 0 top .. 100 bottom

    // Performance
    public int ThumbnailCacheItems { get; set; } = 600;
    public int ThumbnailParallelism { get; set; } = 2;

    // Updates (same model as FeatherPDF / FileGlade: daily check, ask first, verify before install)
    public bool AutoUpdateCheck { get; set; } = true;
    public DateTime? LastUpdateCheckUtc { get; set; }
    public string? SkippedVersion { get; set; }

    public const int MinThumb = 96, MaxThumb = 320;

    /// <summary>Clamps every value into its supported range so a hand-edited file cannot break the app.</summary>
    public void Normalize()
    {
        ThumbnailSize = Math.Clamp(ThumbnailSize, MinThumb, MaxThumb);
        SlideshowSeconds = Math.Clamp(SlideshowSeconds, 1, 120);
        ThumbnailCacheItems = Math.Clamp(ThumbnailCacheItems, 50, 5000);
        ThumbnailParallelism = Math.Clamp(ThumbnailParallelism, 1, 8);
        MaxRecentFiles = Math.Clamp(MaxRecentFiles, 0, 50);
        VideoVolume = Math.Clamp(VideoVolume, 0, 200);
        SubtitleSize = Math.Clamp(SubtitleSize, 20, 120);
        SubtitleBackgroundOpacity = Math.Clamp(SubtitleBackgroundOpacity, 0, 100);
        SubtitlePosition = Math.Clamp(SubtitlePosition, 0, 100);
        if (!Enum.IsDefined(VideoEngine)) VideoEngine = VideoEnginePreference.Auto;
        if (string.IsNullOrWhiteSpace(SubtitleColor) || !System.Text.RegularExpressions.Regex.IsMatch(SubtitleColor, "^#[0-9A-Fa-f]{6}$")) SubtitleColor = "#FFFFFF";
        PreferredSubtitleLanguages ??= ""; PreferredAudioLanguages ??= "";
        if (!Enum.IsDefined(Theme)) Theme = AppTheme.System;
        if (!Enum.IsDefined(Sort)) Sort = SortMode.Name;
        if (!(WindowWidth >= 400)) WindowWidth = 1100;
        if (!(WindowHeight >= 300)) WindowHeight = 720;
        RecentFiles ??= new();
        Favorites ??= new();
        if (RecentFiles.Count > MaxRecentFiles) RecentFiles.RemoveRange(MaxRecentFiles, RecentFiles.Count - MaxRecentFiles);
    }

    public void AddRecent(string path)
    {
        RecentFiles.RemoveAll(p => string.Equals(p, path, StringComparison.OrdinalIgnoreCase));
        RecentFiles.Insert(0, path);
        Normalize();
    }

    public bool IsFavorite(string path) => Favorites.Any(p => string.Equals(p, path, StringComparison.OrdinalIgnoreCase));

    /// <summary>Adds or removes a favorite; returns true when the file is now a favorite.</summary>
    public bool ToggleFavorite(string path)
    {
        if (Favorites.RemoveAll(p => string.Equals(p, path, StringComparison.OrdinalIgnoreCase)) > 0) return false;
        Favorites.Insert(0, path);
        if (Favorites.Count > 2000) Favorites.RemoveRange(2000, Favorites.Count - 2000);
        return true;
    }

    public AppSettings Clone() => SettingsJson.Deserialize(SettingsJson.Serialize(this)) ?? new AppSettings();
}

[JsonSourceGenerationOptions(WriteIndented = true, PropertyNameCaseInsensitive = true,
    DefaultIgnoreCondition = JsonIgnoreCondition.Never, NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals)]
[JsonSerializable(typeof(AppSettings))]
internal sealed partial class SettingsJsonContext : JsonSerializerContext { }

public static class SettingsJson
{
    public static string Serialize(AppSettings s) => System.Text.Json.JsonSerializer.Serialize(s, SettingsJsonContext.Default.AppSettings);
    public static AppSettings? Deserialize(string json) => System.Text.Json.JsonSerializer.Deserialize(json, SettingsJsonContext.Default.AppSettings);
}
