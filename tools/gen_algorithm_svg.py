#!/usr/bin/env python3
"""Generates resources/algorithms/algorithm{0..7}.svg and algorithm{0..7}_fb.svg,
the YM2151 algorithm diagrams shown in the editor; the _fb variant lights the
feedback loop in amber for feedback > 0. Operators are in voice order (1 = M1, 2 = C1,
3 = M2, 4 = C2). Edges and roles mirror src/dsp/AlgorithmInfo.h; only the
box positions live here. Run from the repository root after changing either.
"""
import os

CARRIER = "#7fb0f0"
MODULATOR = "#c76aa3"
LINE = "#5f7269"
INK = "#0b0f12"
MUTED = "#9fb3a8"
AMBER = "#ffb454"
W, H = 160, 100
BW, BH = 28, 14

# (carrierMask, edges, positions of operator centres 1-4)
ALGORITHMS = [
    (0x08, [(0, 1), (1, 2), (2, 3)], [(80, 17), (80, 37), (80, 57), (80, 77)]),
    (0x08, [(0, 2), (1, 2), (2, 3)], [(60, 17), (100, 17), (80, 46), (80, 75)]),
    (0x08, [(0, 3), (1, 2), (2, 3)], [(60, 46), (100, 17), (100, 46), (80, 75)]),
    (0x08, [(0, 1), (1, 3), (2, 3)], [(60, 17), (60, 46), (100, 46), (80, 75)]),
    (0x0A, [(0, 1), (2, 3)],         [(56, 43), (56, 75), (104, 43), (104, 75)]),
    (0x0E, [(0, 1), (0, 2), (0, 3)], [(80, 17), (44, 75), (80, 75), (116, 75)]),
    (0x0E, [(0, 1)],                 [(44, 43), (44, 75), (80, 75), (116, 75)]),
    (0x0F, [],                       [(24, 75), (58, 75), (92, 75), (126, 75)]),
]


ARROW = 5  # arrowhead length; JUCE's SVG reader has no marker support, so heads are explicit


def arrow_down(x, y):
    return f'<path d="M{x - 3},{y - ARROW} L{x + 3},{y - ARROW} L{x},{y} Z" fill="{LINE}"/>'


def arrow_right(x, y):
    return f'<path d="M{x - ARROW},{y - 3} L{x - ARROW},{y + 3} L{x},{y} Z" fill="{LINE}"/>'


def edge(src, dst):
    sx, sy = src
    tx, ty = dst
    bottom = sy + BH / 2
    top = ty - BH / 2
    if sx == tx:
        line = f"M{sx},{bottom} V{top - ARROW}"
    else:
        line = f"M{sx},{bottom} V{top - 8} H{tx} V{top - ARROW}"
    return f'<path d="{line}"/>', arrow_down(tx, top)


def svg(index, feedback_on):
    carrier_mask, edges, pos = ALGORITHMS[index]
    lines, heads = [], []
    fb_colour = AMBER if feedback_on else LINE
    for s, d in edges:
        line, head = edge(pos[s], pos[d])
        lines.append(line)
        heads.append(head)
    # feedback loop: up from operator 1, around its left side and back in, right-angled
    x, y = pos[0]
    left = x - BW / 2
    top = y - BH / 2
    lines.append(f'<path d="M{x - 4},{top} V{top - 8} H{left - 8} V{y} H{left - ARROW - 1}" stroke="{fb_colour}"/>')
    heads.append(arrow_right(left - 1, y).replace(f'fill="{LINE}"', f'fill="{fb_colour}"'))
    # carriers drop onto a shared output bus
    carriers = [i for i in range(4) if (carrier_mask >> i) & 1]
    bus_y = 93
    xs = [pos[i][0] for i in carriers]
    for i in carriers:
        lines.append(f'<path d="M{pos[i][0]},{pos[i][1] + BH / 2} V{bus_y}"/>')
    lines.append(f'<path d="M{min(xs)},{bus_y} H{max(xs) + 14 - ARROW}"/>')
    heads.append(arrow_right(max(xs) + 14, bus_y))
    
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">',
           f'<g fill="none" stroke="{LINE}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">']
    out += lines
    out.append('</g>')
    out += heads
    out.append(f'<text x="{max(xs) + 17}" y="{bus_y + 3}" font-family="Menlo, monospace" font-size="7" fill="{MUTED}">OUT</text>')
    for i, (x, y) in enumerate(pos):
        colour = CARRIER if (carrier_mask >> i) & 1 else MODULATOR
        out.append(f'<rect x="{x - BW / 2}" y="{y - BH / 2}" width="{BW}" height="{BH}" rx="3" fill="{colour}"/>')
        out.append(f'<text x="{x}" y="{y + 3.5}" text-anchor="middle" font-family="Menlo, monospace" '
                   f'font-size="9" font-weight="bold" fill="{INK}">{i + 1}</text>')
    out.append('</svg>')
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target = os.path.join(root, "resources", "algorithms")
    os.makedirs(target, exist_ok=True)
    for i in range(8):
        with open(os.path.join(target, f"algorithm{i}.svg"), "w") as f:
            f.write(svg(i, False))
        with open(os.path.join(target, f"algorithm{i}_fb.svg"), "w") as f:
            f.write(svg(i, True))
    print(f"wrote 16 diagrams to {target}")
