using System;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;

namespace PneuSim;

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
				select p).ToArray());
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
