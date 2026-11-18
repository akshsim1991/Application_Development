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

public partial class MainForm
{

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
		_checkList.SelectedIndexChanged += OnIssueActivated;
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
			_properties.Resize += delegate
			{
				WidenPropertyNames();
			};
			_properties.SelectedObjectsChanged += delegate
			{
				WidenPropertyNames();
			};
			Panel value2 = TitledPanel("Properties", _properties);
			Panel panel = new Panel
			{
				Dock = DockStyle.Fill
			};
			panel.Controls.Add(_canvas);
			panel.Controls.Add(_pageTabs);
			panel.Controls.Add(_docTabs);
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
			AddBottomTab("GRAFCET", _grafcet);
			_grafcet.GenerateRequested += RefreshGrafcet;
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
			_status.Items.AddRange(new ToolStripItem[4]
			{
				_statusMode,
				_statusMessage,
				_statusZoom,
				new ToolStripStatusLabel(AppInfo.Full)
			});
			CS_0024_003C_003E8__locals0._0024VB_0024Local_mainSplit.Panel1Collapsed = true;
			base.Controls.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_mainSplit);
			BuildRibbon();
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
		toolStripMenuItem.DropDownItems.Add(Item("&Close", OnClose, Keys.W | Keys.Control));
		toolStripMenuItem.DropDownItems.Add(Item("Close a&ll", OnCloseAll, Keys.W | Keys.Control | Keys.Shift));
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
		ToolStripMenuItem valueLabelsItem = Item("Live &value labels", null);
		valueLabelsItem.CheckOnClick = true;
		valueLabelsItem.Checked = AppSettings.ShowValueLabels;
		valueLabelsItem.Click += delegate
		{
			AppSettings.ShowValueLabels = valueLabelsItem.Checked;
			AppSettings.Save();
			_canvas.Invalidate();
		};
		toolStripMenuItem12.DropDownItems.Add(valueLabelsItem);
		ToolStripMenuItem gridItem = Item("Show &grid", null, Keys.Control | Keys.OemQuotes);
		gridItem.CheckOnClick = true;
		gridItem.Checked = AppSettings.ShowGrid;
		gridItem.Click += delegate
		{
			AppSettings.ShowGrid = gridItem.Checked;
			AppSettings.Save();
			_canvas.Invalidate();
		};
		toolStripMenuItem12.DropDownItems.Add(gridItem);
		_menuGrid = gridItem;
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
		toolStripMenuItem14.DropDownItems.Add(Item("Sizing &sheets (bore, tube, valve, pump, motor, accumulator)...", OnSizing));
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
		toolStripMenuItem15.DropDownItems.Add(Item("Cutaway: &whole workspace (all components together)", OnCutawayWhole, Keys.F7 | Keys.Shift));
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
		toolStripMenuItem17.DropDownItems.Add(Item("&Update to new version...", OnUpdateCheck));
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
		ToolStripButton panButton = new ToolStripButton("Pan", Icons.PanIcon(), null)
		{
			CheckOnClick = true,
			DisplayStyle = ToolStripItemDisplayStyle.Image,
			ToolTipText = "Pan (hand tool): drag to move the view. Middle mouse button, Space + drag and touchpad swipes also pan."
		};
		panButton.Click += delegate
		{
			_canvas.PanMode = panButton.Checked;
		};
		toolStrip.Items.Add(panButton);
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
			case Keys.Oemcomma:
				obj = ",";
				break;
			case Keys.OemPeriod:
				obj = ".";
				break;
			case Keys.Next:
				obj = "PgDn";
				break;
			case Keys.Prior:
				obj = "PgUp";
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

	private void BuildRibbon()
	{
		_ribbon = new RibbonBar();
		_ribbonCommands = new Dictionary<string, Action>
		{
			["Generator"] = delegate { OnGenerator(null, EventArgs.Empty); },
			["Check"] = delegate { OnFullCheck(null, EventArgs.Empty); },
			["Explain"] = delegate { OnExplain(null, EventArgs.Empty); },
			["Calculators"] = delegate { OnCalculators(null, EventArgs.Empty); },
			["Sizing"] = delegate { OnSizing(null, EventArgs.Empty); },
			["Parts"] = delegate { OnPartsList(null, EventArgs.Empty); },
			["Sweep"] = delegate { OnSweep(null, EventArgs.Empty); },
			["Cutaway"] = delegate { OnCutaway(null, EventArgs.Empty); },
			["CutawayAll"] = delegate { OnCutawayWhole(null, EventArgs.Empty); },
			["Settings"] = delegate { OnSettings(null, EventArgs.Empty); },
			["NewFault"] = delegate { StartExercise(); }
		};
		RibbonBuilder.Build(_ribbon, _ribbonCommands, ColorScheme.Light);
		_ribbon.PresetChosen += delegate(LibraryPreset preset)
		{
			HideStartScreen();
			if (_running)
			{
				_statusMessage.Text = "Stop the simulation to place components.";
				return;
			}
			_canvas.PlacingPreset = preset;
			_statusMessage.Text = $"Click in the workspace to place: {preset.Name}  (hold Shift to place several, Esc to cancel)";
			_canvas.Focus();
		};
		base.Controls.Add(_ribbon);
	}
}
