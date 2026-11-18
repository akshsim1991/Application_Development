using System;
using System.Globalization;
using System.IO;
using System.Net;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Security.Cryptography;
using System.Text;

namespace PneuSim;

/// <summary>What the server publishes in latest.json.</summary>
[DataContract]
public sealed class UpdateManifest
{
	[DataMember(Name = "version")]
	public string Version { get; set; }

	[DataMember(Name = "codename")]
	public string CodeName { get; set; }

	[DataMember(Name = "released")]
	public string Released { get; set; }

	[DataMember(Name = "installerUrl")]
	public string InstallerUrl { get; set; }

	[DataMember(Name = "sha256")]
	public string Sha256 { get; set; }

	[DataMember(Name = "notes")]
	public string Notes { get; set; }

	public static UpdateManifest Parse(string json)
	{
		using MemoryStream stream = new MemoryStream(Encoding.UTF8.GetBytes(json));
		UpdateManifest updateManifest = (UpdateManifest)new DataContractJsonSerializer(typeof(UpdateManifest)).ReadObject(stream);
		if (updateManifest == null || !System.Version.TryParse(updateManifest.Version, out var _) || string.IsNullOrEmpty(updateManifest.InstallerUrl) || string.IsNullOrEmpty(updateManifest.Sha256))
		{
			throw new InvalidDataException("The update information is incomplete.");
		}
		if (!updateManifest.InstallerUrl.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
		{
			throw new InvalidDataException("The update must be downloaded over HTTPS.");
		}
		updateManifest.Sha256 = updateManifest.Sha256.Trim().ToLowerInvariant();
		return updateManifest;
	}
}

/// <summary>Checks for, downloads and verifies a new PneuSim installer. No personal data is sent.</summary>
public static class UpdateService
{
	/// <summary>Where latest.json is looked for, in order (GitHub first, then a mirror).</summary>
	public static readonly string[] ManifestUrls = new string[2] { "https://raw.githubusercontent.com/akshsim1991/PneuSim-releases/main/latest.json", "https://cdn.jsdelivr.net/gh/akshsim1991/PneuSim-releases@main/latest.json" };

	public static Version Current => System.Version.Parse(AppInfo.Version);

	public static bool IsNewer(string candidate, string current)
	{
		return System.Version.TryParse(candidate, out var result) && System.Version.TryParse(current, out var result2) && result > result2;
	}

	public static string Sha256Of(string path)
	{
		using FileStream inputStream = File.OpenRead(path);
		using SHA256 sHA = SHA256.Create();
		byte[] array = sHA.ComputeHash(inputStream);
		StringBuilder stringBuilder = new StringBuilder();
		byte[] array2 = array;
		foreach (byte b in array2)
		{
			stringBuilder.Append(b.ToString("x2", CultureInfo.InvariantCulture));
		}
		return stringBuilder.ToString();
	}

	private static WebClient NewClient()
	{
		ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072;
		WebClient webClient = new WebClient
		{
			Encoding = Encoding.UTF8
		};
		webClient.Headers[HttpRequestHeader.UserAgent] = AppInfo.Name + "/" + AppInfo.Version;
		webClient.Headers[HttpRequestHeader.CacheControl] = "no-cache";
		return webClient;
	}

	/// <summary>Downloads latest.json (tries each address) and returns the manifest if a newer version exists, otherwise null. Throws if no address can be reached.</summary>
	public static UpdateManifest CheckForNewer()
	{
		Exception ex = null;
		int num404 = 0;
		string[] manifestUrls = ManifestUrls;
		foreach (string text in manifestUrls)
		{
			try
			{
				using WebClient webClient = NewClient();
				string text2 = webClient.DownloadString(text + "?t=" + DateTime.UtcNow.Ticks);
				UpdateManifest updateManifest = UpdateManifest.Parse(text2);
				return IsNewer(updateManifest.Version, AppInfo.Version) ? updateManifest : null;
			}
			catch (Exception ex2)
			{
				ex = ex2;
				if (ex2 is WebException ex3 && ex3.Response is HttpWebResponse httpWebResponse && httpWebResponse.StatusCode == HttpStatusCode.NotFound)
				{
					num404++;
				}
			}
		}
		if (num404 == ManifestUrls.Length)
		{
			return null;
		}
		throw new WebException("Could not reach the update server. " + ex?.Message, ex);
	}

	public static string DownloadFolder => Path.Combine(Path.GetTempPath(), "PneuSim-update");

	/// <summary>Starts an asynchronous download of the installer and returns the path it will be saved to.</summary>
	public static string StartDownload(UpdateManifest m, WebClient client)
	{
		Directory.CreateDirectory(DownloadFolder);
		string text = Path.Combine(DownloadFolder, "PneuSim-Setup-" + m.Version + ".exe");
		if (File.Exists(text))
		{
			File.Delete(text);
		}
		client.DownloadFileAsync(new Uri(m.InstallerUrl), text);
		return text;
	}

	public static WebClient CreateClient()
	{
		return NewClient();
	}

	/// <summary>True if the downloaded file has exactly the SHA-256 published in the manifest.</summary>
	public static bool Verify(string path, UpdateManifest m)
	{
		return File.Exists(path) && string.Equals(Sha256Of(path), m.Sha256, StringComparison.Ordinal);
	}

	/// <summary>Command-line options for the silent in-place upgrade (same installer AppId).</summary>
	public const string InstallerArguments = "/SILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /NORESTARTAPPLICATIONS";
}
