using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Linq;

namespace PneuSim;

/// <summary>Offers the live values of a component (position, speed, force, pressure, flow...) as choices for its live value label. Several can be typed separated by semicolons.</summary>
public sealed class ValueLabelConverter : StringConverter
{
	public override bool GetStandardValuesSupported(ITypeDescriptorContext context)
	{
		return context?.Instance is CircuitElement;
	}

	public override bool GetStandardValuesExclusive(ITypeDescriptorContext context)
	{
		return false;
	}

	public override StandardValuesCollection GetStandardValues(ITypeDescriptorContext context)
	{
		List<string> list = new List<string> { "" };
		if (context?.Instance is CircuitElement circuitElement)
		{
			list.AddRange(circuitElement.InspectValues().Select(((string Name, string Value) v) => v.Name).Where((string n) => n != "Completed cycles"));
		}
		return new StandardValuesCollection(list);
	}
}
