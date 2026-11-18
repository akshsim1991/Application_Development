using System;
using System.ComponentModel;
using System.Globalization;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Xml.Linq;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class ProjectInfo
{
	[Category("Title block")]
	[DisplayName("Title")]
	public string Title { get; set; }

	[Category("Title block")]
	[DisplayName("Drawn by")]
	public string Author { get; set; }

	[Category("Title block")]
	[DisplayName("Company / institute")]
	public string Company { get; set; }

	[Category("Title block")]
	[DisplayName("Drawing number")]
	public string DrawingNumber { get; set; }

	[Category("Title block")]
	[DisplayName("Revision")]
	public string Revision { get; set; }

	[Category("Title block")]
	[DisplayName("Date")]
	public string DateText { get; set; }

	[Category("Title block")]
	[DisplayName("Description")]
	public string Description { get; set; }

	[Category("Running cost")]
	[DisplayName("Compressed air cost (₹ per m³)")]
	[Description("Cost of producing one cubic metre of free air, typically ₹1.5–3 including electricity.")]
	public double AirCostPerM3 { get; set; }

	[Category("Running cost")]
	[DisplayName("Cycles per minute")]
	public double CyclesPerMinute { get; set; }

	[Category("Running cost")]
	[DisplayName("Hours per day")]
	public double HoursPerDay { get; set; }

	[Category("Running cost")]
	[DisplayName("Days per year")]
	public double DaysPerYear { get; set; }

	public ProjectInfo()
	{
		Title = "Untitled circuit";
		Author = "";
		Company = "";
		DrawingNumber = "";
		Revision = "A";
		DateText = DateTime.Today.ToString("dd-MM-yyyy", CultureInfo.InvariantCulture);
		Description = "";
		AirCostPerM3 = 2.0;
		CyclesPerMinute = 2.0;
		HoursPerDay = 8.0;
		DaysPerYear = 300.0;
	}

	internal XElement ToXElement()
	{
		CultureInfo invariantCulture = CultureInfo.InvariantCulture;
		XElement xElement = new XElement("Info");
		PropertyInfo[] properties = typeof(ProjectInfo).GetProperties();
		foreach (PropertyInfo propertyInfo in properties)
		{
			xElement.Add(new XAttribute(propertyInfo.Name, Convert.ToString(RuntimeHelpers.GetObjectValue(propertyInfo.GetValue(this)), invariantCulture)));
		}
		return xElement;
	}

	internal static ProjectInfo FromXElement(XElement x)
	{
		ProjectInfo projectInfo = new ProjectInfo();
		if (x == null)
		{
			return projectInfo;
		}
		PropertyInfo[] properties = typeof(ProjectInfo).GetProperties();
		foreach (PropertyInfo propertyInfo in properties)
		{
			XAttribute xAttribute = x.Attribute(propertyInfo.Name);
			if (xAttribute != null)
			{
				try
				{
					propertyInfo.SetValue(projectInfo, RuntimeHelpers.GetObjectValue(Convert.ChangeType(xAttribute.Value, propertyInfo.PropertyType, CultureInfo.InvariantCulture)));
				}
				catch (FormatException ex)
				{
					ProjectData.SetProjectError(ex);
					FormatException ex2 = ex;
					ProjectData.ClearProjectError();
				}
			}
		}
		return projectInfo;
	}
}
