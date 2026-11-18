using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class AppSettings
{
	public static bool DarkMode { get; set; }

	public static bool RealPhysics { get; set; }

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
				$"RealPhysics={(RealPhysics ? 1 : 0)}"
			};
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
