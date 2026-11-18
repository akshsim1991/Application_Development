using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum CentrePosition
{
	[Description("Closed centre")]
	Closed,
	[Description("Exhaust centre")]
	Exhausted,
	[Description("Pressure centre")]
	Pressurized
}
