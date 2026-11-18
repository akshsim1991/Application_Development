using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class ElementFactory
{
	public static CircuitElement Create(string typeName)
	{
		return typeName switch
		{
			"AirSupply" => new AirSupply(), 
			"PressureGauge" => new PressureGauge(), 
			"SingleActingCylinder" => new SingleActingCylinder(), 
			"DoubleActingCylinder" => new DoubleActingCylinder(), 
			"SemiRotaryActuator" => new SemiRotaryActuator(), 
			"AirMotor" => new AirMotor(), 
			"Valve22" => new Valve22(), 
			"Valve32" => new Valve32(), 
			"Valve42" => new Valve42(), 
			"Valve52" => new Valve52(), 
			"Valve53" => new Valve53(), 
			"ShuttleValve" => new ShuttleValve(), 
			"TwoPressureValve" => new TwoPressureValve(), 
			"FlowControlValve" => new FlowControlValve(), 
			"CheckValve" => new CheckValve(), 
			"QuickExhaustValve" => new QuickExhaustValve(), 
			"PressureRegulator" => new PressureRegulator(), 
			"Silencer" => new Silencer(), 
			"Junction" => new Junction(), 
			"Manifold" => new Manifold(), 
			"TextNote" => new TextNote(), 
			"PageConnector" => new PageConnector(), 
			"HydraulicPump" => new HydraulicPump(), 
			"HydraulicTank" => new HydraulicTank(), 
			"ReliefValve" => new ReliefValve(), 
			"HydraulicCylinder" => new HydraulicCylinder(), 
			"HydraulicSingleActingCylinder" => new HydraulicSingleActingCylinder(), 
			"HydraulicMotor" => new HydraulicMotor(), 
			"Accumulator" => new Accumulator(), 
			"HydraulicValve43" => new HydraulicValve43(), 
			"HydraulicValve42" => new HydraulicValve42(), 
			"CompensatedFlowControl" => new CompensatedFlowControl(), 
			"CounterbalanceValve" => new CounterbalanceValve(), 
			"PowerTerminal" => new PowerTerminal(), 
			"ElectricContact" => new ElectricContact(), 
			"ElectricCoil" => new ElectricCoil(), 
			"ElectricCounter" => new ElectricCounter(), 
			"PressureSwitch" => new PressureSwitch(), 
			"ShutOffValve" => new ShutOffValve(), 
			"VacuumGenerator" => new VacuumGenerator(), 
			"SuctionCup" => new SuctionCup(), 
			"Gripper" => new Gripper(), 
			"Compressor" => new Compressor(), 
			"AirReceiver" => new AirReceiver(), 
			"FlowMeter" => new FlowMeter(), 
			"ForceSensor" => new ForceSensor(), 
			"TandemCylinder" => new TandemCylinder(), 
			"ThroughRodCylinder" => new ThroughRodCylinder(), 
			"HydraulicThroughRodCylinder" => new HydraulicThroughRodCylinder(), 
			"TelescopicCylinder" => new TelescopicCylinder(), 
			"TelescopicSingleActingCylinder" => new TelescopicSingleActingCylinder(), 
			"HydraulicTelescopicCylinder" => new HydraulicTelescopicCylinder(), 
			"HydraulicTelescopicSingleActingCylinder" => new HydraulicTelescopicSingleActingCylinder(), 
			"DCMotor" => new DCMotor(), 
			"ServoMotor" => new ServoMotor(), 
			"ElectricCylinder" => new ElectricCylinder(), 
			_ => null, 
		};
	}
}
