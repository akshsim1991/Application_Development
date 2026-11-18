using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum RegulatorStyle
{
	[Description("Pressure regulator")]
	Regulator,
	[Description("Service unit (filter, regulator, gauge)")]
	ServiceUnit
}
