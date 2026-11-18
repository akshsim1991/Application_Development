using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;
using Microsoft.VisualBasic;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class MainForm : Form
{
	private class DarkColorTable : ProfessionalColorTable
	{
		private static readonly Color Back = Color.FromArgb(37, 39, 46);

		private static readonly Color Hot = Color.FromArgb(62, 68, 82);

		private static readonly Color Edge = Color.FromArgb(70, 74, 86);

		public override Color MenuStripGradientBegin { get; }

		public override Color MenuStripGradientEnd { get; }

		public override Color ToolStripGradientBegin { get; }

		public override Color ToolStripGradientMiddle { get; }

		public override Color ToolStripGradientEnd { get; }

		public override Color ToolStripDropDownBackground { get; }

		public override Color ImageMarginGradientBegin { get; }

		public override Color ImageMarginGradientMiddle { get; }

		public override Color ImageMarginGradientEnd { get; }

		public override Color MenuItemSelected { get; }

		public override Color MenuItemSelectedGradientBegin { get; }

		public override Color MenuItemSelectedGradientEnd { get; }

		public override Color MenuItemPressedGradientBegin { get; }

		public override Color MenuItemPressedGradientEnd { get; }

		public override Color MenuItemBorder { get; }

		public override Color MenuBorder { get; }

		public override Color ButtonSelectedGradientBegin { get; }

		public override Color ButtonSelectedGradientEnd { get; }

		public override Color ButtonCheckedGradientBegin { get; }

		public override Color ButtonCheckedGradientEnd { get; }

		public override Color ButtonSelectedBorder { get; }

		public override Color SeparatorDark { get; }

		public override Color SeparatorLight { get; }

		public override Color StatusStripGradientBegin { get; }

		public override Color StatusStripGradientEnd { get; }

		public override Color ToolStripBorder { get; }

		public DarkColorTable()
		{
			MenuStripGradientBegin = Back;
			MenuStripGradientEnd = Back;
			ToolStripGradientBegin = Back;
			ToolStripGradientMiddle = Back;
			ToolStripGradientEnd = Back;
			ToolStripDropDownBackground = Back;
			ImageMarginGradientBegin = Back;
			ImageMarginGradientMiddle = Back;
			ImageMarginGradientEnd = Back;
			MenuItemSelected = Hot;
			MenuItemSelectedGradientBegin = Hot;
			MenuItemSelectedGradientEnd = Hot;
			MenuItemPressedGradientBegin = Hot;
			MenuItemPressedGradientEnd = Hot;
			MenuItemBorder = Edge;
			MenuBorder = Edge;
			ButtonSelectedGradientBegin = Hot;
			ButtonSelectedGradientEnd = Hot;
			ButtonCheckedGradientBegin = Hot;
			ButtonCheckedGradientEnd = Hot;
			ButtonSelectedBorder = Edge;
			SeparatorDark = Edge;
			SeparatorLight = Back;
			StatusStripGradientBegin = Back;
			StatusStripGradientEnd = Back;
			ToolStripBorder = Back;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__125_002D0
	{
		public object _0024VB_0024Local_target;

		public Func<CircuitElement, bool> _0024I0;

		public Func<Tube, bool> _0024I1;

		public _Closure_0024__125_002D0(_Closure_0024__125_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_target = arg0._0024VB_0024Local_target;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(CircuitElement x)
		{
			return x != _0024VB_0024Local_target;
		}

		[SpecialName]
		internal bool _Lambda_0024__1(Tube x)
		{
			return x != _0024VB_0024Local_target;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__168_002D0
	{
		public SplitContainer _0024VB_0024Local_mainSplit;

		public SplitContainer _0024VB_0024Local_rightSplit;

		public MainForm _0024VB_0024Me;

		public _Closure_0024__168_002D0(_Closure_0024__168_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_mainSplit = arg0._0024VB_0024Local_mainSplit;
				_0024VB_0024Local_rightSplit = arg0._0024VB_0024Local_rightSplit;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__R27(object a0, EventArgs a1)
		{
			_Lambda_0024__6();
		}

		[SpecialName]
		internal void _Lambda_0024__6()
		{
			_0024VB_0024Local_mainSplit.SplitterDistance = 270;
			checked
			{
				_0024VB_0024Local_rightSplit.SplitterDistance = Math.Max(200, _0024VB_0024Local_rightSplit.Width - 290);
				_0024VB_0024Me._diagramSplit.SplitterDistance = Math.Max(200, _0024VB_0024Me._diagramSplit.Height - 230);
			}
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__172_002D0
	{
		public ToolStripMenuItem _0024VB_0024Local_tools;

		public MainForm _0024VB_0024Me;

		public _Closure_0024__172_002D0(_Closure_0024__172_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_tools = arg0._0024VB_0024Local_tools;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__R47(object a0, EventArgs a1)
		{
			_Lambda_0024__19();
		}

		[SpecialName]
		internal void _Lambda_0024__19()
		{
			_0024VB_0024Me._diagramSplit.Panel2Collapsed = !_0024VB_0024Local_tools.Checked;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__172_002D1
	{
		public Func<Circuit> _0024VB_0024Local_build;

		public _Closure_0024__172_002D0 _0024VB_0024NonLocal__0024VB_0024Closure_2;

		public _Closure_0024__172_002D1(_Closure_0024__172_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_build = arg0._0024VB_0024Local_build;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__R56(object a0, EventArgs a1)
		{
			_Lambda_0024__28();
		}

		[SpecialName]
		internal void _Lambda_0024__28()
		{
			_0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Me.OpenExample(_0024VB_0024Local_build);
		}
	}

	private Project _project;

	private int _pageIndex;

	private bool _switchingPages;

	private string _filePath;

	private bool _dirty;

	private readonly List<(string Xml, int Page)> _history;

	private int _historyIndex;

	private const int MaxHistory = 100;

	private ToolStripMenuItem _menuUndo;

	private ToolStripMenuItem _menuRedo;

	private ToolStripButton _btnUndo;

	private ToolStripButton _btnRedo;

	private readonly Timer _autosaveTimer;

	private const int TickMs = 20;

	private const int SubSteps = 4;

	private readonly Timer _timer;

	private Simulator _simulator;

	private bool _running;

	private bool _paused;

	private bool _realPhysics;

	private int _tickCount;

	private GifRecorder _gif;

	private RectangleF _gifBounds;

	private Circuit _gifPage;

	private readonly CircuitCanvas _gifCanvas;

	private readonly ListView _checkList;

	private readonly Timer _checkTimer;

	private readonly Button _fixCascade;

	private readonly Button _fixElectro;

	private readonly TextBox _explainText;

	private readonly ComboBox _explainOperate;

	private List<CheckIssue> _issues;

	private CutawayWindow _cutaway;

	private readonly InspectorPanel _inspector;

	private readonly TroubleshootPanel _trouble;

	private TroubleshootExercise _exercise;

	private SoundMonitor _sounds = new SoundMonitor();

	private readonly Random _rnd;

	private SimulationRecorder _recorder;

	private ToolStripButton _btnBack;

	private ToolStripButton _btnStepFwd;

	private ToolStripButton _btnPrevEvent;

	private ToolStripButton _btnNextEvent;

	private TrackBar _timeline;

	private ToolStripControlHost _timelineHost;

	private bool _updatingTimeline;

	private const string AppName = "PneuSim";

	private const string AppVersion = "3.2";

	private const string FileFilter = "PneuSim projects (*.pneu)|*.pneu|All files (*.*)|*.*";

	private readonly CircuitCanvas _canvas;

	private readonly TabControl _pageTabs;

	private readonly DiagramPanel _diagram;

	private readonly PlotterPanel _plotter;

	private readonly LessonsPanel _lessons;

	private readonly TabControl _bottomTabs;

	private readonly ListView _library;

	private readonly PropertyGrid _properties;

	private readonly ToolTip _tooltip;

	private readonly ToolStripStatusLabel _statusMode;

	private readonly ToolStripStatusLabel _statusMessage;

	private readonly ToolStripStatusLabel _statusZoom;

	private StatusStrip _status;

	private MenuStrip _menu;

	private ToolStrip _toolbar;

	private ToolStripButton _btnStart;

	private ToolStripButton _btnPause;

	private ToolStripButton _btnStop;

	private ToolStripButton _btnReal;

	private ToolStripButton _btnRecord;

	private ToolStripMenuItem _menuStart;

	private ToolStripMenuItem _menuPause;

	private ToolStripMenuItem _menuStop;

	private ToolStripMenuItem _menuReal;

	private ToolStripMenuItem _menuDark;

	private ToolStripMenuItem _menuRecord;

	private ToolStripComboBox _speedBox;

	private SplitContainer _diagramSplit;

	private Circuit CurrentCircuit => _project.Pages[_pageIndex].Circuit;

	private static string AutosavePath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "PneuSim", "autosave.pneu");

	private void NewProject(Project p, string path = null)
	{
		StopSimulation();
		_project = p;
		_pageIndex = 0;
		_filePath = path;
		_dirty = false;
		_history.Clear();
		_history.Add((p.ToXml(), 0));
		_historyIndex = 0;
		RefreshPageTabs();
		ShowPage(0);
		UpdateTitle();
		UpdateUndoButtons();
		ScheduleCheck();
		CheckForHiddenFault();
	}

	private void RefreshPageTabs()
	{
		_switchingPages = true;
		_pageTabs.TabPages.Clear();
		foreach (ProjectPage page in _project.Pages)
		{
			_pageTabs.TabPages.Add(new TabPage(page.Name));
		}
		_pageTabs.SelectedIndex = Math.Min(_pageIndex, checked(_project.Pages.Count - 1));
		_switchingPages = false;
	}

	private void ShowPage(int index, bool keepView = false)
	{
		if (index >= 0 && index < _project.Pages.Count)
		{
			_pageIndex = index;
			if (_pageTabs.SelectedIndex != index)
			{
				_switchingPages = true;
				_pageTabs.SelectedIndex = index;
				_switchingPages = false;
			}
			_project.UpdateCrossReferences();
			_canvas.Circuit = CurrentCircuit;
			_canvas.Simulator = _simulator;
			_properties.SelectedObject = null;
			if (!keepView)
			{
				_canvas.ScrollToCircuit();
			}
			_canvas.Invalidate();
		}
	}

	private void AddPage()
	{
		if (!_running)
		{
			_project.AddPage();
			RefreshPageTabs();
			ShowPage(checked(_project.Pages.Count - 1));
			OnCircuitModified();
		}
	}

	private void RenamePage()
	{
		if (!_running)
		{
			string text = Interaction.InputBox("Page name:", "PneuSim", _project.Pages[_pageIndex].Name);
			if (!string.IsNullOrWhiteSpace(text))
			{
				_project.Pages[_pageIndex].Name = text.Trim();
				RefreshPageTabs();
				OnCircuitModified();
			}
		}
	}

	private void DeletePage()
	{
		if (!_running && _project.Pages.Count > 1 && (CurrentCircuit.Elements.Count <= 0 || MessageBox.Show($"Delete page '{_project.Pages[_pageIndex].Name}' and everything on it?", "PneuSim", MessageBoxButtons.YesNo, MessageBoxIcon.Exclamation) == DialogResult.Yes))
		{
			_project.Pages.RemoveAt(_pageIndex);
			_pageIndex = Math.Max(0, checked(_pageIndex - 1));
			RefreshPageTabs();
			ShowPage(_pageIndex);
			OnCircuitModified();
		}
	}

	private void OnCircuitModified()
	{
		checked
		{
			if (_historyIndex < _history.Count - 1)
			{
				_history.RemoveRange(_historyIndex + 1, _history.Count - _historyIndex - 1);
			}
			_history.Add((_project.ToXml(), _pageIndex));
			if (_history.Count > 100)
			{
				_history.RemoveAt(0);
			}
			_historyIndex = _history.Count - 1;
			_project.UpdateCrossReferences();
			MarkDirty();
			UpdateUndoButtons();
			ScheduleCheck();
		}
	}

	private void Undo()
	{
		checked
		{
			if (_running)
			{
				_statusMessage.Text = "Stop the simulation (F11) to undo changes.";
			}
			else if (_historyIndex > 0)
			{
				_historyIndex--;
				RestoreSnapshot();
			}
		}
	}

	private void Redo()
	{
		checked
		{
			if (_running)
			{
				_statusMessage.Text = "Stop the simulation (F11) to redo changes.";
			}
			else if (_historyIndex < _history.Count - 1)
			{
				_historyIndex++;
				RestoreSnapshot();
			}
		}
	}

	private void RestoreSnapshot()
	{
		(string, int) tuple = _history[_historyIndex];
		bool flag = tuple.Item2 == _pageIndex;
		Point autoScrollPosition = _canvas.AutoScrollPosition;
		_project = Project.FromXml(tuple.Item1);
		checked
		{
			_pageIndex = Math.Min(tuple.Item2, _project.Pages.Count - 1);
			RefreshPageTabs();
			ShowPage(_pageIndex, flag);
			if (flag)
			{
				_canvas.AutoScrollPosition = new Point(-autoScrollPosition.X, -autoScrollPosition.Y);
			}
			MarkDirty();
			UpdateUndoButtons();
			ScheduleCheck();
		}
	}

	private void UpdateUndoButtons()
	{
		if (_btnUndo != null)
		{
			bool enabled = !_running && _historyIndex > 0;
			bool enabled2 = !_running && _historyIndex < checked(_history.Count - 1);
			_btnUndo.Enabled = enabled;
			_menuUndo.Enabled = enabled;
			_btnRedo.Enabled = enabled2;
			_menuRedo.Enabled = enabled2;
		}
	}

	private void MarkDirty()
	{
		_dirty = true;
		UpdateTitle();
	}

	private void UpdateTitle()
	{
		string text = ((_filePath == null) ? _project.Info.Title : Path.GetFileNameWithoutExtension(_filePath));
		Text = string.Format("{0}{1} - {2} {3}", text, _dirty ? " *" : "", "PneuSim", "3.2");
	}

	private bool ConfirmDiscard()
	{
		if (!_dirty)
		{
			return true;
		}
		return MessageBox.Show("Save changes to the current project?", "PneuSim", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question) switch
		{
			DialogResult.Yes => SaveProject(_filePath), 
			DialogResult.No => true, 
			_ => false, 
		};
	}

	private void OnNew(object sender, EventArgs e)
	{
		if (ConfirmDiscard())
		{
			NewProject(new Project(new Circuit()));
		}
	}

	private void OnOpen(object sender, EventArgs e)
	{
		if (!ConfirmDiscard())
		{
			return;
		}
		using OpenFileDialog openFileDialog = new OpenFileDialog
		{
			Filter = "PneuSim projects (*.pneu)|*.pneu|All files (*.*)|*.*"
		};
		if (openFileDialog.ShowDialog(this) != DialogResult.OK)
		{
			return;
		}
		try
		{
			NewProject(Project.Load(openFileDialog.FileName), openFileDialog.FileName);
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			MessageBox.Show(string.Format("Could not open the file:{0}{1}", "\r\n", ex2.Message), "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			ProjectData.ClearProjectError();
		}
	}

	public void OpenFile(string path)
	{
		try
		{
			NewProject(Project.Load(path), path);
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			MessageBox.Show(string.Format("Could not open the file:{0}{1}", "\r\n", ex2.Message), "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			ProjectData.ClearProjectError();
		}
	}

	private void OnSave(object sender, EventArgs e)
	{
		SaveProject(_filePath);
	}

	private void OnSaveAs(object sender, EventArgs e)
	{
		SaveProject(null);
	}

	private bool SaveProject(string path)
	{
		bool result;
		if (path == null)
		{
			using SaveFileDialog saveFileDialog = new SaveFileDialog
			{
				Filter = "PneuSim projects (*.pneu)|*.pneu|All files (*.*)|*.*",
				DefaultExt = "pneu",
				FileName = SafeName(_project.Info.Title) + ".pneu"
			};
			if (saveFileDialog.ShowDialog(this) != DialogResult.OK)
			{
				result = false;
				goto IL_00d7;
			}
			path = saveFileDialog.FileName;
		}
		try
		{
			_project.Save(path);
			_filePath = path;
			_dirty = false;
			UpdateTitle();
			_statusMessage.Text = $"Saved {path}";
			result = true;
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			MessageBox.Show(string.Format("Could not save the file:{0}{1}", "\r\n", ex2.Message), "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			result = false;
			ProjectData.ClearProjectError();
		}
		goto IL_00d7;
		IL_00d7:
		return result;
	}

	private static string SafeName(string s)
	{
		char[] invalidFileNameChars = Path.GetInvalidFileNameChars();
		string text = new string((s ?? "circuit").Select([SpecialName] (char c) => (!invalidFileNameChars.Contains(c)) ? c : '_').ToArray()).Trim();
		if (Operators.CompareString(text, "", TextCompare: false) != 0)
		{
			return text;
		}
		return "circuit";
	}

	private void OnProjectInfo(object sender, EventArgs e)
	{
		string right = _project.ToXml();
		Form form = new Form
		{
			Text = "Project information",
			Size = new Size(520, 480),
			StartPosition = FormStartPosition.CenterParent,
			ShowInTaskbar = false,
			Font = Font,
			KeyPreview = true
		};
		try
		{
			PropertyGrid value = new PropertyGrid
			{
				Dock = DockStyle.Fill,
				SelectedObject = _project.Info,
				ToolbarVisible = false
			};
			form.Controls.Add(value);
			form.KeyDown += [SpecialName] (object s2, KeyEventArgs k) =>
			{
				if (k.KeyCode == Keys.Escape)
				{
					form.Close();
				}
			};
			Theme.Apply(form);
			form.ShowDialog(this);
		}
		finally
		{
			if (form != null)
			{
				((IDisposable)form).Dispose();
			}
		}
		if (Operators.CompareString(_project.ToXml(), right, TextCompare: false) != 0)
		{
			OnCircuitModified();
		}
	}

	private string AskPath(string filter, string ext, string name)
	{
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = filter,
			DefaultExt = ext,
			FileName = SafeName(name) + "." + ext
		};
		return (saveFileDialog.ShowDialog(this) == DialogResult.OK) ? saveFileDialog.FileName : null;
	}

	private void OnExportImage(object sender, EventArgs e)
	{
		if (CurrentCircuit.Elements.Count == 0)
		{
			return;
		}
		string text = AskPath("PNG image (*.png)|*.png", "png", _project.Pages[_pageIndex].Name);
		if (text != null)
		{
			using (Bitmap bitmap = RenderCircuitImage(CurrentCircuit, _running))
			{
				bitmap.Save(text, ImageFormat.Png);
			}
			_statusMessage.Text = $"Exported {text}";
		}
	}

	private void OnExportSvg(object sender, EventArgs e)
	{
		if (CurrentCircuit.Elements.Count != 0)
		{
			string text = AskPath("SVG drawing (*.svg)|*.svg", "svg", _project.Pages[_pageIndex].Name);
			if (text != null)
			{
				VectorExport.SaveSvg(CurrentCircuit, text);
				_statusMessage.Text = $"Exported {text}";
			}
		}
	}

	private void OnExportDxf(object sender, EventArgs e)
	{
		if (CurrentCircuit.Elements.Count != 0)
		{
			string text = AskPath("AutoCAD DXF (*.dxf)|*.dxf", "dxf", _project.Pages[_pageIndex].Name);
			if (text != null)
			{
				VectorExport.SaveDxf(CurrentCircuit, text);
				_statusMessage.Text = $"Exported {text} (units: millimetres)";
			}
		}
	}

	private void OnExportPdf(object sender, EventArgs e)
	{
		string text = AskPath("PDF document (*.pdf)|*.pdf", "pdf", _project.Info.Title);
		if (text == null)
		{
			return;
		}
		string text2 = "";
		try
		{
			text2 = CircuitAnalysis.Explain(_project.SimulationCircuit(), null, _realPhysics);
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			text2 = "";
			ProjectData.ClearProjectError();
		}
		try
		{
			Reports.SavePdfReport(_project, text, includeParts: true, text2);
			_statusMessage.Text = $"Exported {text}";
			try
			{
				Process.Start(new ProcessStartInfo(text)
				{
					UseShellExecute = true
				});
			}
			catch (Exception ex3)
			{
				ProjectData.SetProjectError(ex3);
				Exception ex4 = ex3;
				ProjectData.ClearProjectError();
			}
		}
		catch (Exception ex5)
		{
			ProjectData.SetProjectError(ex5);
			Exception ex6 = ex5;
			MessageBox.Show("Could not write the PDF: " + ex6.Message, "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			ProjectData.ClearProjectError();
		}
	}

	public static Bitmap RenderCircuitImage(Circuit c, bool simulating)
	{
		RectangleF rectangleF = c.Bounds();
		rectangleF.Inflate(30f, 30f);
		rectangleF = RectangleF.FromLTRB(rectangleF.Left, rectangleF.Top - 30f, rectangleF.Right, rectangleF.Bottom);
		Bitmap bitmap = checked(new Bitmap(Math.Max(1, (int)Math.Round(rectangleF.Width)), Math.Max(1, (int)Math.Round(rectangleF.Height))));
		using Graphics graphics = Graphics.FromImage(bitmap);
		graphics.Clear(Color.White);
		graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
		graphics.TranslateTransform(0f - rectangleF.Left, 0f - rectangleF.Top);
		using CircuitCanvas circuitCanvas = new CircuitCanvas
		{
			Circuit = c,
			Simulating = simulating,
			ShowColumns = true
		};
		circuitCanvas.PaintTo(graphics);
		return bitmap;
	}

	private void OpenExample(Func<Circuit> build)
	{
		if (ConfirmDiscard())
		{
			NewProject(new Project(build()));
			_statusMessage.Text = "Example loaded. Press Start (F9), then click the push buttons and switches.";
		}
	}

	private void LoadLessonCircuit(Circuit c)
	{
		if (ConfirmDiscard())
		{
			NewProject(new Project(c, "Lesson"));
			FitView();
			_statusMessage.Text = "Tutorial parts loaded. Connect them, then press 'Check my solution'.";
		}
	}

	protected override void OnFormClosing(FormClosingEventArgs e)
	{
		if (!ConfirmDiscard())
		{
			e.Cancel = true;
		}
		if (!e.Cancel)
		{
			_sounds.Stop();
			DeleteAutosave();
		}
		base.OnFormClosing(e);
	}

	private void StartAutosave()
	{
		_autosaveTimer.Tick += [SpecialName] (object a0, EventArgs a1) =>
		{
			Autosave();
		};
		_autosaveTimer.Start();
	}

	private void Autosave()
	{
		if (_dirty)
		{
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(AutosavePath));
				_project.Save(AutosavePath);
				File.WriteAllText(AutosavePath + ".source", _filePath ?? "");
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				ProjectData.ClearProjectError();
			}
		}
	}

	private void DeleteAutosave()
	{
		try
		{
			if (File.Exists(AutosavePath))
			{
				File.Delete(AutosavePath);
			}
			if (File.Exists(AutosavePath + ".source"))
			{
				File.Delete(AutosavePath + ".source");
			}
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ProjectData.ClearProjectError();
		}
	}

	public void OfferRecovery()
	{
		if (!File.Exists(AutosavePath))
		{
			return;
		}
		if (MessageBox.Show("PneuSim was not closed normally last time. Restore the unsaved work from " + $"{File.GetLastWriteTime(AutosavePath):dd-MM-yyyy HH:mm}?", "PneuSim", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
		{
			try
			{
				string text = (File.Exists(AutosavePath + ".source") ? File.ReadAllText(AutosavePath + ".source").Trim() : "");
				NewProject(Project.Load(AutosavePath), (Operators.CompareString(text, "", TextCompare: false) == 0) ? null : text);
				_dirty = true;
				UpdateTitle();
				_statusMessage.Text = "Recovered unsaved work. Save it to keep it.";
				return;
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				MessageBox.Show("The recovery file could not be read: " + ex2.Message, "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
				ProjectData.ClearProjectError();
			}
		}
		DeleteAutosave();
	}

	public void RecoverFromError()
	{
		try
		{
			StopSimulation();
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			_running = false;
			_timer.Stop();
			ProjectData.ClearProjectError();
		}
		_dirty = true;
		Autosave();
	}

	private void OnPropertyValueChanged(object s, PropertyValueChangedEventArgs e)
	{
		CurrentCircuit.CleanupTubes();
		_properties.Refresh();
		_canvas.Invalidate();
		OnCircuitModified();
	}

	private void OnStart(object sender, EventArgs e)
	{
		if (_running && !_paused)
		{
			return;
		}
		if (!_running)
		{
			if (_project.AllElements().Count() == 0)
			{
				_statusMessage.Text = "Place some components first, or load one of the examples.";
				return;
			}
			_running = true;
			_canvas.PlacingPreset = null;
			_canvas.ClearSelection();
			_project.UpdateCrossReferences();
			_simulator = new Simulator(_project.SimulationCircuit())
			{
				RealPhysics = _realPhysics
			};
			_canvas.Simulator = _simulator;
			_canvas.Simulating = true;
			_simulator.Reset();
			_recorder = new SimulationRecorder(_simulator);
			_recorder.Record(force: true);
			_diagram.Simulator = _simulator;
			_plotter.Simulator = _simulator;
			_statusMessage.Text = "Click push buttons and selector switches to operate the valves.";
		}
		_recorder?.TruncateAfterCurrent();
		_paused = false;
		_timer.Start();
		UpdateUiState();
		_canvas.Invalidate();
	}

	private void OnPause(object sender, EventArgs e)
	{
		if (_running)
		{
			_paused = !_paused;
			if (_paused)
			{
				_timer.Stop();
			}
			else
			{
				_timer.Start();
			}
			UpdateUiState();
		}
	}

	private void OnStop(object sender, EventArgs e)
	{
		StopSimulation();
	}

	private void StopSimulation()
	{
		_timer.Stop();
		if (_gif != null)
		{
			FinishRecording();
		}
		if (!_running)
		{
			return;
		}
		_running = false;
		_paused = false;
		_sounds.Stop();
		_canvas.AlarmElements = null;
		foreach (CircuitElement item in _project.AllElements())
		{
			item.ResetSim();
		}
		_canvas.Simulating = false;
		_canvas.PickingTarget = false;
		_recorder = null;
		ClearInspector();
		_canvas.Invalidate();
		_diagram.Invalidate();
		_plotter.RefreshPlot();
		_statusMessage.Text = "";
		_statusMessage.ForeColor = SystemColors.ControlText;
		UpdateUiState();
	}

	private void OnToggleReal(object sender, EventArgs e)
	{
		_realPhysics = !_realPhysics;
		AppSettings.RealPhysics = _realPhysics;
		AppSettings.Save();
		if (_running)
		{
			StopSimulation();
			OnStart(null, EventArgs.Empty);
		}
		_statusMessage.Text = (_realPhysics ? "Realistic physics: pressures build up, cylinders move according to bore, load and friction, air consumption is measured." : "Ideal mode: pressures switch instantly and cylinders move at their set stroke time.");
		UpdateUiState();
	}

	private void OnTick(object sender, EventArgs e)
	{
		try
		{
			SimulationTick();
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			StopSimulation();
			_statusMessage.Text = "The simulation stopped because of an error: " + ex2.Message;
			_statusMessage.ForeColor = Color.DarkRed;
			ProjectData.ClearProjectError();
		}
	}

	private void SimulationTick()
	{
		double num = (new double[6] { 0.1, 0.25, 0.5, 1.0, 2.0, 4.0 })[Math.Max(0, _speedBox.SelectedIndex)];
		double dt = 0.02 * num / 4.0;
		int num2 = 1;
		checked
		{
			do
			{
				_simulator.Step(dt);
				num2++;
			}
			while (num2 <= 4);
			_recorder?.Record();
			_tickCount++;
			UpdateSoundsAndAlarm();
			ShowSimulationStatus();
			_canvas.Invalidate();
			_diagram.Invalidate();
		}
		if (_tickCount % 5 == 0)
		{
			_plotter.RefreshPlot();
			_inspector.RefreshValues();
			UpdateReplayUi();
		}
		if (_tickCount % 25 == 0 && _exercise != null)
		{
			_trouble.ShowExercise(_exercise);
		}
		if (_gif != null && _tickCount % 3 == 0)
		{
			CaptureFrame();
		}
	}

	private void UpdateSoundsAndAlarm()
	{
		_sounds.Update(_simulator.Circuit);
		string text = _sounds.WarningText();
		bool flag = text != null;
		_canvas.AlarmElements = (flag ? _sounds.OverRated.Select((KeyValuePair<CircuitElement, double> o) => o.Key).ToList() : null);
		_alarmShown = flag;
		if (flag)
		{
			_statusMessage.Text = text;
			_statusMessage.ForeColor = ((_tickCount / 12 % 2 == 0) ? Color.Firebrick : (AppSettings.DarkMode ? Color.Gainsboro : SystemColors.ControlText));
		}
		else if (_alarmWasShown)
		{
			_statusMessage.ForeColor = (AppSettings.DarkMode ? Color.Gainsboro : SystemColors.ControlText);
		}
		_alarmWasShown = flag;
	}

	private bool _alarmShown;

	private bool _alarmWasShown;

	private void OnElementOperated(object sender, EventArgs e)
	{
		if (_running)
		{
			_simulator.RunLogic();
			if (_paused)
			{
				_recorder?.Record(force: true);
				UpdateReplayUi();
			}
			_inspector.RefreshValues();
			ShowSimulationStatus();
			_canvas.Invalidate();
		}
	}

	private void ShowSimulationStatus()
	{
		_statusMode.Text = string.Format("Simulating{0}   t = {1:0.00} s{2}", _realPhysics ? " (realistic)" : "", _simulator.Time, _paused ? "  (paused)" : "") + ((_gif != null) ? $"   ● REC {_gif.FrameCount}" : "");
		if (_simulator.Warnings.Count > 0)
		{
			_statusMessage.Text = "Warning: " + _simulator.Warnings[0];
			_statusMessage.ForeColor = Color.DarkRed;
			return;
		}
		if (_statusMessage.ForeColor == Color.DarkRed)
		{
			_statusMessage.ForeColor = (AppSettings.DarkMode ? Color.Gainsboro : SystemColors.ControlText);
		}
		string text = _simulator.CostSummary(_project.Info);
		if (Operators.CompareString(text, "", TextCompare: false) != 0)
		{
			_statusMessage.Text = text;
		}
	}

	private void OnToggleRecord(object sender, EventArgs e)
	{
		if (_gif != null)
		{
			FinishRecording();
		}
		else if (CurrentCircuit.Elements.Count != 0)
		{
			_gifPage = CurrentCircuit;
			_gifBounds = _gifPage.Bounds();
			_gifBounds.Inflate(30f, 30f);
			float num = Math.Min(1f, 900f / Math.Max(1f, _gifBounds.Width));
			_gif = checked(new GifRecorder((int)Math.Round(_gifBounds.Width * num), (int)Math.Round(_gifBounds.Height * num), 6));
			_statusMessage.Text = "Recording: start the simulation and operate the circuit. Click 'Record GIF' again to save.";
			if (!_running)
			{
				OnStart(null, EventArgs.Empty);
			}
			UpdateUiState();
		}
	}

	private void CaptureFrame()
	{
		using Bitmap bitmap = checked(new Bitmap((int)Math.Round(_gifBounds.Width), (int)Math.Round(_gifBounds.Height)));
		using (Graphics graphics = Graphics.FromImage(bitmap))
		{
			graphics.Clear(Color.White);
			graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
			graphics.TranslateTransform(0f - _gifBounds.Left, 0f - _gifBounds.Top);
			_gifCanvas.Circuit = _gifPage;
			_gifCanvas.Simulating = true;
			_gifCanvas.PaintTo(graphics);
		}
		if (!_gif.AddFrame(bitmap))
		{
			FinishRecording();
		}
	}

	private void FinishRecording()
	{
		GifRecorder gif = _gif;
		_gif = null;
		UpdateUiState();
		if (gif == null || gif.FrameCount == 0)
		{
			return;
		}
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "Animated GIF (*.gif)|*.gif",
			FileName = SafeName(_project.Info.Title) + ".gif"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			gif.Save(saveFileDialog.FileName);
			_statusMessage.Text = $"Saved {gif.FrameCount} frames to {saveFileDialog.FileName}";
		}
	}

	private Control BuildCheckPanel()
	{
		Panel panel = new Panel();
		_checkList.Columns.Add("", 70);
		_checkList.Columns.Add("Problem", 900);
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 32,
			WrapContents = false
		};
		Button button = new Button
		{
			Text = "Run full check (with a test run)",
			AutoSize = true
		};
		button.Click += OnFullCheck;
		flowLayoutPanel.Controls.AddRange(new Control[3] { button, _fixCascade, _fixElectro });
		panel.Controls.Add(_checkList);
		panel.Controls.Add(flowLayoutPanel);
		_checkTimer.Tick += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__86_002D0();
		};
		_checkList.SelectedIndexChanged += OnIssueSelected;
		_checkList.DoubleClick += OnIssueActivated;
		_fixCascade.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__86_002D1();
		};
		_fixElectro.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__86_002D2();
		};
		return panel;
	}

	private void ScheduleCheck()
	{
		_checkTimer.Stop();
		_checkTimer.Start();
	}

	private void RunStaticChecks()
	{
		if (_project == null)
		{
			return;
		}
		try
		{
			List<CheckIssue> list = CircuitAnalysis.StaticChecks(_project.SimulationCircuit());
			if (list.Count == 0 && _project.AllElements().Any())
			{
				list.Add(new CheckIssue
				{
					Severity = IssueSeverity.Info,
					Message = "No problems found while drawing. Click 'Run full check' to test-run the circuit as well."
				});
			}
			ShowIssues(list);
			FillOperateList();
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ShowIssues(new List<CheckIssue>
			{
				new CheckIssue
				{
					Severity = IssueSeverity.Info,
					Message = "The checker could not analyse this circuit: " + ex2.Message
				}
			});
			ProjectData.ClearProjectError();
		}
	}

	private void OnFullCheck(object sender, EventArgs e)
	{
		Cursor = Cursors.WaitCursor;
		try
		{
			Circuit c = _project.SimulationCircuit();
			List<CheckIssue> list = CircuitAnalysis.StaticChecks(c);
			if (!list.Any([SpecialName] (CheckIssue i) => i.Severity == IssueSeverity.Error && i.Message.Contains("is empty")))
			{
				list.AddRange(CircuitAnalysis.DynamicCheck(c, _realPhysics));
			}
			if (list.Count == 0)
			{
				list.Add(new CheckIssue
				{
					Severity = IssueSeverity.Info,
					Message = "No problems found. The test run completed without warnings."
				});
			}
			ShowIssues(list);
			ShowBottomTab("Check circuit");
		}
		finally
		{
			Cursor = Cursors.Default;
		}
	}

	private void ShowIssues(List<CheckIssue> issues)
	{
		_issues = issues.OrderBy([SpecialName] (CheckIssue i) => i.Severity).ToList();
		_checkList.BeginUpdate();
		_checkList.Items.Clear();
		foreach (CheckIssue issue in _issues)
		{
			ListViewItem listViewItem = new ListViewItem(new string[2]
			{
				issue.Severity.ToString(),
				issue.Message
			})
			{
				Tag = issue
			};
			listViewItem.ForeColor = ((issue.Severity == IssueSeverity.Error) ? Color.Firebrick : ((issue.Severity == IssueSeverity.Warning) ? Color.DarkOrange : (AppSettings.DarkMode ? Color.Gainsboro : Color.DimGray)));
			_checkList.Items.Add(listViewItem);
		}
		_checkList.EndUpdate();
		int num = _issues.Where([SpecialName] (CheckIssue i) => i.Severity == IssueSeverity.Error).Count();
		foreach (TabPage tabPage in _bottomTabs.TabPages)
		{
			if (tabPage.Text.StartsWith("Check circuit"))
			{
				tabPage.Text = "Check circuit" + ((num > 0) ? $" ({num})" : "");
			}
		}
		OnIssueSelected(null, EventArgs.Empty);
	}

	private CheckIssue SelectedIssue()
	{
		if (_checkList.SelectedItems.Count != 0)
		{
			return _checkList.SelectedItems[0].Tag as CheckIssue;
		}
		return null;
	}

	private void OnIssueSelected(object sender, EventArgs e)
	{
		CheckIssue checkIssue = SelectedIssue();
		_fixCascade.Enabled = checkIssue != null && checkIssue.Fix != null && !_running;
		_fixElectro.Enabled = _fixCascade.Enabled;
		if (checkIssue != null && checkIssue.FixLabel != null)
		{
			_fixCascade.Text = "Fix: " + checkIssue.FixLabel.Replace("Redesign", "redesign");
		}
	}

	private void OnIssueActivated(object sender, EventArgs e)
	{
		CheckIssue checkIssue = SelectedIssue();
		if (checkIssue != null && checkIssue.Element != null && !_running)
		{
			int num = _project.Pages.FindIndex([SpecialName] (ProjectPage p) => p.Circuit.Elements.Contains(checkIssue.Element));
			if (num >= 0)
			{
				ShowPage(num);
				_canvas.SelectElements(new CircuitElement[1] { checkIssue.Element });
			}
		}
	}

	private void ApplyFix(Func<Circuit> fix)
	{
		if (fix != null && !_running && MessageBox.Show("Replace the circuit on this page with a corrected design? (You can undo this.)", "PneuSim", MessageBoxButtons.OKCancel, MessageBoxIcon.Question) == DialogResult.OK)
		{
			_project.Pages[_pageIndex].Circuit = fix();
			ShowPage(_pageIndex);
			FitView();
			OnCircuitModified();
			_statusMessage.Text = "Circuit redesigned without signal overlap. Press Start (F9) to try it.";
		}
	}

	private Control BuildExplainPanel()
	{
		Panel panel = new Panel();
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 32,
			WrapContents = false
		};
		flowLayoutPanel.Controls.Add(new Label
		{
			Text = "Operate:",
			AutoSize = true,
			Padding = new Padding(0, 7, 0, 0)
		});
		flowLayoutPanel.Controls.Add(_explainOperate);
		Button button = new Button
		{
			Text = "Explain my circuit",
			AutoSize = true
		};
		button.Click += OnExplain;
		flowLayoutPanel.Controls.Add(button);
		panel.Controls.Add(_explainText);
		panel.Controls.Add(flowLayoutPanel);
		_explainOperate.DropDown += [SpecialName] (object a0, EventArgs a1) =>
		{
			FillOperateList();
		};
		return panel;
	}

	private void FillOperateList()
	{
		CircuitElement circuitElement = _explainOperate.SelectedItem as CircuitElement;
		_explainOperate.Items.Clear();
		foreach (CircuitElement item in CircuitAnalysis.ManualElements(_project.SimulationCircuit()))
		{
			_explainOperate.Items.Add(item);
		}
		if (circuitElement != null && _explainOperate.Items.Contains(circuitElement))
		{
			_explainOperate.SelectedItem = circuitElement;
		}
		else if (_explainOperate.Items.Count > 0)
		{
			_explainOperate.SelectedIndex = 0;
		}
	}

	private void OnExplain(object sender, EventArgs e)
	{
		if (_explainOperate.SelectedItem == null)
		{
			FillOperateList();
		}
		Cursor = Cursors.WaitCursor;
		try
		{
			string text = CircuitAnalysis.Explain(_project.SimulationCircuit(), _explainOperate.SelectedItem as CircuitElement, _realPhysics);
			_explainText.Text = text.Replace("\n", "\r\n").Replace("\r\r\n", "\r\n");
			ShowBottomTab("Explain");
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			_explainText.Text = "Could not explain this circuit: " + ex2.Message;
			ProjectData.ClearProjectError();
		}
		finally
		{
			Cursor = Cursors.Default;
		}
	}

	private void OnGenerator(object sender, EventArgs e)
	{
		if (_running)
		{
			return;
		}
		using GeneratorDialog generatorDialog = new GeneratorDialog();
		Theme.Apply(generatorDialog);
		if (generatorDialog.ShowDialog(this) == DialogResult.OK && generatorDialog.Result != null && ConfirmDiscard())
		{
			NewProject(new Project(generatorDialog.Result.Circuit, "Generated circuit"));
			FitView();
			_dirty = true;
			UpdateTitle();
			_explainText.Text = generatorDialog.Result.Explanation.Replace("\n", "\r\n").Replace("\r\r\n", "\r\n");
			ShowBottomTab("Explain");
			_statusMessage.Text = "Circuit generated. Press Start (F9), then press the start button S1 / 1S0.";
		}
	}

	private void OnCalculators(object sender, EventArgs e)
	{
		using CalculatorsDialog calculatorsDialog = new CalculatorsDialog();
		Theme.Apply(calculatorsDialog);
		calculatorsDialog.ShowDialog(this);
	}

	private void OnPartsList(object sender, EventArgs e)
	{
		using PartsListDialog partsListDialog = new PartsListDialog(_project);
		Theme.Apply(partsListDialog);
		partsListDialog.ShowDialog(this);
		if (partsListDialog.PricesChanged)
		{
			OnCircuitModified();
		}
	}

	private void OnCutaway(object sender, EventArgs e)
	{
		if (_cutaway == null || _cutaway.IsDisposed)
		{
			_cutaway = new CutawayWindow
			{
				Owner = this,
				Location = checked(new Point(base.Right - 640, base.Top + 120)),
				ElementSource = [SpecialName] () => _canvas.SelectedElement ?? _canvas.LastHoveredElement,
				SimulatingSource = [SpecialName] () => _running
			};
		}
		_cutaway.Show();
		_cutaway.BringToFront();
	}

	private void OnQuickGuide(object sender, EventArgs e)
	{
		MessageBox.Show("BUILDING A CIRCUIT\r\n• Click a component in the library, then click on the drawing (or drag it there). Hover over a symbol for an explanation.\r\n• Drag from one port to another to connect them. Red circles are electrical terminals.\r\n• Drag a box to select several; R rotates, Del deletes, arrow keys nudge, Ctrl+Z / Ctrl+Y undo / redo.\r\n• Ctrl+C, Ctrl+X, Ctrl+V and Ctrl+D copy, cut, paste and duplicate. Drag a tube's middle segment to move it.\r\n• Use several pages (Page menu) and join lines across pages with page connectors of the same name.\r\n\r\nSIMULATING\r\n• Start (F9). Pressurized tubes turn blue, live wires red, hydraulic pressure lines orange.\r\n• Click push buttons, switches and pumps; clicking a solenoid valve operates its manual override.\r\n• Realistic physics: pressures build up, cylinders move by bore, load and friction; air consumption and cost are shown.\r\n• Right-click any component or tube to see its live values in the Inspector tab and change settings with sliders.\r\n• Replay: the step and event buttons (or drag the time slider) go back and forward in time; Start continues from there.\r\n• Plotter tab: tick any quantity, click for cursor A, right-click for cursor B (min / max / average between them);\r\n  set a trigger, zoom with the mouse wheel, and export CSV or PNG.\r\n• Record GIF saves an animation of the running circuit.\r\n\r\nFAULT FINDING\r\n• Tools > Fault finding > New fault-finding task hides a fault; find it with as few checks and hints as possible.\r\n• 'Add fault…' puts a chosen fault into the selected part or tube (tick Hidden to make a fault-finding file for a colleague).\r\n\r\nTOOLS\r\n• Check my circuit (F6) finds mistakes and signal overlap; Explain (F8) describes the circuit step by step.\r\n• Circuit Generator (Ctrl+G) designs a circuit from a sequence such as A+ B+ B- A-.\r\n• Calculators, parts list with costs, PDF report with title block, SVG and DXF export.\r\n• Parameter sweep: run the circuit with a setting changed step by step and compare cycle time, air use and speed.\r\n• Learn menu: getting-started tutorials with automatic checking, cutaway view of any component (F7).", "PneuSim Quick Guide", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
	}

	private void OnAbout(object sender, EventArgs e)
	{
		MessageBox.Show(string.Format("{0} {1}{2}Pneumatic, electro-pneumatic and hydraulic circuit design and simulation{3}{4}", "PneuSim", "3.2", "\r\n", "\r\n", "\r\n") + "(c) 2026 Akshaya Simha.\r\n\r\nDeveloped in VB.NET initially, and improved it with vibe coding using Claude Pro.\r\n\r\nDesign, simulate and check pneumatic and hydraulic circuits before you build them.\r\n\r\nSymbols based on ISO 1219 / IEC 60617.", "About PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
	}

	private void InitTroubleshooting()
	{
		_canvas.InspectRequested += OnInspect;
		_canvas.TargetPicked += OnTargetPicked;
		_inspector.SettingChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__117_002D0();
		};
		_inspector.SettingCommitted += [SpecialName] (object a0, EventArgs a1) =>
		{
			OnCircuitModified();
		};
		_trouble.Command += OnTroubleCommand;
	}

	private ToolStripMenuItem BuildTroubleshootMenu()
	{
		return new ToolStripMenuItem("&Fault finding")
		{
			DropDownItems = 
			{
				(ToolStripItem)Item("&New fault-finding task (hidden fault)", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D0();
				}),
				(ToolStripItem)Item("&Hint", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D1();
				}, Keys.H | Keys.Control),
				(ToolStripItem)Item("I &found the fault…", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D2();
				}),
				(ToolStripItem)Item("&Show the fault", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D3();
				}),
				(ToolStripItem)new ToolStripSeparator(),
				(ToolStripItem)Item("&Put a fault into the selection…", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D4();
				}),
				(ToolStripItem)Item("&Repair all faults", [SpecialName] (object a0, EventArgs a1) =>
				{
					_Lambda_0024__118_002D5();
				})
			}
		};
	}

	private void OnInspect(object sender, object target)
	{
		_inspector.Target = RuntimeHelpers.GetObjectValue(target);
		if (_exercise != null && !_exercise.IsOver && _exercise.RecordCheck(RuntimeHelpers.GetObjectValue(target)))
		{
			_trouble.ShowExercise(_exercise);
			_statusMessage.Text = $"Check {_exercise.Checks.Count}: {TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(target))} (see the Inspector tab).";
		}
		_inspector.Note = ((_exercise != null && !_exercise.IsOver) ? $"Fault finding: check {_exercise.Checks.Count}, {_exercise.HintsUsed} hints" : "");
		ShowBottomTab("Inspector");
	}

	private void ClearInspector()
	{
		_inspector.Target = null;
		_canvas.InspectedObject = null;
	}

	private void OnTroubleCommand(object sender, string cmd)
	{
		switch (cmd)
		{
		case "start":
			StartExercise();
			goto default;
		case "hint":
			if (_exercise != null && !_exercise.IsOver)
			{
				_trouble.AddMessage("Hint: " + _exercise.NextHint(_project));
				goto default;
			}
			break;
		case "found":
			if (_exercise != null && !_exercise.IsOver)
			{
				_canvas.PickingTarget = true;
				_canvas.Cursor = Cursors.Hand;
				_statusMessage.Text = "Click the component or tube you think is faulty (Esc to cancel).";
				ShowBottomTab("Fault finding");
				goto default;
			}
			break;
		case "giveup":
			if (_exercise != null && !_exercise.IsOver)
			{
				_trouble.AddMessage(_exercise.GiveUp(_project));
				MarkDirty();
				goto default;
			}
			break;
		case "report":
			SaveExerciseReport();
			goto default;
		case "add":
			AddFaultToSelection();
			goto default;
		case "clear":
			if (_project.Pages.Sum([SpecialName] (ProjectPage p) => p.Circuit.FaultCount()) == 0)
			{
				_statusMessage.Text = "There are no faults in the project.";
				break;
			}
			TroubleshootExercise.RemoveAllFaults(_project);
			_exercise = null;
			_trouble.AddMessage("All faults repaired.");
			AfterFaultChange();
			goto default;
		default:
			_trouble.ShowExercise(_exercise);
			_canvas.Invalidate();
			break;
		}
	}

	private void StartExercise()
	{
		if (_running)
		{
			StopSimulation();
		}
		TroubleshootExercise troubleshootExercise = TroubleshootExercise.StartRandom(_project, _rnd);
		if (troubleshootExercise == null)
		{
			_trouble.AddMessage("This circuit has nothing that can fail. Load an example or draw a circuit first.");
			return;
		}
		_exercise = troubleshootExercise;
		AfterFaultChange();
		_trouble.AddMessage("A fault is now hidden somewhere in the circuit. Start the simulation, operate the circuit and right-click components or tubes to measure them. When you know where the fault is, click 'I found it…'.");
		ShowBottomTab("Fault finding");
		_trouble.ShowExercise(_exercise);
	}

	private void CheckForHiddenFault()
	{
		if (TroubleshootExercise.FindHidden(_project) == null)
		{
			_exercise = null;
		}
		else
		{
			_exercise = new TroubleshootExercise();
			_trouble.AddMessage("This circuit contains a hidden fault. Find it! Run the simulation, measure with right-clicks, then click 'I found it…'.");
			ShowBottomTab("Fault finding");
		}
		_trouble.ShowExercise(_exercise);
	}

	private void OnTargetPicked(object sender, object target)
	{
		if (_exercise == null || _exercise.IsOver)
		{
			return;
		}
		using (FaultDialog faultDialog = new FaultDialog(RuntimeHelpers.GetObjectValue(target), diagnosis: true))
		{
			if (faultDialog.ShowDialog(this) != DialogResult.OK)
			{
				_statusMessage.Text = "";
				return;
			}
			(bool, string) tuple = _exercise.Guess(_project, RuntimeHelpers.GetObjectValue(target), faultDialog.SelectedKind);
			_trouble.AddMessage(tuple.Item2);
			if (tuple.Item1)
			{
				_trouble.AddMessage($"Result: {_exercise.Score()} / 100 ({_exercise.Checks.Count} checks, {_exercise.HintsUsed} hints, {_exercise.WrongGuesses} wrong guesses). " + "Use 'Repair all' to fix the circuit, or start a new task.");
				MarkDirty();
			}
			_statusMessage.Text = tuple.Item2;
		}
		_trouble.ShowExercise(_exercise);
		_canvas.Invalidate();
	}

	private void AddFaultToSelection()
	{
		_Closure_0024__125_002D0 arg = default(_Closure_0024__125_002D0);
		_Closure_0024__125_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__125_002D0(arg);
		if (_running)
		{
			_statusMessage.Text = "Stop the simulation first (F11) to put in a fault.";
			return;
		}
		CS_0024_003C_003E8__locals0._0024VB_0024Local_target = RuntimeHelpers.GetObjectValue(((object)_canvas.SelectedElement) ?? ((object)_canvas.SelectedTube));
		if (CS_0024_003C_003E8__locals0._0024VB_0024Local_target == null)
		{
			_statusMessage.Text = "Select a component or a tube first, then click 'Add fault…'.";
			ShowBottomTab("Fault finding");
			return;
		}
		if (TroubleshootExercise.PossibleFaultsOf(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target)).Length == 0)
		{
			_statusMessage.Text = $"{TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target))} cannot have a fault.";
			return;
		}
		using (FaultDialog faultDialog = new FaultDialog(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target), diagnosis: false))
		{
			if (faultDialog.ShowDialog(this) != DialogResult.OK)
			{
				return;
			}
			TroubleshootExercise.SetFault(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target), faultDialog.SelectedKind, faultDialog.Hidden);
			if (faultDialog.Hidden && faultDialog.SelectedKind != 0)
			{
				foreach (ProjectPage page in _project.Pages)
				{
					foreach (CircuitElement item in page.Circuit.Elements.Where([SpecialName] (CircuitElement x) => x != CS_0024_003C_003E8__locals0._0024VB_0024Local_target))
					{
						item.FaultHidden = false;
					}
					foreach (Tube item2 in page.Circuit.Tubes.Where([SpecialName] (Tube x) => x != CS_0024_003C_003E8__locals0._0024VB_0024Local_target))
					{
						item2.FaultHidden = false;
					}
				}
				_trouble.AddMessage($"Hidden fault put into {TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target))}. Save the file and give it to a colleague: it opens as a fault-finding task.");
			}
			else if (faultDialog.SelectedKind != 0)
			{
				_trouble.AddMessage($"{TroubleshootExercise.NameOf_(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target))}: {TroubleshootExercise.DescribeFault(RuntimeHelpers.GetObjectValue(CS_0024_003C_003E8__locals0._0024VB_0024Local_target), faultDialog.SelectedKind)}. Run the simulation to see what it does.");
			}
		}
		AfterFaultChange();
	}

	private void AfterFaultChange()
	{
		_canvas.Invalidate();
		OnCircuitModified();
	}

	private void SaveExerciseReport()
	{
		if (_exercise == null)
		{
			return;
		}
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "Text file (*.txt)|*.txt",
			FileName = SafeName(_project.Info.Title) + " troubleshooting.txt"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			File.WriteAllText(saveFileDialog.FileName, _exercise.Report(_project.Info.Title), Encoding.UTF8);
			_statusMessage.Text = "Saved " + saveFileDialog.FileName;
		}
	}

	private void AddReplayButtons(ToolStrip bar)
	{
		_btnBack = Button("Step back", Icons.StepIcon(forward: false, toEvent: false), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D0();
		}, iconOnly: true);
		_btnStepFwd = Button("Step forward", Icons.StepIcon(forward: true, toEvent: false), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D1();
		}, iconOnly: true);
		_btnPrevEvent = Button("Back to the previous event", Icons.StepIcon(forward: false, toEvent: true), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D2();
		}, iconOnly: true);
		_btnNextEvent = Button("Forward to the next event (a valve, relay or cylinder changes)", Icons.StepIcon(forward: true, toEvent: true), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D3();
		}, iconOnly: true);
		_timeline = new TrackBar
		{
			Minimum = 0,
			Maximum = 0,
			TickStyle = TickStyle.None,
			AutoSize = false,
			Height = 22,
			Width = 150
		};
		_timelineHost = new ToolStripControlHost(_timeline)
		{
			ToolTipText = "Replay: drag to go back in time"
		};
		_timeline.Scroll += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__128_002D4();
		};
		bar.Items.AddRange(new ToolStripItem[5] { _btnPrevEvent, _btnBack, _btnStepFwd, _btnNextEvent, _timelineHost });
	}

	private void PauseForReplay()
	{
		if (_running && !_paused)
		{
			_paused = true;
			_timer.Stop();
		}
	}

	private void ReplayStep(int direction)
	{
		checked
		{
			if (_running && _recorder != null)
			{
				PauseForReplay();
				if (direction < 0)
				{
					_recorder.Restore(_recorder.Index - 1);
				}
				else if (_recorder.IsRewound)
				{
					_recorder.Restore(_recorder.Index + 1);
				}
				else
				{
					AdvanceSimulation(0.05);
				}
				AfterReplayMove();
			}
		}
	}

	private void ReplayEvent(int direction)
	{
		if (!_running || _recorder == null)
		{
			return;
		}
		PauseForReplay();
		if (direction < 0)
		{
			int num = _recorder.PreviousEventFrame();
			_recorder.Restore((num >= 0) ? num : 0);
		}
		else
		{
			int num2 = _recorder.NextEventFrame();
			if (num2 >= 0)
			{
				_recorder.Restore(num2);
			}
			else
			{
				if (_recorder.IsRewound)
				{
					_recorder.Restore(checked(_recorder.Count - 1));
				}
				string right = _simulator.DiscreteState();
				double num3 = _simulator.Time + 30.0;
				do
				{
					AdvanceSimulation(0.05);
				}
				while (Operators.CompareString(_simulator.DiscreteState(), right, TextCompare: false) == 0 && _simulator.Time < num3);
				if (Operators.CompareString(_simulator.DiscreteState(), right, TextCompare: false) == 0)
				{
					_statusMessage.Text = "Nothing switched in the next 30 s.";
				}
			}
		}
		AfterReplayMove();
	}

	private void AdvanceSimulation(double seconds)
	{
		double num = 0.005;
		checked
		{
			int num2 = Math.Max(1, (int)Math.Round(seconds / num));
			for (int i = 1; i <= num2; i++)
			{
				_simulator.Step(num);
				_recorder.Record();
			}
		}
	}

	private void AfterReplayMove()
	{
		_canvas.Invalidate();
		_diagram.Invalidate();
		_plotter.RefreshPlot();
		_inspector.RefreshValues();
		UpdateUiState();
		UpdateReplayUi();
	}

	private void UpdateReplayUi()
	{
		checked
		{
			if (_timeline != null)
			{
				bool flag = _running && _recorder != null && _recorder.Count > 0;
				ToolStripButton[] array = new ToolStripButton[4] { _btnBack, _btnStepFwd, _btnPrevEvent, _btnNextEvent };
				for (int i = 0; i < array.Length; i++)
				{
					array[i].Enabled = _running;
				}
				_timelineHost.Enabled = flag;
				_updatingTimeline = true;
				if (flag)
				{
					_timeline.Maximum = Math.Max(0, _recorder.Count - 1);
					_timeline.Value = Math.Max(0, Math.Min(_timeline.Maximum, _recorder.Index));
				}
				else
				{
					_timeline.Maximum = 0;
				}
				_updatingTimeline = false;
			}
		}
	}

	private void OnSweep(object sender, EventArgs e)
	{
		if (_project.AllElements().Count() == 0)
		{
			_statusMessage.Text = "Draw or load a circuit first.";
			return;
		}
		if (_running)
		{
			StopSimulation();
		}
		using SweepDialog sweepDialog = new SweepDialog(_project, _realPhysics);
		sweepDialog.ShowDialog(this);
	}

	public MainForm()
	{
		_history = new List<(string, int)>();
		_historyIndex = -1;
		_autosaveTimer = new Timer
		{
			Interval = 60000
		};
		_timer = new Timer
		{
			Interval = 20
		};
		_gifCanvas = new CircuitCanvas();
		_checkList = new ListView
		{
			View = View.Details,
			FullRowSelect = true,
			HideSelection = false,
			Dock = DockStyle.Fill
		};
		_checkTimer = new Timer
		{
			Interval = 700
		};
		_fixCascade = new Button
		{
			Text = "Fix: redesign (cascade)",
			AutoSize = true,
			Enabled = false
		};
		_fixElectro = new Button
		{
			Text = "Fix: redesign (electro-pneumatic)",
			AutoSize = true,
			Enabled = false
		};
		_explainText = new TextBox
		{
			Multiline = true,
			ReadOnly = true,
			ScrollBars = ScrollBars.Vertical,
			WordWrap = true,
			Dock = DockStyle.Fill,
			Font = new Font("Consolas", 9f)
		};
		_explainOperate = new ComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			Width = 220
		};
		_issues = new List<CheckIssue>();
		_inspector = new InspectorPanel
		{
			Dock = DockStyle.Fill
		};
		_trouble = new TroubleshootPanel
		{
			Dock = DockStyle.Fill
		};
		_rnd = new Random();
		_canvas = new CircuitCanvas
		{
			Dock = DockStyle.Fill
		};
		_pageTabs = new TabControl
		{
			Dock = DockStyle.Top,
			Height = 24
		};
		_diagram = new DiagramPanel
		{
			Dock = DockStyle.Fill
		};
		_plotter = new PlotterPanel
		{
			Dock = DockStyle.Fill
		};
		_lessons = new LessonsPanel
		{
			Dock = DockStyle.Fill
		};
		_bottomTabs = new TabControl
		{
			Dock = DockStyle.Fill
		};
		_library = new ListView();
		_properties = new PropertyGrid();
		_tooltip = new ToolTip
		{
			AutoPopDelay = 12000,
			InitialDelay = 700,
			ReshowDelay = 300
		};
		_statusMode = new ToolStripStatusLabel
		{
			BorderSides = ToolStripStatusLabelBorderSides.Right,
			Padding = new Padding(0, 0, 8, 0)
		};
		_statusMessage = new ToolStripStatusLabel
		{
			Spring = true,
			TextAlign = ContentAlignment.MiddleLeft
		};
		_statusZoom = new ToolStripStatusLabel();
		AppSettings.Load();
		Text = "PneuSim";
		Font = new Font("Segoe UI", 9f);
		base.Size = new Size(1400, 900);
		base.StartPosition = FormStartPosition.CenterScreen;
		base.KeyPreview = true;
		base.Icon = Icons.AppIcon();
		BuildLayout();
		Theme.OwnerDrawTabs(_bottomTabs);
		Theme.OwnerDrawTabs(_pageTabs);
		_canvas.SelectionChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__167_002D0();
		};
		_canvas.CircuitModified += [SpecialName] (object a0, EventArgs a1) =>
		{
			OnCircuitModified();
		};
		_canvas.ElementOperated += OnElementOperated;
		_canvas.StatusMessage += [SpecialName] (object s, string msg) =>
		{
			_statusMessage.Text = msg;
		};
		_canvas.HoverElementChanged += OnHoverElement;
		_canvas.ZoomChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__167_002D3();
		};
		_properties.PropertyValueChanged += OnPropertyValueChanged;
		_timer.Tick += OnTick;
		_pageTabs.SelectedIndexChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__167_002D4();
		};
		_lessons.LoadCircuit += [SpecialName] (object s, Circuit c) =>
		{
			LoadLessonCircuit(c);
		};
		_lessons.CurrentCircuit = [SpecialName] () => _project.SimulationCircuit();
		_canvas.NameScope = [SpecialName] () => _project.AllElements();
		InitTroubleshooting();
		NewProject(new Project(new Circuit()));
		_realPhysics = AppSettings.RealPhysics;
		ApplyTheme(AppSettings.ResolveDark());
		Microsoft.Win32.SystemEvents.UserPreferenceChanged += OnWindowsPreferenceChanged;
		UpdateUiState();
		StartAutosave();
	}

	private void BuildLayout()
	{
		_Closure_0024__168_002D0 arg = default(_Closure_0024__168_002D0);
		_Closure_0024__168_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__168_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Me = this;
		_library.Dock = DockStyle.Fill;
		_library.View = View.Tile;
		_library.TileSize = new Size(236, 58);
		_library.MultiSelect = false;
		_library.HideSelection = false;
		_library.LargeImageList = new ImageList
		{
			ImageSize = new Size(72, 48),
			ColorDepth = ColorDepth.Depth32Bit
		};
		foreach (string item in Library.Presets.Select([SpecialName] (LibraryPreset p) => p.Category).Distinct())
		{
			_library.Groups.Add(item, item);
		}
		RenderLibraryImages(ColorScheme.Light);
		checked
		{
			int num = Library.Presets.Length - 1;
			for (int i = 0; i <= num; i++)
			{
				LibraryPreset libraryPreset = Library.Presets[i];
				_library.Items.Add(new ListViewItem(libraryPreset.Name, i, _library.Groups[libraryPreset.Category])
				{
					Tag = libraryPreset
				});
			}
			_library.ItemDrag += [SpecialName] (object s, ItemDragEventArgs e) =>
			{
				ListViewItem listViewItem = (ListViewItem)e.Item;
				_library.DoDragDrop(RuntimeHelpers.GetObjectValue(listViewItem.Tag), DragDropEffects.Copy);
			};
			_library.ItemActivate += OnLibraryActivate;
			_library.MouseClick += [SpecialName] (object a0, MouseEventArgs a1) =>
			{
				_Lambda_0024__168_002D2();
			};
			Panel value = TitledPanel("Component library", _library);
			_properties.Dock = DockStyle.Fill;
			_properties.ToolbarVisible = false;
			_properties.HelpVisible = true;
			Panel value2 = TitledPanel("Properties", _properties);
			Panel panel = new Panel
			{
				Dock = DockStyle.Fill
			};
			panel.Controls.Add(_canvas);
			panel.Controls.Add(_pageTabs);
			ContextMenuStrip contextMenuStrip = new ContextMenuStrip();
			contextMenuStrip.Items.Add("Add page", null, [SpecialName] (object a0, EventArgs a1) =>
			{
				AddPage();
			});
			contextMenuStrip.Items.Add("Rename page...", null, [SpecialName] (object a0, EventArgs a1) =>
			{
				RenamePage();
			});
			contextMenuStrip.Items.Add("Delete page", null, [SpecialName] (object a0, EventArgs a1) =>
			{
				DeletePage();
			});
			_pageTabs.ContextMenuStrip = contextMenuStrip;
			AddBottomTab("Displacement-step diagram", _diagram);
			AddBottomTab("Plotter", _plotter);
			AddBottomTab("Check circuit", BuildCheckPanel());
			AddBottomTab("Explain", BuildExplainPanel());
			AddBottomTab("Getting started", _lessons);
			AddBottomTab("Inspector", _inspector);
			AddBottomTab("Fault finding", _trouble);
			_diagramSplit = new SplitContainer
			{
				Dock = DockStyle.Fill,
				Orientation = Orientation.Horizontal,
				FixedPanel = FixedPanel.Panel2
			};
			_diagramSplit.Panel1.Controls.Add(panel);
			_diagramSplit.Panel2.Controls.Add(_bottomTabs);
			CS_0024_003C_003E8__locals0._0024VB_0024Local_rightSplit = new SplitContainer
			{
				Dock = DockStyle.Fill,
				FixedPanel = FixedPanel.Panel2
			};
			CS_0024_003C_003E8__locals0._0024VB_0024Local_rightSplit.Panel1.Controls.Add(_diagramSplit);
			CS_0024_003C_003E8__locals0._0024VB_0024Local_rightSplit.Panel2.Controls.Add(value2);
			CS_0024_003C_003E8__locals0._0024VB_0024Local_mainSplit = new SplitContainer
			{
				Dock = DockStyle.Fill,
				FixedPanel = FixedPanel.Panel1
			};
			CS_0024_003C_003E8__locals0._0024VB_0024Local_mainSplit.Panel1.Controls.Add(value);
			CS_0024_003C_003E8__locals0._0024VB_0024Local_mainSplit.Panel2.Controls.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_rightSplit);
			_status = new StatusStrip();
			_status.Items.AddRange(new ToolStripItem[3] { _statusMode, _statusMessage, _statusZoom });
			base.Controls.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_mainSplit);
			_toolbar = BuildToolbar();
			base.Controls.Add(_toolbar);
			_menu = BuildMenu();
			base.Controls.Add(_menu);
			base.Controls.Add(_status);
			base.Load += [SpecialName] (object a0, EventArgs a1) =>
			{
				CS_0024_003C_003E8__locals0._Lambda_0024__6();
			};
		}
	}

	private void AddBottomTab(string title, Control content)
	{
		TabPage tabPage = new TabPage(title);
		content.Dock = DockStyle.Fill;
		tabPage.Controls.Add(content);
		_bottomTabs.TabPages.Add(tabPage);
	}

	private void ShowBottomTab(string title)
	{
		foreach (TabPage tabPage in _bottomTabs.TabPages)
		{
			if (Operators.CompareString(tabPage.Text, title, TextCompare: false) == 0)
			{
				_bottomTabs.SelectedTab = tabPage;
			}
		}
		_diagramSplit.Panel2Collapsed = false;
	}

	private static Panel TitledPanel(string title, Control content)
	{
		Panel obj = new Panel
		{
			Dock = DockStyle.Fill
		};
		Label value = new Label
		{
			Text = title,
			Dock = DockStyle.Top,
			Height = 24,
			Padding = new Padding(6, 4, 0, 0),
			BackColor = Color.FromArgb(232, 236, 244),
			Font = new Font("Segoe UI", 8.5f, FontStyle.Bold),
			Tag = "header"
		};
		obj.Controls.Add(content);
		obj.Controls.Add(value);
		return obj;
	}

	private MenuStrip BuildMenu()
	{
		_Closure_0024__172_002D0 arg = default(_Closure_0024__172_002D0);
		_Closure_0024__172_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__172_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Me = this;
		MenuStrip menuStrip = new MenuStrip();
		ToolStripMenuItem toolStripMenuItem = new ToolStripMenuItem("&File");
		toolStripMenuItem.DropDownItems.Add(Item("&New", OnNew, Keys.N | Keys.Control));
		toolStripMenuItem.DropDownItems.Add(Item("&Open...", OnOpen, Keys.O | Keys.Control));
		toolStripMenuItem.DropDownItems.Add(Item("&Save", OnSave, Keys.S | Keys.Control));
		toolStripMenuItem.DropDownItems.Add(Item("Save &As...", OnSaveAs));
		toolStripMenuItem.DropDownItems.Add(new ToolStripSeparator());
		toolStripMenuItem.DropDownItems.Add(Item("Project &information (title block)...", OnProjectInfo));
		ToolStripMenuItem toolStripMenuItem2 = new ToolStripMenuItem("&Export");
		toolStripMenuItem2.DropDownItems.Add(Item("PDF report with title block...", OnExportPdf, Keys.P | Keys.Control));
		toolStripMenuItem2.DropDownItems.Add(Item("SVG drawing (this page)...", OnExportSvg));
		toolStripMenuItem2.DropDownItems.Add(Item("DXF drawing for CAD (this page)...", OnExportDxf));
		toolStripMenuItem2.DropDownItems.Add(Item("PNG image (this page)...", OnExportImage));
		toolStripMenuItem2.DropDownItems.Add(Item("Parts list (CSV)...", OnPartsList));
		toolStripMenuItem.DropDownItems.Add(toolStripMenuItem2);
		toolStripMenuItem.DropDownItems.Add(new ToolStripSeparator());
		toolStripMenuItem.DropDownItems.Add(Item("E&xit", [SpecialName] (object a0, EventArgs a1) =>
		{
			Close();
		}));
		ToolStripMenuItem toolStripMenuItem3 = new ToolStripMenuItem("&Edit");
		_menuUndo = Item("&Undo", [SpecialName] (object a0, EventArgs a1) =>
		{
			Undo();
		});
		_menuUndo.ShortcutKeyDisplayString = "Ctrl+Z";
		_menuRedo = Item("&Redo", [SpecialName] (object a0, EventArgs a1) =>
		{
			Redo();
		});
		_menuRedo.ShortcutKeyDisplayString = "Ctrl+Y";
		ToolStripMenuItem toolStripMenuItem4 = Item("Cu&t", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D3();
		});
		toolStripMenuItem4.ShortcutKeyDisplayString = "Ctrl+X";
		ToolStripMenuItem toolStripMenuItem5 = Item("&Copy", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D4();
		});
		toolStripMenuItem5.ShortcutKeyDisplayString = "Ctrl+C";
		ToolStripMenuItem toolStripMenuItem6 = Item("&Paste", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D5();
		});
		toolStripMenuItem6.ShortcutKeyDisplayString = "Ctrl+V";
		ToolStripMenuItem toolStripMenuItem7 = Item("D&uplicate", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D6();
		});
		toolStripMenuItem7.ShortcutKeyDisplayString = "Ctrl+D";
		ToolStripMenuItem toolStripMenuItem8 = Item("Select &All", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D7();
		});
		toolStripMenuItem8.ShortcutKeyDisplayString = "Ctrl+A";
		ToolStripMenuItem toolStripMenuItem9 = Item("&Delete", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D8();
		});
		toolStripMenuItem9.ShortcutKeyDisplayString = "Del";
		ToolStripMenuItem toolStripMenuItem10 = Item("&Rotate 90°", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D9();
		});
		toolStripMenuItem10.ShortcutKeyDisplayString = "R";
		toolStripMenuItem3.DropDownItems.AddRange(new ToolStripItem[11]
		{
			_menuUndo,
			_menuRedo,
			new ToolStripSeparator(),
			toolStripMenuItem4,
			toolStripMenuItem5,
			toolStripMenuItem6,
			toolStripMenuItem7,
			new ToolStripSeparator(),
			toolStripMenuItem8,
			toolStripMenuItem9,
			toolStripMenuItem10
		});
		ToolStripMenuItem toolStripMenuItem11 = new ToolStripMenuItem("&Page");
		toolStripMenuItem11.DropDownItems.Add(Item("&Add page", [SpecialName] (object a0, EventArgs a1) =>
		{
			AddPage();
		}));
		toolStripMenuItem11.DropDownItems.Add(Item("&Rename page...", [SpecialName] (object a0, EventArgs a1) =>
		{
			RenamePage();
		}));
		toolStripMenuItem11.DropDownItems.Add(Item("&Delete page", [SpecialName] (object a0, EventArgs a1) =>
		{
			DeletePage();
		}));
		toolStripMenuItem11.DropDownItems.Add(new ToolStripSeparator());
		toolStripMenuItem11.DropDownItems.Add(Item("&Next page", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D13();
		}, Keys.Next | Keys.Control));
		toolStripMenuItem11.DropDownItems.Add(Item("&Previous page", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D14();
		}, Keys.Prior | Keys.Control));
		ToolStripMenuItem toolStripMenuItem12 = new ToolStripMenuItem("&View");
		toolStripMenuItem12.DropDownItems.Add(Item("Zoom &In", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D15();
		}, Keys.Oemplus | Keys.Control));
		toolStripMenuItem12.DropDownItems.Add(Item("Zoom &Out", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D16();
		}, Keys.OemMinus | Keys.Control));
		toolStripMenuItem12.DropDownItems.Add(Item("&Actual Size", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D17();
		}, Keys.D0 | Keys.Control));
		toolStripMenuItem12.DropDownItems.Add(Item("Zoom to &Fit", [SpecialName] (object a0, EventArgs a1) =>
		{
			FitView();
		}, Keys.D9 | Keys.Control));
		toolStripMenuItem12.DropDownItems.Add(new ToolStripSeparator());
		CS_0024_003C_003E8__locals0._0024VB_0024Local_tools = Item("&Bottom panel (diagram, plotter, check, explain, getting started, inspector, fault finding)", null);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_tools.Checked = true;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_tools.CheckOnClick = true;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_tools.CheckedChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			CS_0024_003C_003E8__locals0._Lambda_0024__19();
		};
		toolStripMenuItem12.DropDownItems.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_tools);
		toolStripMenuItem12.DropDownItems.Add(Item("&Cutaway view (animated)", OnCutaway, Keys.F7));
		_menuDark = Item("&Dark mode", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D20();
		});
		toolStripMenuItem12.DropDownItems.Add(_menuDark);
		ToolStripMenuItem toolStripMenuItem13 = new ToolStripMenuItem("&Simulation");
		_menuStart = Item("&Start", OnStart, Keys.F9);
		_menuPause = Item("&Pause", OnPause, Keys.F10);
		_menuStop = Item("S&top and Reset", OnStop, Keys.F11);
		_menuReal = Item("&Realistic physics (pressure build-up, loads, air consumption)", OnToggleReal);
		_menuRecord = Item("Record animated &GIF", OnToggleRecord);
		toolStripMenuItem13.DropDownItems.AddRange(new ToolStripItem[11]
		{
			_menuStart,
			_menuPause,
			_menuStop,
			new ToolStripSeparator(),
			Item("Step &back", [SpecialName] (object a0, EventArgs a1) =>
			{
				_Lambda_0024__172_002D21();
			}, Keys.F12 | Keys.Shift),
			Item("Step &forward", [SpecialName] (object a0, EventArgs a1) =>
			{
				_Lambda_0024__172_002D22();
			}, Keys.F12),
			Item("Back to the &previous event", [SpecialName] (object a0, EventArgs a1) =>
			{
				_Lambda_0024__172_002D23();
			}, Keys.F12 | Keys.Shift | Keys.Control),
			Item("Forward to the &next event", [SpecialName] (object a0, EventArgs a1) =>
			{
				_Lambda_0024__172_002D24();
			}, Keys.F12 | Keys.Control),
			new ToolStripSeparator(),
			_menuReal,
			_menuRecord
		});
		ToolStripMenuItem toolStripMenuItem14 = new ToolStripMenuItem("&Tools");
		toolStripMenuItem14.DropDownItems.Add(Item("&Circuit Generator (from a sequence)...", OnGenerator, Keys.G | Keys.Control));
		toolStripMenuItem14.DropDownItems.Add(Item("C&heck my circuit", OnFullCheck, Keys.F6));
		toolStripMenuItem14.DropDownItems.Add(Item("&Explain my circuit", OnExplain, Keys.F8));
		toolStripMenuItem14.DropDownItems.Add(new ToolStripSeparator());
		toolStripMenuItem14.DropDownItems.Add(Item("Ca&lculators...", OnCalculators));
		toolStripMenuItem14.DropDownItems.Add(Item("&Parts list and costs...", OnPartsList));
		toolStripMenuItem14.DropDownItems.Add(Item("Parameter &sweep (compare settings)...", OnSweep));
		toolStripMenuItem14.DropDownItems.Add(new ToolStripSeparator());
		toolStripMenuItem14.DropDownItems.Add(BuildTroubleshootMenu());
		toolStripMenuItem14.DropDownItems.Add(new ToolStripSeparator());
		toolStripMenuItem14.DropDownItems.Add(Item("&Settings...", OnSettings, Keys.Control | Keys.Oemcomma));
		ToolStripMenuItem toolStripMenuItem15 = new ToolStripMenuItem("&Learn");
		toolStripMenuItem15.DropDownItems.Add(Item("&Getting started tutorials", [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__172_002D25();
		}));
		toolStripMenuItem15.DropDownItems.Add(Item("&Cutaway view (inside the components)", OnCutaway));
		ToolStripMenuItem toolStripMenuItem16 = new ToolStripMenuItem("E&xamples");
		(string, Func<Circuit>)[] all = Examples.All;
		_Closure_0024__172_002D1 closure_0024__172_002D = default(_Closure_0024__172_002D1);
		for (int i = 0; i < all.Length; i = checked(i + 1))
		{
			(string, Func<Circuit>) tuple = all[i];
			closure_0024__172_002D = new _Closure_0024__172_002D1(closure_0024__172_002D);
			closure_0024__172_002D._0024VB_0024NonLocal__0024VB_0024Closure_2 = CS_0024_003C_003E8__locals0;
			closure_0024__172_002D._0024VB_0024Local_build = tuple.Item2;
			toolStripMenuItem16.DropDownItems.Add(Item(tuple.Item1, closure_0024__172_002D._Lambda_0024__R56));
		}
		ToolStripMenuItem toolStripMenuItem17 = new ToolStripMenuItem("&Help");
		toolStripMenuItem17.DropDownItems.Add(Item("&Quick Guide", OnQuickGuide, Keys.F1));
		toolStripMenuItem17.DropDownItems.Add(Item("&About PneuSim", OnAbout));
		menuStrip.Items.AddRange(new ToolStripItem[9] { toolStripMenuItem, toolStripMenuItem3, toolStripMenuItem11, toolStripMenuItem12, toolStripMenuItem13, toolStripMenuItem14, toolStripMenuItem15, toolStripMenuItem16, toolStripMenuItem17 });
		base.MainMenuStrip = menuStrip;
		return menuStrip;
	}

	private ToolStrip BuildToolbar()
	{
		ToolStrip toolStrip = new ToolStrip
		{
			GripStyle = ToolStripGripStyle.Hidden,
			ImageScalingSize = new Size(16, 16)
		};
		toolStrip.Items.Add(Button("New (Ctrl+N)", Icons.NewFile(), OnNew, iconOnly: true));
		toolStrip.Items.Add(Button("Open (Ctrl+O)", Icons.Open(), OnOpen, iconOnly: true));
		toolStrip.Items.Add(Button("Save (Ctrl+S)", Icons.Save(), OnSave, iconOnly: true));
		toolStrip.Items.Add(new ToolStripSeparator());
		_btnUndo = Button("Undo (Ctrl+Z)", Icons.UndoIcon(redo: false), [SpecialName] (object a0, EventArgs a1) =>
		{
			Undo();
		}, iconOnly: true);
		_btnRedo = Button("Redo (Ctrl+Y)", Icons.UndoIcon(redo: true), [SpecialName] (object a0, EventArgs a1) =>
		{
			Redo();
		}, iconOnly: true);
		toolStrip.Items.AddRange(new ToolStripItem[2] { _btnUndo, _btnRedo });
		toolStrip.Items.Add(new ToolStripSeparator());
		toolStrip.Items.Add(Button("Generator", Icons.Wand(), OnGenerator));
		toolStrip.Items.Add(Button("Check", Icons.CheckIcon(), OnFullCheck));
		toolStrip.Items.Add(new ToolStripSeparator());
		_btnStart = Button("Start", Icons.Play(), OnStart);
		_btnPause = Button("Pause", Icons.Pause(), OnPause);
		_btnStop = Button("Stop", Icons.StopIcon(), OnStop);
		toolStrip.Items.AddRange(new ToolStripItem[3] { _btnStart, _btnPause, _btnStop });
		AddReplayButtons(toolStrip);
		toolStrip.Items.Add(new ToolStripLabel("  Speed:"));
		_speedBox = new ToolStripComboBox
		{
			DropDownStyle = ComboBoxStyle.DropDownList,
			AutoSize = false,
			Width = 60
		};
		_speedBox.Items.AddRange(new object[6] { "0.1x", "0.25x", "0.5x", "1x", "2x", "4x" });
		_speedBox.SelectedIndex = 3;
		toolStrip.Items.Add(_speedBox);
		_btnReal = Button("Realistic", Icons.GaugeIcon(), OnToggleReal);
		_btnRecord = Button("Record GIF", Icons.RecordIcon(), OnToggleRecord);
		toolStrip.Items.AddRange(new ToolStripItem[2] { _btnReal, _btnRecord });
		toolStrip.Items.Add(new ToolStripSeparator());
		toolStrip.Items.Add(Button("Rotate (R)", Icons.RotateIcon(), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__173_002D2();
		}, iconOnly: true));
		toolStrip.Items.Add(Button("Delete (Del)", Icons.DeleteIcon(), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__173_002D3();
		}, iconOnly: true));
		toolStrip.Items.Add(new ToolStripSeparator());
		toolStrip.Items.Add(Button("Zoom in", Icons.ZoomIn(), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__173_002D4();
		}, iconOnly: true));
		toolStrip.Items.Add(Button("Zoom out", Icons.ZoomOut(), [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__173_002D5();
		}, iconOnly: true));
		toolStrip.Items.Add(new ToolStripButton("Fit")
		{
			ToolTipText = "Zoom to fit (Ctrl+9)"
		});
		toolStrip.Items[checked(toolStrip.Items.Count - 1)].Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			FitView();
		};
		return toolStrip;
	}

	protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
	{
		if ((keyData & Keys.Control) == Keys.Control && (keyData & Keys.Alt) != Keys.Alt && !IsEditingText())
		{
			bool flag = (keyData & Keys.Shift) == Keys.Shift;
			switch (keyData & Keys.KeyCode)
			{
			case Keys.Z:
				if (flag)
				{
					Redo();
				}
				else
				{
					Undo();
				}
				return true;
			case Keys.Y:
				Redo();
				return true;
			case Keys.X:
				_canvas.CutSelection();
				return true;
			case Keys.C:
				_canvas.CopySelection();
				return true;
			case Keys.V:
				_canvas.Paste();
				return true;
			case Keys.D:
				_canvas.DuplicateSelection();
				return true;
			case Keys.A:
				_canvas.SelectAll();
				return true;
			}
		}
		return base.ProcessCmdKey(ref msg, keyData);
	}

	private bool IsEditingText()
	{
		Control control = this;
		while (control is ContainerControl && ((ContainerControl)control).ActiveControl != null)
		{
			control = ((ContainerControl)control).ActiveControl;
		}
		Control control2 = FocusedLeaf(control);
		if (!(control2 is TextBoxBase) && !(control2 is ComboBox))
		{
			return control2 is NumericUpDown;
		}
		return true;
	}

	private static Control FocusedLeaf(Control c)
	{
		foreach (Control control in c.Controls)
		{
			if (control.ContainsFocus)
			{
				return FocusedLeaf(control);
			}
		}
		return c;
	}

	private static ToolStripMenuItem Item(string text, EventHandler handler, Keys keys = Keys.None)
	{
		ToolStripMenuItem toolStripMenuItem = new ToolStripMenuItem(text);
		if (handler != null)
		{
			toolStripMenuItem.Click += handler;
		}
		if (keys != 0)
		{
			toolStripMenuItem.ShortcutKeys = keys;
			Keys keys2 = keys & Keys.KeyCode;
			object obj;
			switch (keys2)
			{
			default:
				obj = null;
				break;
			case Keys.D0:
			case Keys.D1:
			case Keys.D2:
			case Keys.D3:
			case Keys.D4:
			case Keys.D5:
			case Keys.D6:
			case Keys.D7:
			case Keys.D8:
			case Keys.D9:
				obj = Strings.ChrW((int)checked(48 + (keys2 - 48))).ToString();
				break;
			case Keys.OemMinus:
				obj = "-";
				break;
			case Keys.Oemplus:
				obj = "+";
				break;
			}
			string text2 = (string)obj;
			if (text2 != null)
			{
				toolStripMenuItem.ShortcutKeyDisplayString = (((keys & Keys.Control) == Keys.Control) ? "Ctrl+" : "") + (((keys & Keys.Shift) == Keys.Shift) ? "Shift+" : "") + text2;
			}
		}
		return toolStripMenuItem;
	}

	private static ToolStripButton Button(string text, Image image, EventHandler handler, bool iconOnly = false)
	{
		ToolStripButton toolStripButton = new ToolStripButton(text, image);
		toolStripButton.DisplayStyle = (iconOnly ? ToolStripItemDisplayStyle.Image : ToolStripItemDisplayStyle.ImageAndText);
		toolStripButton.ToolTipText = text;
		toolStripButton.Click += handler;
		return toolStripButton;
	}

	private void OnLibraryActivate(object sender, EventArgs e)
	{
		if (!_running && _library.SelectedItems.Count != 0)
		{
			LibraryPreset libraryPreset = (LibraryPreset)_library.SelectedItems[0].Tag;
			_canvas.PlacingPreset = libraryPreset;
			_statusMessage.Text = $"Click on the drawing area to place: {libraryPreset.Name}  (hold Shift to place several, Esc to cancel)";
			_canvas.Focus();
		}
	}

	private void OnHoverElement(object sender, CircuitElement e)
	{
		string caption = ((e == null) ? null : ComponentHelp.HelpFor(e));
		_tooltip.SetToolTip(_canvas, caption);
	}

	private void FitView()
	{
		_canvas.ZoomToFit();
		UpdateUiState();
	}

	private void SetZoom(float z)
	{
		_canvas.Zoom = z;
		UpdateUiState();
	}

	private void UpdateUiState()
	{
		_btnStart.Enabled = !_running || _paused;
		_menuStart.Enabled = _btnStart.Enabled;
		_btnPause.Enabled = _running;
		_menuPause.Enabled = _running;
		_btnPause.Checked = _paused;
		_btnStop.Enabled = _running;
		_menuStop.Enabled = _running;
		_btnReal.Checked = _realPhysics;
		_menuReal.Checked = _realPhysics;
		_btnRecord.Checked = _gif != null;
		_menuRecord.Checked = _gif != null;
		_menuDark.Checked = AppSettings.DarkMode;
		_library.Enabled = !_running;
		_properties.Enabled = !_running;
		if (_running)
		{
			ShowSimulationStatus();
		}
		else
		{
			_statusMode.Text = "Edit mode" + (_realPhysics ? "  (realistic physics on)" : "");
		}
		_statusZoom.Text = $"Zoom {_canvas.Zoom * 100f:0}%";
		UpdateUndoButtons();
		UpdateReplayUi();
	}

	private void OnSettings(object sender, EventArgs e)
	{
		using SettingsDialog settingsDialog = new SettingsDialog();
		Theme.Apply(settingsDialog);
		if (settingsDialog.ShowDialog(this) == DialogResult.OK)
		{
			settingsDialog.ApplyTo();
			ApplyTheme(AppSettings.ResolveDark());
			UpdateUiState();
			_statusMessage.Text = "Settings saved.";
		}
	}

	private void OnWindowsPreferenceChanged(object sender, Microsoft.Win32.UserPreferenceChangedEventArgs e)
	{
		if (AppSettings.ThemeMode == "System" && AppSettings.ResolveDark() != AppSettings.DarkMode)
		{
			BeginInvoke(new Action(delegate
			{
				ApplyTheme(AppSettings.ResolveDark());
			}));
		}
	}

	private void ApplyTheme(bool dark)
	{
		AppSettings.DarkMode = dark;
		AppSettings.Save();
		ColorScheme scheme = (dark ? ColorScheme.Dark : ColorScheme.Light);
		_canvas.Scheme = scheme;
		_diagram.Scheme = scheme;
		_plotter.Scheme = scheme;
		RenderLibraryImages(scheme);
		Color color = (dark ? Color.FromArgb(37, 39, 46) : SystemColors.Control);
		Color color2 = (dark ? Color.FromArgb(225, 228, 235) : SystemColors.ControlText);
		Color color3 = (dark ? Color.FromArgb(28, 30, 36) : SystemColors.Window);
		Color color4 = (dark ? Color.FromArgb(52, 56, 66) : Color.FromArgb(232, 236, 244));
		BackColor = color;
		ForeColor = color2;
		ThemeControls(base.Controls, color, color2, color3, color4);
		ToolStripProfessionalRenderer renderer = (dark ? new ToolStripProfessionalRenderer(new DarkColorTable()) : new ToolStripProfessionalRenderer());
		ToolStrip[] array = new ToolStrip[3] { _menu, _toolbar, _status };
		foreach (ToolStrip toolStrip in array)
		{
			toolStrip.Renderer = renderer;
			toolStrip.ForeColor = color2;
			foreach (ToolStripItem item in toolStrip.Items)
			{
				ThemeItem(item, color2, color, renderer);
			}
		}
		_properties.ViewBackColor = color3;
		_properties.ViewForeColor = color2;
		_properties.LineColor = color4;
		_properties.CategoryForeColor = color2;
		_properties.HelpBackColor = color;
		_properties.HelpForeColor = color2;
		if (dark)
		{
			try
			{
				SetSplitterColor(color4);
			}
			catch (MissingMethodException ex)
			{
				ProjectData.SetProjectError(ex);
				MissingMethodException ex2 = ex;
				ProjectData.ClearProjectError();
			}
		}
		_speedBox.ComboBox.BackColor = color3;
		_speedBox.ComboBox.ForeColor = color2;
		if (_menuDark != null)
		{
			_menuDark.Checked = dark;
		}
		_canvas.Invalidate();
		_diagram.Invalidate();
		_plotter.Invalidate();
	}

	[MethodImpl(MethodImplOptions.NoInlining)]
	private void SetSplitterColor(Color c)
	{
		_properties.CategorySplitterColor = c;
	}

	private void RenderLibraryImages(ColorScheme scheme)
	{
		ImageList.ImageCollection images = _library.LargeImageList.Images;
		List<Image> list = images.Cast<Image>().ToList();
		images.Clear();
		LibraryPreset[] presets = Library.Presets;
		foreach (LibraryPreset libraryPreset in presets)
		{
			images.Add(Library.RenderThumbnail(libraryPreset.Factory(), 72, 48, scheme));
		}
		foreach (Image item in list)
		{
			item.Dispose();
		}
		_library.Invalidate();
	}

	private void ThemeItem(ToolStripItem it, Color fore, Color back, ToolStripRenderer renderer)
	{
		it.ForeColor = fore;
		if (!(it is ToolStripDropDownItem toolStripDropDownItem))
		{
			return;
		}
		toolStripDropDownItem.DropDown.Renderer = renderer;
		toolStripDropDownItem.DropDown.BackColor = back;
		foreach (ToolStripItem dropDownItem in toolStripDropDownItem.DropDownItems)
		{
			ThemeItem(dropDownItem, fore, back, renderer);
		}
	}

	private void ThemeControls(Control.ControlCollection controls, Color back, Color fore, Color field, Color header)
	{
		foreach (Control control in controls)
		{
			if (!(control is CircuitCanvas) && !(control is ToolStrip))
			{
				if (control is Label && object.Equals(RuntimeHelpers.GetObjectValue(control.Tag), "header"))
				{
					control.BackColor = header;
					control.ForeColor = fore;
				}
				else if (control is TextBox || control is ListBox || control is ListView || control is ComboBox)
				{
					control.BackColor = field;
					control.ForeColor = fore;
				}
				else if (control is Button)
				{
					control.BackColor = back;
					control.ForeColor = fore;
					((Button)control).FlatStyle = ((!AppSettings.DarkMode) ? FlatStyle.Standard : FlatStyle.Flat);
				}
				else if (!(control is PropertyGrid))
				{
					control.BackColor = back;
					control.ForeColor = fore;
				}
				if (control.HasChildren)
				{
					ThemeControls(control.Controls, back, fore, field, header);
				}
			}
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__86_002D0()
	{
		_checkTimer.Stop();
		RunStaticChecks();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__86_002D1()
	{
		ApplyFix(SelectedIssue()?.Fix);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__86_002D2()
	{
		CheckIssue checkIssue = SelectedIssue();
		if (checkIssue != null)
		{
			ApplyFix(CircuitAnalysis.ElectroFix(checkIssue));
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__117_002D0()
	{
		if (_running)
		{
			_simulator.RunLogic();
		}
		_canvas.Invalidate();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D0()
	{
		OnTroubleCommand(null, "start");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D1()
	{
		OnTroubleCommand(null, "hint");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D2()
	{
		OnTroubleCommand(null, "found");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D3()
	{
		OnTroubleCommand(null, "giveup");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D4()
	{
		OnTroubleCommand(null, "add");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D5()
	{
		OnTroubleCommand(null, "clear");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D0()
	{
		ReplayStep(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D1()
	{
		ReplayStep(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D2()
	{
		ReplayEvent(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D3()
	{
		ReplayEvent(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D4()
	{
		if (!_updatingTimeline && _recorder != null)
		{
			PauseForReplay();
			_recorder.Restore(_timeline.Value);
			AfterReplayMove();
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__167_002D0()
	{
		_properties.SelectedObjects = _canvas.SelectedElements.Cast<object>().ToArray();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__167_002D3()
	{
		_statusZoom.Text = $"Zoom {_canvas.Zoom * 100f:0}%";
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__167_002D4()
	{
		if (!_switchingPages)
		{
			ShowPage(_pageTabs.SelectedIndex);
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__168_002D2()
	{
		OnLibraryActivate(null, EventArgs.Empty);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D3()
	{
		_canvas.CutSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D4()
	{
		_canvas.CopySelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D5()
	{
		_canvas.Paste();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D6()
	{
		_canvas.DuplicateSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D7()
	{
		_canvas.SelectAll();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D8()
	{
		_canvas.DeleteSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D9()
	{
		_canvas.RotateSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D13()
	{
		ShowPage(checked(Math.Min(_project.Pages.Count - 1, _pageIndex + 1)));
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D14()
	{
		ShowPage(Math.Max(0, checked(_pageIndex - 1)));
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D15()
	{
		SetZoom(_canvas.Zoom * 1.2f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D16()
	{
		SetZoom(_canvas.Zoom / 1.2f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D17()
	{
		SetZoom(1f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D20()
	{
		AppSettings.ThemeMode = (AppSettings.DarkMode ? "Light" : "Dark");
		ApplyTheme(!AppSettings.DarkMode);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D21()
	{
		ReplayStep(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D22()
	{
		ReplayStep(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D23()
	{
		ReplayEvent(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D24()
	{
		ReplayEvent(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D25()
	{
		ShowBottomTab("Getting started");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D2()
	{
		_canvas.RotateSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D3()
	{
		_canvas.DeleteSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D4()
	{
		SetZoom(_canvas.Zoom * 1.2f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D5()
	{
		SetZoom(_canvas.Zoom / 1.2f);
	}
}
