#!/usr/bin/env python3
"""Write the flat SVG layers of icon G into CoD2 Silicon.icon/Assets.

Geometry comes from the approved design board, whose rounded tile spans
100-924 of a 1024 canvas. Icon Composer masks the whole canvas to the macOS
shape, so every layer is scaled by 1024/824 about the tile origin. Layers are
flat colour: the system supplies specular highlights, translucency and shadow.
The worn-paint chips are seeded, so the output is reproducible."""
import math, random, sys, pathlib
out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent / "CoD2 Silicon.icon"
T = 'transform="matrix(1.242718 0 0 1.242718 -124.2718 -124.2718)"'
TWO = "M352 404 C 352 300 428 246 516 246 C 612 246 676 306 676 392 C 676 470 628 516 566 566 L 372 740 L 700 740"
# Two stencil bridges: the design's third, on the top-left hook, read as a separate tick at
# Dock sizes (32-64 px), so the 2 keeps its crown whole.
CUT = ('<mask id="cut" maskUnits="userSpaceOnUse" x="0" y="0" width="1024" height="1024"><rect width="1024" height="1024" fill="#fff"/>'
       '<rect x="452" y="612" width="34" height="110" fill="#000" transform="rotate(42 469 667)"/>'
       '<rect x="548" y="676" width="30" height="118" fill="#000"/></mask>')
def bez(p0, p1, p2, p3, n):
    for i in range(n + 1):
        t = i / n; u = 1 - t
        yield (u**3*p0[0] + 3*u*u*t*p1[0] + 3*u*t*t*p2[0] + t**3*p3[0], u**3*p0[1] + 3*u*u*t*p1[1] + 3*u*t*t*p2[1] + t**3*p3[1])
def line(a, b, n):
    for i in range(n + 1):
        t = i / n; yield (a[0] + (b[0]-a[0])*t, a[1] + (b[1]-a[1])*t)
centre = list(bez((352,404),(352,300),(428,246),(516,246),30)) + list(bez((516,246),(612,246),(676,306),(676,392),30)) + \
         list(bez((676,392),(676,470),(628,516),(566,566),30)) + list(line((566,566),(372,740),30)) + list(line((372,740),(700,740),30))
rng = random.Random(1944)
chips = []
for _ in range(34):
    i = rng.randrange(2, len(centre) - 2)
    (x0, y0), (x1, y1) = centre[i - 1], centre[i + 1]
    length = math.hypot(x1 - x0, y1 - y0) or 1
    nx, ny = -(y1 - y0) / length, (x1 - x0) / length
    offset = rng.uniform(-44, 44)
    cx, cy = centre[i][0] + nx * offset, centre[i][1] + ny * offset
    r = rng.uniform(3.5, 9.5); sides = rng.randrange(5, 8); phase = rng.uniform(0, math.tau)
    pts = []
    for k in range(sides):
        a = phase + k * math.tau / sides
        rr = r * rng.uniform(0.55, 1.15)
        pts.append(f"{cx + math.cos(a)*rr*1.35:.1f} {cy + math.sin(a)*rr:.1f}")
    chips.append("M" + " L".join(pts) + " Z")
def numeral(paint, chip):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">\n<defs>{CUT}'
            f'</defs>\n'
            f'<g {T}><g mask="url(#cut)"><path d="{TWO}" fill="none" stroke="{paint}" stroke-width="124" stroke-linejoin="miter"/>\n'
            + (f'<path d="{" ".join(chips)}" fill="{chip}"/>\n' if chip else '') + '</g></g>\n</svg>\n')
def star(lit, shade):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">\n<defs>'
            f'<linearGradient id="lit" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{lit[0]}"/><stop offset="1" stop-color="{lit[1]}"/></linearGradient>'
            f'<linearGradient id="shade" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{shade[0]}"/><stop offset="1" stop-color="{shade[1]}"/></linearGradient></defs>\n'
            f'<g {T}>\n<path d="M512 520 L431.2 408.7 L512 160 Z M512 520 L592.8 408.7 L854.4 408.8 Z M512 520 L642.8 562.5 L723.6 811.2 Z M512 520 L512 657.5 L300.4 811.2 Z M512 520 L381.2 562.5 L169.6 408.8 Z" fill="url(#lit)"/>\n'
            '<path d="M512 520 L512 160 L592.8 408.7 Z M512 520 L854.4 408.8 L642.8 562.5 Z M512 520 L723.6 811.2 L512 657.5 Z M512 520 L300.4 811.2 L381.2 562.5 Z M512 520 L169.6 408.8 L431.2 408.7 Z" fill="url(#shade)"/>\n</g>\n</svg>\n')
def plate(ring, rivet):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">\n'
            f'<g {T}>\n<rect x="170" y="170" width="684" height="684" rx="130" fill="none" stroke="{ring}" stroke-width="6"/>\n'
            + "".join(f'<circle cx="{x}" cy="{y}" r="16" fill="{rivet}"/>' for x, y in [(236,236),(788,236),(236,788),(788,788)]) + '\n</g>\n</svg>\n')
assets = out / "Assets"; assets.mkdir(parents=True, exist_ok=True)
(assets / "stencil-two.svg").write_text(numeral("#ecdfb4", "#c7b689"))
(assets / "stencil-two-dark.svg").write_text(numeral("#ddcf9f", "#a99a6c"))
(assets / "star.svg").write_text(star(("#d9c78f", "#9c8a4e"), ("#7d6d33", "#3f3716")))
(assets / "star-dark.svg").write_text(star(("#b9a76c", "#7b6b38"), ("#5c4f22", "#2a240c")))
(assets / "star-mono.svg").write_text(star(("#7a7a7a", "#585858"), ("#3a3a3a", "#242424")))
(assets / "plate.svg").write_text(plate("#848f57", "#b7ad86"))
(assets / "plate-dark.svg").write_text(plate("#3d4228", "#7a7358"))
