using System.Collections.Generic;

namespace PneuSim;

public interface ISignalSource
{
	bool IsSignalOn(string name);

	IEnumerable<string> SignalNames();
}
