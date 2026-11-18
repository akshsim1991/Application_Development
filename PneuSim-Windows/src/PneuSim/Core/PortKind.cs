using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum PortKind
{
	[Description("Compressed air")]
	Pneumatic,
	[Description("Electric (24 V)")]
	Electric,
	[Description("Hydraulic oil")]
	Hydraulic
}
