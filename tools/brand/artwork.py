"""CoD2 Unbound's artwork: one set of shapes for the app icon, the mark inside the launcher and the logo.

usage: python3 tools/brand/artwork.py          (from anywhere; writes into this checkout)

Writes
  tools/cod2x/icon-source/CoD2 Unbound.icon    the Icon Composer document the app icon is compiled from
  docs/brand/*.svg                             mark, monogram, wordmark and lockups
  launcher/BrandShapes.swift                   the same shapes for the launcher's own drawing

Needs glyphs.py beside it. No font is used anywhere, and nothing is traced from another work.

  app icon   a flat gold star with a cream soft block 2 cut out of it with a gap, on a plain olive plate
             (no bevel line and no rivets: the owner's final choice)
  monogram   the app icon with a small C and U under the star's arms
"""
import json, math, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from glyphs import draw, path, word
root = pathlib.Path(__file__).resolve().parents[2]
out = root / "docs/brand"; out.mkdir(parents=True, exist_ok=True)
icons = root / "tools/cod2x/icon-source"

# --- geometry, in the 1024 canvas ---------------------------------------------------------------------
TWO = (331, 229, 362, 574, 114, 74, 23)      # x, y, W, H, stroke, corner cut, corner softening
KEY = 28                                      # the gap between the 2 (or a letter) and the star
STAR = (512, 536, 410)                        # centre x, centre y, radius
LETTER = dict(W=150, H=232, t=54, c=34, soft=12)
LETTERS = [("C", 138), ("U", 736)]            # tucked under the star's arms, on the 2's baseline
BASE = TWO[1] + TWO[3]
CREAM, CREAM_D = ("#f6edcd", "#dccb97"), ("#ece0b6", "#c4b480")
GOLD, GOLD_D = ("#d9b55c", "#a57d2a"), ("#bf9a42", "#86651f")
PLATE, PLATE_D = ("#6f7a44", "#323819"), ("#262a1b", "#0c0d08")
INK, FLAT_CREAM, FLAT_GOLD = "#191c0f", "#efe4be", "#c9a44c"

def star_path():
    cx, cy, R = STAR
    pts = [(cx + (R if i % 2 == 0 else R * 0.382) * math.sin(i * math.pi / 5), cy - (R if i % 2 == 0 else R * 0.382) * math.cos(i * math.pi / 5)) for i in range(10)]
    return "M" + " L".join(f"{x:.1f} {y:.1f}" for x, y in pts) + " Z"
def characters(fill, grow=0, letters=False, only=None):
    s = "" if only == "letters" else draw("2", *TWO, fill, grow=grow)
    if letters and only != "two":
        for ch, x in LETTERS:
            s += draw(ch, x, BASE - LETTER["H"], LETTER["W"], LETTER["H"], LETTER["t"], LETTER["c"], LETTER["soft"], fill, grow=grow)
    return s
def star(fill, letters=False, uid="k"):
    return (f'<defs><mask id="{uid}" maskUnits="userSpaceOnUse" x="0" y="0" width="1024" height="1024"><rect width="1024" height="1024" fill="#fff"/>'
            f'{characters("#000", KEY, letters)}</mask></defs><path mask="url(#{uid})" d="{star_path()}" fill="{fill}"/>')

# --- Icon Composer documents --------------------------------------------------------------------------
SVG = '<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">\n{}\n</svg>\n'
def srgb(h): return "srgb:" + ",".join(f"{int(h[i:i+2], 16) / 255:.5f}" for i in (1, 3, 5)) + ",1.00000"
def fills(light, dark): return [{"value": {"linear-gradient": [srgb(light[0]), srgb(light[1])]}}, {"appearance": "dark", "value": {"linear-gradient": [srgb(dark[0]), srgb(dark[1])]}},
                                {"appearance": "tinted", "value": {"solid": "srgb:1.00000,1.00000,1.00000,1.00000"}}]
def document(name, letters):
    doc = icons / f"{name}.icon"; assets = doc / "Assets"; assets.mkdir(parents=True, exist_ok=True)
    (assets / "characters.svg").write_text(SVG.format(characters(CREAM[0], letters=letters)))
    (assets / "star.svg").write_text(SVG.format(star(CREAM[0], letters)))
    group = lambda n, layer, shadow: {"name": n, "layers": [layer], "shadow": {"kind": shadow, "opacity": 0.5}, "translucency": {"enabled": False, "value": 0.5}}
    js = {"fill-specializations": [{"value": {"linear-gradient": [srgb(PLATE[0]), srgb(PLATE[1])]}}, {"appearance": "dark", "value": {"linear-gradient": [srgb(PLATE_D[0]), srgb(PLATE_D[1])]}}],
          "groups": [group("Characters", {"name": "Characters", "image-name-specializations": [{"value": "characters.svg"}], "fill-specializations": fills(CREAM, CREAM_D)}, "layer-color"),
                     group("Star", {"name": "Star", "image-name-specializations": [{"value": "star.svg"}], "fill-specializations": fills(GOLD, GOLD_D)}, "neutral")],
          "supported-platforms": {"squares": ["macOS"]}}
    (doc / "icon.json").write_text(json.dumps(js, indent=2) + "\n")
document("CoD2 Unbound", False)

# --- flat SVG marks (logo, Windows, Linux, web) -------------------------------------------------------
def mark(x=0, y=0, size=1024, dark=False, letters=False, uid="m"):
    p, g, c = (PLATE_D, GOLD_D, CREAM_D) if dark else (PLATE, GOLD, CREAM)
    grad = lambda i, a, b, extra='x1="0" y1="0" x2="0" y2="1"': f'<linearGradient id="{uid}{i}" {extra}><stop offset="0" stop-color="{a}"/><stop offset="1" stop-color="{b}"/></linearGradient>'
    return (f'<g transform="translate({x} {y}) scale({size / 1024})"><defs>' + grad("b", *p) + grad("g", *g)
            + grad("c", *c, f'gradientUnits="userSpaceOnUse" x1="0" y1="{TWO[1]}" x2="0" y2="{BASE}"')
            + f'<filter id="{uid}f" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="12"/></filter></defs>'
            f'<rect width="1024" height="1024" rx="230" fill="url(#{uid}b)"/>'
            + star(f"url(#{uid}g)", letters, uid + "k")
            + f'<g transform="translate(0 12)" opacity="0.4" filter="url(#{uid}f)">{characters("#000", letters=letters)}</g>' + characters(f"url(#{uid}c)", letters=letters) + '</g>')
def mark_plain(x, y, size, color, uid, star_color=None, letters=False):
    """No plate: for one colour (star_color None) or two colours on any background."""
    return f'<g transform="translate({x} {y}) scale({size / 1024})">' + star(star_color or color, letters, uid) + characters(color, letters=letters) + '</g>'
def svg(w, h, body): return f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">{body}</svg>\n'
TAG = "Call of Duty 2 multiplayer, on every platform"
def tagline(x, y, size, color, anchor="start"):
    return f'<text x="{x}" y="{y}" font-family="Helvetica Neue, Helvetica, Arial, sans-serif" font-size="{size}" letter-spacing="{size * 0.04:.1f}" fill="{color}" text-anchor="{anchor}">{TAG}</text>'
def name(x, y, H, fg, accent):
    a, e = word("COD", x, y, H, fg); b, e = word("2", e + 0.13 * H, y, H, accent); c, e = word("UNBOUND", e + 0.36 * H, y, H, fg)
    return a + b + c, e
def horizontal(dark=True, mono=None):
    fg = mono or (FLAT_CREAM if dark else INK); accent = mono or (FLAT_GOLD if dark else "#8a6a1c"); sub = mono or ("#8f9668" if dark else "#5c6537")
    n, e = name(330, 70, 150, fg, accent)
    m = mark_plain(30, 20, 260, mono, "p") if mono else mark(40, 30, 240, dark=dark, uid="h")
    return svg(int(e + 60), 300, m + n + tagline(334, 262, 26, sub))
def stacked(dark=True):
    fg = FLAT_CREAM if dark else INK; accent = FLAT_GOLD if dark else "#8a6a1c"; sub = "#8f9668" if dark else "#5c6537"; W = 900
    a, xa = word("COD", 0, 0, 150, fg); b, xb = word("2", xa + 0.13 * 150, 0, 150, accent); c, xc = word("UNBOUND", 0, 0, 110, fg)
    return svg(W, 800, mark((W - 340) / 2, 40, 340, dark=dark, uid="s") + f'<g transform="translate({(W - xb) / 2:.1f} 420)">{a}{b}</g><g transform="translate({(W - xc) / 2:.1f} 600)">{c}</g>'
               + tagline(W / 2, 765, 24, sub, "middle"))
def wordmark(fg, accent):
    n, e = name(20, 20, 150, fg, accent); return svg(int(e + 20), 190, n)
files = {
    "mark.svg": svg(1024, 1024, mark(uid="a")), "mark-dark.svg": svg(1024, 1024, mark(dark=True, uid="a")),
    "monogram.svg": svg(1024, 1024, mark(letters=True, uid="a")), "monogram-dark.svg": svg(1024, 1024, mark(dark=True, letters=True, uid="a")),
    "mark-free.svg": svg(1024, 1024, mark_plain(0, 0, 1024, FLAT_CREAM, "p", FLAT_GOLD)), "mark-free-light.svg": svg(1024, 1024, mark_plain(0, 0, 1024, INK, "p", "#8a6a1c")),
    "mark-mono-cream.svg": svg(1024, 1024, mark_plain(0, 0, 1024, FLAT_CREAM, "p")), "mark-mono-ink.svg": svg(1024, 1024, mark_plain(0, 0, 1024, INK, "p")),
    "wordmark.svg": wordmark(FLAT_CREAM, FLAT_GOLD), "wordmark-ink.svg": wordmark(INK, "#8a6a1c"),
    "lockup-horizontal.svg": horizontal(True), "lockup-horizontal-light.svg": horizontal(False),
    "lockup-horizontal-mono.svg": horizontal(True, FLAT_CREAM), "lockup-horizontal-mono-ink.svg": horizontal(False, INK),
    "lockup-stacked.svg": stacked(True), "lockup-stacked-light.svg": stacked(False),
}
s, e = word("CODUNB2", 20, 20, 150, FLAT_CREAM, gap=40); files["lettering.svg"] = svg(int(e + 20), 190, s)
for k, v in files.items(): (out / k).write_text(v)

# --- the launcher's copy of the shapes ----------------------------------------------------------------
def rgb(h): return "0x" + h[1:]
def glyphs(text, x, y, H):
    """word() again, as (path, is the 2) pairs, so the launcher can colour the 2 on its own."""
    t, gap, soft, c = 0.172 * H, 0.13 * H, 0.04 * H, 0.125 * H; found = []
    for ch in text:
        W = 0.64 * H if ch == "2" else 0.62 * H
        found.append((path(ch, x, y, W, H, t, c, soft), ch == "2")); x += W + gap
    return found, x - gap
a, e = glyphs("COD", 20, 20, 150); b, e = glyphs("2", e + 0.13 * 150, 20, 150); c, e = glyphs("UNBOUND", e + 0.36 * 150, 20, 150)
rows = ",\n".join(f'        ("{p}", {"true" if two else "false"})' for p, two in a + b + c)
swift = f"""// Written by tools/brand/artwork.py. Change the geometry there and run it again; do not edit this file.
import CoreGraphics

/// CoD2 Unbound's mark and wordmark as path data, the same shapes as the app icon and the logo files.
enum BrandShapes {{
    /// The mark is drawn in a 1024 square.
    static let star = "{star_path()}"
    /// The 2's outline before its corners are softened: fill it and stroke it `twoStroke` wide with round joins.
    static let two = "{path("2", *TWO)}"
    static let twoStroke: CGFloat = {2 * TWO[6]}
    /// The same stroke, wider: what the 2 cuts out of the star.
    static let cutStroke: CGFloat = {2 * (TWO[6] + KEY)}
    static let twoTop: CGFloat = {TWO[1]}, twoBase: CGFloat = {BASE}
    static let starTop: CGFloat = {STAR[1] - STAR[2]}, starBase: CGFloat = {STAR[1] + STAR[2] * 0.809:.1f}
    static let plateRadius: CGFloat = 230
    static let plate: (top: UInt32, base: UInt32) = ({rgb(PLATE[0])}, {rgb(PLATE[1])})
    static let gold: (top: UInt32, base: UInt32) = ({rgb(GOLD[0])}, {rgb(GOLD[1])})
    static let cream: (top: UInt32, base: UInt32) = ({rgb(CREAM[0])}, {rgb(CREAM[1])})

    /// The wordmark, COD 2 UNBOUND, in a {int(e + 20)} by 190 box. Each glyph is filled and stroked `letterStroke` wide.
    static let wordmarkSize = CGSize(width: {int(e + 20)}, height: 190)
    static let letterStroke: CGFloat = {2 * 0.04 * 150:.0f}
    /// On dark grounds, and on light ones: the letters, then the 2.
    static let onDark: (letters: UInt32, two: UInt32) = ({rgb(FLAT_CREAM)}, {rgb(FLAT_GOLD)})
    static let onLight: (letters: UInt32, two: UInt32) = ({rgb(INK)}, 0x8a6a1c)
    static let wordmark: [(path: String, two: Bool)] = [
{rows}
    ]
}}
"""
(root / "launcher/BrandShapes.swift").write_text(swift)
print("wrote", len(files), "svg files, the icon document and launcher/BrandShapes.swift")
