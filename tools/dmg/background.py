#!/usr/bin/env python3
"""Draws the disk image's window background as SVG.

usage: python3 tools/dmg/background.py <design> <out.svg>      design: dusk | day

What Finder does with it, checked on macOS 27 in Dark appearance with a partly transparent picture:
a window with a background picture is painted as a light window whatever the appearance. The ground
under the picture is white, the two names are black, and one picture serves everyone; it cannot
follow the appearance. Only the icons do (the app's dark icon shows in Dark).

Finder draws the app (centre 180,170) and the Applications link (centre 480,170) over the picture as
128 pt icons, each with its name about 76 pt below the centre. So the names need light behind them.
In the dusk scene that light is the sunset: the sky is at its brightest in the band the names sit
in, just above the horizon. The picture is 660 by 420 pt. The window shows about 400 pt of it, and
about 300 when the person's Finder has its tab, path and status bars on, so the icons, the names,
the arrow and the wordmark all sit above y 270.

The scenes are the launcher's own North Africa scene (HeroScene in launcher/Artwork.swift),
recomposed: the same dunes, town and palm, drawn by this project. The wordmark comes from
docs/brand (tools/brand/artwork.py).
"""
import pathlib, random, re, sys

root = pathlib.Path(__file__).resolve().parents[2]
W, H = 660, 420
APP, LINK, ROW = 180, 480, 170          # icon centres
CREAM, GOLD, GOLD_D = "#f6edcd", "#d9b55c", "#a57d2a"

def wordmark(x, y, height, ink=False):
    """The drawn name, its box `height` tall, centred on x."""
    svg = (root / "docs/brand" / ("wordmark-ink.svg" if ink else "wordmark.svg")).read_text()
    w, h = map(float, re.search(r'viewBox="0 0 (\S+) (\S+)"', svg).groups())
    body = svg[svg.index(">") + 1:svg.rindex("</svg>")]
    s = height / h
    return f'<g transform="translate({x - w * s / 2:.1f} {y:.1f}) scale({s:.4f})">{body}</g>'

def chevrons(x, y, color_a, color_b, size=17, gap=30, weight=7):
    out = f'<defs><linearGradient id="arrow" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{color_a}"/><stop offset="1" stop-color="{color_b}"/></linearGradient></defs>'
    for index, opacity in enumerate((0.35, 0.65, 1)):
        cx = x + (index - 1) * gap
        out += (f'<path d="M{cx - size / 2:.1f} {y - size} L{cx + size / 2:.1f} {y} L{cx - size / 2:.1f} {y + size}" fill="none" stroke="url(#arrow)" '
                f'stroke-width="{weight}" stroke-linecap="round" stroke-linejoin="round" opacity="{opacity}"/>')
    return out

# --- the launcher's North Africa scene, in its own 1440 x 620 frame ------------------------------------
TOWN = [(610, 444, 72, 80), (682, 420, 58, 104), (740, 434, 92, 90), (832, 404, 50, 120), (882, 428, 112, 96), (994, 398, 66, 126),
        (1060, 304, 22, 220), (1054, 296, 34, 9), (1070, 268, 2, 12), (1082, 422, 80, 102), (1162, 390, 58, 134), (1220, 416, 96, 108),
        (1316, 400, 60, 124), (1376, 432, 64, 92)]
WINDOWS = [(704, 444), (772, 456), (904, 446), (1012, 414), (1100, 440), (1180, 406), (1250, 440), (1334, 416)]
DUNE_BACK = "M0 438 Q 210 400 430 424 T 870 408 T 1440 420 V620 H0Z"
DUNE_FRONT = "M0 476 Q 270 450 540 466 T 1060 452 T 1440 462 V620 H0Z"
RIDGE = "M0 566 L 110 552 L 170 560 L 290 542 L 372 556 L 470 548 L 540 566 L 640 560 L 760 574 L 900 566 L 1040 578 L 1200 570 L 1440 580 V620 H0Z"
FRONDS = ["M598 352 C 570 340 548 346 528 362", "M598 352 C 584 326 560 316 538 318", "M598 352 C 612 324 638 316 660 322",
          "M598 352 C 628 344 652 352 668 370", "M598 352 C 600 330 596 312 586 300"]

def town(ink, palm, lit=None):
    out = "".join(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{ink}"/>' for x, y, w, h in TOWN)
    out += f'<circle cx="1071" cy="288" r="10" fill="{ink}"/><path d="M760 434 a14 14 0 0 1 28 0 Z M1236 416 a16 16 0 0 1 32 0 Z" fill="{ink}"/>'
    out += f'<path d="M583 478 C 578 430 584 392 598 352" fill="none" stroke="{palm}" stroke-width="7" stroke-linecap="round"/>'
    out += "".join(f'<path d="{d}" fill="none" stroke="{palm}" stroke-width="5" stroke-linecap="round"/>' for d in FRONDS)
    if lit: out += "".join(f'<rect x="{x}" y="{y}" width="6" height="9" fill="{lit}" opacity="0.85"/>' for x, y in WINDOWS)
    return out

def scene(dusk):
    # The horizon runs just under the two names. The town stands to the right of the Applications
    # link, its tower clear of both icons; the palm stands between them; the sun is behind the app.
    lift = 70
    place = f'transform="translate(52 {lift}) scale(0.5)"'
    wide = f'transform="translate(-30 {lift}) scale(0.5)"'
    if dusk:
        # Night at the top, and the brightest band of the sunset where the names are (y 236 to 258).
        sky = [("#0a1214", 0), ("#121c1b", 0.24), ("#2a2a20", 0.37), ("#57452b", 0.45), ("#95683a", 0.51), ("#dba057", 0.57), ("#e8b06a", 0.63), ("#c98a48", 0.72), ("#2a2116", 1)]
        sun, glow, band, back, front, ink, palm, lit, ridge = "#f1c47d", "#f6cf86", ("#f6cf86", 0.32), ("#3a2f1f", 0.78), ("#231c13", 1), "#15120c", "#120f0a", "#e9b45f", "#0a0907"
    else:
        sky = [("#aebbbb", 0), ("#d3d1bf", 0.38), ("#ecdcb9", 0.62), ("#d8be8e", 1)]
        sun, glow, band, back, front, ink, palm, lit, ridge = "#fff1cf", "#fff4d8", ("#fff4d8", 0.5), ("#c9a878", 0.7), ("#bc9d6d", 1), "#86704e", "#6e5a3c", None, "#a98e64"
    stops = "".join(f'<stop offset="{at}" stop-color="{c}"/>' for c, at in sky)
    out = (f'<defs><linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">{stops}</linearGradient>'
           f'<radialGradient id="glow"><stop offset="0" stop-color="{glow}" stop-opacity="{0.8 if dusk else 0.75}"/><stop offset="1" stop-color="{glow}" stop-opacity="0"/></radialGradient>'
           f'<radialGradient id="core"><stop offset="0" stop-color="{sun}" stop-opacity="{0.55 if dusk else 0.8}"/><stop offset="0.55" stop-color="{sun}" stop-opacity="{0.22 if dusk else 0.4}"/><stop offset="1" stop-color="{sun}" stop-opacity="0"/></radialGradient>'
           f'<radialGradient id="edge" cx="0.5" cy="0.4" r="0.8"><stop offset="0.62" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="{0.4 if dusk else 0.1}"/></radialGradient>'
           f'<filter id="soft" x="-20%" y="-80%" width="140%" height="260%"><feGaussianBlur stdDeviation="10"/></filter></defs>'
           f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    if dusk:
        rng = random.Random(1942)
        for _ in range(46):
            x, y, r = rng.uniform(6, W - 6), rng.uniform(6, 140), rng.choice((0.5, 0.6, 0.6, 0.8, 1.0))
            out += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{CREAM}" opacity="{rng.uniform(0.18, 0.6) * (1 - y / 180):.2f}"/>'
    out += f'<circle cx="{APP}" cy="{ROW + 4}" r="168" fill="url(#glow)"/><circle cx="{APP}" cy="{ROW + 4}" r="118" fill="url(#core)"/>'
    out += f'<ellipse cx="330" cy="254" rx="440" ry="22" fill="{band[0]}" opacity="{band[1]}" filter="url(#soft)"/>'
    out += f'<g {wide}><path d="{DUNE_BACK}" fill="{back[0]}" opacity="{back[1]}"/></g>'
    out += f'<g {place}>{town(ink, palm, lit)}</g>'
    out += f'<g {wide}><path d="{DUNE_FRONT}" fill="{front[0]}" opacity="{front[1]}"/><path d="{RIDGE}" fill="{ridge}"/></g>'
    out += f'<rect y="{lift + 309}" width="{W}" height="{H - lift - 309}" fill="{ridge}"/><rect width="{W}" height="{H}" fill="url(#edge)"/>'
    if dusk:
        out += chevrons(330, ROW, GOLD, GOLD_D) + wordmark(330, 20, 21)
    else:
        out += chevrons(330, ROW, "#5c6537", "#323819") + wordmark(330, 20, 21, ink=True)
    return out

design, target = sys.argv[1], pathlib.Path(sys.argv[2])
target.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">{scene(design == "dusk")}</svg>\n')
