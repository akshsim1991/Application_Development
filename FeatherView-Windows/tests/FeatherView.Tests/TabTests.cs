using FeatherView.Core.Tabs;
using Xunit;

namespace FeatherView.Tests;

public class TabManagerTests
{
    private static TabState T(string folder) => new() { FolderPath = folder };

    [Fact] public void StartsWithOneEmptyTab()
    {
        var m = new TabManager(); Assert.Single(m.Tabs); Assert.Equal("New tab", TabManager.TitleOf(m.Active));
    }

    [Theory]
    [InlineData("C:\\Users\\me\\Pictures\\Holiday", "Holiday")] [InlineData("C:\\Users\\me\\Pictures\\Holiday\\", "Holiday")]
    [InlineData("C:\\", "C:")] [InlineData("/home/me/photos", "photos")] [InlineData("\\\\nas\\media\\trip", "trip")]
    public void TitleFromFolder(string path, string expected) => Assert.Equal(expected, TabManager.TitleOf(T(path)));

    [Fact] public void CustomTitleWinsAndCanBeCleared()
    {
        var m = new TabManager(); m.Active.FolderPath = "C:\\a\\b"; m.Rename(0, "  Trip  ");
        Assert.Equal("Trip", TabManager.TitleOf(m.Active)); m.Rename(0, " "); Assert.Equal("b", TabManager.TitleOf(m.Active));
        Assert.Equal("Favorites", TabManager.TitleOf(new TabState { Section = "Favorites" }));
    }

    [Fact] public void AddInsertsAfterActiveAndActivates()
    {
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Add(T("B")); m.Add(T("C"));
        Assert.Equal(new[] { "A", "B", "C" }, m.Tabs.Select(t => t.FolderPath)); Assert.Equal(2, m.ActiveIndex);
        m.Activate(0); m.Add(T("X")); Assert.Equal(new[] { "A", "X", "B", "C" }, m.Tabs.Select(t => t.FolderPath)); Assert.Equal(1, m.ActiveIndex);
    }

    [Fact] public void LimitIsEnforced()
    {
        var m = new TabManager(); for (int i = 0; i < 50; i++) m.Add(T("F" + i));
        Assert.Equal(TabManager.MaxTabs, m.Tabs.Count); Assert.Null(m.Add(T("more"))); Assert.Null(m.Duplicate(0));
    }

    [Fact] public void DuplicateCopiesStateWithNewId()
    {
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Active.SelectedPath = "A\\1.jpg"; m.Active.Filter = "Png"; m.Active.CustomTitle = "Mine";
        var d = m.Duplicate(0)!;
        Assert.NotEqual(m.Tabs[0].Id, d.Id); Assert.Equal("A\\1.jpg", d.SelectedPath); Assert.Equal("Png", d.Filter); Assert.Equal(1, m.ActiveIndex);
    }

    [Fact] public void CloseActivatesNeighbourAndAdjustsIndex()
    {
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Add(T("B")); m.Add(T("C")); m.Add(T("D"));   // A B C D, active D (3)
        m.Close(3); Assert.Equal(2, m.ActiveIndex); Assert.Equal("C", m.Active.FolderPath);
        m.Activate(2); m.Close(0); Assert.Equal(1, m.ActiveIndex); Assert.Equal("C", m.Active.FolderPath);   // closing before the active tab keeps it active
        m.Activate(0); m.Close(0); Assert.Equal("C", m.Active.FolderPath);                                      // closing the active one selects the next
    }

    [Fact] public void ClosingTheLastTabLeavesAnEmptyOne()
    {
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Close(0);
        Assert.Single(m.Tabs); Assert.Null(m.Active.FolderPath); Assert.True(m.CanReopen);
    }

    [Fact] public void ReopenRestoresAtOldPositionNewestFirst()
    {
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Add(T("B")); m.Add(T("C"));
        m.Close(1); m.Close(1);                                    // closes B then C
        Assert.Equal(new[] { "A" }, m.Tabs.Select(t => t.FolderPath));
        Assert.Equal("C", m.Reopen()!.FolderPath); Assert.Equal("B", m.Reopen()!.FolderPath);
        Assert.Equal(new[] { "A", "B", "C" }, m.Tabs.Select(t => t.FolderPath)); Assert.False(m.CanReopen); Assert.Null(m.Reopen());
    }

    [Fact] public void ReopenReplacesTheLoneEmptyTab()
    {
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Close(0); m.Reopen();
        Assert.Single(m.Tabs); Assert.Equal("A", m.Active.FolderPath);
    }

    [Fact] public void ClosedHistoryIsBoundedAndEmptyTabsAreNotRemembered()
    {
        var m = new TabManager();
        for (int i = 0; i < 30; i++) { m.Add(T("F" + i)); m.Close(m.ActiveIndex); }
        int n = 0; while (m.Reopen() != null) n++; Assert.Equal(TabManager.MaxClosed, n);
        var e = new TabManager(); e.Add(); e.Close(1); Assert.False(e.CanReopen);
    }

    [Fact] public void CloseOthersKeepsOnlyThatTab()
    {
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Add(T("B")); m.Add(T("C"));
        m.CloseOthers(1); Assert.Single(m.Tabs); Assert.Equal("B", m.Active.FolderPath);
    }

    [Fact] public void NextAndPreviousWrap()
    {
        var m = new TabManager(); m.Add(T("B")); m.Add(T("C"));   // active 2
        m.Next(); Assert.Equal(0, m.ActiveIndex); m.Previous(); Assert.Equal(2, m.ActiveIndex);
    }

    [Fact] public void SessionRoundTrip()
    {
        var path = Path.Combine(Path.GetTempPath(), "fv-sess-" + Guid.NewGuid().ToString("N") + ".json");
        var m = new TabManager(); m.Active.FolderPath = "A"; m.Active.Mode = "Photo"; m.Active.SelectedPath = "A\\x.jpg"; m.Add(T("B")); m.Rename(1, "Second");
        var store = new SessionStore(path); store.Save(m.ToData());
        var r = TabManager.FromData(store.Load());
        Assert.Equal(2, r.Tabs.Count); Assert.Equal(1, r.ActiveIndex); Assert.Equal("Second", TabManager.TitleOf(r.Tabs[1]));
        Assert.Equal("Photo", r.Tabs[0].Mode); Assert.Equal("A\\x.jpg", r.Tabs[0].SelectedPath);
        store.Delete(); Assert.Null(store.Load());
    }

    [Fact] public void DamagedOrMissingSessionIsHarmless()
    {
        var path = Path.Combine(Path.GetTempPath(), "fv-sess-" + Guid.NewGuid().ToString("N") + ".json");
        Assert.Null(new SessionStore(path).Load()); File.WriteAllText(path, "{ nope"); Assert.Null(new SessionStore(path).Load());
        Assert.Single(TabManager.FromData(null).Tabs); Assert.Single(TabManager.FromData(new TabSessionData()).Tabs);
        Assert.Equal(0, TabManager.FromData(new TabSessionData { Tabs = { new TabState() }, Active = 99 }).ActiveIndex);
        File.Delete(path);
    }
}
