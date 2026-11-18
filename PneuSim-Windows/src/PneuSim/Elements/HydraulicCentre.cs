using System.ComponentModel;

namespace PneuSim;

[TypeConverter(typeof(EnumDescriptionConverter))]
public enum HydraulicCentre
{
	[Description("Closed centre (all ports blocked)")]
	Closed,
	[Description("Tandem centre (P to T, A and B blocked)")]
	Tandem,
	[Description("Float centre (A and B to T, P blocked)")]
	Float,
	[Description("Open centre (all ports connected)")]
	Open
}
