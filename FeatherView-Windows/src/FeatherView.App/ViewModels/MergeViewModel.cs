using System.Collections.ObjectModel;
using System.IO;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Media;
using FeatherView.Core.Mvvm;
using FeatherView.Core.Video;

namespace FeatherView.ViewModels;

public sealed class MergeItem
{
    public MergeItem(MediaFile file) { File = file; }
    public MediaFile File { get; }
    public string Name => File.Name;
    public string Size => MainViewModel.FormatSize(File.Length);
}

/// <summary>Logic of the "Merge videos" window: order, output name, running ffmpeg with progress and cancel.</summary>
public sealed class MergeViewModel : ObservableObject
{
    private readonly VideoMerger _merger;
    private readonly ILog _log;
    private CancellationTokenSource? _cts;
    private string _outputName, _status = "";
    private double _progress;
    private bool _running, _done;
    private int _modeIndex;

    public MergeViewModel(string ffmpegPath, IEnumerable<MediaFile> files, ILog log)
    {
        _log = log;
        _merger = new VideoMerger(ffmpegPath);
        foreach (var f in files) Items.Add(new MergeItem(f));
        _outputName = "Merged - " + Path.GetFileNameWithoutExtension(Items[0].Name);
        MoveUpCommand = new RelayCommand(p => Move(p as MergeItem, -1), _ => !Running);
        MoveDownCommand = new RelayCommand(p => Move(p as MergeItem, 1), _ => !Running);
        RemoveCommand = new RelayCommand(p => { if (p is MergeItem m && Items.Count > 2) { Items.Remove(m); Refresh(); } }, _ => !Running);
        StartCommand = new AsyncRelayCommand(StartAsync, ex => { _log.Error("Merge failed", ex); Status = "Something went wrong: " + ex.Message; Running = false; });
        CancelCommand = new RelayCommand(() => _cts?.Cancel(), () => Running);
    }

    public ObservableCollection<MergeItem> Items { get; } = new();
    public string OutputFolder => Path.GetDirectoryName(Items[0].File.Path) ?? "";
    public string OutputName { get => _outputName; set => Set(ref _outputName, value); }
    public string Status { get => _status; private set => Set(ref _status, value); }
    public double Progress { get => _progress; private set => Set(ref _progress, value); }
    public bool Running { get => _running; private set { if (Set(ref _running, value)) { OnPropertyChanged(nameof(CanEdit)); RaiseCommands(); } } }
    public bool Done { get => _done; private set => Set(ref _done, value); }
    public bool CanEdit => !Running && !Done;
    public string? OutputPath { get; private set; }
    /// <summary>0 = automatic (lossless when the videos match, otherwise re-encoded), 1 = always re-encode.</summary>
    public int ModeIndex { get => _modeIndex; set => Set(ref _modeIndex, value); }
    public string Summary => $"{Items.Count} videos, {MainViewModel.FormatSize(Items.Sum(i => i.File.Length))} in total";

    public RelayCommand MoveUpCommand { get; }
    public RelayCommand MoveDownCommand { get; }
    public RelayCommand RemoveCommand { get; }
    public AsyncRelayCommand StartCommand { get; }
    public RelayCommand CancelCommand { get; }

    private void RaiseCommands() { MoveUpCommand.RaiseCanExecuteChanged(); MoveDownCommand.RaiseCanExecuteChanged(); RemoveCommand.RaiseCanExecuteChanged(); CancelCommand.RaiseCanExecuteChanged(); }
    private void Refresh() { OnPropertyChanged(nameof(Summary)); OnPropertyChanged(nameof(OutputFolder)); }

    private void Move(MergeItem? item, int delta)
    {
        if (item == null) return;
        int i = Items.IndexOf(item), j = i + delta;
        if (i < 0 || j < 0 || j >= Items.Count) return;
        Items.Move(i, j); Refresh();
    }

    private async Task StartAsync()
    {
        if (Running || Done || Items.Count < 2) return;
        var folder = OutputFolder;
        var output = VideoMerger.UniqueOutput(folder, OutputName.Trim(), ".mp4");   // never overwrites an existing file
        _cts = new CancellationTokenSource();
        Running = true; Progress = 0; Status = "Joining the videos... this can take a while for large files.";
        var mode = ModeIndex == 1 ? MergeMode.Reencode : MergeMode.Auto;
        var result = await _merger.MergeAsync(Items.Select(i => i.File.Path).ToList(), output, mode, new Progress<double>(v => Progress = v), _cts.Token);
        Running = false;
        if (result.Cancelled) { Status = "Cancelled. Nothing was changed."; Progress = 0; return; }
        if (!result.Success) { Status = "The videos could not be merged: " + (result.Error ?? "unknown error"); Progress = 0; return; }
        OutputPath = result.Output; Done = true; OnPropertyChanged(nameof(CanEdit));
        Status = (result.Reencoded ? "Done (re-encoded so all videos match). " : "Done (no quality loss, the videos were joined as they are). ") + "Saved as " + Path.GetFileName(result.Output);
    }
}
