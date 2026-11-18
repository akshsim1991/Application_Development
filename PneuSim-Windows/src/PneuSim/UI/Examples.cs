using System;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Examples
{
	public static readonly (string Name, Func<Circuit> Build)[] All = new(string, Func<Circuit>)[12]
	{
		("1. Direct control of a single-acting cylinder", DirectSingleActing),
		("2. Indirect control of a double-acting cylinder with speed control", IndirectDoubleActing),
		("3. OR: operation from two places (shuttle valve)", OrControl),
		("4. AND: two-hand safety control (two-pressure valve)", TwoHandControl),
		("5. Automatic reciprocation with limit valves", Reciprocation),
		("6. Delayed return with a time delay valve", DelayedReturn),
		("7. Electro-pneumatic: direct control with a solenoid valve", ElectroDirect),
		("8. Electro-pneumatic: self-holding circuit with start / stop and lamp", ElectroLatch),
		("9. Hydraulics: cylinder with 4/3 valve, relief valve and gauge", HydraulicBasic),
		("10. Realistic mode: pressure sequence valve (clamp, then drill)", SequenceValveDemo),
		("11. Vacuum handling: ejector, suction cup and vacuum switch", VacuumHandling),
		("12. Air generation: compressor, receiver, service unit and flow meter", AirGeneration)
	};

	private static Circuit VacuumHandling()
	{
		Circuit circuit = new Circuit();
		SuctionCup b = circuit.Add(new SuctionCup(), 240f, 180f, "1U1");
		PressureSwitch b2 = circuit.Add(new PressureSwitch
		{
			Setting = -0.5
		}, 320f, 40f, "B1");
		Junction junction = circuit.Add(new Junction(), 170f, 140f);
		VacuumGenerator vacuumGenerator = circuit.Add(new VacuumGenerator(), 150f, 190f, "1Z1");
		Valve32 valve = circuit.Add(Library.V32(ValveActuator.Selector), 100f, 300f, "1S1");
		AirSupply a = circuit.Add(new AirSupply(), 150f, 420f, "0Z");
		circuit.Connect(a, "1", valve, "1");
		circuit.Connect(valve, "2", vacuumGenerator, "1");
		circuit.Connect(vacuumGenerator, "V", junction, "1");
		circuit.Connect(junction, "1", b, "1");
		circuit.Connect(junction, "1", b2, "1");
		PowerTerminal a2 = circuit.Add(new PowerTerminal(), 520f, 40f);
		ElectricContact electricContact = circuit.Add(Library.Contact(ContactOperator.PressureSwitch, normallyClosed: false, "", "B1"), 520f, 120f);
		ElectricCoil electricCoil = circuit.Add(new ElectricCoil
		{
			Kind = CoilKind.Lamp
		}, 520f, 230f, "H1");
		PowerTerminal b3 = circuit.Add(new PowerTerminal
		{
			Polarity = Polarity.Zero0V
		}, 520f, 330f);
		circuit.Connect(a2, "1", electricContact, "1");
		circuit.Connect(electricContact, "2", electricCoil, "A1");
		circuit.Connect(electricCoil, "A2", b3, "1");
		circuit.Add(new TextNote
		{
			Text = "Switch on 1S1: the ejector sucks the air out of the cup. Click the cup to take the part away or put it back."
		}, 40f, 0f);
		return circuit;
	}

	private static Circuit AirGeneration()
	{
		Circuit circuit = new Circuit();
		Compressor a = circuit.Add(new Compressor(), 40f, 440f, "0P1");
		AirReceiver airReceiver = circuit.Add(new AirReceiver
		{
			VolumeLitres = 10.0,
			InitialPressure = 0.0
		}, 120f, 340f, "0Z1");
		ShutOffValve shutOffValve = circuit.Add(new ShutOffValve(), 260f, 340f, "0V1");
		PressureRegulator pressureRegulator = circuit.Add(new PressureRegulator
		{
			Style = RegulatorStyle.ServiceUnit,
			Setting = 6.0
		}, 360f, 340f, "0Z2");
		FlowMeter flowMeter = circuit.Add(new FlowMeter(), 500f, 340f, "0F1");
		Valve32 valve = circuit.Add(Library.V32(ValveActuator.PushButton), 560f, 200f, "1S1");
		SingleActingCylinder b = circuit.Add(new SingleActingCylinder(), 600f, 40f, "1A");
		circuit.Connect(a, "1", airReceiver, "1");
		circuit.Connect(airReceiver, "2", shutOffValve, "1");
		circuit.Connect(shutOffValve, "2", pressureRegulator, "1");
		circuit.Connect(pressureRegulator, "2", flowMeter, "1");
		circuit.Connect(flowMeter, "2", valve, "1");
		circuit.Connect(valve, "2", b, "1");
		circuit.Add(new TextNote
		{
			Text = "The receiver starts empty: watch the compressor fill it, then operate 1S1 and read the flow meter."
		}, 40f, 0f);
		return circuit;
	}

	private static Circuit DirectSingleActing()
	{
		Circuit circuit = new Circuit();
		SingleActingCylinder b = circuit.Add(new SingleActingCylinder(), 200f, 60f, "1A");
		Valve32 valve = circuit.Add(Library.V32(ValveActuator.PushButton), 140f, 200f, "1S1");
		AirSupply a = circuit.Add(new AirSupply(), 190f, 320f, "0Z");
		circuit.Connect(valve, "2", b, "1");
		circuit.Connect(a, "1", valve, "1");
		return circuit;
	}

	private static Circuit IndirectDoubleActing()
	{
		Circuit circuit = new Circuit();
		DoubleActingCylinder b = circuit.Add(new DoubleActingCylinder
		{
			StrokeTime = 0.8
		}, 220f, 40f, "1A");
		FlowControlValve flowControlValve = circuit.Add(new FlowControlValve
		{
			OpeningPercent = 40,
			Rotation = 3
		}, 200f, 210f, "1V2");
		FlowControlValve flowControlValve2 = circuit.Add(new FlowControlValve
		{
			OpeningPercent = 15,
			Rotation = 3
		}, 300f, 210f, "1V3");
		Valve52 valve = circuit.Add(Library.V52(ValveActuator.Pilot, ValveReturn.Spring), 130f, 280f, "1V1");
		AirSupply a = circuit.Add(new AirSupply(), 220f, 380f, "0Z");
		Valve32 valve2 = circuit.Add(Library.V32(ValveActuator.PushButton), 10f, 380f, "1S1");
		AirSupply a2 = circuit.Add(new AirSupply(), 60f, 480f);
		circuit.Connect(flowControlValve, "2", b, "1");
		circuit.Connect(flowControlValve2, "2", b, "2");
		circuit.Connect(valve, "4", flowControlValve, "1");
		circuit.Connect(valve, "2", flowControlValve2, "1");
		circuit.Connect(a, "1", valve, "1");
		circuit.Connect(valve2, "2", valve, "14");
		circuit.Connect(a2, "1", valve2, "1");
		return circuit;
	}

	private static Circuit OrControl()
	{
		Circuit circuit = new Circuit();
		SingleActingCylinder b = circuit.Add(new SingleActingCylinder(), 230f, 60f, "1A");
		ShuttleValve shuttleValve = circuit.Add(new ShuttleValve(), 200f, 200f, "1V1");
		Valve32 valve = circuit.Add(Library.V32(ValveActuator.PushButton), 60f, 300f, "1S1");
		Valve32 valve2 = circuit.Add(Library.V32(ValveActuator.PushButton), 250f, 300f, "1S2");
		AirSupply a = circuit.Add(new AirSupply(), 110f, 400f);
		AirSupply a2 = circuit.Add(new AirSupply(), 300f, 400f);
		circuit.Connect(shuttleValve, "2", b, "1");
		circuit.Connect(valve, "2", shuttleValve, "1a");
		circuit.Connect(valve2, "2", shuttleValve, "1b");
		circuit.Connect(a, "1", valve, "1");
		circuit.Connect(a2, "1", valve2, "1");
		return circuit;
	}

	private static Circuit TwoHandControl()
	{
		Circuit circuit = new Circuit();
		DoubleActingCylinder b = circuit.Add(new DoubleActingCylinder(), 360f, 40f, "1A");
		Valve52 valve = circuit.Add(Library.V52(ValveActuator.Pilot, ValveReturn.Spring), 270f, 170f, "1V1");
		AirSupply a = circuit.Add(new AirSupply(), 360f, 270f, "0Z");
		TwoPressureValve twoPressureValve = circuit.Add(new TwoPressureValve(), 150f, 260f, "1V2");
		Valve32 valve2 = circuit.Add(Library.V32(ValveActuator.PushButton), 50f, 360f, "1S1");
		Valve32 valve3 = circuit.Add(Library.V32(ValveActuator.PushButton), 170f, 360f, "1S2");
		AirSupply a2 = circuit.Add(new AirSupply(), 100f, 460f);
		AirSupply a3 = circuit.Add(new AirSupply(), 220f, 460f);
		circuit.Connect(valve, "4", b, "1");
		circuit.Connect(valve, "2", b, "2");
		circuit.Connect(a, "1", valve, "1");
		circuit.Connect(twoPressureValve, "2", valve, "14");
		circuit.Connect(valve2, "2", twoPressureValve, "1a");
		circuit.Connect(valve3, "2", twoPressureValve, "1b");
		circuit.Connect(a2, "1", valve2, "1");
		circuit.Connect(a3, "1", valve3, "1");
		return circuit;
	}

	private static Circuit Reciprocation()
	{
		Circuit circuit = new Circuit();
		DoubleActingCylinder b = circuit.Add(new DoubleActingCylinder
		{
			RetractedMark = "1S1",
			ExtendedMark = "1S2"
		}, 360f, 40f, "1A");
		Valve52 valve = circuit.Add(Library.V52(ValveActuator.Pilot, ValveReturn.Pilot), 270f, 170f, "1V1");
		AirSupply a = circuit.Add(new AirSupply(), 360f, 270f, "0Z");
		Valve32 valve2 = circuit.Add(Library.V32(ValveActuator.RollerLever), 520f, 260f, "1S2");
		valve2.TriggerMark = "1S2";
		AirSupply a2 = circuit.Add(new AirSupply(), 570f, 360f);
		Valve32 valve3 = circuit.Add(Library.V32(ValveActuator.RollerLever), 80f, 260f, "1S1");
		valve3.TriggerMark = "1S1";
		Valve32 valve4 = circuit.Add(Library.V32(ValveActuator.Selector), 80f, 400f, "1S3");
		AirSupply a3 = circuit.Add(new AirSupply(), 130f, 500f);
		circuit.Connect(valve, "4", b, "1");
		circuit.Connect(valve, "2", b, "2");
		circuit.Connect(a, "1", valve, "1");
		circuit.Connect(valve2, "2", valve, "12");
		circuit.Connect(a2, "1", valve2, "1");
		circuit.Connect(valve3, "2", valve, "14");
		circuit.Connect(valve4, "2", valve3, "1");
		circuit.Connect(a3, "1", valve4, "1");
		return circuit;
	}

	private static Circuit DelayedReturn()
	{
		Circuit circuit = new Circuit();
		DoubleActingCylinder b = circuit.Add(new DoubleActingCylinder
		{
			ExtendedMark = "1S2"
		}, 360f, 40f, "1A");
		Valve52 valve = circuit.Add(Library.V52(ValveActuator.Pilot, ValveReturn.Pilot), 270f, 170f, "1V1");
		AirSupply a = circuit.Add(new AirSupply(), 360f, 270f, "0Z");
		Valve32 valve2 = circuit.Add(Library.V32(ValveActuator.PushButton), 80f, 260f, "1S1");
		AirSupply a2 = circuit.Add(new AirSupply(), 130f, 360f);
		Valve32 valve3 = circuit.Add(Library.V32(ValveActuator.DelayedPilot), 520f, 260f, "1V2");
		valve3.DelaySeconds = 2.0;
		AirSupply a3 = circuit.Add(new AirSupply(), 570f, 360f);
		Valve32 valve4 = circuit.Add(Library.V32(ValveActuator.RollerLever), 380f, 380f, "1S2");
		valve4.TriggerMark = "1S2";
		AirSupply a4 = circuit.Add(new AirSupply(), 430f, 480f);
		circuit.Connect(valve, "4", b, "1");
		circuit.Connect(valve, "2", b, "2");
		circuit.Connect(a, "1", valve, "1");
		circuit.Connect(valve2, "2", valve, "14");
		circuit.Connect(a2, "1", valve2, "1");
		circuit.Connect(valve3, "2", valve, "12");
		circuit.Connect(a3, "1", valve3, "1");
		circuit.Connect(valve4, "2", valve3, "12");
		circuit.Connect(a4, "1", valve4, "1");
		return circuit;
	}

	private static Valve52 SolenoidPowerPart(Circuit c)
	{
		DoubleActingCylinder b = c.Add(new DoubleActingCylinder(), 200f, 60f, "1A");
		Valve52 valve = c.Add(Library.V52(ValveActuator.Solenoid, ValveReturn.Spring), 110f, 190f, "1V1");
		valve.SolenoidLabel = "1M1";
		AirSupply a = c.Add(new AirSupply(), 200f, 290f, "0Z");
		c.Connect(valve, "4", b, "1");
		c.Connect(valve, "2", b, "2");
		c.Connect(a, "1", valve, "1");
		return valve;
	}

	private static Circuit ElectroDirect()
	{
		Circuit circuit = new Circuit();
		SolenoidPowerPart(circuit);
		PowerTerminal a = circuit.Add(new PowerTerminal(), 450f, 60f);
		ElectricContact electricContact = circuit.Add(Library.Contact(ContactOperator.PushButton, normallyClosed: false, "S1"), 450f, 120f);
		ElectricCoil electricCoil = circuit.Add(new ElectricCoil
		{
			Kind = CoilKind.Solenoid,
			Label = "1M1"
		}, 450f, 220f);
		PowerTerminal b = circuit.Add(new PowerTerminal
		{
			Polarity = Polarity.Zero0V
		}, 450f, 300f);
		circuit.Connect(a, "1", electricContact, "1");
		circuit.Connect(electricContact, "2", electricCoil, "A1");
		circuit.Connect(electricCoil, "A2", b, "1");
		return circuit;
	}

	private static Circuit ElectroLatch()
	{
		Circuit circuit = new Circuit();
		SolenoidPowerPart(circuit);
		PowerTerminal a = circuit.Add(new PowerTerminal(), 450f, 40f);
		ElectricContact electricContact = circuit.Add(Library.Contact(ContactOperator.PushButton, normallyClosed: true, "S0"), 450f, 100f);
		ElectricContact electricContact2 = circuit.Add(Library.Contact(ContactOperator.PushButton, normallyClosed: false, "S1"), 450f, 190f);
		ElectricContact electricContact3 = circuit.Add(Library.Contact(ContactOperator.Relay, normallyClosed: false, "", "K1"), 530f, 190f);
		ElectricCoil electricCoil = circuit.Add(new ElectricCoil
		{
			Label = "K1"
		}, 450f, 290f);
		PowerTerminal b = circuit.Add(new PowerTerminal
		{
			Polarity = Polarity.Zero0V
		}, 450f, 370f);
		circuit.Connect(a, "1", electricContact, "1");
		circuit.Connect(electricContact, "2", electricContact2, "1");
		circuit.Connect(electricContact, "2", electricContact3, "1");
		circuit.Connect(electricContact2, "2", electricCoil, "A1");
		circuit.Connect(electricContact3, "2", electricCoil, "A1").Mid = 280f;
		circuit.Connect(electricCoil, "A2", b, "1");
		PowerTerminal a2 = circuit.Add(new PowerTerminal(), 650f, 40f);
		ElectricContact electricContact4 = circuit.Add(Library.Contact(ContactOperator.Relay, normallyClosed: false, "", "K1"), 650f, 100f);
		ElectricCoil electricCoil2 = circuit.Add(new ElectricCoil
		{
			Kind = CoilKind.Solenoid,
			Label = "1M1"
		}, 650f, 290f);
		PowerTerminal b2 = circuit.Add(new PowerTerminal
		{
			Polarity = Polarity.Zero0V
		}, 650f, 370f);
		circuit.Connect(a2, "1", electricContact4, "1");
		circuit.Connect(electricContact4, "2", electricCoil2, "A1");
		circuit.Connect(electricCoil2, "A2", b2, "1");
		PowerTerminal a3 = circuit.Add(new PowerTerminal(), 770f, 40f);
		ElectricContact electricContact5 = circuit.Add(Library.Contact(ContactOperator.Relay, normallyClosed: false, "", "K1"), 770f, 100f);
		ElectricCoil electricCoil3 = circuit.Add(new ElectricCoil
		{
			Kind = CoilKind.Lamp,
			Label = "H1"
		}, 770f, 290f);
		PowerTerminal b3 = circuit.Add(new PowerTerminal
		{
			Polarity = Polarity.Zero0V
		}, 770f, 370f);
		circuit.Connect(a3, "1", electricContact5, "1");
		circuit.Connect(electricContact5, "2", electricCoil3, "A1");
		circuit.Connect(electricCoil3, "A2", b3, "1");
		circuit.Add(new TextNote
		{
			Text = "Press S1 to start, S0 to stop.\r\nK1 holds itself through its own contact."
		}, 450f, 420f);
		return circuit;
	}

	private static Circuit HydraulicBasic()
	{
		Circuit circuit = new Circuit();
		HydraulicCylinder b = circuit.Add(new HydraulicCylinder(), 200f, 60f, "1A");
		HydraulicValve43 hydraulicValve = circuit.Add(new HydraulicValve43(), 126f, 200f, "1V1");
		hydraulicValve.SolenoidLabel = "1M1";
		hydraulicValve.ReturnSolenoidLabel = "1M2";
		HydraulicPump a = circuit.Add(new HydraulicPump(), 180f, 380f, "0P1");
		Junction junction = circuit.Add(new Junction
		{
			Medium = PortKind.Hydraulic
		}, 210f, 340f);
		ReliefValve reliefValve = circuit.Add(new ReliefValve(), 280f, 340f, "0V1");
		HydraulicTank b2 = circuit.Add(new HydraulicTank(), 280f, 430f);
		HydraulicTank b3 = circuit.Add(new HydraulicTank(), 250f, 290f);
		PressureGauge a2 = circuit.Add(new PressureGauge
		{
			Hydraulic = true
		}, 360f, 240f, "0G1");
		circuit.Connect(hydraulicValve, "A", b, "1");
		circuit.Connect(hydraulicValve, "B", b, "2");
		circuit.Connect(a, "P", junction, "1");
		circuit.Connect(junction, "1", hydraulicValve, "P");
		circuit.Connect(junction, "1", reliefValve, "P");
		circuit.Connect(reliefValve, "T", b2, "T");
		circuit.Connect(hydraulicValve, "T", b3, "T");
		circuit.Connect(a2, "1", reliefValve, "P");
		float[] array = new float[2] { 470f, 560f };
		(string, string)[] array2 = new(string, string)[2]
		{
			("S1", "1M1"),
			("S2", "1M2")
		};
		int num = 0;
		do
		{
			PowerTerminal a3 = circuit.Add(new PowerTerminal(), array[num], 60f);
			ElectricContact electricContact = circuit.Add(Library.Contact(ContactOperator.PushButton, normallyClosed: false, array2[num].Item1), array[num], 120f);
			ElectricCoil electricCoil = circuit.Add(new ElectricCoil
			{
				Kind = CoilKind.Solenoid,
				Label = array2[num].Item2
			}, array[num], 220f);
			PowerTerminal b4 = circuit.Add(new PowerTerminal
			{
				Polarity = Polarity.Zero0V
			}, array[num], 300f);
			circuit.Connect(a3, "1", electricContact, "1");
			circuit.Connect(electricContact, "2", electricCoil, "A1");
			circuit.Connect(electricCoil, "A2", b4, "1");
			num = checked(num + 1);
		}
		while (num <= 1);
		circuit.Add(new TextNote
		{
			Text = "Hold S1 to extend, S2 to retract. Watch the gauge: the pressure is set by the load\r\nwhile the cylinder moves, and rises to the relief setting at the end of the stroke.\r\nIn the tandem centre the pump flow returns to tank at low pressure."
		}, 40f, 470f);
		return circuit;
	}

	private static Circuit SequenceValveDemo()
	{
		Circuit circuit = new Circuit();
		SingleActingCylinder a = circuit.Add(new SingleActingCylinder
		{
			LoadForceN = 100.0
		}, 200f, 60f, "1A");
		DoubleActingCylinder b = circuit.Add(new DoubleActingCylinder(), 560f, 60f, "2A");
		Valve32 b2 = circuit.Add(Library.V32(ValveActuator.Selector), 140f, 190f, "1V1");
		Valve32 valve = circuit.Add(Library.V32(ValveActuator.Pilot), 400f, 200f, "1V2");
		valve.SwitchingPressure = 4.0;
		Valve52 valve2 = circuit.Add(Library.V52(ValveActuator.Pilot, ValveReturn.Spring), 470f, 190f, "2V1");
		AirSupply a2 = circuit.Add(new AirSupply(), 190f, 290f, "0Z");
		AirSupply a3 = circuit.Add(new AirSupply(), 560f, 290f);
		AirSupply a4 = circuit.Add(new AirSupply(), 450f, 300f);
		PressureGauge b3 = circuit.Add(new PressureGauge(), 290f, 90f, "");
		Junction junction = circuit.Add(new Junction(), 210f, 150f);
		circuit.Connect(a, "1", junction, "1");
		circuit.Connect(junction, "1", b2, "2");
		circuit.Connect(junction, "1", valve, "12");
		circuit.Connect(junction, "1", b3, "1");
		circuit.Connect(a2, "1", b2, "1");
		circuit.Connect(valve2, "4", b, "1");
		circuit.Connect(valve2, "2", b, "2");
		circuit.Connect(a3, "1", valve2, "1");
		circuit.Connect(a4, "1", valve, "1");
		circuit.Connect(valve, "2", valve2, "14");
		circuit.Add(new TextNote
		{
			Text = "Switch on Simulation > Realistic physics, start, and switch on 1V1.\r\n1A clamps; the sequence valve 1V2 starts 2A only when the clamping\r\npressure has risen to 4 bar (watch the gauge)."
		}, 40f, 380f);
		return circuit;
	}
}
