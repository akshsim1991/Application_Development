using System;
using System.Collections.Generic;
using System.Linq;

namespace PneuSim;

/// <summary>Defines PneuSim's ribbon tabs and groups from the component library. Pure layout data, no logic.</summary>
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

	/// <summary>Fills the ribbon. commands maps a command name to its action (Generator, Check, Explain, Parts, Sweep, Cutaway, Calculators, Settings, NewFault).</summary>
	public static void Build(RibbonBar bar, Dictionary<string, Action> commands, ColorScheme scheme)
	{
		LibraryPreset[] all = Library.Presets;
		Func<string, RibbonBar.Item> thumb = (string name) => ItemFor(all.First((LibraryPreset p) => p.Name == name), scheme);
		RibbonBar.Tab tab = bar.AddTab("Home");
		AddNames(tab, "Air supply", thumb, "Compressed air supply", "Service unit", "Pressure gauge");
		AddNames(tab, "Actuators", thumb, "Single-acting cylinder", "Double-acting cylinder");
		AddNames(tab, "Valves", thumb, "3/2 valve, push button, NC", "5/2 valve, double solenoid", "One-way flow control valve", "Shuttle valve (OR)");
		AddNames(tab, "Electrical", thumb, "Push button, NO", "Relay coil", "Valve solenoid");
		AddCommands(bar, tab, "Quick tools", commands, scheme, ("Circuit generator", "Generator"), ("Check circuit", "Check"), ("Explain circuit", "Explain"));
		AddTab(bar, "Supply & Air", Library.CatSupply, scheme, all, R("Air supply", Has("supply", "Compressor", "receiver")), R("Air preparation", Has("Shut-off", "Service unit", "regulator", "Silencer", "junction")), R("Measuring", Has("gauge", "Flow meter", "Force", "switch")));
		AddTab(bar, "Actuators", Library.CatActuators, scheme, all, R("Cylinders and drives", Has("cylinder", "actuator", "motor")), R("Handling and vacuum", Has("gripper", "Vacuum", "Suction")));
		AddTab(bar, "Valves", null, scheme, all.Where((LibraryPreset p) => p.Category == Library.CatDirectional || p.Category == Library.CatSolenoid).ToArray(), R("Hand and mechanical", Has("push button", "selector", "roller")), R("Pilot and timing", Has("pilot", "delay", "sequence")), R("Solenoid operated", (LibraryPreset p) => p.Category == Library.CatSolenoid));
		AddTab(bar, "Logic & Flow", Library.CatFlow, scheme, all, R("Logic", Has("Shuttle", "Two-pressure")), R("Flow and non-return", Has("flow", "Check", "Quick exhaust")));
		AddTab(bar, "Electrical", Library.CatElectric, scheme, all, R("Power", Has("24 V", "0 V")), R("Switches and sensors", Has("Push button", "Selector", "Emergency", "Proximity", "Limit", "Pressure switch")), R("Relays and outputs", Has("Relay", "Timer", "solenoid", "lamp", "counter")));
		AddTab(bar, "Hydraulics", Library.CatHydraulic, scheme, all, R("Power and tank", Has("power unit", "Tank", "relief", "Accumulator")), R("Cylinders and motors", Has("cylinder", "motor")), R("Valves", Has("valve", "throttle", "Counterbalance")), R("Measuring and pipes", Has("gauge", "junction")));
		AddTab(bar, "Drawing", Library.CatDrawing, scheme, all);
		RibbonBar.Tab tab2 = bar.AddTab("Tools");
		AddCommands(bar, tab2, "Design", commands, scheme, ("Circuit generator", "Generator"), ("Calculators", "Calculators"), ("Parts list and costs", "Parts"), ("Parameter sweep", "Sweep"));
		AddCommands(bar, tab2, "Check", commands, scheme, ("Check circuit", "Check"), ("Explain circuit", "Explain"), ("Cutaway view", "Cutaway"));
		AddCommands(bar, tab2, "Fault finding", commands, scheme, ("New fault-finding task", "NewFault"));
		AddCommands(bar, tab2, "PneuSim", commands, scheme, ("Settings", "Settings"));
		bar.IndexForSearch();
		bar.SelectTab("Home");
	}

	/// <summary>Redraws all icons for a new colour scheme.</summary>
	public static void Refresh(RibbonBar bar, ColorScheme scheme)
	{
		bar.SetDark(scheme.IsDark);
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

	private static void AddNames(RibbonBar.Tab tab, string title, Func<string, RibbonBar.Item> thumb, params string[] names)
	{
		RibbonBar.Group group = new RibbonBar.Group { Title = title };
		foreach (string arg in names)
		{
			group.Items.Add(thumb(arg));
		}
		tab.Groups.Add(group);
	}

	private static void AddCommands(RibbonBar bar, RibbonBar.Tab tab, string title, Dictionary<string, Action> commands, ColorScheme scheme, params (string Label, string Key)[] items)
	{
		RibbonBar.Group group = new RibbonBar.Group { Title = title };
		foreach (var (label, key) in items)
		{
			if (commands.TryGetValue(key, out var value))
			{
				group.Items.Add(new RibbonBar.Item
				{
					Label = label,
					Command = value,
					Icon = RibbonIcons.Command(key, scheme)
				});
			}
		}
		tab.Groups.Add(group);
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
