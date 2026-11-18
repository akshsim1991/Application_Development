using System;
using System.IO;
using System.Reflection;

namespace PneuSim;

/// <summary>Product name, version and contact details, in one place.</summary>
public static class AppInfo
{
	public const string Name = "PneuSim";

	public const string Version = "3.3.10";

	public const string ShortVersion = "3.3";

	public const string CodeName = "Mistral";

	public const string DeveloperEmail = "akshsimha@gmail.com";

	public const string Developer = "Akshaya Simha";

	/// <summary>For example: PneuSim 3.3.0 Mistral</summary>
	public static string Full => "PneuSim " + Version + " " + CodeName;

	/// <summary>Build date taken from the program file.</summary>
	public static DateTime BuildDate
	{
		get
		{
			try
			{
				return File.GetLastWriteTime(Assembly.GetExecutingAssembly().Location);
			}
			catch (Exception)
			{
				return DateTime.Now;
			}
		}
	}
}
