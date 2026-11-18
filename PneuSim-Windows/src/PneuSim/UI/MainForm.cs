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

public partial class MainForm : Form
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

	private bool _alarmShown;

	private bool _alarmWasShown;

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

	private RibbonBar _ribbon;

	private Dictionary<string, Action> _ribbonCommands;
}
