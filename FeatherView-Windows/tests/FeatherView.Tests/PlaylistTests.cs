using FeatherView.Core.Video;
using Xunit;

namespace FeatherView.Tests;

public class PlaylistTests
{
    private static Playlist Make(int n, RepeatMode r = RepeatMode.Off, bool shuffle = false, int? seed = 1)
    {
        var p = new Playlist(seed) { Repeat = r, Shuffle = shuffle };
        p.SetItems(Enumerable.Range(1, n).Select(i => $"/v/{i}.mp4"));
        return p;
    }

    [Fact]
    public void PlaysInOrderThenStops()
    {
        var p = Make(3);
        Assert.Equal("/v/2.mp4", p.Next(auto: true));
        Assert.Equal("/v/3.mp4", p.Next(auto: true));
        Assert.Null(p.Next(auto: true));
        Assert.Equal("/v/3.mp4", p.Current);
    }

    [Fact]
    public void RepeatAllWrapsAndRepeatOneReplays()
    {
        var all = Make(2, RepeatMode.All);
        all.Next(true); Assert.Equal("/v/1.mp4", all.Next(true));
        var one = Make(3, RepeatMode.One);
        Assert.Equal("/v/1.mp4", one.Next(auto: true));        // ended: same video again
        Assert.Equal("/v/2.mp4", one.Next(auto: false));       // user pressed Next: moves on
    }

    [Fact]
    public void ShuffleVisitsEveryVideoOncePerRound()
    {
        var p = Make(6, RepeatMode.Off, shuffle: true, seed: 7);
        var seen = new HashSet<string> { p.Current! };
        string? n;
        while ((n = p.Next(true)) != null) Assert.True(seen.Add(n));
        Assert.Equal(6, seen.Count);
    }

    [Fact]
    public void ShuffleWithRepeatAllKeepsGoing()
    {
        var p = Make(3, RepeatMode.All, shuffle: true, seed: 3);
        for (int i = 0; i < 20; i++) Assert.NotNull(p.Next(true));
    }

    [Fact]
    public void AddSkipsDuplicatesAndRemoveKeepsCurrent()
    {
        var p = Make(3);
        Assert.Equal(1, p.Add(new[] { "/v/2.mp4", "/v/9.mp4" }));
        Assert.Equal(4, p.Count);
        p.JumpTo(2);
        p.Remove(0);
        Assert.Equal("/v/3.mp4", p.Current);
        p.Remove(0 + p.Index);
        Assert.NotNull(p.Current);
        p.Clear(); Assert.Null(p.Current); Assert.Equal(0, p.Count);
    }

    [Fact]
    public void PreviousGoesBack()
    {
        var p = Make(3); p.Next(false); p.Next(false);
        Assert.Equal("/v/2.mp4", p.Previous());
        Assert.Equal("/v/1.mp4", p.Previous());
        Assert.Null(p.Previous());
    }
}
