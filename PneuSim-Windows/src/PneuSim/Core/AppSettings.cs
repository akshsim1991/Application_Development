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
		return ThemeMode == "Dark" || (ThemeMode == "System" && WindowsPrefersDark());
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
					if (text3 == "Light" || text3 == "Dark" || text3 == "System")
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
