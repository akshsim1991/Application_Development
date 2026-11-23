using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using FeatherView.Video;
using FeatherView.ViewModels;

namespace FeatherView.Views;

/// <summary>View-only code: the seek bar's drag handling and the drop-down menus (built from the view-model's current tracks).</summary>
public partial class VideoPlayerView : UserControl
{
    private bool _dragging;

    public VideoPlayerView()
    {
        InitializeComponent();
        Seek.PreviewMouseLeftButtonDown += (_, _) =>
        {
            if (Vm is not { } vm) return;
            _dragging = true; vm.BeginSeek();
            Dispatcher.BeginInvoke(new Action(() => { if (_dragging) vm.SeekTo(Seek.Value); }));
        };
        Seek.ValueChanged += (_, e) => { if (_dragging) Vm?.SeekTo(e.NewValue); };
        Seek.PreviewMouseLeftButtonUp += (_, _) => EndDrag();
        Seek.AddHandler(Thumb.DragCompletedEvent, new DragCompletedEventHandler((_, _) => EndDrag()));
        Seek.LostMouseCapture += (_, _) => EndDrag();
    }

    private VideoPlayerViewModel? Vm => DataContext as VideoPlayerViewModel;

    private void EndDrag()
    {
        if (!_dragging) return;
        _dragging = false;
        Vm?.EndSeek(Seek.Value);
    }

    private static MenuItem Item(string header, Action action, bool check = false, bool enabled = true)
    {
        var m = new MenuItem { Header = header, IsCheckable = false, IsChecked = check, IsEnabled = enabled };
        m.Click += (_, _) => action();
        return m;
    }

    private void Open(Button anchor, ContextMenu menu)
    {
        menu.PlacementTarget = anchor; menu.Placement = PlacementMode.Top; menu.IsOpen = true;
    }

    private void SpeedButton_Click(object sender, RoutedEventArgs e)
    {
        if (Vm is not { } vm) return;
        var menu = new ContextMenu();
        foreach (var s in vm.Speeds)
            menu.Items.Add(Item(s.ToString("0.##", System.Globalization.CultureInfo.InvariantCulture) + "×", () => vm.SetSpeed(s), check: vm.SpeedText == s.ToString("0.##", System.Globalization.CultureInfo.InvariantCulture) + "×"));
        Open((Button)sender, menu);
    }

    private void SubtitleButton_Click(object sender, RoutedEventArgs e)
    {
        if (Vm is not { } vm) return;
        var menu = new ContextMenu();
        if (!vm.CanSubtitleTracks)
        {
            menu.Items.Add(Item("Subtitles need the extended video engine", () => { }, enabled: false));
            menu.Items.Add(Item("Get extended video support...", () => vm.InstallEngineCommand.Execute(null)));
        }
        else
        {
            var tracks = vm.SubtitleTracks;
            menu.Items.Add(Item("Off", () => vm.SelectSubtitle(null), check: !tracks.Any(t => t.Selected)));
            foreach (var t in tracks) { var id = t.Id; menu.Items.Add(Item(t.Label, () => vm.SelectSubtitle(id), check: t.Selected)); }
            menu.Items.Add(new Separator());
            menu.Items.Add(Item("Load subtitle file...", () => vm.LoadSubtitleCommand.Execute(null)));
            menu.Items.Add(Item("Subtitles earlier (-0.1 s)", () => vm.SubtitleDelay -= 0.1));
            menu.Items.Add(Item("Subtitles later (+0.1 s)", () => vm.SubtitleDelay += 0.1));
            menu.Items.Add(Item("Reset subtitle delay", () => vm.SubtitleDelay = 0));
        }
        Open((Button)sender, menu);
    }

    private void AudioButton_Click(object sender, RoutedEventArgs e)
    {
        if (Vm is not { } vm) return;
        var menu = new ContextMenu();
        if (!vm.CanAudioTracks)
        {
            menu.Items.Add(Item("Audio track choice needs the extended video engine", () => { }, enabled: false));
            menu.Items.Add(Item("Get extended video support...", () => vm.InstallEngineCommand.Execute(null)));
        }
        else
        {
            foreach (var t in vm.AudioTracks) { var id = t.Id; menu.Items.Add(Item(t.Label, () => vm.SelectAudio(id), check: t.Selected)); }
            menu.Items.Add(new Separator());
            menu.Items.Add(Item("Load audio file...", () => vm.LoadAudioCommand.Execute(null)));
            menu.Items.Add(Item("Audio earlier (-0.1 s)", () => vm.AudioDelay -= 0.1));
            menu.Items.Add(Item("Audio later (+0.1 s)", () => vm.AudioDelay += 0.1));
            menu.Items.Add(Item("Reset audio delay", () => vm.AudioDelay = 0));
        }
        Open((Button)sender, menu);
    }

    private void MoreButton_Click(object sender, RoutedEventArgs e)
    {
        if (Vm is not { } vm) return;
        var menu = new ContextMenu();
        menu.Items.Add(Item("Go to time... (Ctrl+T)", () => vm.GoToTimeCommand.Execute(null)));
        menu.Items.Add(Item("Rotate video 90°", () => vm.RotateCommand.Execute(null), enabled: vm.CanRotate));
        var aspect = new MenuItem { Header = "Aspect ratio", IsEnabled = vm.CanAspect };
        foreach (var (label, value) in new (string, string?)[] { ("Original", null), ("16:9", "16:9"), ("4:3", "4:3"), ("21:9", "21:9"), ("1:1", "1:1") })
            { var v = value; aspect.Items.Add(Item(label, () => vm.SetAspect(v))); }
        menu.Items.Add(aspect);
        if (vm.CanChapters)
        {
            var ch = vm.Chapters;
            if (ch.Count > 0)
            {
                var chapters = new MenuItem { Header = $"Chapters ({ch.Count})" };
                chapters.Items.Add(Item("Previous chapter", () => vm.ChapterPreviousCommand.Execute(null)));
                chapters.Items.Add(Item("Next chapter", () => vm.ChapterNextCommand.Execute(null)));
                chapters.Items.Add(new Separator());
                foreach (var (title, time) in ch) { var t = time; chapters.Items.Add(Item($"{FeatherView.Core.Video.TimeCode.Format(t)}  {title}", () => vm.SeekRelative(t - vm.Position))); }
                menu.Items.Add(chapters);
            }
        }
        menu.Items.Add(new Separator());
        menu.Items.Add(Item("Engine: " + vm.EngineName, () => { }, enabled: false));
        Open((Button)sender, menu);
    }
}
