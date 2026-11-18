namespace PneuSim;

public interface IHydraulicConsumer
{
	double LoadPressure { get; }

	bool WantsFlow();
}
