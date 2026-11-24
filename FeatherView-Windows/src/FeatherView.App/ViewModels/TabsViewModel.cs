using System.Collections.ObjectModel;
using FeatherView.Core.Mvvm;
using FeatherView.Core.Tabs;
using FeatherView.Services;

namespace FeatherView.ViewModels;

public sealed class TabItemViewModel : ObservableObject
{
    private string _title = "";
    private bool _active;
    public TabItemViewModel(Guid id) { Id = id; }
    public Guid Id { get; }
    public string Title { get => _title; set { if (Set(ref _title, value)) OnPropertyChanged(nameof(DisplayTitle)); } }
    private bool _playing;
    /// <summary>True while a video that belongs to this tab is playing (shown as a small speaker on the tab).</summary>
    public bool IsPlaying { get => _playing; set { if (Set(ref _playing, value)) OnPropertyChanged(nameof(ShowPlayIcon)); } }
    private int _number; private bool _showNumber = true, _showColor = true, _showClose = true, _showPlayIcon = true;
    private System.Windows.Media.Brush _color = System.Windows.Media.Brushes.Transparent;
    public int Number { get => _number; set { if (Set(ref _number, value)) OnPropertyChanged(nameof(DisplayTitle)); } }
    public bool ShowNumber { get => _showNumber; set { if (Set(ref _showNumber, value)) OnPropertyChanged(nameof(DisplayTitle)); } }
    public bool ShowColor { get => _showColor; set => Set(ref _showColor, value); }
    public bool ShowClose { get => _showClose; set => Set(ref _showClose, value); }
    public bool ShowPlayIconSetting { get => _showPlayIcon; set { if (Set(ref _showPlayIcon, value)) OnPropertyChanged(nameof(ShowPlayIcon)); } }
    public bool ShowPlayIcon => _playing && _showPlayIcon;
    public System.Windows.Media.Brush ColorBrush { get => _color; set => Set(ref _color, value); }
    public string DisplayTitle => _showNumber ? $"{_number}  {_title}" : _title;
    public bool IsActive { get => _active; set => Set(ref _active, value); }
}

/// <summary>
/// The tab strip. Inactive tabs keep only a small <see cref="TabState"/>; switching saves the active tab's state and re-opens the
/// other tab's folder, so memory stays flat no matter how many tabs are open (only the active tab holds decoded images).
/// </summary>
public sealed class TabsViewModel : ObservableObject
{
    private readonly MainViewModel _host;
    private readonly IDialogService _dialogs;
    private TabManager _m = new();
    private double _available = 600;
    private Guid? _playingTab;

    public TabsViewModel(MainViewModel host, IDialogService dialogs)
    {
        _host = host; _dialogs = dialogs;
        NewTabCommand = new RelayCommand(() => NewTab());
        CloseTabCommand = new RelayCommand(p => CloseTab(p as TabItemViewModel));
        ReopenTabCommand = new RelayCommand(ReopenTab, () => _m.CanReopen);
        NextTabCommand = new RelayCommand(() => Switch(() => _m.Next()));
        PreviousTabCommand = new RelayCommand(() => Switch(() => _m.Previous()));
        DuplicateTabCommand = new RelayCommand(p => DuplicateTab(p as TabItemViewModel));
        RenameTabCommand = new RelayCommand(p => RenameTab(p as TabItemViewModel));
        CloseOtherTabsCommand = new RelayCommand(p => CloseOthers(p as TabItemViewModel));
        Refresh();
    }

    public ObservableCollection<TabItemViewModel> Items { get; } = new();
    public RelayCommand NewTabCommand { get; }
    public RelayCommand CloseTabCommand { get; }
    public RelayCommand ReopenTabCommand { get; }
    public RelayCommand NextTabCommand { get; }
    public RelayCommand PreviousTabCommand { get; }
    public RelayCommand DuplicateTabCommand { get; }
    public RelayCommand RenameTabCommand { get; }
    public RelayCommand CloseOtherTabsCommand { get; }

    public Guid ActiveId => _m.Active.Id;
    public bool CanReopen => _m.CanReopen;

    public void SetPlayingTab(Guid? id) { _playingTab = id; foreach (var i in Items) i.IsPlaying = id != null && i.Id == id; }
    public int Count => _m.Tabs.Count;

    /// <summary>Width of each tab chip: they share the space in the title bar.</summary>
    public double TabWidth => Math.Clamp((_available - 56) / Math.Max(1, _m.Tabs.Count), 70, 170);
    public void SetAvailableWidth(double width) { _available = width; OnPropertyChanged(nameof(TabWidth)); }

    private static readonly System.Windows.Media.Brush[] Palette = new[] { "#FF6EA8FE", "#FFFFB454", "#FF5FD38D", "#FFC792EA", "#FFFF7B7B", "#FF4DD0C4", "#FFFFD24D", "#FFF48FB1", "#FF9CCC65", "#FF80CBC4" }
        .Select(c => { var b = (System.Windows.Media.SolidColorBrush)new System.Windows.Media.BrushConverter().ConvertFromString(c)!; b.Freeze(); return (System.Windows.Media.Brush)b; }).ToArray();

    /// <summary>A random-looking but stable colour per tab (derived from the tab's own random id).</summary>
    private static System.Windows.Media.Brush ColorFor(Guid id) => Palette[(int)((uint)id.GetHashCode() % (uint)Palette.Length)];

    /// <summary>Applies the tab options from Settings (numbers, colours, close button, playing icon) to every tab.</summary>
    public void RefreshAppearance()
    {
        var s = _host.Settings;
        for (int i = 0; i < Items.Count; i++)
        {
            var t = Items[i];
            t.Number = i + 1; t.ShowNumber = s.ShowTabNumbers; t.ShowColor = s.RandomTabColors; t.ShowClose = s.ShowTabCloseButton; t.ShowPlayIconSetting = s.ShowTabPlayingIcon;
        }
    }

    private int IndexOf(TabItemViewModel? item) => item == null ? _m.ActiveIndex : Math.Max(0, _m.Tabs.ToList().FindIndex(t => t.Id == item.Id));

    private void Refresh()
    {
        Items.Clear();
        for (int i = 0; i < _m.Tabs.Count; i++)
            Items.Add(new TabItemViewModel(_m.Tabs[i].Id) { Title = TabManager.TitleOf(_m.Tabs[i]), IsActive = i == _m.ActiveIndex, Number = i + 1, ColorBrush = ColorFor(_m.Tabs[i].Id) });
        RefreshAppearance();
        OnPropertyChanged(nameof(TabWidth)); OnPropertyChanged(nameof(CanReopen)); OnPropertyChanged(nameof(Count));
        ReopenTabCommand.RaiseCanExecuteChanged();
        SetPlayingTab(_playingTab);
        _host.ValidateVideoOwner(_m.Tabs.Select(t => t.Id).ToList());
    }

    /// <summary>Called by the main view-model after it loads a folder, so the active tab's title follows what it shows.</summary>
    public void UpdateActiveTitle()
    {
        _host.CaptureTabState(_m.Active);
        for (int i = 0; i < Items.Count && i < _m.Tabs.Count; i++) Items[i].Title = TabManager.TitleOf(_m.Tabs[i]);
    }

    private void Switch(Action change)
    {
        _host.CaptureTabState(_m.Active);
        change();
        _host.RestoreTabState(_m.Active);
        Refresh();
    }

    public void Activate(TabItemViewModel item) { int i = IndexOf(item); if (i != _m.ActiveIndex) Switch(() => _m.Activate(i)); }

    public void NewTab()
    {
        if (!_m.CanAdd) { _dialogs.Info("Tabs", $"You can have up to {TabManager.MaxTabs} tabs open."); return; }
        Switch(() => _m.Add());
    }

    /// <summary>Opens a folder or file in a fresh tab.</summary>
    public void OpenInNewTab(string path)
    {
        if (!_m.CanAdd) { _host.OpenPath(path); return; }
        Switch(() => _m.Add());
        _host.OpenPath(path);
    }

    private void CloseTab(TabItemViewModel? item)
    {
        int i = IndexOf(item);
        bool active = i == _m.ActiveIndex;
        _host.CaptureTabState(_m.Active);
        _m.Close(i);
        if (active) _host.RestoreTabState(_m.Active);
        Refresh();
    }

    private void CloseOthers(TabItemViewModel? item)
    {
        int i = IndexOf(item);
        _host.CaptureTabState(_m.Active);
        var keep = _m.Tabs[i];
        bool activeWasKept = ReferenceEquals(_m.Active, keep);
        _m.CloseOthers(i);
        if (!activeWasKept) _host.RestoreTabState(_m.Active);
        Refresh();
    }

    private void ReopenTab()
    {
        _host.CaptureTabState(_m.Active);
        if (_m.Reopen() == null) return;
        _host.RestoreTabState(_m.Active);
        Refresh();
    }

    private void DuplicateTab(TabItemViewModel? item)
    {
        int i = IndexOf(item);
        _host.CaptureTabState(_m.Active);
        if (_m.Duplicate(i) == null) { _dialogs.Info("Tabs", $"You can have up to {TabManager.MaxTabs} tabs open."); return; }
        _host.RestoreTabState(_m.Active);
        Refresh();
    }

    private void RenameTab(TabItemViewModel? item)
    {
        int i = IndexOf(item);
        var text = _dialogs.Prompt("Rename tab", "Leave empty to use the folder name again.", _m.Tabs[i].CustomTitle ?? TabManager.TitleOf(_m.Tabs[i]));
        if (text == null) return;
        _m.Rename(i, text);
        Items[i].Title = TabManager.TitleOf(_m.Tabs[i]);
    }

    // ---- session ----
    public TabSessionData ToSession() { _host.CaptureTabState(_m.Active); return _m.ToData(); }

    public void LoadSession(TabSessionData data)
    {
        _m = TabManager.FromData(data);
        _host.RestoreTabState(_m.Active);
        Refresh();
    }
}
