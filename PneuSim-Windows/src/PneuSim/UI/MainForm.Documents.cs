using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Several circuits open at once, each in its own tab (like browser or spreadsheet tabs). Pages (sheets) of one circuit are tabs at the bottom.</summary>
public partial class MainForm
{
	private sealed class DocTab
	{
		public Project Project;

		public int PageIndex;

		public string FilePath;

		public bool Dirty;

		public List<(string Xml, int Page)> History = new List<(string, int)>();

		public int HistoryIndex;
	}

	private readonly List<DocTab> _docs = new List<DocTab>();

	private int _docIndex;

	private DocumentTabs _docTabs;

	private void BuildDocumentTabs()
	{
		_docTabs = new DocumentTabs();
		_docTabs.TabSelected += delegate(int i)
		{
			SwitchDocument(i);
		};
		_docTabs.TabClosed += delegate(int i)
		{
			CloseDocument(i);
		};
		_docTabs.NewRequested += delegate
		{
			NewProject(new Project(new Circuit()));
		};
	}

	private string DocTitle(Project p, string path, bool dirty)
	{
		string text = ((path == null) ? p.Info.Title : Path.GetFileNameWithoutExtension(path));
		return text + (dirty ? " *" : "");
	}

	private void RefreshDocTabs()
	{
		if (_docTabs != null && _docs.Count != 0 && _project != null)
		{
			List<string> list = new List<string>();
			for (int i = 0; i < _docs.Count; i++)
			{
				list.Add((i == _docIndex) ? DocTitle(_project, _filePath, _dirty) : DocTitle(_docs[i].Project, _docs[i].FilePath, _docs[i].Dirty));
			}
			_docTabs.SetTabs(list, _docIndex);
		}
	}

	private void SaveCurrentToTab()
	{
		if (_docs.Count != 0 && _docIndex >= 0 && _docIndex < _docs.Count)
		{
			DocTab docTab = _docs[_docIndex];
			docTab.Project = _project;
			docTab.PageIndex = _pageIndex;
			docTab.FilePath = _filePath;
			docTab.Dirty = _dirty;
			docTab.History = _history.ToList();
			docTab.HistoryIndex = _historyIndex;
		}
	}

	private void LoadTab(int index)
	{
		DocTab docTab = _docs[index];
		_docIndex = index;
		_project = docTab.Project;
		_pageIndex = docTab.PageIndex;
		_filePath = docTab.FilePath;
		_dirty = docTab.Dirty;
		_history.Clear();
		_history.AddRange(docTab.History);
		_historyIndex = docTab.HistoryIndex;
		_exercise = null;
		RefreshPageTabs();
		ShowPage(_pageIndex);
		UpdateTitle();
		UpdateUndoButtons();
		ScheduleCheck();
		UpdateUiState();
		RefreshDocTabs();
	}

	private void SwitchDocument(int index)
	{
		if (index >= 0 && index < _docs.Count && index != _docIndex)
		{
			StopSimulation();
			SaveCurrentToTab();
			HideStartScreen();
			LoadTab(index);
		}
	}

	private bool IsPristine()
	{
		return _filePath == null && !_dirty && _project != null && _project.Pages.Count == 1 && !_project.AllElements().Any();
	}

	/// <summary>Opens a project in a tab: a new tab, or the empty untitled tab if that is all there is, or the tab where the file is already open.</summary>
	private void NewProject(Project p, string path = null)
	{
		StopSimulation();
		if (path != null)
		{
			SaveCurrentToTab();
			int num = _docs.FindIndex((DocTab d) => d.FilePath != null && string.Equals(Path.GetFullPath(d.FilePath), Path.GetFullPath(path), StringComparison.OrdinalIgnoreCase));
			if (num >= 0)
			{
				SwitchDocument(num);
				_statusMessage.Text = "That file is already open.";
				return;
			}
		}
		if (_docs.Count == 0)
		{
			_docs.Add(new DocTab());
			_docIndex = 0;
		}
		else if (!IsPristine())
		{
			SaveCurrentToTab();
			_docs.Add(new DocTab());
			_docIndex = _docs.Count - 1;
		}
		ActivateProject(p, path);
		SaveCurrentToTab();
		RefreshDocTabs();
	}

	/// <summary>Closes a tab. Asks to save if it has unsaved changes. The last tab is replaced by an empty circuit.</summary>
	private void CloseDocument(int index)
	{
		if (index < 0 || index >= _docs.Count)
		{
			return;
		}
		if (index != _docIndex)
		{
			SwitchDocument(index);
		}
		if (ConfirmDiscard())
		{
			StopSimulation();
			_docs.RemoveAt(_docIndex);
			if (_docs.Count == 0)
			{
				_docs.Add(new DocTab());
				_docIndex = 0;
				ActivateProject(new Project(new Circuit()), null);
				SaveCurrentToTab();
				RefreshDocTabs();
				ShowStartScreen();
			}
			else
			{
				LoadTab(Math.Min(_docIndex, _docs.Count - 1));
			}
		}
	}

	private void OnClose(object sender, EventArgs e)
	{
		CloseDocument(_docIndex);
	}

	private void OnCloseAll(object sender, EventArgs e)
	{
		for (int num = _docs.Count - 1; num >= 0; num--)
		{
			int count = _docs.Count;
			CloseDocument(Math.Min(num, _docs.Count - 1));
			if (_docs.Count == count && count > 1)
			{
				break;
			}
		}
	}

	/// <summary>True if every tab with unsaved changes has been saved or discarded.</summary>
	private bool ConfirmCloseAll()
	{
		SaveCurrentToTab();
		for (int i = 0; i < _docs.Count; i++)
		{
			if (_docs[i].Dirty)
			{
				SwitchDocument(i);
				if (!ConfirmDiscard())
				{
					return false;
				}
			}
		}
		return true;
	}
}
