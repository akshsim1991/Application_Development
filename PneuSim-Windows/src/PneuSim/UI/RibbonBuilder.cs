using System;
using System.Collections.Generic;
using System.Linq;

namespace PneuSim;

/// <summary>Defines PneuSim's complete ribbon: File, Home, component tabs, Simulate, Tools, View, Learn and Help. Pure layout data, no logic.</summary>
public static class RibbonBuilder
{
	private sealed class Rule
	{
		public string Title;

		public Func<LibraryPreset, bool> Match;
	}

	private static Rule R(string title, Func<LibraryPreset, bool> match)
	{
		return new Rule { Title = title, Match = match };
	}

	private static Func<LibraryPreset, bool> Has(params string[] words)
	{
		return (LibraryPreset p) => words.Any((string w) => p.Name.IndexOf(w, StringComparison.OrdinalIgnoreCase) >= 0);
	}

	private static RibbonBar.Item It(Func<string, RCmd> cmd, ColorScheme sc, string key, string label, string icon, bool small, string tip = null)
	{
		RCmd rCmd = cmd(key);
		if (rCmd == null)
		{
			return null;
		}
		return new RibbonBar.Item
		{
			Key = key,
			Label = label,
			Command = rCmd.Run,
			Enabled = rCmd.Enabled,
			Checked = rCmd.Checked,
			Tip = tip ?? rCmd.Tip ?? label,
			Icon = RibbonGlyphs.Make(icon, sc, small ? 16 : 36),
			IsSmall = small
		};
	}

	private static RibbonBar.Item Drop(Func<string, RCmd> cmd, ColorScheme sc, string label, string icon, Func<List<(string Text, Action Run)>> menu, string tip)
	{
		return new RibbonBar.Item
		{
			Label = label,
			Menu = menu,
			Tip = tip,
			Icon = RibbonGlyphs.Make(icon, sc, 36)
		};
	}

	private static void G(RibbonBar.Tab tab, string title, params RibbonBar.Item[] items)
	{
		RibbonBar.Group group = new RibbonBar.Group { Title = title };
		group.Items.AddRange(items.Where((RibbonBar.Item i) => i != null));
		if (group.Items.Count > 0)
		{
			tab.Groups.Add(group);
		}
	}

	private static RibbonBar.Item Part(string name, ColorScheme scheme)
	{
		LibraryPreset libraryPreset = Library.Presets.FirstOrDefault((LibraryPreset p) => p.Name == name);
		return (libraryPreset == null) ? null : ItemFor(libraryPreset, scheme);
	}

	/// <summary>Fills the ribbon. cmd looks up a command by key (null if it does not exist). examples lists the bundled examples for the Examples drop-down.</summary>
	public static void Build(RibbonBar bar, Func<string, RCmd> cmd, Func<List<(string Text, Action Run)>> examples, ColorScheme sc)
	{
		bar.AddTab("File", isFile: true);
		foreach (string key in new string[5] { "New", "Open", "Save", "Undo", "Redo" })
		{
			string icon = ((key == "New") ? "New" : ((key == "Open") ? "Open" : ((key == "Save") ? "Save" : key)));
			RibbonBar.Item item = It(cmd, sc, key, key, icon, small: true);
			if (item != null)
			{
				bar.QuickAccess.Add(item);
			}
		}
		RibbonBar.Tab tab = bar.AddTab("Home");
		G(tab, "Clipboard", It(cmd, sc, "Paste", "Paste", "Paste", small: false), It(cmd, sc, "Cut", "Cut", "Cut", small: true), It(cmd, sc, "Copy", "Copy", "Copy", small: true), It(cmd, sc, "Duplicate", "Duplicate", "Duplicate", small: true));
		G(tab, "Edit", It(cmd, sc, "Undo", "Undo", "Undo", small: true), It(cmd, sc, "Redo", "Redo", "Redo", small: true), It(cmd, sc, "SelectAll", "Select all", "SelectAll", small: true), It(cmd, sc, "Delete", "Delete", "Delete", small: true), It(cmd, sc, "Rotate", "Rotate 90°", "Rotate", small: true));
		G(tab, "Run", It(cmd, sc, "Start", "Start", "Start", small: false), It(cmd, sc, "Pause", "Pause", "Pause", small: true), It(cmd, sc, "Stop", "Stop", "Stop", small: true), It(cmd, sc, "Real", "Realistic", "Real", small: true));
		G(tab, "View", It(cmd, sc, "Fit", "Fit", "Fit", small: false), It(cmd, sc, "ZoomIn", "Zoom in", "ZoomIn", small: true), It(cmd, sc, "ZoomOut", "Zoom out", "ZoomOut", small: true), It(cmd, sc, "Pan", "Pan", "Pan", small: true), It(cmd, sc, "Grid", "Grid", "Grid", small: true), It(cmd, sc, "ActualSize", "100 %", "ActualSize", small: true));
		G(tab, "Pages", It(cmd, sc, "AddPage", "Add page", "AddPage", small: true), It(cmd, sc, "RenamePage", "Rename page", "RenamePage", small: true), It(cmd, sc, "DeletePage", "Delete page", "DeletePage", small: true), It(cmd, sc, "PrevPage", "Previous page", "PrevPage", small: true), It(cmd, sc, "NextPage", "Next page", "NextPage", small: true));
		G(tab, "Quick tools", It(cmd, sc, "Generator", "Circuit generator", "Generator", small: false), It(cmd, sc, "Check", "Check circuit", "Check", small: false), It(cmd, sc, "Explain", "Explain circuit", "Explain", small: false));
		G(tab, "Quick parts", Part("Compressed air supply", sc), Part("Single-acting cylinder", sc), Part("Double-acting cylinder", sc), Part("3/2 valve, push button, NC", sc), Part("5/2 valve, double solenoid", sc), Part("Push button, NO", sc));
		LibraryPreset[] presets = Library.Presets;
		AddTab(bar, "Supply & Air", Library.CatSupply, sc, presets, R("Air supply", Has("supply", "Compressor", "receiver")), R("Air preparation", Has("Shut-off", "Service unit", "regulator", "Silencer", "junction", "Manifold")), R("Measuring", Has("gauge", "Flow meter", "Force", "switch")));
		AddTab(bar, "Actuators", Library.CatActuators, sc, presets, R("Electric drives", Has("DC motor", "Servo", "Electric cylinder")), R("Cylinders and drives", Has("cylinder", "actuator", "motor")), R("Handling and vacuum", Has("gripper", "Vacuum", "Suction")));
		AddTab(bar, "Valves", null, sc, presets.Where((LibraryPreset p) => p.Category == Library.CatDirectional || p.Category == Library.CatSolenoid).ToArray(), R("Hand and mechanical", Has("push button", "selector", "roller")), R("Pilot and timing", Has("pilot", "delay", "sequence")), R("Solenoid operated", (LibraryPreset p) => p.Category == Library.CatSolenoid));
		AddTab(bar, "Logic & Flow", Library.CatFlow, sc, presets, R("Logic", Has("Shuttle", "Two-pressure")), R("Flow and non-return", Has("flow", "Check", "Quick exhaust")), R("Load holding", Has("Counterbalance")));
		AddTab(bar, "Electrical", Library.CatElectric, sc, presets, R("Power", Has("24 V", "0 V")), R("Switches and sensors", Has("Push button", "Selector", "Emergency", "Proximity", "Limit", "Pressure switch")), R("Relays and outputs", Has("Relay", "Timer", "solenoid", "lamp", "counter")));
		AddTab(bar, "Hydraulics", Library.CatHydraulic, sc, presets, R("Power and tank", Has("power unit", "Tank", "relief", "Accumulator")), R("Cylinders and motors", Has("cylinder", "motor")), R("Valves", Has("valve", "throttle", "Counterbalance")), R("Measuring and pipes", Has("gauge", "junction", "manifold")));
		AddTab(bar, "Annotate", Library.CatDrawing, sc, presets);
		RibbonBar.Tab tab2 = bar.AddTab("Simulate");
		G(tab2, "Run", It(cmd, sc, "Start", "Start", "Start", small: false), It(cmd, sc, "Pause", "Pause", "Pause", small: false), It(cmd, sc, "Stop", "Stop and reset", "Stop", small: false));
		G(tab2, "Replay", It(cmd, sc, "StepBack", "Step back", "StepBack", small: true), It(cmd, sc, "StepFwd", "Step forward", "StepFwd", small: true), It(cmd, sc, "PrevEvent", "Previous event", "PrevEvent", small: true), It(cmd, sc, "NextEvent", "Next event", "NextEvent", small: true));
		G(tab2, "Speed", It(cmd, sc, "Speed0", "0.1x slow", "Speed", small: true), It(cmd, sc, "Speed1", "0.25x", "Speed", small: true), It(cmd, sc, "Speed2", "0.5x", "Speed", small: true), It(cmd, sc, "Speed3", "1x normal", "Speed", small: true), It(cmd, sc, "Speed4", "2x", "Speed", small: true), It(cmd, sc, "Speed5", "4x fast", "Speed", small: true));
		G(tab2, "Options", It(cmd, sc, "Real", "Realistic physics", "Real", small: true), It(cmd, sc, "Record", "Record GIF", "Record", small: true), It(cmd, sc, "Settings", "Automatic operation…", "Settings", small: true));
		G(tab2, "Analyse", It(cmd, sc, "Check", "Check circuit", "Check", small: false), It(cmd, sc, "Explain", "Explain circuit", "Explain", small: false), It(cmd, sc, "PanelGrafcet", "GRAFCET", "Grafcet", small: false), It(cmd, sc, "PanelDiagram", "Step diagram", "Diagram", small: false));
		RibbonBar.Tab tab3 = bar.AddTab("Tools");
		G(tab3, "Design", It(cmd, sc, "Generator", "Circuit generator", "Generator", small: false), It(cmd, sc, "Sizing", "Sizing sheets", "Sizing", small: false), It(cmd, sc, "Calculators", "Calculators", "Calculators", small: false));
		G(tab3, "Documents", It(cmd, sc, "Parts", "Parts list and costs", "Parts", small: false), It(cmd, sc, "Sweep", "Parameter sweep", "Sweep", small: false), It(cmd, sc, "Pdf", "Print preview / PDF", "Pdf", small: false));
		G(tab3, "Check", It(cmd, sc, "Check", "Check circuit", "Check", small: false), It(cmd, sc, "Explain", "Explain circuit", "Explain", small: false));
		G(tab3, "Fault finding", It(cmd, sc, "NewFault", "New task", "Fault", small: false), It(cmd, sc, "Hint", "Hint", "Hint", small: true), It(cmd, sc, "Found", "I found it…", "Found", small: true), It(cmd, sc, "ShowFault", "Show the fault", "ShowFault", small: true), It(cmd, sc, "AddFault", "Add fault…", "AddFault", small: true), It(cmd, sc, "Repair", "Repair all", "Repair", small: true));
		G(tab3, "Look inside", It(cmd, sc, "Cutaway", "Cutaway", "Cutaway", small: false), It(cmd, sc, "CutawayAll", "Whole workspace", "CutawayAll", small: false));
		G(tab3, "PneuSim", It(cmd, sc, "Settings", "Settings", "Settings", small: false));
		RibbonBar.Tab tab4 = bar.AddTab("View");
		G(tab4, "Zoom", It(cmd, sc, "Fit", "Fit", "Fit", small: false), It(cmd, sc, "ZoomIn", "Zoom in", "ZoomIn", small: true), It(cmd, sc, "ZoomOut", "Zoom out", "ZoomOut", small: true), It(cmd, sc, "ActualSize", "100 %", "ActualSize", small: true), It(cmd, sc, "Pan", "Pan (hand)", "Pan", small: true));
		G(tab4, "Show", It(cmd, sc, "Grid", "Grid", "Grid", small: true), It(cmd, sc, "Labels", "Live value labels", "Labels", small: true), It(cmd, sc, "Dark", "Dark mode", "Dark", small: true));
		G(tab4, "Panels", It(cmd, sc, "BottomPanel", "Bottom panel on/off", "Diagram", small: true), It(cmd, sc, "PanelDiagram", "Step diagram", "Diagram", small: true), It(cmd, sc, "PanelGrafcet", "GRAFCET", "Grafcet", small: true), It(cmd, sc, "PanelPlotter", "Plotter", "Plotter", small: true), It(cmd, sc, "PanelCheck", "Check circuit", "CheckPanel", small: true), It(cmd, sc, "PanelExplain", "Explain", "ExplainPanel", small: true), It(cmd, sc, "PanelTutorial", "Getting started", "Tutorial", small: true), It(cmd, sc, "PanelInspector", "Inspector", "Inspector", small: true), It(cmd, sc, "PanelFault", "Fault finding", "Fault", small: true));
		G(tab4, "Ribbon", It(cmd, sc, "Collapse", "Collapse ribbon", "Collapse", small: true));
		RibbonBar.Tab tab5 = bar.AddTab("Learn");
		G(tab5, "Learn", It(cmd, sc, "PanelTutorial", "Getting started", "Tutorial", small: false), It(cmd, sc, "Cutaway", "Cutaway", "Cutaway", small: false), It(cmd, sc, "CutawayAll", "Whole workspace", "CutawayAll", small: false), Drop(cmd, sc, "Examples", "Examples", examples, "Open one of the example circuits in a new tab"));
		RibbonBar.Tab tab6 = bar.AddTab("Help");
		G(tab6, "Help", It(cmd, sc, "Guide", "Quick guide (PDF)", "Guide", small: false), It(cmd, sc, "Update", "Update to new version", "Update", small: false), It(cmd, sc, "About", "About PneuSim", "About", small: false));
		bar.IndexForSearch();
		bar.SelectTab("Home");
	}

	private static RibbonBar.Item ItemFor(LibraryPreset preset, ColorScheme scheme)
	{
		return new RibbonBar.Item
		{
			Label = preset.Name,
			Preset = preset,
			Icon = Library.RenderThumbnail(preset.Factory(), 62, 42, scheme)
		};
	}

	private static void AddTab(RibbonBar bar, string name, string category, ColorScheme scheme, LibraryPreset[] source, params Rule[] rules)
	{
		RibbonBar.Tab tab = bar.AddTab(name);
		List<LibraryPreset> list = ((category == null) ? source.ToList() : source.Where((LibraryPreset p) => p.Category == category).ToList());
		List<LibraryPreset> list2 = new List<LibraryPreset>(list);
		foreach (Rule rule in rules)
		{
			RibbonBar.Group group = new RibbonBar.Group { Title = rule.Title };
			foreach (LibraryPreset item in list2.Where(rule.Match).ToList())
			{
				group.Items.Add(ItemFor(item, scheme));
				list2.Remove(item);
			}
			if (group.Items.Count > 0)
			{
				tab.Groups.Add(group);
			}
		}
		if (list2.Count > 0)
		{
			RibbonBar.Group group2 = new RibbonBar.Group { Title = (rules.Length == 0) ? name : "Other" };
			foreach (LibraryPreset item2 in list2)
			{
				group2.Items.Add(ItemFor(item2, scheme));
			}
			tab.Groups.Add(group2);
		}
	}
}
