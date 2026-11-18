using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum ContactOperator
{
	[Description("Push button")]
	PushButton,
	[Description("Selector switch (detented)")]
	Selector,
	[Description("Relay / timer relay")]
	Relay,
	[Description("Proximity sensor")]
	ProximitySensor,
	[Description("Limit switch (roller)")]
	LimitSwitch,
	[Description("Pressure switch")]
	PressureSwitch,
	[Description("Emergency stop (latching)")]
	EmergencyStop
}
