// Draws the app icon (original art: two cartoon fish meeting in a tank, for "co-op").
// Usage: swift icon.swift out.png [size]   (scripts/make-icons.sh builds the .ico/.icns)
import AppKit

let args = CommandLine.arguments
let out = args.count > 1 ? args[1] : "icon.png"
let S = CGFloat(args.count > 2 ? Double(args[2]) ?? 1024 : 1024)
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(S), pixelsHigh: Int(S), bitsPerSample: 8,
                           samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                           colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
let ctx = NSGraphicsContext.current!.cgContext
ctx.scaleBy(x: S / 1024, y: S / 1024)			// draw in 1024 units, y up

func rgb(_ r: CGFloat, _ g: CGFloat, _ b: CGFloat, _ a: CGFloat = 1) -> NSColor {
    NSColor(calibratedRed: r / 255, green: g / 255, blue: b / 255, alpha: a)
}

// The tank: a rounded square (macOS icon grid: 824 px inside a 1024 canvas).
let tank = NSBezierPath(roundedRect: NSRect(x: 100, y: 100, width: 824, height: 824), xRadius: 185, yRadius: 185)
NSGraphicsContext.saveGraphicsState()
let shadow = NSShadow()
shadow.shadowColor = rgb(0, 20, 50, 0.45); shadow.shadowBlurRadius = 28; shadow.shadowOffset = NSSize(width: 0, height: -12)
shadow.set()
rgb(10, 70, 150).setFill(); tank.fill()
NSGraphicsContext.restoreGraphicsState()
NSGraphicsContext.saveGraphicsState()
tank.addClip()
NSGradient(colors: [rgb(8, 52, 128), rgb(22, 120, 200), rgb(95, 215, 245)])!.draw(in: NSRect(x: 100, y: 100, width: 824, height: 824), angle: 90)
// Light rays.
for (x, w) in [(260.0, 70.0), (430.0, 110.0), (640.0, 80.0)] {
    let ray = NSBezierPath()
    ray.move(to: NSPoint(x: x, y: 924)); ray.line(to: NSPoint(x: x + w, y: 924))
    ray.line(to: NSPoint(x: x + w * 2.2 - 60, y: 120)); ray.line(to: NSPoint(x: x - 130, y: 120)); ray.close()
    rgb(255, 255, 255, 0.10).setFill(); ray.fill()
}
// Sand and a couple of plants.
let sand = NSBezierPath()
sand.move(to: NSPoint(x: 100, y: 250))
sand.curve(to: NSPoint(x: 924, y: 235), controlPoint1: NSPoint(x: 380, y: 320), controlPoint2: NSPoint(x: 640, y: 170))
sand.line(to: NSPoint(x: 924, y: 100)); sand.line(to: NSPoint(x: 100, y: 100)); sand.close()
NSGradient(colors: [rgb(214, 170, 90), rgb(240, 205, 125)])!.draw(in: sand, angle: 90)
for (x, h, lean) in [(180.0, 300.0, 30.0), (225.0, 220.0, -20.0), (830.0, 260.0, -30.0)] {
    let weed = NSBezierPath()
    weed.move(to: NSPoint(x: x, y: 230))
    weed.curve(to: NSPoint(x: x + lean, y: 230 + h), controlPoint1: NSPoint(x: x - 60, y: 230 + h * 0.4), controlPoint2: NSPoint(x: x + 60, y: 230 + h * 0.7))
    weed.lineWidth = 26; weed.lineCapStyle = .round
    rgb(60, 185, 90).setStroke(); weed.stroke()
}

// A cartoon fish centered at (cx, cy); facing right when dir = 1, left when -1.
func fish(cx: CGFloat, cy: CGFloat, r: CGFloat, dir: CGFloat, body: NSColor, belly: NSColor, fin: NSColor) {
    NSGraphicsContext.saveGraphicsState()
    let t = NSAffineTransform()
    t.translateX(by: cx, yBy: cy); t.scaleX(by: dir, yBy: 1); t.concat()
    // Tail.
    let tail = NSBezierPath()
    tail.move(to: NSPoint(x: -r * 0.8, y: 0))
    tail.curve(to: NSPoint(x: -r * 1.75, y: r * 0.75), controlPoint1: NSPoint(x: -r * 1.2, y: r * 0.2), controlPoint2: NSPoint(x: -r * 1.5, y: r * 0.6))
    tail.curve(to: NSPoint(x: -r * 1.75, y: -r * 0.75), controlPoint1: NSPoint(x: -r * 1.45, y: r * 0.1), controlPoint2: NSPoint(x: -r * 1.45, y: -r * 0.1))
    tail.curve(to: NSPoint(x: -r * 0.8, y: 0), controlPoint1: NSPoint(x: -r * 1.5, y: -r * 0.6), controlPoint2: NSPoint(x: -r * 1.2, y: -r * 0.2))
    fin.setFill(); tail.fill()
    // Top fin.
    let top = NSBezierPath()
    top.move(to: NSPoint(x: -r * 0.45, y: r * 0.72))
    top.curve(to: NSPoint(x: r * 0.35, y: r * 0.78), controlPoint1: NSPoint(x: -r * 0.3, y: r * 1.35), controlPoint2: NSPoint(x: r * 0.2, y: r * 1.25))
    top.close(); fin.setFill(); top.fill()
    // Body with a belly highlight.
    let bodyPath = NSBezierPath(ovalIn: NSRect(x: -r, y: -r * 0.8, width: r * 2, height: r * 1.6))
    NSGraphicsContext.saveGraphicsState()
    let sh = NSShadow(); sh.shadowColor = rgb(0, 0, 0, 0.3); sh.shadowBlurRadius = r * 0.12; sh.shadowOffset = NSSize(width: 0, height: -r * 0.06); sh.set()
    body.setFill(); bodyPath.fill()
    NSGraphicsContext.restoreGraphicsState()
    NSGraphicsContext.saveGraphicsState()
    bodyPath.addClip()
    belly.setFill(); NSBezierPath(ovalIn: NSRect(x: -r * 0.8, y: -r * 1.05, width: r * 1.7, height: r * 0.95)).fill()
    rgb(255, 255, 255, 0.35).setFill(); NSBezierPath(ovalIn: NSRect(x: -r * 0.5, y: r * 0.25, width: r * 0.9, height: r * 0.35)).fill()
    NSGraphicsContext.restoreGraphicsState()
    // Side fin.
    let side = NSBezierPath()
    side.move(to: NSPoint(x: -r * 0.1, y: -r * 0.15))
    side.curve(to: NSPoint(x: -r * 0.55, y: -r * 0.5), controlPoint1: NSPoint(x: -r * 0.3, y: -r * 0.1), controlPoint2: NSPoint(x: -r * 0.55, y: -r * 0.25))
    side.curve(to: NSPoint(x: -r * 0.1, y: -r * 0.15), controlPoint1: NSPoint(x: -r * 0.35, y: -r * 0.5), controlPoint2: NSPoint(x: -r * 0.15, y: -r * 0.35))
    fin.setFill(); side.fill()
    // Eye and smile.
    rgb(255, 255, 255).setFill(); NSBezierPath(ovalIn: NSRect(x: r * 0.22, y: r * 0.05, width: r * 0.5, height: r * 0.55)).fill()
    rgb(20, 20, 30).setFill(); NSBezierPath(ovalIn: NSRect(x: r * 0.42, y: r * 0.15, width: r * 0.25, height: r * 0.3)).fill()
    rgb(255, 255, 255).setFill(); NSBezierPath(ovalIn: NSRect(x: r * 0.52, y: r * 0.32, width: r * 0.08, height: r * 0.08)).fill()
    let smile = NSBezierPath()
    smile.move(to: NSPoint(x: r * 0.55, y: -r * 0.3))
    smile.curve(to: NSPoint(x: r * 0.9, y: -r * 0.12), controlPoint1: NSPoint(x: r * 0.7, y: -r * 0.42), controlPoint2: NSPoint(x: r * 0.85, y: -r * 0.3))
    smile.lineWidth = r * 0.07; smile.lineCapStyle = .round
    rgb(90, 30, 20).setStroke(); smile.stroke()
    NSGraphicsContext.restoreGraphicsState()
}

fish(cx: 395, cy: 515, r: 128, dir: 1, body: rgb(255, 160, 30), belly: rgb(255, 215, 90), fin: rgb(240, 90, 50))
fish(cx: 655, cy: 585, r: 116, dir: -1, body: rgb(80, 205, 150), belly: rgb(175, 240, 190), fin: rgb(40, 150, 170))
// Bubbles rising between them.
for (x, y, d) in [(515.0, 700.0, 46.0), (545.0, 780.0, 34.0), (505.0, 845.0, 26.0)] {
    let b = NSBezierPath(ovalIn: NSRect(x: x - d / 2, y: y - d / 2, width: d, height: d))
    rgb(255, 255, 255, 0.25).setFill(); b.fill()
    b.lineWidth = 6; rgb(255, 255, 255, 0.85).setStroke(); b.stroke()
}
NSGraphicsContext.restoreGraphicsState()
// A glassy rim.
tank.lineWidth = 14
rgb(255, 255, 255, 0.35).setStroke(); tank.stroke()
NSGraphicsContext.restoreGraphicsState()
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: out))
