namespace PneuSim;

public class PartLine
{
	public string Description { get; set; }

	public string Labels { get; set; }

	public int Quantity { get; set; }

	public double UnitPrice { get; set; }

	public double Total => (double)Quantity * UnitPrice;
}
