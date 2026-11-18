using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum Polarity
{
	[Description("+24 V")]
	Plus24V,
	[Description("0 V")]
	Zero0V
}
