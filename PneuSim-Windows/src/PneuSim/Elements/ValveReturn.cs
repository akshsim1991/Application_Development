using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum ValveReturn
{
	[Description("Spring")]
	Spring,
	[Description("Pneumatic pilot")]
	Pilot,
	[Description("Solenoid")]
	Solenoid
}
