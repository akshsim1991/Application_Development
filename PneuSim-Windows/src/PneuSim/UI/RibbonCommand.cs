using System;

namespace PneuSim;

/// <summary>A command the ribbon (and the File view) can run: what to do, whether it is available, and whether it is switched on.</summary>
public sealed class RCmd
{
	public Action Run;

	public Func<bool> Enabled;

	public Func<bool> Checked;

	public string Tip;
}
