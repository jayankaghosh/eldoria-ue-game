// Renders the ELDORIA title logo and the Chandrabindu Studios card to transparent PNGs.
// Also the "in association with Claude Code" card, and the app icon (T_AppIcon.png, 1024 px),, from the Claude app icon (Art/ClaudeIcon.png).
// usage: swift Scripts/make_logo.swift Art/Fonts Art/Logo  (then copy Art/Logo/*.png to Content/Splash)
import AppKit
import CoreText

let args = CommandLine.arguments
let fontsDir = URL(fileURLWithPath: args[1])
let outDir = URL(fileURLWithPath: args[2])
for name in ["CinzelDecorative-Black.ttf", "CinzelDecorative-Bold.ttf", "Cinzel-Variable.ttf"] {
    CTFontManagerRegisterFontsForURL(fontsDir.appendingPathComponent(name) as CFURL, .process, nil)
}

func font(_ name: String, _ size: CGFloat, weight: CGFloat? = nil) -> CTFont {
    var f = CTFontCreateWithName(name as CFString, size, nil)
    if let w = weight {
        // variable Cinzel: 'wght' axis
        let axis = 0x77676874 as NSNumber
        let desc = CTFontDescriptorCreateCopyWithVariation(CTFontCopyFontDescriptor(f), axis as CFNumber, w)
        f = CTFontCreateWithFontDescriptor(desc, size, nil)
    }
    return f
}

func makeContext(_ w: Int, _ h: Int) -> CGContext {
    let ctx = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: 0,
                        space: CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    ctx.setAllowsAntialiasing(true)
    ctx.setShouldSmoothFonts(true)
    return ctx
}

func save(_ ctx: CGContext, _ name: String) {
    let rep = NSBitmapImageRep(cgImage: ctx.makeImage()!)
    try! rep.representation(using: .png, properties: [:])!.write(to: outDir.appendingPathComponent(name))
}

// glyph outlines of a line of text, centred on (cx, baseline)
func textPath(_ text: String, _ f: CTFont, cx: CGFloat, baseline: CGFloat, tracking: CGFloat = 0) -> (CGPath, CGRect) {
    let attr = NSAttributedString(string: text, attributes: [kCTFontAttributeName as NSAttributedString.Key: f, kCTKernAttributeName as NSAttributedString.Key: tracking])
    let line = CTLineCreateWithAttributedString(attr)
    let width = CTLineGetTypographicBounds(line, nil, nil, nil) - Double(tracking)
    let path = CGMutablePath()
    let runs = CTLineGetGlyphRuns(line) as! [CTRun]
    for run in runs {
        let count = CTRunGetGlyphCount(run)
        var glyphs = [CGGlyph](repeating: 0, count: count)
        var positions = [CGPoint](repeating: .zero, count: count)
        CTRunGetGlyphs(run, CFRange(location: 0, length: count), &glyphs)
        CTRunGetPositions(run, CFRange(location: 0, length: count), &positions)
        let runFont = (CTRunGetAttributes(run) as NSDictionary)[kCTFontAttributeName] as! CTFont
        for i in 0..<count {
            if let g = CTFontCreatePathForGlyph(runFont, glyphs[i], nil) {
                let t = CGAffineTransform(translationX: cx - CGFloat(width) / 2 + positions[i].x, y: baseline + positions[i].y)
                path.addPath(g, transform: t)
            }
        }
    }
    return (path, path.boundingBoxOfPath)
}

let goldStops: [CGFloat] = [
    1.00, 0.97, 0.80, 1,   // pale highlight
    0.98, 0.82, 0.42, 1,
    0.80, 0.55, 0.18, 1,   // horizon line of the metal
    0.99, 0.86, 0.50, 1,
    0.55, 0.33, 0.09, 1,   // deep shadow
]
let goldLocs: [CGFloat] = [0, 0.38, 0.5, 0.62, 1]
let gold = CGGradient(colorSpace: CGColorSpace(name: CGColorSpace.sRGB)!, colorComponents: goldStops, locations: goldLocs, count: 5)!

func goldText(_ ctx: CGContext, _ path: CGPath, _ box: CGRect, outline: CGFloat) {
    // soft dark halo so it reads over any background
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: -outline * 1.2), blur: outline * 6, color: CGColor(red: 0, green: 0, blue: 0, alpha: 0.95))
    ctx.addPath(path); ctx.setFillColor(CGColor(red: 0.08, green: 0.05, blue: 0.02, alpha: 1)); ctx.fillPath()
    ctx.restoreGState()
    // dark bronze rim
    ctx.saveGState()
    ctx.addPath(path); ctx.setLineWidth(outline * 2); ctx.setLineJoin(.round)
    ctx.setStrokeColor(CGColor(red: 0.18, green: 0.10, blue: 0.03, alpha: 1)); ctx.strokePath()
    ctx.restoreGState()
    // thin bright inner rim
    ctx.saveGState()
    ctx.addPath(path); ctx.setLineWidth(outline * 0.7); ctx.setLineJoin(.round)
    ctx.setStrokeColor(CGColor(red: 0.95, green: 0.78, blue: 0.40, alpha: 1)); ctx.strokePath()
    ctx.restoreGState()
    // gold face
    ctx.saveGState()
    ctx.addPath(path); ctx.clip()
    ctx.drawLinearGradient(gold, start: CGPoint(x: 0, y: box.maxY), end: CGPoint(x: 0, y: box.minY), options: [])
    ctx.restoreGState()
}

func diamond(_ ctx: CGContext, _ c: CGPoint, _ r: CGFloat) {
    let p = CGMutablePath()
    p.move(to: CGPoint(x: c.x, y: c.y + r)); p.addLine(to: CGPoint(x: c.x + r * 0.7, y: c.y))
    p.addLine(to: CGPoint(x: c.x, y: c.y - r)); p.addLine(to: CGPoint(x: c.x - r * 0.7, y: c.y)); p.closeSubpath()
    goldText(ctx, p, p.boundingBoxOfPath.insetBy(dx: 0, dy: -r), outline: 2)
}

// tapered rule: thick at the centre end, a hairline at the far end
func rule(_ ctx: CGContext, from a: CGPoint, to b: CGPoint, thick: CGFloat) {
    let p = CGMutablePath()
    p.move(to: CGPoint(x: a.x, y: a.y + thick / 2)); p.addLine(to: CGPoint(x: b.x, y: b.y + 0.6))
    p.addLine(to: CGPoint(x: b.x, y: b.y - 0.6)); p.addLine(to: CGPoint(x: a.x, y: a.y - thick / 2)); p.closeSubpath()
    goldText(ctx, p, p.boundingBoxOfPath.insetBy(dx: 0, dy: -6), outline: 1.5)
}

// ---------- ELDORIA ----------
do {
    let W = 2400, H = 820
    let ctx = makeContext(W, H)
    let cx = CGFloat(W) / 2
    let (title, tbox) = textPath("ELDORIA", font("CinzelDecorative-Black", 330), cx: cx, baseline: 330, tracking: 18)
    goldText(ctx, title, tbox, outline: 7)
    // ornament under the title: rules either side of a diamond, then the subtitle
    let y: CGFloat = 255
    diamond(ctx, CGPoint(x: cx, y: y), 22)
    rule(ctx, from: CGPoint(x: cx - 40, y: y), to: CGPoint(x: cx - 760, y: y), thick: 7)
    rule(ctx, from: CGPoint(x: cx + 40, y: y), to: CGPoint(x: cx + 760, y: y), thick: 7)
    diamond(ctx, CGPoint(x: cx - 790, y: y), 10)
    diamond(ctx, CGPoint(x: cx + 790, y: y), 10)
    let (sub, sbox) = textPath("THE BONE KING", font("Cinzel", 92, weight: 700), cx: cx, baseline: 120, tracking: 34)
    goldText(ctx, sub, sbox, outline: 3)
    save(ctx, "T_Logo_Eldoria.png")
}

// ---------- Chandrabindu Studios ----------
do {
    let W = 1900, H = 900
    let ctx = makeContext(W, H)
    let cx = CGFloat(W) / 2
    // emblem: the chandrabindu, a crescent cradling a dot, inside a thin ring
    let ec = CGPoint(x: cx, y: 600)
    let ring = CGMutablePath()
    ring.addEllipse(in: CGRect(x: ec.x - 165, y: ec.y - 165, width: 330, height: 330))
    ring.addEllipse(in: CGRect(x: ec.x - 152, y: ec.y - 152, width: 304, height: 304))
    let moon = CGMutablePath()
    // crescent opening upwards: a big disc minus a disc shifted up
    let outer = CGPath(ellipseIn: CGRect(x: ec.x - 105, y: ec.y - 115, width: 210, height: 210), transform: nil)
    let inner = CGPath(ellipseIn: CGRect(x: ec.x - 92, y: ec.y - 72, width: 184, height: 184), transform: nil)
    moon.addPath(outer.subtracting(inner))
    let dot = CGPath(ellipseIn: CGRect(x: ec.x - 30, y: ec.y + 42, width: 60, height: 60), transform: nil)
    let emblem = CGMutablePath()
    emblem.addPath(ring.normalized(using: .evenOdd)); emblem.addPath(moon); emblem.addPath(dot)
    goldText(ctx, emblem, emblem.boundingBoxOfPath, outline: 3)
    let (name, nbox) = textPath("CHANDRABINDU", font("Cinzel", 150, weight: 800), cx: cx, baseline: 245, tracking: 22)
    goldText(ctx, name, nbox, outline: 4)
    let (studios, sbox) = textPath("STUDIOS", font("Cinzel", 70, weight: 600), cx: cx, baseline: 150, tracking: 60)
    goldText(ctx, studios, sbox, outline: 2.5)
    rule(ctx, from: CGPoint(x: cx - 400, y: 172), to: CGPoint(x: cx - 760, y: 175), thick: 4)
    rule(ctx, from: CGPoint(x: cx + 400, y: 172), to: CGPoint(x: cx + 760, y: 175), thick: 4)
    save(ctx, "T_Logo_Studio.png")
}
// ---------- in association with Claude Code ----------
// same treatment as the studio card: gold Cinzel, tapered rules; the Claude icon as the emblem
do {
    let W = 1900, H = 900
    let ctx = makeContext(W, H)
    let cx = CGFloat(W) / 2
    let (with, wbox) = textPath("IN ASSOCIATION WITH", font("Cinzel", 52, weight: 600), cx: cx, baseline: 800, tracking: 30)
    goldText(ctx, with, wbox, outline: 2)
    let icon = NSImage(contentsOf: URL(fileURLWithPath: fontsDir.deletingLastPathComponent().appendingPathComponent("ClaudeIcon.png").path))!
    let rep = icon.cgImage(forProposedRect: nil, context: nil, hints: nil)!
    let size: CGFloat = 380
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: -6), blur: 30, color: CGColor(red: 0, green: 0, blue: 0, alpha: 0.9))
    ctx.draw(rep, in: CGRect(x: cx - size / 2, y: 330, width: size, height: size))
    ctx.restoreGState()
    let (name, nbox) = textPath("CLAUDE CODE", font("Cinzel", 150, weight: 800), cx: cx, baseline: 175, tracking: 22)
    goldText(ctx, name, nbox, outline: 4)
    rule(ctx, from: CGPoint(x: cx - 120, y: 115), to: CGPoint(x: cx - 700, y: 115), thick: 4)
    rule(ctx, from: CGPoint(x: cx + 120, y: 115), to: CGPoint(x: cx + 700, y: 115), thick: 4)
    diamond(ctx, CGPoint(x: cx, y: 115), 14)
    save(ctx, "T_Logo_Claude.png")
}
// ---------- app icon: a gold E on dark leather, macOS rounded-square shape ----------
do {
    let N = 1024
    let ctx = makeContext(N, N)
    // Apple's icon grid: the body is an 824 px rounded square with a soft shadow
    let body = CGRect(x: 100, y: 100, width: 824, height: 824)
    let shape = CGPath(roundedRect: body, cornerWidth: 185, cornerHeight: 185, transform: nil)
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: -10), blur: 28, color: CGColor(red: 0, green: 0, blue: 0, alpha: 0.55))
    ctx.addPath(shape); ctx.setFillColor(CGColor(red: 0.05, green: 0.035, blue: 0.025, alpha: 1)); ctx.fillPath()
    ctx.restoreGState()
    // warm glow from the centre, like firelight on dark leather
    ctx.saveGState()
    ctx.addPath(shape); ctx.clip()
    let glow = CGGradient(colorSpace: CGColorSpace(name: CGColorSpace.sRGB)!, colorComponents: [0.30, 0.18, 0.08, 1, 0.12, 0.07, 0.035, 1, 0.04, 0.025, 0.02, 1], locations: [0, 0.55, 1], count: 3)!
    ctx.drawRadialGradient(glow, startCenter: CGPoint(x: 512, y: 560), startRadius: 0, endCenter: CGPoint(x: 512, y: 512), endRadius: 560, options: [])
    ctx.restoreGState()
    // a thin gold frame just inside the edge
    let frame = CGMutablePath()
    frame.addPath(CGPath(roundedRect: body.insetBy(dx: 34, dy: 34), cornerWidth: 150, cornerHeight: 150, transform: nil))
    frame.addPath(CGPath(roundedRect: body.insetBy(dx: 44, dy: 44), cornerWidth: 140, cornerHeight: 140, transform: nil))
    goldText(ctx, frame.copy(using: nil)!.normalized(using: .evenOdd), body, outline: 1.5)
    // the initial
    let (e, ebox) = textPath("E", font("CinzelDecorative-Black", 560), cx: 512, baseline: 335, tracking: 0)
    goldText(ctx, e, ebox, outline: 6)
    // underline ornament
    diamond(ctx, CGPoint(x: 512, y: 262), 16)
    rule(ctx, from: CGPoint(x: 482, y: 262), to: CGPoint(x: 300, y: 262), thick: 5)
    rule(ctx, from: CGPoint(x: 542, y: 262), to: CGPoint(x: 724, y: 262), thick: 5)
    save(ctx, "T_AppIcon.png")
}
print("ok")
