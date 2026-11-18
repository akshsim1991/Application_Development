using System;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;

namespace PneuSim;

/// <summary>Shows pressure properties (stored in bar) in the pressure unit chosen in Settings.</summary>
internal sealed class UnitProperty : PropertyDescriptor
{
	private readonly PropertyDescriptor _p;

	private readonly double _factor;

	private readonly string _unit;

	private UnitProperty(PropertyDescriptor p, double factor, string unit)
		: base(p)
	{
		_p = p;
		_factor = factor;
		_unit = unit;
	}

	public static PropertyDescriptor Wrap(PropertyDescriptor p)
	{
		string unit = AppSettings.PressureUnit;
		if (unit == "bar" || p.DisplayName == null || !p.DisplayName.Contains("(bar)") || (p.PropertyType != typeof(double) && p.PropertyType != typeof(float)))
		{
			return p;
		}
		return new UnitProperty(p, (unit == "MPa") ? 0.1 : 100.0, unit);
	}

	public override string DisplayName => _p.DisplayName.Replace("(bar)", "(" + _unit + ")");

	public override Type ComponentType => _p.ComponentType;

	public override bool IsReadOnly => _p.IsReadOnly;

	public override Type PropertyType => typeof(double);

	public override bool CanResetValue(object component)
	{
		return false;
	}

	public override object GetValue(object component)
	{
		return Math.Round(Convert.ToDouble(_p.GetValue(component)) * _factor, 4);
	}

	public override void ResetValue(object component)
	{
	}

	public override void SetValue(object component, object value)
	{
		double v = Convert.ToDouble(value) / _factor;
		_p.SetValue(component, (_p.PropertyType == typeof(float)) ? (object)(float)v : (object)v);
	}

	public override bool ShouldSerializeValue(object component)
	{
		return false;
	}
}

public class ElementDescriptionProvider : TypeDescriptionProvider
{
	private class ElementTypeDescriptor : CustomTypeDescriptor
	{
		private readonly CircuitElement _element;

		public ElementTypeDescriptor(ICustomTypeDescriptor parent, CircuitElement element)
			: base(parent)
		{
			_element = element;
		}

		public override PropertyDescriptorCollection GetProperties()
		{
			return Filter(base.GetProperties());
		}

		public override PropertyDescriptorCollection GetProperties(Attribute[] attributes)
		{
			return Filter(base.GetProperties(attributes));
		}

		private PropertyDescriptorCollection Filter(PropertyDescriptorCollection all)
		{
			if (_element == null)
			{
				return all;
			}
			return new PropertyDescriptorCollection((from PropertyDescriptor p in all
				where _element.ShowProperty(p.Name) && (p.Name != "RatedPressureBar" || (AppSettings.AdvancedMode && _element.FluidKind().HasValue))
				select UnitProperty.Wrap(p)).ToArray());
		}
	}

	public ElementDescriptionProvider()
		: base(TypeDescriptor.GetProvider(typeof(object)))
	{
	}

	public override ICustomTypeDescriptor GetTypeDescriptor(Type objectType, object instance)
	{
		return new ElementTypeDescriptor(base.GetTypeDescriptor(objectType, RuntimeHelpers.GetObjectValue(instance)), instance as CircuitElement);
	}
}
