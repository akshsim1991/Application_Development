using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Library
{
	public const string CatSupply = "Supply, air preparation and measuring";

	public const string CatActuators = "Actuators";

	public const string CatDirectional = "Directional control valves";

	public const string CatSolenoid = "Solenoid valves";

	public const string CatFlow = "Logic, non-return and flow valves";

	public const string CatElectric = "Electrical (24 V DC)";

	public const string CatHydraulic = "Hydraulics";

	public const string CatDrawing = "Drawing";

	public static readonly LibraryPreset[] Presets = new LibraryPreset[]
	{
		new LibraryPreset("Supply, air preparation and measuring", "Compressed air supply", [SpecialName] () => new AirSupply()),
		new LibraryPreset("Supply, air preparation and measuring", "Compressor", [SpecialName] () => new Compressor()),
		new LibraryPreset("Supply, air preparation and measuring", "Air receiver (tank)", [SpecialName] () => new AirReceiver()),
		new LibraryPreset("Supply, air preparation and measuring", "Shut-off valve", [SpecialName] () => new ShutOffValve()),
		new LibraryPreset("Supply, air preparation and measuring", "Service unit", [SpecialName] () => new PressureRegulator
		{
			Style = RegulatorStyle.ServiceUnit,
			Setting = 6.0
		}),
		new LibraryPreset("Supply, air preparation and measuring", "Pressure regulator", [SpecialName] () => new PressureRegulator()),
		new LibraryPreset("Supply, air preparation and measuring", "Pressure gauge", [SpecialName] () => new PressureGauge()),
		new LibraryPreset("Supply, air preparation and measuring", "Flow meter", [SpecialName] () => new FlowMeter()),
		new LibraryPreset("Supply, air preparation and measuring", "Force sensor", [SpecialName] () => new ForceSensor()),
		new LibraryPreset("Supply, air preparation and measuring", "Pressure switch", [SpecialName] () => new PressureSwitch()),
		new LibraryPreset("Supply, air preparation and measuring", "Vacuum switch", [SpecialName] () => new PressureSwitch
		{
			Setting = -0.5
		}),
		new LibraryPreset("Supply, air preparation and measuring", "Silencer", [SpecialName] () => new Silencer()),
		new LibraryPreset("Supply, air preparation and measuring", "Tube junction (T)", [SpecialName] () => new Junction()),
		new LibraryPreset("Actuators", "Single-acting cylinder", [SpecialName] () => new SingleActingCylinder()),
		new LibraryPreset("Actuators", "Double-acting cylinder", [SpecialName] () => new DoubleActingCylinder()),
		new LibraryPreset("Actuators", "Tandem cylinder", [SpecialName] () => new TandemCylinder()),
		new LibraryPreset("Actuators", "Double-end cylinder (through rod)", [SpecialName] () => new ThroughRodCylinder()),
		new LibraryPreset("Actuators", "Telescopic cylinder", [SpecialName] () => new TelescopicCylinder()),
		new LibraryPreset("Actuators", "Telescopic cylinder, single-acting", [SpecialName] () => new TelescopicSingleActingCylinder()),
		new LibraryPreset("Actuators", "DC motor", [SpecialName] () => new DCMotor()),
		new LibraryPreset("Actuators", "Servo motor", [SpecialName] () => new ServoMotor()),
		new LibraryPreset("Actuators", "Electric cylinder (lead screw)", [SpecialName] () => new ElectricCylinder()),
		new LibraryPreset("Actuators", "Semi-rotary actuator", [SpecialName] () => new SemiRotaryActuator()),
		new LibraryPreset("Actuators", "Air motor", [SpecialName] () => new AirMotor()),
		new LibraryPreset("Actuators", "Parallel gripper", [SpecialName] () => new Gripper()),
		new LibraryPreset("Actuators", "Vacuum generator (ejector)", [SpecialName] () => new VacuumGenerator()),
		new LibraryPreset("Actuators", "Suction cup", [SpecialName] () => new SuctionCup()),
		new LibraryPreset("Directional control valves", "2/2 valve, push button, NC", [SpecialName] () => new Valve22
		{
			Actuator = ValveActuator.PushButton
		}),
		new LibraryPreset("Directional control valves", "3/2 valve, push button, NC", [SpecialName] () => V32(ValveActuator.PushButton)),
		new LibraryPreset("Directional control valves", "3/2 valve, push button, NO", [SpecialName] () => V32(ValveActuator.PushButton, normallyOpen: true)),
		new LibraryPreset("Directional control valves", "3/2 valve, selector switch, NC", [SpecialName] () => V32(ValveActuator.Selector)),
		new LibraryPreset("Directional control valves", "3/2 valve, roller lever, NC", [SpecialName] () => V32(ValveActuator.RollerLever)),
		new LibraryPreset("Directional control valves", "3/2 valve, idle-return roller, NC", [SpecialName] () => V32(ValveActuator.IdleReturnRoller)),
		new LibraryPreset("Directional control valves", "3/2 valve, pneumatic pilot, NC", [SpecialName] () => V32(ValveActuator.Pilot)),
		new LibraryPreset("Directional control valves", "Time delay valve, 3/2 NC", [SpecialName] () => V32(ValveActuator.DelayedPilot)),
		new LibraryPreset("Directional control valves", "Pressure sequence valve, 3/2 NC", [SpecialName] () => new Valve32
		{
			Actuator = ValveActuator.Pilot,
			SwitchingPressure = 4.0
		}),
		new LibraryPreset("Directional control valves", "4/2 valve, push button", [SpecialName] () => new Valve42
		{
			Actuator = ValveActuator.PushButton
		}),
		new LibraryPreset("Directional control valves", "5/2 valve, push button", [SpecialName] () => V52(ValveActuator.PushButton, ValveReturn.Spring)),
		new LibraryPreset("Directional control valves", "5/2 valve, selector switch", [SpecialName] () => V52(ValveActuator.Selector, ValveReturn.Spring)),
		new LibraryPreset("Directional control valves", "5/2 valve, single pilot", [SpecialName] () => V52(ValveActuator.Pilot, ValveReturn.Spring)),
		new LibraryPreset("Directional control valves", "5/2 valve, double pilot (memory)", [SpecialName] () => V52(ValveActuator.Pilot, ValveReturn.Pilot)),
		new LibraryPreset("Directional control valves", "5/3 valve, double pilot, closed centre", [SpecialName] () => new Valve53()),
		new LibraryPreset("Directional control valves", "5/3 valve, double pilot, exhaust centre", [SpecialName] () => new Valve53
		{
			Centre = CentrePosition.Exhausted
		}),
		new LibraryPreset("Solenoid valves", "3/2 valve, solenoid, NC", [SpecialName] () => V32(ValveActuator.Solenoid)),
		new LibraryPreset("Solenoid valves", "5/2 valve, single solenoid", [SpecialName] () => V52(ValveActuator.Solenoid, ValveReturn.Spring)),
		new LibraryPreset("Solenoid valves", "5/2 valve, double solenoid", [SpecialName] () => V52(ValveActuator.Solenoid, ValveReturn.Solenoid)),
		new LibraryPreset("Solenoid valves", "5/3 valve, double solenoid, closed centre", [SpecialName] () => new Valve53
		{
			Actuator = ValveActuator.Solenoid,
			ReturnType = ValveReturn.Solenoid
		}),
		new LibraryPreset("Logic, non-return and flow valves", "Shuttle valve (OR)", [SpecialName] () => new ShuttleValve()),
		new LibraryPreset("Logic, non-return and flow valves", "Two-pressure valve (AND)", [SpecialName] () => new TwoPressureValve()),
		new LibraryPreset("Logic, non-return and flow valves", "Check valve", [SpecialName] () => new CheckValve()),
		new LibraryPreset("Logic, non-return and flow valves", "Quick exhaust valve", [SpecialName] () => new QuickExhaustValve()),
		new LibraryPreset("Logic, non-return and flow valves", "One-way flow control valve", [SpecialName] () => new FlowControlValve()),
		new LibraryPreset("Logic, non-return and flow valves", "Flow control valve", [SpecialName] () => new FlowControlValve
		{
			HasCheckValve = false
		}),
		new LibraryPreset("Electrical (24 V DC)", "+24 V connection", [SpecialName] () => new PowerTerminal()),
		new LibraryPreset("Electrical (24 V DC)", "0 V connection", [SpecialName] () => new PowerTerminal
		{
			Polarity = Polarity.Zero0V
		}),
		new LibraryPreset("Electrical (24 V DC)", "Push button, NO", [SpecialName] () => Contact(ContactOperator.PushButton, normallyClosed: false, "S1")),
		new LibraryPreset("Electrical (24 V DC)", "Push button, NC", [SpecialName] () => Contact(ContactOperator.PushButton, normallyClosed: true, "S0")),
		new LibraryPreset("Electrical (24 V DC)", "Selector switch, NO", [SpecialName] () => Contact(ContactOperator.Selector, normallyClosed: false, "S2")),
		new LibraryPreset("Electrical (24 V DC)", "Emergency stop, NC (latching)", [SpecialName] () => Contact(ContactOperator.EmergencyStop, normallyClosed: true, "S0")),
		new LibraryPreset("Electrical (24 V DC)", "Relay contact, NO", [SpecialName] () => Contact(ContactOperator.Relay, normallyClosed: false, "", "K1")),
		new LibraryPreset("Electrical (24 V DC)", "Relay contact, NC", [SpecialName] () => Contact(ContactOperator.Relay, normallyClosed: true, "", "K1")),
		new LibraryPreset("Electrical (24 V DC)", "Proximity sensor, NO", [SpecialName] () => Contact(ContactOperator.ProximitySensor, normallyClosed: false, "", "1B1")),
		new LibraryPreset("Electrical (24 V DC)", "Limit switch, NO", [SpecialName] () => Contact(ContactOperator.LimitSwitch, normallyClosed: false, "", "1S1")),
		new LibraryPreset("Electrical (24 V DC)", "Pressure switch contact, NO", [SpecialName] () => Contact(ContactOperator.PressureSwitch, normallyClosed: false, "", "B1")),
		new LibraryPreset("Electrical (24 V DC)", "Relay coil", [SpecialName] () => new ElectricCoil
		{
			Label = "K1"
		}),
		new LibraryPreset("Electrical (24 V DC)", "Timer relay, on-delay", [SpecialName] () => new ElectricCoil
		{
			Kind = CoilKind.OnDelayTimer,
			Label = "K2"
		}),
		new LibraryPreset("Electrical (24 V DC)", "Timer relay, off-delay", [SpecialName] () => new ElectricCoil
		{
			Kind = CoilKind.OffDelayTimer,
			Label = "K3"
		}),
		new LibraryPreset("Electrical (24 V DC)", "Valve solenoid", [SpecialName] () => new ElectricCoil
		{
			Kind = CoilKind.Solenoid,
			Label = "1M1"
		}),
		new LibraryPreset("Electrical (24 V DC)", "Indicator lamp", [SpecialName] () => new ElectricCoil
		{
			Kind = CoilKind.Lamp,
			Label = "H1"
		}),
		new LibraryPreset("Electrical (24 V DC)", "Preset counter", [SpecialName] () => new ElectricCounter()),
		new LibraryPreset("Electrical (24 V DC)", "Wire junction", [SpecialName] () => new Junction
		{
			IsElectric = true
		}),
		new LibraryPreset("Hydraulics", "Hydraulic power unit (pump)", [SpecialName] () => new HydraulicPump()),
		new LibraryPreset("Hydraulics", "Tank", [SpecialName] () => new HydraulicTank()),
		new LibraryPreset("Hydraulics", "Pressure relief valve", [SpecialName] () => new ReliefValve()),
		new LibraryPreset("Hydraulics", "Hydraulic cylinder", [SpecialName] () => new HydraulicCylinder()),
		new LibraryPreset("Hydraulics", "Hydraulic double-end cylinder (through rod)", [SpecialName] () => new HydraulicThroughRodCylinder()),
		new LibraryPreset("Hydraulics", "Hydraulic telescopic cylinder", [SpecialName] () => new HydraulicTelescopicCylinder()),
		new LibraryPreset("Hydraulics", "Hydraulic telescopic cylinder, single-acting", [SpecialName] () => new HydraulicTelescopicSingleActingCylinder()),
		new LibraryPreset("Hydraulics", "Hydraulic single-acting cylinder", [SpecialName] () => new HydraulicSingleActingCylinder()),
		new LibraryPreset("Hydraulics", "Hydraulic motor", [SpecialName] () => new HydraulicMotor()),
		new LibraryPreset("Hydraulics", "4/3 valve, tandem centre, solenoids", [SpecialName] () => new HydraulicValve43()),
		new LibraryPreset("Hydraulics", "4/3 valve, closed centre, solenoids", [SpecialName] () => new HydraulicValve43
		{
			Centre = HydraulicCentre.Closed
		}),
		new LibraryPreset("Hydraulics", "4/2 valve, solenoid", [SpecialName] () => new HydraulicValve42()),
		new LibraryPreset("Hydraulics", "Pressure-compensated flow control", [SpecialName] () => new CompensatedFlowControl()),
		new LibraryPreset("Hydraulics", "Hydraulic one-way flow control", [SpecialName] () => new FlowControlValve
		{
			Hydraulic = true,
			OpeningPercent = 30
		}),
		new LibraryPreset("Hydraulics", "Hydraulic throttle valve", [SpecialName] () => new FlowControlValve
		{
			Hydraulic = true,
			HasCheckValve = false,
			OpeningPercent = 30
		}),
		new LibraryPreset("Hydraulics", "Hydraulic check valve", [SpecialName] () => new CheckValve
		{
			Hydraulic = true
		}),
		new LibraryPreset("Hydraulics", "Pressure reducing valve", [SpecialName] () => new PressureRegulator
		{
			Hydraulic = true,
			Setting = 30.0
		}),
		new LibraryPreset("Hydraulics", "Counterbalance valve", [SpecialName] () => new CounterbalanceValve()),
		new LibraryPreset("Hydraulics", "Accumulator", [SpecialName] () => new Accumulator()),
		new LibraryPreset("Hydraulics", "Hydraulic pressure gauge", [SpecialName] () => new PressureGauge
		{
			Hydraulic = true
		}),
		new LibraryPreset("Hydraulics", "Hydraulic junction (T)", [SpecialName] () => new Junction
		{
			Medium = PortKind.Hydraulic
		}),
		new LibraryPreset("Drawing", "Page connector", [SpecialName] () => new PageConnector()),
		new LibraryPreset("Drawing", "Text", [SpecialName] () => new TextNote())
	};

	public static Valve32 V32(ValveActuator actuator, bool normallyOpen = false)
	{
		return new Valve32
		{
			Actuator = actuator,
			NormallyOpen = normallyOpen
		};
	}

	public static Valve52 V52(ValveActuator actuator, ValveReturn ret)
	{
		return new Valve52
		{
			Actuator = actuator,
			ReturnType = ret
		};
	}

	public static ElectricContact Contact(ContactOperator op, bool normallyClosed, string label, string reference = "")
	{
		return new ElectricContact
		{
			Operator = op,
			NormallyClosed = normallyClosed,
			Label = label,
			Reference = reference
		};
	}

	public static Bitmap RenderThumbnail(CircuitElement e, int width, int height, ColorScheme scheme = null)
	{
		if (scheme == null)
		{
			scheme = ColorScheme.Light;
		}
		Bitmap bitmap = new Bitmap(width, height);
		using Graphics graphics = Graphics.FromImage(bitmap);
		using RenderContext r = new RenderContext();
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		graphics.Clear(scheme.Background);
		RectangleF localBounds = e.LocalBounds;
		float val = checked(Math.Min((float)(width - 6) / localBounds.Width, (float)(height - 6) / localBounds.Height));
		val = Math.Min(val, 1f);
		graphics.TranslateTransform((float)width / 2f, (float)height / 2f);
		graphics.ScaleTransform(val, val);
		graphics.TranslateTransform(0f - (localBounds.Left + localBounds.Width / 2f), 0f - (localBounds.Top + localBounds.Height / 2f));
		using GdiSurface g = new GdiSurface(graphics, scheme);
		e.DrawSymbol(g, r);
		return bitmap;
	}
}
