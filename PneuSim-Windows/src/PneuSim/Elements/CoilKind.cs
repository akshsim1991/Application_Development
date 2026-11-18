using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum CoilKind
{
	[Description("Relay")]
	Relay,
	[Description("Timer relay, on-delay")]
	OnDelayTimer,
	[Description("Timer relay, off-delay")]
	OffDelayTimer,
	[Description("Valve solenoid")]
	Solenoid,
	[Description("Indicator lamp")]
	Lamp
}
