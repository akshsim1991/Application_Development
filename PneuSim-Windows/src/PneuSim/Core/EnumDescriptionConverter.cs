using System;
using System.ComponentModel;
using System.Globalization;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace PneuSim;

public class EnumDescriptionConverter : EnumConverter
{
	public EnumDescriptionConverter(Type type)
		: base(type)
	{
	}

	private static string Describe(object value)
	{
		FieldInfo field = value.GetType().GetField(value.ToString());
		DescriptionAttribute descriptionAttribute = (((object)field == null) ? null : ((DescriptionAttribute)Attribute.GetCustomAttribute(field, typeof(DescriptionAttribute))));
		if (descriptionAttribute != null)
		{
			return descriptionAttribute.Description;
		}
		return value.ToString();
	}

	public override object ConvertTo(ITypeDescriptorContext context, CultureInfo culture, object value, Type destinationType)
	{
		if ((object)destinationType == typeof(string) && value != null && value.GetType().IsEnum)
		{
			return Describe(RuntimeHelpers.GetObjectValue(value));
		}
		return base.ConvertTo(context, culture, RuntimeHelpers.GetObjectValue(value), destinationType);
	}

	public override object ConvertFrom(ITypeDescriptorContext context, CultureInfo culture, object value)
	{
		if (value is string b)
		{
			foreach (object value2 in Enum.GetValues(base.EnumType))
			{
				object objectValue = RuntimeHelpers.GetObjectValue(value2);
				if (string.Equals(Describe(RuntimeHelpers.GetObjectValue(objectValue)), b, StringComparison.OrdinalIgnoreCase))
				{
					return objectValue;
				}
			}
		}
		return base.ConvertFrom(context, culture, RuntimeHelpers.GetObjectValue(value));
	}
}
