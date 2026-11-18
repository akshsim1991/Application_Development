using System;
using System.IO;
using System.Text;

namespace PneuSim;

/// <summary>Local error log in %LOCALAPPDATA%\PneuSim\logs. Nothing is ever sent anywhere.</summary>
public static class ErrorLog
{
	public static string Folder => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "PneuSim", "logs");

	/// <summary>Writes the error to today's log file and returns a report text the user can copy.</summary>
	public static string Write(Exception ex, string context = null)
	{
		string text = BuildReport(ex, context);
		try
		{
			Directory.CreateDirectory(Folder);
			string path = Path.Combine(Folder, "pneusim-" + DateTime.Now.ToString("yyyyMMdd") + ".log");
			File.AppendAllText(path, text + "\r\n----\r\n", Encoding.UTF8);
			TrimOldLogs();
		}
		catch (Exception)
		{
		}
		return text;
	}

	public static string BuildReport(Exception ex, string context)
	{
		StringBuilder stringBuilder = new StringBuilder();
		stringBuilder.AppendLine(AppInfo.Full + "  " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
		stringBuilder.AppendLine("Windows: " + Environment.OSVersion.VersionString + "   .NET: " + Environment.Version);
		if (!string.IsNullOrEmpty(context))
		{
			stringBuilder.AppendLine("Context: " + context);
		}
		stringBuilder.AppendLine((ex == null) ? "(no exception)" : ex.ToString());
		return stringBuilder.ToString();
	}

	private static void TrimOldLogs()
	{
		string[] files = Directory.GetFiles(Folder, "pneusim-*.log");
		Array.Sort(files);
		for (int i = 0; i < files.Length - 14; i++)
		{
			File.Delete(files[i]);
		}
	}
}
