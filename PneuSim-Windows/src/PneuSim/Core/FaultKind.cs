using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum FaultKind
{
	[Description("No fault")]
	None,
	[Description("Leaking")]
	Leak,
	[Description("Blocked")]
	Blocked,
	[Description("Stuck in the normal position")]
	StuckNormal,
	[Description("Stuck in the operated position")]
	StuckOperated,
	[Description("Coil burnt out")]
	BurntCoil,
	[Description("Contact does not close")]
	ContactOpen,
	[Description("Contact stuck closed")]
	ContactWelded,
	[Description("Sticking (high friction)")]
	Sticking,
	[Description("Jammed")]
	Jammed,
	[Description("Low output")]
	LowOutput
}
