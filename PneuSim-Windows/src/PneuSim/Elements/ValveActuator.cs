using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum ValveActuator
{
	[Description("Push button")]
	PushButton,
	[Description("Selector switch (detented)")]
	Selector,
	[Description("Roller lever")]
	RollerLever,
	[Description("Pneumatic pilot")]
	Pilot,
	[Description("Pneumatic pilot with time delay")]
	DelayedPilot,
	[Description("Solenoid")]
	Solenoid,
	[Description("Idle-return roller (one direction only)")]
	IdleReturnRoller
}
