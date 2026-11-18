using System;
using System.ComponentModel;
using System.Linq;
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
