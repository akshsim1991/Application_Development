# Narration, split into scenes -> sentences (each sentence becomes one caption).
SCENES = [
 ("intro", "Flanged Hub in Autodesk Fusion", [
  "Hi everyone, and welcome!",
  "In this lesson we will model a flanged hub in Autodesk Fusion, starting from a simple engineering drawing.",
  "It has a round flange, one hundred and twenty millimetres across, a sixty millimetre hub, a thirty millimetre bore with a key slot, and four bolt holes.",
  "Along the way you will practise sketches, extrude, cut, the hole tool, and patterns, the core of everyday parametric modelling.",
  "Let's get started.",
 ]),
 ("read", "Step 1 · Read the drawing", [
  "First, let's read the drawing.",
  "The flange is one hundred and twenty millimetres in diameter and sixteen millimetres thick.",
  "The hub is sixty in diameter, and the total height is forty, so the hub rises twenty four millimetres above the flange.",
  "The bore is thirty millimetres, with an eight millimetre wide key slot.",
  "The thirty three point three dimension runs from the bottom of the bore to the top of the slot, so the slot is three point three millimetres deeper than the bore.",
  "Finally, four holes of twelve millimetres sit equally spaced on a ninety millimetre pitch circle.",
 ]),
 ("setup", "Step 2 · Set up the design", [
  "Open Fusion and start a new design.",
  "Check that your units are millimetres, and save the file right away. I will call mine flanged hub.",
  "In the browser on the left, you can see the origin, with its planes and axes. We will sketch on the X Y plane.",
 ]),
 ("sketch1", "Step 3 · Sketch the base circles", [
  "Choose Create Sketch, and click the X Y plane.",
  "Pick Center Diameter Circle, or just press C. Click on the origin, and type one hundred and twenty for the diameter.",
  "Draw a second circle from the same centre, and set it to sixty.",
  "Press D for the Sketch Dimension tool if you ever need to add or fix a size.",
  "When both circles turn black, they are fully constrained. Click Finish Sketch.",
 ]),
 ("extrude", "Step 4 · Extrude the flange and hub", [
  "Now press E for Extrude.",
  "Select the outer ring and the inner disc together, and enter sixteen millimetres. That gives us the flange.",
  "Extrude again, but this time select only the inner sixty millimetre circle, enter forty millimetres, and set the operation to Join.",
  "The hub now rises to a total height of forty.",
 ]),
 ("boresketch", "Step 5 · Sketch the bore and key slot", [
  "Next, the bore and the key slot. Start a new sketch on the top face of the hub.",
  "Draw a thirty millimetre circle from the origin.",
  "Then add a rectangle for the key slot. It is eight millimetres wide, centred on the Y axis, and it reaches eighteen point three millimetres up from the origin.",
  "That matches the thirty three point three on the drawing: fifteen for the bore radius, plus three point three.",
  "Use Trim to remove the lines inside the circle, so you have one closed profile, then finish the sketch.",
 ]),
 ("cut", "Step 6 · Cut the bore", [
  "Press E again and select the bore profile.",
  "Set the extent to All, and the operation to Cut.",
  "Fusion removes the material straight through the hub, making the bore and the key slot in one go.",
 ]),
 ("holesketch", "Step 7 · Sketch the bolt hole position", [
  "Now for the bolt holes. Start a sketch on the top face of the flange.",
  "Draw a circle ninety millimetres in diameter, and press X to turn it into a construction line. This is our pitch circle.",
  "Place a point at the top of that circle, on the Y axis, and finish the sketch.",
 ]),
 ("hole", "Step 8 · Hole tool and circular pattern", [
  "Press H for the Hole tool. Pick the sketch point, set the diameter to twelve, and the extent to All, so it goes right through.",
  "Then choose Create, Pattern, Circular Pattern.",
  "Set the object type to Features, select the hole, choose the hub's cylindrical face as the axis, and enter four for the quantity.",
  "Fusion spaces the holes evenly around three hundred and sixty degrees. That is the E Q S, equally spaced, note on the drawing.",
 ]),
 ("finish", "Step 9 · Check and finish", [
  "To check your work, use Inspect, Measure, and compare against the drawing: the overall diameter, the height, and the hole positions.",
  "You can also assign a material such as steel to see the mass.",
  "And that's it! A complete parametric flange in just a handful of features.",
  "Save your model, then try changing a dimension in the timeline and watch everything update.",
  "Thanks for watching, and happy modelling!",
 ]),
]
