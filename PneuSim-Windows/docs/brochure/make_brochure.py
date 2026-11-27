import sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.platypus import BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle, Image, PageBreak, KeepTogether
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

F='/usr/share/fonts/truetype/liberation/'
pdfmetrics.registerFont(TTFont('LS',F+'LiberationSans-Regular.ttf'))
pdfmetrics.registerFont(TTFont('LSB',F+'LiberationSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('LSI',F+'LiberationSans-Italic.ttf'))
pdfmetrics.registerFontFamily('LS',normal='LS',bold='LSB',italic='LSI',boldItalic='LSB')

TEAL=colors.HexColor('#007684'); DARK=colors.HexColor('#1e262e'); GREY=colors.HexColor('#64707a'); LIGHT=colors.HexColor('#e6f2f4'); ORANGE=colors.HexColor('#d9822b')
W,H=A4; M=16*mm
S='img/'
OUT=sys.argv[1]

body=ParagraphStyle('b',fontName='LS',fontSize=9.4,leading=13,textColor=DARK)
small=ParagraphStyle('s',parent=body,fontSize=8.2,leading=11,textColor=GREY)
h1=ParagraphStyle('h1',fontName='LSB',fontSize=17,leading=21,textColor=TEAL,spaceBefore=2,spaceAfter=4)
h2=ParagraphStyle('h2',fontName='LSB',fontSize=11,leading=14,textColor=DARK,spaceBefore=5,spaceAfter=2)
bul=ParagraphStyle('bl',parent=body,leftIndent=11,bulletIndent=1,spaceAfter=1.5)
cell=ParagraphStyle('c',parent=body,fontSize=8.6,leading=11.2)
cellb=ParagraphStyle('cb',parent=cell,fontName='LSB')
cellw=ParagraphStyle('cw',parent=cellb,textColor=colors.white)
big=ParagraphStyle('big',fontName='LSB',fontSize=22,leading=24,textColor=TEAL,alignment=1)
bigl=ParagraphStyle('bigl',parent=small,alignment=1)

def P(t,s=body): return Paragraph(t,s)
def B(t): return Paragraph(t,bul,bulletText='•')

def deco(c,doc):
    c.saveState()
    # header
    c.setFillColor(TEAL); c.rect(0,H-13*mm,W,13*mm,stroke=0,fill=1)
    c.setFillColor(colors.white); c.setFont('LSB',11); c.drawString(M,H-8.6*mm,'PneuSim')
    c.setFont('LS',8.5); c.drawString(M+22*mm,H-8.6*mm,'Pneumatic, electro-pneumatic and hydraulic circuit simulator')
    c.setFont('LS',8.5); c.drawRightString(W-M,H-8.6*mm,'Product brochure')
    # footer
    c.setStrokeColor(colors.HexColor('#c8d3d8')); c.setLineWidth(0.6); c.line(M,14*mm,W-M,14*mm)
    c.setFillColor(GREY); c.setFont('LS',8)
    c.drawString(M,9.5*mm,'\u00a9 2026 PneuSim. All rights reserved.')
    c.drawCentredString(W/2+10*mm,9.5*mm,'Pneumatic  \u00b7  Electro-pneumatic  \u00b7  Hydraulic')
    c.setFillColor(TEAL); c.setFont('LSB',9); c.drawRightString(W-M,9.5*mm,'Page %d of 4'%doc.page)
    c.restoreState()

doc=BaseDocTemplate(OUT,pagesize=A4,leftMargin=M,rightMargin=M,topMargin=19*mm,bottomMargin=19*mm,
  title='PneuSim product brochure',author='PneuSim',subject='PneuSim product brochure')
fw=W-2*M
doc.addPageTemplates([PageTemplate(id='p',frames=[Frame(M,18*mm,fw,H-18*mm-19*mm,leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0)],onPage=deco)])

def img(name,width):
    from reportlab.lib.utils import ImageReader
    iw,ih=ImageReader(S+name).getSize()
    return Image(S+name,width=width,height=width*ih/iw)

def legend(items,cols=2,width=None):
    width=width or fw
    cells=[Paragraph('<font color="#d9822b"><b>%d</b></font>&nbsp;&nbsp;%s'%(i+1,t),cell) for i,t in enumerate(items)]
    rows=[]
    n=(len(cells)+cols-1)//cols
    for r in range(n):
        rows.append([cells[r+n*c] if r+n*c<len(cells) else '' for c in range(cols)])
    t=Table(rows,colWidths=[width/cols]*cols)
    t.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),0),('TOPPADDING',(0,0),(-1,-1),1.5),('BOTTOMPADDING',(0,0),(-1,-1),1.5)]))
    return t

def box(flow,bg=LIGHT):
    t=Table([[flow]],colWidths=[fw]); t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),bg),('LEFTPADDING',(0,0),(-1,-1),9),('RIGHTPADDING',(0,0),(-1,-1),9),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7)])); return t

st=[]
# ---------- PAGE 1
tt=Table([[Paragraph('<font color="white" size="25"><b>See your circuit work<br/>before you build it.</b></font>',ParagraphStyle('t',fontName='LSB',leading=30)),
           Paragraph('<font color="white" size="10">Draw pneumatic, electro-pneumatic and hydraulic circuits, press Start, and watch pressure, motion and signals live. Check it, have it explained in plain English, and document it, all in one Windows program.</font>',ParagraphStyle('t2',fontName='LS',leading=14.5))]],colWidths=[fw*0.58,fw*0.42])
tt.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),TEAL),('VALIGN',(0,0),(-1,-1),'MIDDLE'),('LEFTPADDING',(0,0),(-1,-1),12),('RIGHTPADDING',(0,0),(-1,-1),12),('TOPPADDING',(0,0),(-1,-1),10),('BOTTOMPADDING',(0,0),(-1,-1),10)]))
st+= [tt,Spacer(1,8)]
st+= [P('1  About the software',h1),
 P('<b>PneuSim</b> is a desktop program for Windows that designs, simulates, checks and documents compressed-air and oil-hydraulic circuits. You place real component symbols (supply, valves, cylinders, switches, relays, pumps and more), connect them with tubes, hoses and wires, and run the circuit. Tubes fill with pressure, cylinders move, solenoids switch, and force, pressure, speed and flow can be read for every part while it runs.'),
 Spacer(1,3),
 P('It is made for <b>engineers, technicians, designers and maintenance teams</b>, and for anyone learning fluid power. Component names are plain English with the standard name in brackets, for example <i>3/2 valve, push button, NC</i>, so you do not need to be a specialist to start. Symbols are drawn by PneuSim following ISO 1219 / IEC 60617 conventions.'),
 Spacer(1,6)]
st+=[Spacer(1,2),
 img('shot16.png',fw),
 P('Figure 1: the PneuSim window (dark theme) with a small circuit and its GRAFCET sequence chart.',ParagraphStyle('cap',parent=small,fontName='LSI')),
 legend(['Quick-access buttons and the File tab (New, Open, Save, Export, Settings)','Ribbon tabs: components and tools by topic','Ribbon commands: edit, run, view, pages, quick parts','Search box: finds any command or component','File tabs: several circuits open at once','Workspace: place, connect and run the circuit','Properties panel: all settings of the selected part','Page tabs: several pages in one circuit','Bottom tabs: step diagram, GRAFCET, plotter, check, explain, inspector, fault finding','GRAFCET chart generated from the circuit']),
 Spacer(1,5),
 P('How it works in four steps',h2),
 Table([[P('<b>1  Draw</b><br/>Click a component, click in the workspace, drag port to port. Lines route themselves.',cell),
         P('<b>2  Run</b><br/>Press Start (F9). Pressure, motion and signals are shown live.',cell),
         P('<b>3  Check</b><br/>Check circuit finds mistakes; Explain circuit describes the sequence.',cell),
         P('<b>4  Document</b><br/>PDF report with title block, parts list with costs, drawings.',cell)]],colWidths=[fw/4]*4,
   style=TableStyle([('BOX',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('INNERGRID',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)])),
 PageBreak()]
# ---------- PAGE 2
def sect(title,items):
    return [P(title,h2)]+[B(i) for i in items]
left=sect('Draw and design',[
 'Ribbon with search, start screen, several circuits in tabs, several pages per circuit',
 'Automatic line routing around components, crossover hops, rotation of whole groups',
 'Undo and redo without limit; copy, paste, duplicate; grid and snap',
 'Circuit generator: type <b>A+ B+ B- A-</b> and get the circuit',
 'Complete my circuit: adds missing valves, supply and wiring around parts you placed; manifolds where one port feeds many tubes'])
left+=sect('Simulate and measure',[
 'Realistic physics: pressure build-up, bore, load, friction, air use',
 'Live values (force, pressure, speed, flow) when you point at a part',
 'Plotter with cursors and trigger; displacement-step diagram; GRAFCET chart generated from the circuit',
 'Replay: step back and forward in time; record an animated GIF',
 'Speed from 0.1x to 4x; automatic operation presses start buttons for you'])
right=sect('Check, explain, learn',[
 'Check circuit: unconnected ports, missing exhausts, overlapping signals, overlapping parts and lines',
 'Explain circuit: step-by-step description in plain English',
 'Animated cutaway inside any component, or of the whole workspace at once',
 'Getting-started tutorials and 13 examples',
 'Fault finding: put faults in (leak, blocked, stuck, burnt coil, welded contact) or hide one for a colleague; faults are stored encrypted'])
right+=sect('Engineer and document',[
 'Calculators and Sizing sheets: bore, force, air use, tube, valve, pump, motor, accumulator',
 'Parts list with costs and yearly air cost; parameter sweep to compare cycle time and air use',
 'PDF report with title block and print preview; SVG, DXF (CAD), PNG, CSV',
 'Rated-pressure alarm with sound; sounds for valves, cylinders, compressor',
 'Light, Dark and AMOLED themes; bar, MPa or kPa; port names as numbers or letters (P, A, B, E, C, T)'])
cols=Table([[left,right]],colWidths=[fw/2-3,fw/2-3]); cols.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),0),('RIGHTPADDING',(0,0),(0,0),8)]))
st+=[P('2  What is included',h1),
 P('Everything below is part of the program. There is nothing to add on and no separate modules to buy.',body),Spacer(1,3),cols,Spacer(1,5),
 P('Component library',h2),
 Table([[P('Air',cellw),P('Actuators',cellw),P('Valves and logic',cellw),P('Electrical and hydraulic',cellw)],
  [P('Compressor, receiver, air supply, service unit, regulator, silencer, gauges, flow meter, manifold, vacuum generator, suction cup',cell),
   P('Single and double-acting, tandem, through-rod and telescopic cylinders, gripper, semi-rotary actuator, air motor, DC motor, servo motor, electric cylinder',cell),
   P('2/2, 3/2, 4/2, 5/2 and 5/3 valves with push-button, lever, pilot or solenoid operation; shuttle, two-pressure, check, quick-exhaust, flow-control, counterbalance valves',cell),
   P('24 V DC supply, push buttons, switches, relays, timers, counters, sensors, lamps, solenoid coils; pump, tank, relief valve, accumulator, motors, 4/2 and 4/3 valves',cell)]],
  colWidths=[fw/4]*4,style=TableStyle([('BACKGROUND',(0,0),(-1,0),TEAL),('BOX',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('INNERGRID',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5)])),
 Spacer(1,6),
 P('Reliability and updates',h2),
 B('Crash recovery autosave; safe saving keeps a <b>.bak</b> copy of your previous file; clear messages for damaged files; errors are logged on your own computer only.'),
 B('Update check at every start, SHA-256 verified downloads, optional automatic installation when you close the program. Installs for your user without administrator rights.'),
 Spacer(1,6),
 P('Built-in worked examples',h2),
 Table([[P('1  Direct control of a single-acting cylinder<br/>2  Indirect control of a double-acting cylinder with speed control<br/>3  OR: operation from two places (shuttle valve)<br/>4  AND: two-hand safety control (two-pressure valve)<br/>5  Automatic reciprocation with limit valves<br/>6  Delayed return with a time delay valve<br/>7  Electro-pneumatic: direct control with a solenoid valve',cell),
         P('8  Electro-pneumatic: self-holding circuit with start / stop and lamp<br/>9  Hydraulics: cylinder with 4/3 valve, relief valve and gauge<br/>10  Realistic mode: pressure sequence valve (clamp, then drill)<br/>11  Vacuum handling: ejector, suction cup and vacuum switch<br/>12  Air generation: compressor, receiver, service unit and flow meter<br/>13  Pneumatic counterbalance: vertical load held and lowered under control',cell)]],
   colWidths=[fw/2]*2,style=TableStyle([('BOX',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('INNERGRID',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('BACKGROUND',(0,0),(-1,-1),LIGHT),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)])),
 PageBreak()]
# ---------- PAGE 3
rows=[[P('What you need',cellw),P('With PneuSim',cellw),P('Often found elsewhere*',cellw)],
 ['Understand a circuit quickly','<b>Explain circuit</b> writes the sequence in plain English; plain-English component names with the standard name in brackets','Symbols and standard names only; interpreting the result is left to the user'],
 ['Get a circuit from a sequence','<b>Circuit generator</b> (A+ B+ B- A-) and <b>Complete my circuit</b> around parts you already placed','Circuits are usually built entirely by hand'],
 ['Keep the drawing tidy','Lines route around parts and each other, hop over crossings, groups rotate as one piece, overlapping parts are moved apart','Manual routing and manual clean-up'],
 ['Find mistakes before building','<b>Check circuit</b> with click-to-jump results, including overlapping parts and lines','Simulation shows the result; finding the cause is manual'],
 ['Three technologies in one program','Pneumatic, electro-pneumatic (24 V) and hydraulic in the same workspace and file','Often separate programs or modules'],
 ['See inside the components','Animated cutaway of any part, or of the whole workspace working together','Symbol view only, or fixed pictures'],
 ['Practise fault finding','Hidden or chosen faults, measuring with live values, hints, repair','Not always included'],
 ['Sizing and documents','Sizing sheets, calculators, parts list with costs, PDF report with title block and print preview, SVG, DXF','Separate tools or add-ons'],
 ['Sequence charts','GRAFCET generated from the circuit and linked to the displacement-step diagram','Drawn manually'],
 ['Easy to adopt','Per-user installer, no administrator rights, starts with ready examples, Quick Guide on F1, simple Settings','Larger installations and longer set-up'],
]
data=[rows[0]]+[[P(a,cellb),P(b,cell),P(c,cell)] for a,b,c in rows[1:]]
ct=Table(data,colWidths=[fw*0.22,fw*0.45,fw*0.33],repeatRows=1)
ct.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),TEAL),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,LIGHT]),('BOX',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('LINEBELOW',(0,0),(-1,-1),0.4,colors.HexColor('#d8e0e4')),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),4.5),('BOTTOMPADDING',(0,0),(-1,-1),4.5)]))
st+=[P('3  Advantages over competitive software',h1),
 P('Circuit simulators differ widely. These are the points where PneuSim is designed to save you time and effort.',body),Spacer(1,4),ct,
 P('* General comparison of typical approaches. Features of individual products vary by product, edition and version; check any product you compare against. PneuSim names no competitor and claims no certification.',small),
 Spacer(1,6),
 Table([[img('shot15.png',fw*0.56),
   [P('Figure 2: View tab, workspace and GRAFCET panel',ParagraphStyle('cap',parent=small,fontName='LSI')),Spacer(1,3),
    legend(['View tab: zoom, pan, grid, live value labels, dark mode','Show and hide the bottom panels','Collapse the ribbon for more room','Workspace with the circuit','GRAFCET chart made from your circuit','Generate it again after a change, or copy it as text'],cols=1,width=fw*0.40)]]],colWidths=[fw*0.58,fw*0.42],style=TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),0),('LEFTPADDING',(1,0),(1,0),12)])),
 PageBreak()]
# ---------- PAGE 4
st+=[P('4  Why students should study this',h1),
 P('Fluid power is how machines move: factory automation, packaging, robots, presses, mobile equipment and test rigs all depend on it. PneuSim lets you learn it by doing, safely, on any Windows PC, without a workshop, compressor or spare parts.',body),Spacer(1,4)]
why=[('Learn by experiment','Change a bore, a pressure or a valve and see the result at once. Mistakes cost nothing: nothing leaks, nothing breaks.'),
 ('Understand, not just memorise','Live values and cutaway views show why a cylinder moves, stalls or hammers, instead of leaving the symbols abstract.'),
 ('Read and write circuit diagrams','Practise the language of fluid power: numbered and lettered ports, valve positions, sequences such as A+ B+ B- A-, displacement-step diagrams and GRAFCET.'),
 ('Think like a maintenance engineer','Fault-finding tasks hide a fault: measure, reason, ask for a hint, then find it. This is a skill that matters every day on the shop floor.'),
 ('Size and cost a design','Use Sizing sheets and calculators for bore, tube, pump, motor and accumulator, and see the air cost per year in the parts list.'),
 ('Build a portfolio','Export PDF reports with a title block, drawings in SVG and DXF, and a parts list: ready for lab reports, projects and interviews.')]
wt=[]
for i in range(0,6,2):
    wt.append([[P(why[i][0],h2),P(why[i][1],body)],[P(why[i+1][0],h2),P(why[i+1][1],body)]])
wtab=Table(wt,colWidths=[fw/2]*2)
wtab.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'),('BOX',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('INNERGRID',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('LEFTPADDING',(0,0),(-1,-1),9),('RIGHTPADDING',(0,0),(-1,-1),9),('TOPPADDING',(0,0),(-1,-1),3),('BOTTOMPADDING',(0,0),(-1,-1),7),('BACKGROUND',(0,0),(-1,-1),colors.white)]))
st+=[wtab,Spacer(1,7),
 P('A suggested learning path',h2),
 Table([[P('<b>Week 1</b><br/>Examples 1 to 4: single and double-acting cylinders, OR and AND control',cell),
         P('<b>Week 2</b><br/>Sequences and limit valves, delay valves, speed control; GRAFCET',cell),
         P('<b>Week 3</b><br/>Electro-pneumatics: relays, self-holding, solenoids',cell),
         P('<b>Week 4</b><br/>Hydraulics, sizing, fault finding, final PDF report',cell)]],colWidths=[fw/4]*4,
   style=TableStyle([('BOX',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('INNERGRID',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('BACKGROUND',(0,0),(-1,-1),LIGHT),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)])),
 Spacer(1,7),
 P('System requirements',h2),
 Table([[P('<b>Windows</b>',cell),P('Windows 7 SP1 or later (Windows 10 or 11 recommended); .NET Framework 4.7.2',cell),
         P('<b>Computer</b>',cell),P('2 GB memory (4 GB recommended), 50 MB disk, 1366 x 768 screen or larger',cell)]],colWidths=[fw*0.11,fw*0.39,fw*0.11,fw*0.39],
   style=TableStyle([('BOX',(0,0),(-1,-1),0.6,colors.HexColor('#c8d3d8')),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5)])),
 Spacer(1,6),
 P('A real circuit',h2),
 Table([[img('shot17.png',fw*0.50),
   [P('Figure 3: pneumatic counterbalance circuit (example 13)',ParagraphStyle('cap',parent=small,fontName='LSI')),Spacer(1,3),
    legend(['Cylinder carrying a vertical load','Counterbalance valve: holds the load','5/3 valve, closed centre: hold, raise, lower','Push button LOWER (S1); RAISE (S2) on the right','Air supply with service unit','Pressure gauge','Notes: text placed on the drawing'],cols=1,width=fw*0.44)]]],colWidths=[fw*0.54,fw*0.46],style=TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),0),('LEFTPADDING',(1,0),(1,0),12)])),
 Spacer(1,6),
 box([P('<font size="13" color="#007684"><b>Ready to try it?</b></font>',body),Spacer(1,2),
      P('Ask your PneuSim supplier for the installer or a live demonstration. Start with the built-in examples, press F1 for the Quick Guide, and have your first circuit running in minutes.',body),Spacer(1,3),
      P('Symbols are drawn by PneuSim following ISO 1219 / IEC 60617 drawing conventions; the software is not certified or approved by any standards body.',small)])]
doc.build(st)
