using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Globalization;
using System.Runtime.CompilerServices;
using Microsoft.Win32;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class AppSettings
{
	public static bool DarkMode { get; set; }

	public static bool RealPhysics { get; set; }

	/// <summary>Light, Dark or System (follow Windows, the default).</summary>
	public static string ThemeMode { get; set; } = "System";

	/// <summary>Pressure unit shown to the user: bar (default), MPa or kPa.</summary>
	public static string PressureUnit { get; set; } = "bar";

	public static bool AdvancedMode { get; set; }

	/// <summary>True when the pure-black (AMOLED) theme is chosen.</summary>
	public static bool Amoled => ThemeMode == "Amoled";

	/// <summary>Automatic operation: Off, Once (press the start button for you) or Continuous (repeat the cycle).</summary>
	public static string AutoMode { get; set; } = "Once";

	/// <summary>Seconds after Start before the automatic start button press.</summary>
	public static double AutoStartDelay { get; set; } = 0.5;

	/// <summary>Seconds of rest between cycles in Continuous mode.</summary>
	public static double AutoCyclePause { get; set; } = 1.5;

	/// <summary>Seconds to hold a start button that must stay pressed (0 = work it out from the circuit).</summary>
	public static double AutoHoldSeconds { get; set; }

	/// <summary>Simulation speed at start-up, index into 0.1x, 0.25x, 0.5x, 1x, 2x, 4x.</summary>
	public static int DefaultSpeedIndex { get; set; } = 3;

	/// <summary>True: automatic operation also operates solenoid valves that have no coil in the circuit (their manual override). False: the user clicks them.</summary>
	public static bool AutoSolenoids { get; set; } = true;

	public static bool ShowHoverValues { get; set; } = true;

	/// <summary>Minutes between automatic recovery saves; 0 = off.</summary>
	public static int AutosaveMinutes { get; set; } = 1;

	public static bool ShowStartScreen { get; set; } = true;

	public static bool SnapToGrid { get; set; } = true;

	public static bool ShowGrid { get; set; } = true;

	/// <summary>Off, Numbers, Letters or Both: port names drawn at every port in the Workspace.</summary>
	public static string PortLabelMode { get; set; } = "Off";

	/// <summary>Also show the classic menu bar and toolbar above the ribbon (applies after restart).</summary>
	public static bool ClassicMenu { get; set; }

	public static bool RibbonCollapsed { get; set; }

	/// <summary>Currency symbol for costs (parts list, air cost).</summary>
	public static string CurrencySymbol { get; set; } = "₹";

	/// <summary>Show live value labels next to components while simulating (View menu).</summary>
	public static bool ShowValueLabels { get; set; } = true;

	/// <summary>Most recently used files, newest first (at most 8).</summary>
	public static List<string> RecentFiles { get; } = new List<string>();

	public static void AddRecent(string path)
	{
		if (string.IsNullOrEmpty(path))
		{
			return;
		}
		RecentFiles.RemoveAll((string p) => string.Equals(p, path, StringComparison.OrdinalIgnoreCase));
		RecentFiles.Insert(0, path);
		if (RecentFiles.Count > 8)
		{
			RecentFiles.RemoveRange(8, RecentFiles.Count - 8);
		}
		Save();
	}

	public static bool CheckForUpdates { get; set; } = true;

	/// <summary>When the last automatic update check ran (UTC ticks).</summary>
	public static long LastUpdateCheckUtcTicks { get; set; }

	public static bool SoundsOn { get; set; } = true;

	/// <summary>Master volume, 0 to 100.</summary>
	public static int SoundVolume { get; set; } = 70;

	/// <summary>Sound names switched off individually (valve, thud, hiss, hum, relief, alarm).</summary>
	public static HashSet<string> MutedSounds { get; } = new HashSet<string>();

	/// <summary>True when Windows is set to dark app mode (false on systems without the setting).</summary>
	public static bool WindowsPrefersDark()
	{
		try
		{
			using RegistryKey registryKey = Registry.CurrentUser.OpenSubKey("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
			object value = registryKey?.GetValue("AppsUseLightTheme");
			return value is int num && num == 0;
		}
		catch (Exception)
		{
			return false;
		}
	}

	/// <summary>The dark/light decision after applying ThemeMode.</summary>
	public static bool ResolveDark()
	{
		return ThemeMode == "Dark" || ThemeMode == "Amoled" || (ThemeMode == "System" && WindowsPrefersDark());
	}

	public static bool IsSoundEnabled(string name)
	{
		return SoundsOn && !MutedSounds.Contains(name);
	}

	public static HashSet<string> CompletedLessons { get; } = new HashSet<string>();


	private static string FilePath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "PneuSim", "settings.ini");

	public static void Load()
	{
		try
		{
			if (!File.Exists(FilePath))
			{
				return;
			}
			string[] array = File.ReadAllLines(FilePath);
			foreach (string text in array)
			{
				int num = text.IndexOf('=');
				if (num <= 0)
				{
					continue;
				}
				string text2 = text.Substring(0, num).Trim();
				string text3 = text.Substring(checked(num + 1)).Trim();
				switch (text2)
				{
				case "DarkMode":
					DarkMode = Operators.CompareString(text3, "1", TextCompare: false) == 0;
					break;
				case "RealPhysics":
					RealPhysics = Operators.CompareString(text3, "1", TextCompare: false) == 0;
					break;
				case "ThemeMode":
					if (text3 == "Light" || text3 == "Dark" || text3 == "System" || text3 == "Amoled")
					{
						ThemeMode = text3;
					}
					break;
				case "PressureUnit":
					if (text3 == "bar" || text3 == "MPa" || text3 == "kPa")
					{
						PressureUnit = text3;
					}
					break;
				case "Recent":
					if (text3 != "" && RecentFiles.Count < 8)
					{
						RecentFiles.Add(text3);
					}
					break;
				case "ValueLabels":
					ShowValueLabels = text3 != "0";
					break;
				case "AutoMode":
					if (text3 == "Off" || text3 == "Once" || text3 == "Continuous")
					{
						AutoMode = text3;
					}
					break;
				case "AutoStartDelay":
					if (double.TryParse(text3, NumberStyles.Float, CultureInfo.InvariantCulture, out var autoDelay))
					{
						AutoStartDelay = Math.Max(0.0, Math.Min(30.0, autoDelay));
					}
					break;
				case "AutoCyclePause":
					if (double.TryParse(text3, NumberStyles.Float, CultureInfo.InvariantCulture, out var autoPause))
					{
						AutoCyclePause = Math.Max(0.0, Math.Min(120.0, autoPause));
					}
					break;
				case "AutoHold":
					if (double.TryParse(text3, NumberStyles.Float, CultureInfo.InvariantCulture, out var autoHold))
					{
						AutoHoldSeconds = Math.Max(0.0, Math.Min(120.0, autoHold));
					}
					break;
				case "DefaultSpeed":
					if (int.TryParse(text3, NumberStyles.Integer, CultureInfo.InvariantCulture, out var speedIndex))
					{
						DefaultSpeedIndex = Math.Max(0, Math.Min(5, speedIndex));
					}
					break;
				case "AutoSolenoids":
					AutoSolenoids = text3 != "0";
					break;
				case "HoverValues":
					ShowHoverValues = text3 != "0";
					break;
				case "AutosaveMinutes":
					if (int.TryParse(text3, NumberStyles.Integer, CultureInfo.InvariantCulture, out var autosaveMinutes))
					{
						AutosaveMinutes = Math.Max(0, Math.Min(60, autosaveMinutes));
					}
					break;
				case "StartScreen":
					ShowStartScreen = text3 != "0";
					break;
				case "Snap":
					SnapToGrid = text3 != "0";
					break;
				case "ClassicMenu":
					ClassicMenu = text3 == "1";
					break;
				case "RibbonCollapsed":
					RibbonCollapsed = text3 == "1";
					break;
				case "PortLabels":
					if (text3 == "Off" || text3 == "Numbers" || text3 == "Letters" || text3 == "Both")
					{
						PortLabelMode = text3;
					}
					break;
				case "Grid":
					ShowGrid = text3 != "0";
					break;
				case "Currency":
					if (text3.Length > 0 && text3.Length <= 4)
					{
						CurrencySymbol = text3;
					}
					break;
				case "Advanced":
					AdvancedMode = text3 == "1";
					break;
				case "CheckForUpdates":
					CheckForUpdates = text3 != "0";
					break;
				case "LastUpdateCheck":
					if (long.TryParse(text3, NumberStyles.Integer, CultureInfo.InvariantCulture, out var result2))
					{
						LastUpdateCheckUtcTicks = result2;
					}
					break;
				case "Sounds":
					SoundsOn = text3 != "0";
					break;
				case "Volume":
					if (int.TryParse(text3, NumberStyles.Integer, CultureInfo.InvariantCulture, out var result))
					{
						SoundVolume = Math.Max(0, Math.Min(100, result));
					}
					break;
				case "MutedSound":
					if (text3 != "")
					{
						MutedSounds.Add(text3);
					}
					break;
				case "Lesson":
					if (Operators.CompareString(text3, "", TextCompare: false) != 0)
					{
						CompletedLessons.Add(text3);
					}
					break;
				}
			}
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ProjectData.ClearProjectError();
		}
	}

	public static void Save()
	{
		try
		{
			Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
			List<string> list = new List<string>
			{
				$"DarkMode={(DarkMode ? 1 : 0)}",
				$"RealPhysics={(RealPhysics ? 1 : 0)}",
				"ThemeMode=" + ThemeMode,
				"PressureUnit=" + PressureUnit,
				$"Advanced={(AdvancedMode ? 1 : 0)}",
				"AutoMode=" + AutoMode,
				"AutoStartDelay=" + AutoStartDelay.ToString(CultureInfo.InvariantCulture),
				"AutoCyclePause=" + AutoCyclePause.ToString(CultureInfo.InvariantCulture),
				"AutoHold=" + AutoHoldSeconds.ToString(CultureInfo.InvariantCulture),
				"DefaultSpeed=" + DefaultSpeedIndex.ToString(CultureInfo.InvariantCulture),
				$"AutoSolenoids={(AutoSolenoids ? 1 : 0)}",
				$"HoverValues={(ShowHoverValues ? 1 : 0)}",
				"AutosaveMinutes=" + AutosaveMinutes.ToString(CultureInfo.InvariantCulture),
				$"StartScreen={(ShowStartScreen ? 1 : 0)}",
				$"Snap={(SnapToGrid ? 1 : 0)}",
				$"Grid={(ShowGrid ? 1 : 0)}",
				"PortLabels=" + PortLabelMode,
				$"ClassicMenu={(ClassicMenu ? 1 : 0)}",
				$"RibbonCollapsed={(RibbonCollapsed ? 1 : 0)}",
				"Currency=" + CurrencySymbol,
				$"ValueLabels={(ShowValueLabels ? 1 : 0)}",
				$"CheckForUpdates={(CheckForUpdates ? 1 : 0)}",
				"LastUpdateCheck=" + LastUpdateCheckUtcTicks.ToString(CultureInfo.InvariantCulture),
				$"Sounds={(SoundsOn ? 1 : 0)}",
				"Volume=" + SoundVolume.ToString(CultureInfo.InvariantCulture)
			};
			list.AddRange(RecentFiles.Select([SpecialName] (string r) => "Recent=" + r));
			list.AddRange(MutedSounds.Select([SpecialName] (string m) => "MutedSound=" + m));
			list.AddRange(CompletedLessons.Select([SpecialName] (string l) => "Lesson=" + l));
			File.WriteAllLines(FilePath, list);
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ProjectData.ClearProjectError();
		}
	}
}
