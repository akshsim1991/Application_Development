using System;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

public partial class MainForm
{
	private StartScreen _startScreen;

	/// <summary>Shows the start screen if nothing has been opened or drawn yet.</summary>
	public void ShowStartScreen()
	{
		if (_startScreen != null || _filePath != null || _dirty || _project.AllElements().Any())
		{
			return;
		}
		_startScreen = new StartScreen(Examples.All.Select(((string Name, Func<Circuit> Build) e) => e.Name), AppSettings.RecentFiles, AppSettings.DarkMode);
		_startScreen.NewRequested += delegate
		{
			NewProject(new Project(new Circuit()));
		};
		_startScreen.OpenRequested += delegate
		{
			OnOpen(null, EventArgs.Empty);
		};
		_startScreen.FileRequested += delegate(string path)
		{
			OpenFile(path);
		};
		_startScreen.ExampleRequested += delegate(string name)
		{
			var tuple = Examples.All.FirstOrDefault(((string Name, Func<Circuit> Build) e) => e.Name == name);
			if (tuple.Build != null)
			{
				OpenExample(tuple.Build);
			}
		};
		_diagramSplit.Panel1.Controls.Add(_startScreen);
		_startScreen.BringToFront();
	}

	private void HideStartScreen()
	{
		if (_startScreen != null)
		{
			StartScreen startScreen = _startScreen;
			_startScreen = null;
			startScreen.Parent?.Controls.Remove(startScreen);
			startScreen.Dispose();
		}
	}
}
