namespace FeatherView.Core.Tabs;

/// <summary>
/// Decides when a video that belongs to one tab keeps playing while another tab is shown ("keep playing in the background").
/// Pure rules so they can be tested; the view-model just asks.
/// </summary>
public static class BackgroundVideoPolicy
{
    /// <summary>Switching tabs: keep the current video running only when the setting is on and there is a video.</summary>
    public static bool ShouldStopOnTabSwitch(bool keepSetting, bool hasVideo) => hasVideo && !keepSetting;

    /// <summary>Showing a picture or another folder: stop the video only if it belongs to the tab being used now (or background play is off).</summary>
    public static bool ShouldStopForOtherContent(bool keepSetting, bool hasVideo, Guid owner, Guid activeTab) =>
        hasVideo && (!keepSetting || owner == activeTab);

    /// <summary>Closing tabs: a background video whose tab no longer exists must not keep playing invisibly.</summary>
    public static bool OwnerGone(bool hasVideo, Guid owner, IEnumerable<Guid> openTabs) =>
        hasVideo && owner != Guid.Empty && !openTabs.Contains(owner);
}
