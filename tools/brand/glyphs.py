"""CoD2 Unbound lettering: block capitals and the 2, drawn from straight strokes with cut corners.
Each glyph is a polygon (plus holes) in a cell; soft corners come from stroking the inset outline
with round joins. No font is used."""
import math

def g2(x, y, W, H, t, c):
    hook = y + 0.34 * H; base = y + H - t; x0, x1 = x, x + W
    ux, uy = x1 - t, y + 0.315 * H; ly = base - 0.08 * H
    slope = (ly - uy) / (x0 - ux); drop = t / math.cos(math.atan(abs(slope)))
    ry = uy + slope * (x1 - ux) + drop; bx = ux + (base - (uy + drop)) / slope
    return [[(x0, y + c), (x0 + c, y), (x1 - c, y), (x1, y + c), (x1, ry), (bx, base), (x1, base), (x1, y + H), (x0, y + H), (x0, ly),
             (ux, uy), (ux, y + t), (x0 + t, y + t), (x0 + t, hook), (x0, hook)]]
def gC(x, y, W, H, t, c):
    a = 0.35 * H
    return [[(x + c, y), (x + W - c, y), (x + W, y + c), (x + W, y + a), (x + W - t, y + a), (x + W - t, y + t), (x + t, y + t), (x + t, y + H - t), (x + W - t, y + H - t),
             (x + W - t, y + H - a), (x + W, y + H - a), (x + W, y + H - c), (x + W - c, y + H), (x + c, y + H), (x, y + H - c), (x, y + c)]]
def gO(x, y, W, H, t, c):
    return [[(x + c, y), (x + W - c, y), (x + W, y + c), (x + W, y + H - c), (x + W - c, y + H), (x + c, y + H), (x, y + H - c), (x, y + c)],
            [(x + t, y + t), (x + W - t, y + t), (x + W - t, y + H - t), (x + t, y + H - t)]]
def gD(x, y, W, H, t, c):
    return [[(x, y), (x + W - c, y), (x + W, y + c), (x + W, y + H - c), (x + W - c, y + H), (x, y + H)],
            [(x + t, y + t), (x + W - t, y + t), (x + W - t, y + H - t), (x + t, y + H - t)]]
def gU(x, y, W, H, t, c):
    return [[(x, y), (x + t, y), (x + t, y + H - t), (x + W - t, y + H - t), (x + W - t, y), (x + W, y), (x + W, y + H - c), (x + W - c, y + H), (x + c, y + H), (x, y + H - c)]]
def gN(x, y, W, H, t, c):
    d = 1.5 * t
    return [[(x, y), (x + t, y), (x + W - t, y + H - d), (x + W - t, y), (x + W, y), (x + W, y + H), (x + W - t, y + H), (x + t, y + d), (x + t, y + H), (x, y + H)]]
def gB(x, y, W, H, t, c):
    m = y + H / 2; n = 0.55 * c; h = t * 0.42
    return [[(x, y), (x + W - c, y), (x + W, y + c), (x + W, m - n), (x + W - n, m), (x + W, m + n), (x + W, y + H - c), (x + W - c, y + H), (x, y + H)],
            [(x + t, y + t), (x + W - t, y + t), (x + W - t, m - h), (x + t, m - h)],
            [(x + t, m + h), (x + W - t, m + h), (x + W - t, y + H - t), (x + t, y + H - t)]]
GLYPHS = {"2": g2, "C": gC, "O": gO, "D": gD, "U": gU, "N": gN, "B": gB}

def path(ch, x, y, W, H, t, c, soft):
    """SVG path data for one glyph whose finished size is W x H with stroke weight t; `soft` is the corner radius."""
    rings = GLYPHS[ch](x + soft, y + soft, W - 2 * soft, H - 2 * soft, t - 2 * soft, max(c - soft * 0.6, 1))
    return " ".join("M" + " L".join(f"{px:.1f} {py:.1f}" for px, py in ring) + " Z" for ring in rings)
def draw(ch, x, y, W, H, t, c, soft, fill, grow=0):
    """`grow` fattens the glyph evenly, for a keyline around it."""
    return (f'<path fill-rule="evenodd" d="{path(ch, x, y, W, H, t, c, soft)}" fill="{fill}" stroke="{fill}" stroke-width="{2 * (soft + grow):.1f}" stroke-linejoin="round"/>')
def word(text, x, y, H, fill, t=None, gap=None, soft=None, widths=None):
    """A run of glyphs; returns (svg, end x). Glyph width is 0.62 H, the 2 a little wider."""
    t = t or 0.172 * H; gap = gap if gap is not None else 0.13 * H; soft = soft if soft is not None else 0.04 * H; c = 0.125 * H
    s = ""
    for ch in text:
        if ch == " ": x += 0.36 * H; continue
        W = (widths or {}).get(ch, 0.64 * H if ch == "2" else 0.62 * H)
        s += draw(ch, x, y, W, H, t, c, soft, fill); x += W + gap
    return s, x - gap
