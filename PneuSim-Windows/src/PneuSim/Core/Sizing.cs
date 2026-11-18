using System;
using System.Linq;

namespace PneuSim;

/// <summary>
/// Sizing calculations for pneumatic and hydraulic components. Results use generic standard sizes (no brand or part numbers).
/// Guidance values only: always check against the manufacturer's data and the applicable standards.
/// </summary>
public static class Sizing
{
	/// <summary>Common cylinder bores in mm (ISO 6432 / ISO 15552 / ISO 3320 series).</summary>
	public static readonly double[] StandardBoresMm = new double[17]
	{
		8.0, 10.0, 12.0, 16.0, 20.0, 25.0, 32.0, 40.0, 50.0, 63.0,
		80.0, 100.0, 125.0, 160.0, 200.0, 250.0, 320.0
	};

	/// <summary>Common rod diameters in mm.</summary>
	public static readonly double[] StandardRodsMm = new double[14]
	{
		4.0, 4.0, 6.0, 6.0, 8.0, 10.0, 12.0, 16.0, 20.0, 20.0,
		25.0, 25.0, 32.0, 40.0
	};

	/// <summary>Pneumatic push-in tube outer diameters and inner diameters, mm.</summary>
	public static readonly (double Od, double Id)[] PneumaticTubes = new(double, double)[8]
	{
		(4.0, 2.5),
		(6.0, 4.0),
		(8.0, 6.0),
		(10.0, 7.5),
		(12.0, 9.0),
		(16.0, 12.0),
		(22.0, 17.0),
		(28.0, 22.0)
	};

	/// <summary>Hydraulic steel tube outer and inner diameters, mm.</summary>
	public static readonly (double Od, double Id)[] HydraulicTubes = new(double, double)[10]
	{
		(8.0, 5.0),
		(10.0, 7.0),
		(12.0, 8.0),
		(15.0, 11.0),
		(18.0, 14.0),
		(22.0, 17.0),
		(28.0, 22.0),
		(35.0, 28.0),
		(42.0, 34.0),
		(54.0, 44.0)
	};

	/// <summary>Valve and fitting sizes: thread, and nominal flow (NL/min of free air at 6 bar with 1 bar drop) of a typical valve of that size.</summary>
	public static readonly (string Port, double Nominal)[] PneumaticValves = new(string, double)[8]
	{
		("M5", 100.0),
		("G1/8", 250.0),
		("G1/4", 600.0),
		("G3/8", 1200.0),
		("G1/2", 2400.0),
		("G3/4", 4500.0),
		("G1", 7500.0),
		("G1 1/4", 12000.0)
	};

	public static readonly double[] StandardMotorsKw = new double[18]
	{
		0.18, 0.25, 0.37, 0.55, 0.75, 1.1, 1.5, 2.2, 3.0, 4.0,
		5.5, 7.5, 11.0, 15.0, 18.5, 22.0, 30.0, 37.0
	};

	public static readonly double[] StandardAccumulatorsL = new double[12]
	{
		0.075, 0.16, 0.35, 0.7, 1.0, 2.0, 4.0, 6.0, 10.0, 20.0,
		32.0, 50.0
	};

	public static readonly double[] StandardReceiversL = new double[11]
	{
		10.0, 25.0, 50.0, 90.0, 150.0, 270.0, 500.0, 900.0, 1500.0, 3000.0,
		5000.0
	};

	public static double NextUp(double[] sizes, double value)
	{
		foreach (double num in sizes)
		{
			if (num >= value - 1E-9)
			{
				return num;
			}
		}
		return sizes[sizes.Length - 1];
	}

	/// <summary>Piston area in m2 for a bore in mm.</summary>
	public static double Area(double boreMm)
	{
		return Math.PI / 4.0 * Math.Pow(boreMm / 1000.0, 2.0);
	}

	/// <summary>Theoretical bore (mm) for a force at a pressure. utilisation: share of the theoretical force that is really usable (0.7 for air, 0.9 for oil).</summary>
	public static double RequiredBoreMm(double forceN, double pressureBar, double utilisation)
	{
		double num = forceN / (pressureBar * 100000.0 * utilisation);
		return Math.Sqrt(4.0 * num / Math.PI) * 1000.0;
	}

	/// <summary>Smallest standard bore that gives the force (extend).</summary>
	public static double RecommendedBoreMm(double forceN, double pressureBar, double utilisation)
	{
		return NextUp(StandardBoresMm, RequiredBoreMm(forceN, pressureBar, utilisation));
	}

	/// <summary>Standard rod diameter for a bore (about 0.3 to 0.4 of the bore).</summary>
	public static double RecommendedRodMm(double boreMm)
	{
		int num = Array.IndexOf(StandardBoresMm, boreMm);
		return (num >= 0) ? StandardRodsMm[num] : Math.Round(boreMm * 0.32);
	}

	/// <summary>Real (usable) force in N when extending (rod = false) or retracting (rod = true).</summary>
	public static double Force(double boreMm, double rodMm, double pressureBar, double utilisation, bool retract)
	{
		double num = Area(boreMm) - (retract ? Area(rodMm) : 0.0);
		return pressureBar * 100000.0 * num * utilisation;
	}

	/// <summary>Free air (NL) used by one extend + retract cycle at a gauge pressure.</summary>
	public static double AirPerCycleNl(double boreMm, double rodMm, double strokeMm, double pressureBar)
	{
		double num = (Area(boreMm) + (Area(boreMm) - Area(rodMm))) * (strokeMm / 1000.0) * 1000.0;
		return num * (pressureBar + 1.013) / 1.013;
	}

	/// <summary>Free air flow (NL/min) needed to move a piston at a speed.</summary>
	public static double AirFlowNlMin(double boreMm, double speedMmS, double pressureBar)
	{
		double num = Area(boreMm) * (speedMmS / 1000.0) * 1000.0 * 60.0;
		return num * (pressureBar + 1.013) / 1.013;
	}

	/// <summary>Smallest standard valve size (port, nominal flow) for a peak flow; a safety factor covers pressure drop and speed.</summary>
	public static (string Port, double Nominal) RecommendedValve(double peakNlMin, double safety = 1.5)
	{
		double num = peakNlMin * safety;
		foreach (var pneumaticValve in PneumaticValves)
		{
			if (pneumaticValve.Nominal >= num)
			{
				return pneumaticValve;
			}
		}
		return PneumaticValves[PneumaticValves.Length - 1];
	}

	/// <summary>Required tube inner diameter (mm) for a flow of free air (NL/min) at a gauge pressure and a maximum velocity (m/s).</summary>
	public static double RequiredAirTubeIdMm(double flowNlMin, double pressureBar, double velocityMs)
	{
		double num = flowNlMin / 1000.0 / 60.0 * 1.013 / (pressureBar + 1.013);
		return Math.Sqrt(4.0 * num / (Math.PI * velocityMs)) * 1000.0;
	}

	/// <summary>Required tube inner diameter (mm) for an oil flow (l/min) at a maximum velocity (m/s).</summary>
	public static double RequiredOilTubeIdMm(double flowLMin, double velocityMs)
	{
		double num = flowLMin / 1000.0 / 60.0;
		return Math.Sqrt(4.0 * num / (Math.PI * velocityMs)) * 1000.0;
	}

	public static (double Od, double Id) RecommendedTube((double Od, double Id)[] list, double idMm)
	{
		foreach (var item in list)
		{
			if (item.Id >= idMm - 1E-9)
			{
				return item;
			}
		}
		return list[list.Length - 1];
	}

	/// <summary>Pump flow (l/min) needed to move a hydraulic cylinder at a speed (extend).</summary>
	public static double PumpFlowLMin(double boreMm, double speedMmS)
	{
		return Area(boreMm) * (speedMmS / 1000.0) * 1000.0 * 60.0;
	}

	/// <summary>Pump displacement (cm3/rev) for a flow at a motor speed (rpm) and volumetric efficiency.</summary>
	public static double PumpDisplacementCc(double flowLMin, double rpm, double volumetricEfficiency = 0.95)
	{
		return flowLMin * 1000.0 / (rpm * volumetricEfficiency);
	}

	/// <summary>Hydraulic power in kW for a flow (l/min) at a pressure (bar).</summary>
	public static double HydraulicPowerKw(double flowLMin, double pressureBar)
	{
		return pressureBar * flowLMin / 600.0;
	}

	/// <summary>Electric motor power in kW (overall pump efficiency about 0.85).</summary>
	public static double MotorPowerKw(double flowLMin, double pressureBar, double efficiency = 0.85)
	{
		return HydraulicPowerKw(flowLMin, pressureBar) / efficiency;
	}

	/// <summary>Tank volume in litres (rule of thumb: three times the pump flow per minute).</summary>
	public static double TankLitres(double flowLMin)
	{
		return 3.0 * flowLMin;
	}

	/// <summary>Gas-charged accumulator volume (litres) to deliver deltaV litres between p1 (minimum) and p2 (maximum working pressure), precharge p0 (bar abs), polytropic exponent n (1.0 slow, 1.4 fast).</summary>
	public static double AccumulatorLitres(double deltaVLitres, double p0Bar, double p1Bar, double p2Bar, double n)
	{
		double num = Math.Pow(p0Bar / p1Bar, 1.0 / n) - Math.Pow(p0Bar / p2Bar, 1.0 / n);
		return deltaVLitres / num;
	}

	/// <summary>Compressor free air delivery (NL/min) for a demand with a simultaneity factor and a reserve (growth) factor.</summary>
	public static double CompressorFadNlMin(double demandNlMin, double simultaneity = 0.8, double reserve = 1.25)
	{
		return demandNlMin * simultaneity * reserve;
	}

	/// <summary>Receiver volume (litres) to supply a peak demand for a buffer time (minutes) while the pressure falls by deltaPBar.</summary>
	public static double ReceiverLitres(double peakNlMin, double bufferMin, double deltaPBar)
	{
		return peakNlMin * bufferMin * 1.013 / deltaPBar;
	}

	/// <summary>Compressor drive power estimate in kW (about 6.5 kW per m3/min at 7 bar for a screw compressor).</summary>
	public static double CompressorKw(double fadNlMin)
	{
		return fadNlMin / 1000.0 * 6.5;
	}
}
