using System;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Faults
{
	public static string Describe(FaultKind kind)
	{
		DescriptionAttribute descriptionAttribute = (DescriptionAttribute)Attribute.GetCustomAttribute(typeof(FaultKind).GetField(kind.ToString()), typeof(DescriptionAttribute));
		if (descriptionAttribute != null)
		{
			return descriptionAttribute.Description;
		}
		return kind.ToString();
	}

	private static byte[][] Keys()
	{
		using Rfc2898DeriveBytes rfc2898DeriveBytes = new Rfc2898DeriveBytes("PneuSim hidden fault v2 / " + AppInfo.DeveloperEmail, new byte[8] { 112, 110, 101, 117, 70, 108, 116, 50 }, 5000);
		return new byte[2][]
		{
			rfc2898DeriveBytes.GetBytes(32),
			rfc2898DeriveBytes.GetBytes(32)
		};
	}

	/// <summary>Encrypts a hidden fault (AES-256-CBC with a random IV, authenticated with HMAC-SHA256). The key is built into the program, so this keeps the answer out of reach of casual reading and editing, not of a determined reverse engineer.</summary>
	public static string Protect(FaultKind kind)
	{
		byte[][] array = Keys();
		using Aes aes = Aes.Create();
		aes.Key = array[0];
		aes.GenerateIV();
		byte[] bytes = Encoding.UTF8.GetBytes("ps2-" + kind);
		byte[] array2;
		using (ICryptoTransform cryptoTransform = aes.CreateEncryptor())
		{
			array2 = cryptoTransform.TransformFinalBlock(bytes, 0, bytes.Length);
		}
		byte[] array3 = aes.IV.Concat(array2).ToArray();
		using HMACSHA256 hMACSHA = new HMACSHA256(array[1]);
		return Convert.ToBase64String(array3.Concat(hMACSHA.ComputeHash(array3)).ToArray());
	}

	/// <summary>Reads a value written by Protect. Returns None if it is damaged or was changed.</summary>
	public static FaultKind Unprotect(string text)
	{
		try
		{
			byte[] array = Convert.FromBase64String(text);
			if (array.Length < 16 + 16 + 32)
			{
				return FaultKind.None;
			}
			byte[][] array2 = Keys();
			byte[] array3 = array.Take(array.Length - 32).ToArray();
			byte[] a = array.Skip(array.Length - 32).ToArray();
			using HMACSHA256 hMACSHA = new HMACSHA256(array2[1]);
			byte[] b = hMACSHA.ComputeHash(array3);
			int num = 0;
			for (int i = 0; i < 32; i++)
			{
				num |= a[i] ^ b[i];
			}
			if (num != 0)
			{
				return FaultKind.None;
			}
			using Aes aes = Aes.Create();
			aes.Key = array2[0];
			aes.IV = array3.Take(16).ToArray();
			string @string;
			using (ICryptoTransform cryptoTransform = aes.CreateDecryptor())
			{
				@string = Encoding.UTF8.GetString(cryptoTransform.TransformFinalBlock(array3, 16, array3.Length - 16));
			}
			if (@string.StartsWith("ps2-") && Enum.TryParse<FaultKind>(@string.Substring(4), out var result))
			{
				return result;
			}
		}
		catch (Exception ex) when (ex is FormatException || ex is CryptographicException || ex is ArgumentException)
		{
		}
		return FaultKind.None;
	}

	public static string Scramble(FaultKind kind)
	{
		char[] chars = ("ps-" + kind).Reverse().ToArray();
		return Convert.ToBase64String(Encoding.UTF8.GetBytes(chars));
	}

	public static FaultKind Unscramble(string text)
	{
		try
		{
			string text2 = new string(Encoding.UTF8.GetString(Convert.FromBase64String(text)).Reverse().ToArray());
			if (!text2.StartsWith("ps-"))
			{
				return FaultKind.None;
			}
			if (Enum.TryParse<FaultKind>(text2.Substring(3), out var result))
			{
				return result;
			}
		}
		catch (FormatException ex)
		{
			ProjectData.SetProjectError(ex);
			FormatException ex2 = ex;
			ProjectData.ClearProjectError();
		}
		return FaultKind.None;
	}

	public static string ShortName(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.Leak => "leaking", 
			FaultKind.Blocked => "blocked", 
			FaultKind.StuckNormal => "stuck", 
			FaultKind.StuckOperated => "stuck on", 
			FaultKind.BurntCoil => "coil burnt", 
			FaultKind.ContactOpen => "no contact", 
			FaultKind.ContactWelded => "welded", 
			FaultKind.Sticking => "sticking", 
			FaultKind.Jammed => "jammed", 
			FaultKind.LowOutput => "low output", 
			_ => "", 
		};
	}

	public static double SpeedFactor(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.Jammed => 0.0, 
			FaultKind.Sticking => 0.3, 
			FaultKind.Leak => 0.4, 
			_ => 1.0, 
		};
	}
}
