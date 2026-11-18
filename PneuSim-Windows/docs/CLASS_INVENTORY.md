# PneuSim 3.2 – class inventory

Every source file in `src/PneuSim/`, generated from the recovered code. Kind `module` = a VB.NET `Module` (shared/static members). Lines include decompiler boilerplate.

| Folder | File | Kind | Base | Lines |
|---|---|---|---|---|
| Core | AppSettings.cs | module |  | 85 |
| Core | Circuit.cs | class |  | 504 |
| Core | CircuitElement.cs | class |  | 228 |
| Core | DrawSurface.cs | class |  | 96 |
| Core | ElementDescriptionProvider.cs | class | TypeDescriptionProvider | 51 |
| Core | EnumDescriptionConverter.cs | class | EnumConverter | 51 |
| Core | FaultKind.cs | enum |  | 30 |
| Core | Faults.cs | module |  | 79 |
| Core | GdiSurface.cs | class | DrawSurface | 164 |
| Core | IHydraulicConsumer.cs | interface |  | 8 |
| Core | ISignalSource.cs | interface |  | 10 |
| Core | LiveTuning.cs | module |  | 163 |
| Core | Naming.cs | module |  | 306 |
| Core | PdfDocument.cs | class |  | 198 |
| Core | PdfImage.cs | class |  | 40 |
| Core | PdfPage.cs | class |  | 183 |
| Core | Polarity.cs | enum |  | 12 |
| Core | Port.cs | class |  | 70 |
| Core | PortKind.cs | enum |  | 14 |
| Core | PortState.cs | enum |  | 8 |
| Core | Project.cs | class |  | 383 |
| Core | ProjectInfo.cs | class |  | 112 |
| Core | ProjectPage.cs | class |  | 13 |
| Core | RenderContext.cs | class | IDisposable | 132 |
| Core | SimulationRecorder.cs | class |  | 242 |
| Core | Simulator.cs | class |  | 1414 |
| Core | Tube.cs | class |  | 195 |
| Core | VectorExport.cs | module |  | 271 |
| Core | VectorShape.cs | class |  | 37 |
| Core | VectorSurface.cs | class | DrawSurface | 202 |
| Elements | Accumulator.cs | class | CircuitElement | 123 |
| Elements | AirMotor.cs | class | CircuitElement | 137 |
| Elements | AirReceiver.cs | class | CircuitElement | 127 |
| Elements | AirSupply.cs | class | CircuitElement | 75 |
| Elements | CentrePosition.cs | enum |  | 14 |
| Elements | CheckValve.cs | class | CircuitElement | 92 |
| Elements | CoilKind.cs | enum |  | 18 |
| Elements | CompensatedFlowControl.cs | class | CircuitElement | 74 |
| Elements | Compressor.cs | class | CircuitElement | 191 |
| Elements | ContactOperator.cs | enum |  | 22 |
| Elements | CounterbalanceValve.cs | class | CircuitElement | 113 |
| Elements | CylinderBase.cs | class | CircuitElement | 667 |
| Elements | DirectionalValve.cs | class | CircuitElement | 878 |
| Elements | DoubleActingCylinder.cs | class | CylinderBase | 61 |
| Elements | ElectricCoil.cs | class | CircuitElement | 218 |
| Elements | ElectricContact.cs | class | CircuitElement | 300 |
| Elements | ElectricCounter.cs | class | CircuitElement | 158 |
| Elements | ElementFactory.cs | module |  | 60 |
| Elements | FlowControlValve.cs | class | CircuitElement | 159 |
| Elements | FlowMeter.cs | class | CircuitElement | 103 |
| Elements | ForceSensor.cs | class | CircuitElement | 62 |
| Elements | Gripper.cs | class | DoubleActingCylinder | 57 |
| Elements | HydraulicCentre.cs | enum |  | 16 |
| Elements | HydraulicCylinder.cs | class | DoubleActingCylinder | 106 |
| Elements | HydraulicMotor.cs | class | CircuitElement | 137 |
| Elements | HydraulicPump.cs | class | CircuitElement | 151 |
| Elements | HydraulicSingleActingCylinder.cs | class | SingleActingCylinder | 72 |
| Elements | HydraulicTank.cs | class | CircuitElement | 36 |
| Elements | HydraulicValve42.cs | class | DirectionalValve | 61 |
| Elements | HydraulicValve43.cs | class | DirectionalValve | 95 |
| Elements | Junction.cs | class | CircuitElement | 78 |
| Elements | LogicValve.cs | class | CircuitElement | 43 |
| Elements | PageConnector.cs | class | CircuitElement | 82 |
| Elements | PowerTerminal.cs | class | CircuitElement | 83 |
| Elements | PressureGauge.cs | class | CircuitElement | 53 |
| Elements | PressureRegulator.cs | class | CircuitElement | 180 |
| Elements | PressureSwitch.cs | class | CircuitElement | 184 |
| Elements | QuickExhaustValve.cs | class | CircuitElement | 96 |
| Elements | RegulatorStyle.cs | enum |  | 12 |
| Elements | ReliefValve.cs | class | CircuitElement | 108 |
| Elements | SemiRotaryActuator.cs | class | DoubleActingCylinder | 46 |
| Elements | ShutOffValve.cs | class | CircuitElement | 159 |
| Elements | ShuttleValve.cs | class | LogicValve | 45 |
| Elements | Silencer.cs | class | CircuitElement | 57 |
| Elements | SingleActingCylinder.cs | class | CylinderBase | 71 |
| Elements | SuctionCup.cs | class | CircuitElement | 159 |
| Elements | TextNote.cs | class | CircuitElement | 85 |
| Elements | TwoPressureValve.cs | class | LogicValve | 43 |
| Elements | VacuumGenerator.cs | class | CircuitElement | 150 |
| Elements | Valve22.cs | class | DirectionalValve | 58 |
| Elements | Valve32.cs | class | DirectionalValve | 58 |
| Elements | Valve42.cs | class | DirectionalValve | 58 |
| Elements | Valve52.cs | class | DirectionalValve | 59 |
| Elements | Valve53.cs | class | DirectionalValve | 102 |
| Elements | ValveActuator.cs | enum |  | 22 |
| Elements | ValveReturn.cs | enum |  | 14 |
| Generator | GeneratedCircuit.cs | class |  | 8 |
| Generator | GeneratorDialog.cs | class | Form | 205 |
| Generator | GeneratorMethod.cs | enum |  | 7 |
| Generator | MotionSequence.cs | class |  | 150 |
| Generator | SequenceGenerator.cs | module |  | 937 |
| Generator | SequenceMove.cs | class |  | 21 |
| Learning | ComponentHelp.cs | module |  | 242 |
| Learning | Lesson.cs | class |  | 21 |
| Learning | LessonResult.cs | class |  | 8 |
| Learning | Lessons.cs | module |  | 480 |
| Learning | QuestionBank.cs | module |  | 275 |
| Learning | QuizQuestion.cs | class |  | 16 |
| Tools | CheckIssue.cs | class |  | 21 |
| Tools | CircuitAnalysis.cs | module |  | 2531 |
| Tools | CircuitMove.cs | class |  | 38 |
| Tools | GifRecorder.cs | class |  | 266 |
| Tools | IssueSeverity.cs | enum |  | 8 |
| Tools | MoveTrigger.cs | class |  | 23 |
| Tools | ParameterSweep.cs | module |  | 97 |
| Tools | PartLine.cs | class |  | 14 |
| Tools | PartsList.cs | module |  | 446 |
| Tools | Reports.cs | module |  | 143 |
| Tools | SweepResult.cs | class |  | 64 |
| Tools | SweepSettings.cs | class |  | 29 |
| Tools | TroubleshootExercise.cs | class |  | 418 |
| UI | CalculatorsDialog.cs | class | Form | 239 |
| UI | CircuitCanvas.cs | class | ScrollableControl | 1186 |
| UI | ColorScheme.cs | class |  | 66 |
| UI | CutawayPalette.cs | class |  | 63 |
| UI | CutawayView.cs | class | Control | 325 |
| UI | CutawayWindow.cs | class | Form | 65 |
| UI | Cutaways.cs | module |  | 1302 |
| UI | DiagramPanel.cs | class | Control | 131 |
| UI | Examples.cs | module |  | 414 |
| UI | FaultDialog.cs | class | Form | 131 |
| UI | Icons.cs | module |  | 280 |
| UI | InspectorPanel.cs | class | UserControl | 266 |
| UI | LessonsPanel.cs | class | UserControl | 210 |
| UI | Library.cs | module |  | 243 |
| UI | LibraryPreset.cs | class |  | 19 |
| UI | MainForm.cs | class | Form | 3234 |
| UI | PartsListDialog.cs | class | Form | 147 |
| UI | PlotterPanel.cs | class | UserControl | 964 |
| UI | Program.cs | module |  | 54 |
| UI | QuizDialog.cs | class | Form | 390 |
| UI | SweepDialog.cs | class | Form | 612 |
| UI | Symbols.cs | module |  | 94 |
| UI | Theme.cs | module |  | 107 |
| UI | TroubleshootPanel.cs | class | UserControl | 146 |
| VBCompat | VB-AnonymousDelegate_0.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_1.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_10.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_11.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_12.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_13.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_14.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_15.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_16.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_17.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_18.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_19.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_2.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_20.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_21.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_22.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_23.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_24.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_3.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_4.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_5.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_6.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_7.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_8.cs | ? |  | 6 |
| VBCompat | VB-AnonymousDelegate_9.cs | ? |  | 6 |
| VBCompat | VB-AnonymousType_0.cs | ? |  | 13 |
