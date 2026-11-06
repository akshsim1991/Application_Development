import AppKit
let output = CommandLine.arguments[1]
let image = NSImage(size: NSSize(width: 1024, height: 1024))
image.lockFocus()
let background = NSBezierPath(roundedRect: NSRect(x: 60, y: 60, width: 904, height: 904), xRadius: 205, yRadius: 205)
NSGradient(starting: NSColor(calibratedRed: 0.15, green: 0.50, blue: 0.98, alpha: 1), ending: NSColor(calibratedRed: 0.06, green: 0.24, blue: 0.64, alpha: 1))!.draw(in: background, angle: -90)
let shadow = NSShadow(); shadow.shadowColor = NSColor.black.withAlphaComponent(0.22); shadow.shadowBlurRadius = 30; shadow.shadowOffset = NSSize(width: 0, height: -14)
NSGraphicsContext.saveGraphicsState(); shadow.set()
NSColor(calibratedRed: 1, green: 0.72, blue: 0.20, alpha: 1).setFill()
let tab = NSBezierPath(roundedRect: NSRect(x: 215, y: 382, width: 300, height: 287), xRadius: 38, yRadius: 38); tab.fill()
let folder = NSBezierPath(roundedRect: NSRect(x: 215, y: 296, width: 594, height: 304), xRadius: 42, yRadius: 42)
NSGradient(starting: NSColor(calibratedRed: 1, green: 0.87, blue: 0.41, alpha: 1), ending: NSColor(calibratedRed: 1, green: 0.65, blue: 0.12, alpha: 1))!.draw(in: folder, angle: -90)
NSGraphicsContext.restoreGraphicsState()
NSColor.white.withAlphaComponent(0.9).setFill()
NSBezierPath(roundedRect: NSRect(x: 510, y: 390, width: 37, height: 117), xRadius: 7, yRadius: 7).fill()
NSBezierPath(roundedRect: NSRect(x: 564, y: 390, width: 139, height: 117), xRadius: 7, yRadius: 7).fill()
image.unlockFocus()
let rep = NSBitmapImageRep(data: image.tiffRepresentation!)!
try rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: output))
