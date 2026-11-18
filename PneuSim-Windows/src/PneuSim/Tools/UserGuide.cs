using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;

namespace PneuSim;

/// <summary>Builds the PneuSim user guide as a PDF: what is new, how to use the program, and a picture and explanation of every component.</summary>
public static class UserGuide
{
	private sealed class Writer
	{
		public readonly PdfDocument Doc = new PdfDocument();

		public readonly List<PdfPage> Pages = new List<PdfPage>();

		public PdfPage Page;

		public float Y;

		public const float W = 595f;

		public const float H = 842f;

		public const float Margin = 54f;

		public static readonly Color Ink = Color.FromArgb(30, 38, 46);

		public static readonly Color Accent = Color.FromArgb(0, 118, 132);

		public static readonly Color Faint = Color.FromArgb(110, 120, 132);

		public void NewPage()
		{
			Page = Doc.AddPage(595f, 842f);
			Pages.Add(Page);
			Y = 64f;
		}

		public void Need(float height)
		{
			if (Page == null || Y + height > 842f - 60f)
			{
				NewPage();
			}
		}

		public static List<string> Wrap(string text, float size, float width, bool bold = false)
		{
			List<string> list = new List<string>();
			float num = size * (bold ? 0.56f : 0.52f);
			int num2 = Math.Max(10, (int)(width / num));
			string[] array = text.Replace("\r", "").Split('\n');
			foreach (string text2 in array)
			{
				if (text2.Length == 0)
				{
					list.Add("");
					continue;
				}
				StringBuilder stringBuilder = new StringBuilder();
				string[] array2 = text2.Split(' ');
				foreach (string text3 in array2)
				{
					if (stringBuilder.Length > 0 && stringBuilder.Length + 1 + text3.Length > num2)
					{
						list.Add(stringBuilder.ToString());
						stringBuilder.Clear();
					}
					if (stringBuilder.Length > 0)
					{
						stringBuilder.Append(' ');
					}
					stringBuilder.Append(text3);
				}
				list.Add(stringBuilder.ToString());
			}
			return list;
		}

		public void Chapter(string title)
		{
			NewPage();
			Page.Text(54f, Y + 18f, 22f, title, bold: true, Accent);
			Page.Path(new PointF[2]
			{
				new PointF(54f, Y + 28f),
				new PointF(541f, Y + 28f)
			}, closed: false, Accent, 1.5f, dashed: false, default(Color));
			Y += 52f;
		}

		public void Heading(string title)
		{
			Need(40f);
			Y += 8f;
			Page.Text(54f, Y + 12f, 13f, title, bold: true, Ink);
			Y += 24f;
		}

		public void Para(string text, float size = 10f, Color? color = null, float indent = 0f)
		{
			foreach (string item in Wrap(text, size, 487f - indent))
			{
				Need(size + 6f);
				Page.Text(54f + indent, Y + size, size, item, bold: false, color ?? Ink);
				Y += size + 4f;
			}
			Y += 3f;
		}

		public void Bullet(string text, float size = 10f)
		{
			List<string> list = Wrap(text, size, 470f);
			for (int i = 0; i < list.Count; i++)
			{
				Need(size + 6f);
				if (i == 0)
				{
					Page.Text(58f, Y + size, size, "-", bold: true, Accent);
				}
				Page.Text(72f, Y + size, size, list[i], bold: false, Ink);
				Y += size + 4f;
			}
			Y += 1.5f;
		}
	}

	private static string Version => AppInfo.Full;

	/// <summary>Creates the guide at the given path. drawSymbols=false skips the component pictures (used by tests on systems without a drawing engine).</summary>
	public static void Create(string path, bool drawSymbols = true)
	{
		Writer w = new Writer();
		w.NewPage();
		w.Page.Rect(0f, 0f, 595f, 250f, default(Color), 0f, Color.FromArgb(0, 118, 132));
		w.Page.Text(54f, 120f, 44f, AppInfo.Name, bold: true, Color.White);
		w.Page.Text(54f, 150f, 16f, "Version " + AppInfo.Version + " \"" + AppInfo.CodeName + "\"", bold: false, Color.White);
		w.Page.Text(54f, 186f, 12f, "User guide: what is new, how to use it, and every component explained", bold: false, Color.White);
		w.Y = 290f;
		w.Para("PneuSim designs, simulates and checks pneumatic, electro-pneumatic and hydraulic circuits. Draw the circuit, press Start and watch it work, then let PneuSim check it for mistakes and explain it in plain English before anything is built.", 12f);
		w.Y += 8f;
		w.Heading("Contents");
		string[] array = new string[10] { "1  What is new in this version", "2  Quick start", "3  The screen", "4  Simulating and measuring", "5  Tools: generator, check, explain, parts, sweep, calculators, cutaway", "6  Fault finding", "7  Settings, units, sounds and rated pressure", "8  Files, recovery and updates", "9  Keyboard shortcuts", "10  Component reference (every component, with its symbol)" };
		string[] array2 = array;
		foreach (string text in array2)
		{
			w.Para(text, 11f, null, 10f);
		}
		w.Y += 20f;
		w.Para("Created by " + AppInfo.Developer + ".  Contact: " + AppInfo.DeveloperEmail, 10f, Writer.Faint);
		w.Chapter("1  What is new in " + AppInfo.Version + " \"" + AppInfo.CodeName + "\"");
		w.Heading("A new, simpler look");
		w.Bullet("Ribbon: all components are now on tabs (Home, Supply & Air, Actuators, Valves, Logic & Flow, Electrical, Hydraulics, Drawing, Tools). Click a symbol and then click the drawing, or drag it there. Use the search box at the top right to find any component by name.");
		w.Bullet("Start screen: new circuit, open, drop a file onto it, recent circuits and the examples.");
		w.Bullet("Light, Dark or Follow Windows theme. Pressure in bar, MPa or kPa.");
		w.Bullet("Properties stay in the panel on the right; nothing pops up when you double-click a part.");
		w.Heading("Safety and awareness");
		w.Bullet("Rated maximum pressure per component (10 bar air, 160 bar oil, -0.95 bar vacuum by default). If it is exceeded the part gets a red frame, the status bar explains it and an alarm sounds.");
		w.Bullet("Sounds, on by default and synthesised by PneuSim: valve click, cylinder end-stop thud, exhaust hiss, compressor and pump hum, relief-valve chatter and the alarm. Everything that makes a sound is also shown on screen. Switch sounds on or off, set the volume and mute single sounds in Settings.");
		w.Heading("Fault finding for maintenance teams");
		w.Bullet("Put faults into a circuit and find them by measuring, or prepare a hidden fault for a colleague. Hidden faults are now stored encrypted.");
		w.Heading("Reliability");
		w.Bullet("Saving is safe: PneuSim writes and checks a temporary file, keeps your previous version as a .bak copy, and only then replaces the file.");
		w.Bullet("Damaged files and files from a newer version give a clear message. Errors are written to a log on your computer, with a Copy error report button; nothing is sent anywhere.");
		w.Bullet("Automatic updates (Help > Update to new version): checked once a day, verified by checksum, installed in place.");
		w.Chapter("2  Quick start");
		w.Bullet("Start PneuSim. On the start screen choose an example or New circuit.");
		w.Bullet("Pick a component from the ribbon and click on the drawing. Place a supply, a cylinder and a valve.");
		w.Bullet("Drag from one port (the small circle on a part) to another to connect them with a tube. Red circles are electrical terminals.");
		w.Bullet("Press Start (F9). Click push buttons, switches and pumps with the mouse. Pressurised tubes turn blue, live wires red, oil lines orange.");
		w.Bullet("Press Check (F6) to find mistakes, Explain (F8) to read how the circuit works, and Stop when you are done.");
		w.Bullet("Save with Ctrl+S. Your previous version is kept as a .bak file next to it.");
		w.Chapter("3  The screen");
		w.Bullet("Menu bar and toolbar at the top: file, edit, simulation, speed, realistic physics, record GIF, zoom.");
		w.Bullet("Ribbon: component tabs. Long groups have a More button. The search box finds components by name.");
		w.Bullet("Drawing area in the middle, with page tabs for multi-page circuits (join pages with page connectors of the same name).");
		w.Bullet("Properties panel on the right: select a part to see and change all its properties. Hover over a property for its explanation.");
		w.Bullet("Bottom tabs: Displacement-step diagram, Plotter, Check circuit, Explain, Getting started, Inspector, Fault finding.");
		w.Bullet("Status bar: mode, messages, zoom and the version.");
		w.Chapter("4  Simulating and measuring");
		w.Bullet("Realistic physics (toolbar) makes pressures build up and cylinders move according to bore, load and friction. Air use and its cost are shown.");
		w.Bullet("Right-click any part or tube while simulating to see its live values in the Inspector, and change settings with sliders.");
		w.Bullet("Replay: the step and event buttons, or the time slider, go back and forward in time.");
		w.Bullet("Plotter: tick any quantity, click for cursor A, right-click for cursor B (minimum, maximum, average). Set a trigger, zoom with the mouse wheel, export CSV or PNG.");
		w.Bullet("Record GIF saves an animation of the running circuit.");
		w.Bullet("Live value labels: choose a value (such as force, velocity or pressure) for a part in its properties and it is written next to the part while simulating. Switch all labels on or off under View.");
		w.Chapter("5  Tools");
		w.Bullet("Circuit generator (Ctrl+G): type a cylinder sequence such as A+ B+ B- A-, choose electro-pneumatic or pneumatic, and PneuSim draws the circuit and explains it.");
		w.Bullet("Check circuit (F6): finds unconnected ports, missing exhausts, signal overlap and more; click a result to jump to the part.");
		w.Bullet("Explain circuit (F8): a step-by-step description of what happens when you operate the circuit.");
		w.Bullet("Parts list and costs: every part with price, running cost of air per year, and a PDF report with a title block.");
		w.Bullet("Parameter sweep: run the circuit with one setting changed step by step and compare cycle time, air use and speed.");
		w.Bullet("Calculators: cylinder force, air consumption, tube sizing and more.");
		w.Bullet("Cutaway view (F7): an animated look inside any component.");
		w.Chapter("6  Fault finding");
		w.Bullet("Tools > Fault finding > New fault-finding task hides one fault in the circuit. Run it, measure with right-click, ask for hints, then say where the fault is.");
		w.Bullet("Add fault puts a chosen fault into the selected part or tube. Tick Hidden to save a file that does not show the fault.");
		w.Bullet("Fault kinds include leak, blocked, stuck, burnt coil, open or welded contact, sticking, jammed and low output.");
		w.Chapter("7  Settings, units, sounds and rated pressure");
		w.Bullet("Open Settings with Ctrl+, (Tools > Settings).");
		w.Bullet("Theme: Light, Dark or Follow Windows (default).");
		w.Bullet("Pressure unit on drawing labels: bar, MPa or kPa. Values in the Properties panel are entered in bar.");
		w.Bullet("Simple / Advanced mode: Advanced shows engineering properties such as the rated maximum pressure.");
		w.Bullet("Sounds: master switch, volume and one switch for each sound.");
		w.Bullet("Updates: switch the daily check on or off.");
		w.Chapter("8  Files, recovery and updates");
		w.Bullet("Circuits are saved as .pneu files. Several pages live in one file.");
		w.Bullet("PneuSim autosaves every minute and offers to restore your work after a crash.");
		w.Bullet("If a file is damaged, look for the .bak copy next to it and rename it to end in .pneu.");
		w.Bullet("Help > Update to new version checks for a newer release. Your work is saved first; PneuSim closes, updates and restarts.");
		w.Bullet("The error log is in %LOCALAPPDATA%\\PneuSim\\logs. It stays on your computer.");
		w.Chapter("9  Keyboard shortcuts");
		(string, string)[] array3 = new(string, string)[32]
		{
			("F1", "This guide"),
			("F6", "Check my circuit"),
			("F7", "Cutaway view of the selected part"),
			("F8", "Explain my circuit"),
			("F9", "Start the simulation"),
			("F10", "Pause"),
			("F11", "Stop and reset"),
			("F12 / Shift+F12", "Step forward / back in time"),
			("Ctrl+F12 / Ctrl+Shift+F12", "Forward / back to the next / previous event"),
			("Ctrl+G", "Circuit generator"),
			("Ctrl+,", "Settings"),
			("Ctrl+N / Ctrl+O / Ctrl+S", "New / open / save"),
			("Ctrl+P", "PDF report with title block"),
			("Ctrl+Z / Ctrl+Y", "Undo / redo"),
			("Ctrl+X / Ctrl+C / Ctrl+V", "Cut / copy / paste"),
			("Ctrl+D", "Duplicate"),
			("Ctrl+A", "Select all"),
			("Ctrl+H", "Hint (fault finding)"),
			("Ctrl+PgDn / Ctrl+PgUp", "Next / previous page"),
			("Ctrl++ / Ctrl+-", "Zoom in / out"),
			("Ctrl+0", "Actual size (100%)"),
			("Ctrl+9", "Zoom to fit the whole circuit"),
			("R", "Rotate the selected part"),
			("Del", "Delete the selection"),
			("Arrow keys", "Nudge the selection"),
			("Shift + click", "Place several copies of a component"),
			("Esc", "Cancel placing"),
			("Right-click (running)", "Live values of a part or tube"),
			("Mouse wheel (plotter)", "Zoom the plot"),
			("Mouse wheel (ribbon)", "Scroll a long ribbon tab"),
			("Drag a port to a port", "Connect with a tube"),
			("Drag a box", "Select several parts")
		};
		(string, string)[] array4 = array3;
		for (int j = 0; j < array4.Length; j++)
		{
			(string, string) tuple = array4[j];
			w.Need(18f);
			w.Page.Text(60f, w.Y + 11f, 10f, tuple.Item1, bold: true, Writer.Accent);
			w.Page.Text(200f, w.Y + 11f, 10f, tuple.Item2, bold: false, Writer.Ink);
			w.Y += 17f;
		}
		w.Chapter("10  Component reference");
		w.Para("Every component in the ribbon, with its symbol and what it does. Symbols are drawn by PneuSim and are based on ISO 1219 (fluid power) and IEC 60617 (electrical) conventions.", 10f, Writer.Faint);
		string text2 = null;
		foreach (LibraryPreset preset in Library.Presets)
		{
			CircuitElement circuitElement;
			string text3;
			try
			{
				circuitElement = preset.Factory();
				text3 = ComponentHelp.HelpFor(circuitElement);
			}
			catch (Exception)
			{
				continue;
			}
			int num = text3.IndexOf('\n');
			string text4 = ((num >= 0) ? text3.Substring(num + 1).Trim() : text3);
			List<string> list = Writer.Wrap(text4, 9.5f, 380f);
			float num2 = Math.Max(78f, 22f + (float)list.Count * 13.5f);
			if (preset.Category != text2)
			{
				text2 = preset.Category;
				w.Need(70f + num2);
				w.Heading(text2);
			}
			w.Need(num2 + 10f);
			w.Page.Rect(54f, w.Y, 100f, num2 - 4f, Color.FromArgb(215, 220, 226), 0.75f, Color.FromArgb(250, 251, 252));
			if (drawSymbols)
			{
				DrawSymbol(w.Page, circuitElement, new RectangleF(58f, w.Y + 4f, 92f, num2 - 12f));
			}
			w.Page.Text(166f, w.Y + 12f, 11f, preset.Name, bold: true, Writer.Ink);
			float num3 = w.Y + 28f;
			foreach (string item in list)
			{
				w.Page.Text(166f, num3, 9.5f, item, bold: false, Writer.Ink);
				num3 += 13.5f;
			}
			w.Y += num2 + 6f;
		}
		for (int k = 0; k < w.Pages.Count; k++)
		{
			if (k > 0)
			{
				w.Pages[k].Text(54f, 812f, 8f, Version + "   |   User guide", bold: false, Writer.Faint);
				w.Pages[k].Text(520f, 812f, 8f, (k + 1).ToString(), bold: false, Writer.Faint);
			}
		}
		w.Doc.Save(path);
	}

	private static void DrawSymbol(PdfPage page, CircuitElement e, RectangleF area)
	{
		try
		{
			VectorSurface vectorSurface = new VectorSurface();
			using RenderContext r = new RenderContext();
			e.DrawSymbol(vectorSurface, r);
			VectorExport.DrawShapes(page, vectorSurface, area);
		}
		catch (Exception ex)
		{
			LastSymbolError = ex.GetType().Name + ": " + ex.Message;
		}
	}

	public static string LastSymbolError;

	/// <summary>Path of the guide for this version in the temp folder; created if missing.</summary>
	public static string EnsureGuide()
	{
		string text = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "PneuSim-Guide-" + AppInfo.Version + ".pdf");
		if (!System.IO.File.Exists(text) || System.IO.File.GetLastWriteTime(text) < AppInfo.BuildDate)
		{
			Create(text);
		}
		return text;
	}
}
