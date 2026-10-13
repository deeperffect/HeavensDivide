"""Bake authored world-space ground weights (R grass, G dirt, B stone).

Uses only Python's standard library. Solid stone interiors, exposed earth
shoulders, and clustered grass replace noisy translucent texture overlaps.
All irregular edge breakup is baked offline. Source painted textures stay intact.
"""
from pathlib import Path
import array
import json
import math
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
cfg = json.loads((ROOT / 'Tools/level1_ground_layout.json').read_text())
N = cfg['resolution']
ox, oy = cfg['world_min']
sx, sy = cfg['world_size']
step_x, step_y = sx / N, sy / N
blend = cfg['blend']
stone_edge = blend['stone_edge_width']
grass_edge = blend['grass_edge_width']
shoulder = blend['dirt_shoulder']
large_breakup = blend['large_edge_breakup']
fine_breakup = blend['fine_edge_breakup']
Wear = array.array('f', [0]) * (N * N)
Stone = array.array('f', [0]) * (N * N)
Islands = array.array('f', [0]) * (N * N)
Edge = array.array('f', [float('nan')]) * (N * N)


def smooth(a, b, value):
    t = max(0., min(1., (value - a) / (b - a)))
    return t * t * (3 - 2 * t)


def noise(x, y):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

    def hashed(a, b):
        value = math.sin(a * 127.1 + b * 311.7) * 43758.5453
        return value - math.floor(value)

    return ((hashed(ix, iy) * (1 - fx) + hashed(ix + 1, iy) * fx) * (1 - fy)
            + (hashed(ix, iy + 1) * (1 - fx) + hashed(ix + 1, iy + 1) * fx) * fy)


def bounds(x0, y0, x1, y1):
    return (range(max(0, math.floor((x0 - ox) / step_x)), min(N, math.ceil((x1 - ox) / step_x) + 1)),
            range(max(0, math.floor((y0 - oy) / step_y)), min(N, math.ceil((y1 - oy) / step_y) + 1)))


def edge_breakup(x, y, idx):
    # Overlapping spline strokes revisit the same pixels. Cache offline noise
    # once per pixel rather than recomputing it for every route segment.
    if Edge[idx] != Edge[idx]:
        Edge[idx] = ((noise(x / 310 + 9, y / 310 - 4) - .5) * large_breakup
                     + (noise(x / 63 + 7, y / 63 + 3) - .5) * fine_breakup)
    return Edge[idx]


def spline(points):
    result = []
    for k in range(len(points) - 1):
        p0, p1, p2, p3 = points[max(0, k - 1)], points[k], points[k + 1], points[min(len(points) - 1, k + 2)]
        count = max(3, int(math.dist(p1, p2) / 110))
        for j in range(count):
            t = j / count
            result.append(tuple(.5 * ((2 * p1[d]) + (-p0[d] + p2[d]) * t
                + (2 * p0[d] - 5 * p1[d] + 4 * p2[d] - p3[d]) * t * t
                + (-p0[d] + 3 * p1[d] - 3 * p2[d] + p3[d]) * t * t * t) for d in range(2)))
    return result + [points[-1]]


def stroke(a, b, width, stone):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    length_squared = dx * dx + dy * dy
    if length_squared <= .0001:
        return
    radius = width * .5
    extent = radius + shoulder + grass_edge + large_breakup + fine_breakup
    xs, ys = bounds(min(ax, bx) - extent, min(ay, by) - extent,
                    max(ax, bx) + extent, max(ay, by) + extent)
    for j in ys:
        y = oy + (j + .5) * step_y
        for i in xs:
            x = ox + (i + .5) * step_x
            t = max(0., min(1., ((x - ax) * dx + (y - ay) * dy) / length_squared))
            distance = math.hypot(x - ax - t * dx, y - ay - t * dy)
            idx = j * N + i
            # Union full interiors. Spline intersections never wash out slabs.
            if distance < radius - stone_edge - (large_breakup + fine_breakup) * .5:
                Stone[idx] = max(Stone[idx], stone)
                Wear[idx] = 1.
                continue
            if Wear[idx] >= 1. and Stone[idx] >= stone:
                continue
            distance += edge_breakup(x, y, idx)
            Stone[idx] = max(Stone[idx], stone * (1 - smooth(radius - stone_edge, radius + stone_edge, distance)))
            Wear[idx] = max(Wear[idx], 1 - smooth(radius + shoulder - grass_edge, radius + shoulder + grass_edge, distance))


def ellipse(item, grass=False):
    cx, cy = item['center']
    rx, ry = item['radius']
    minimum_radius = min(rx, ry)
    outer = 1 + (shoulder + grass_edge + large_breakup + fine_breakup) / minimum_radius
    xs, ys = bounds(cx - rx * outer, cy - ry * outer, cx + rx * outer, cy + ry * outer)
    for j in ys:
        y = oy + (j + .5) * step_y
        for i in xs:
            x = ox + (i + .5) * step_x
            distance = math.hypot((x - cx) / rx, (y - cy) / ry) * minimum_radius
            idx = j * N + i
            distance += edge_breakup(x, y, idx)
            if grass:
                coverage = 1 - smooth(minimum_radius - grass_edge, minimum_radius + grass_edge, distance)
                # Authored overlapping lobes and small edge notches keep these
                # five connected clumps from reading as perfect oval decals.
                for kind in ('lobes', 'indents'):
                    for shape in item.get(kind, []):
                        dx, dy = shape['offset']
                        shape_rx, shape_ry = shape['radius']
                        shape_radius = min(shape_rx, shape_ry)
                        shape_distance = math.hypot((x - cx - dx) / shape_rx, (y - cy - dy) / shape_ry) * shape_radius
                        shape_distance += edge_breakup(x, y, idx)
                        shape_weight = 1 - smooth(shape_radius - grass_edge, shape_radius + grass_edge, shape_distance)
                        coverage = max(coverage, shape_weight) if kind == 'lobes' else min(coverage, 1 - shape_weight)
                Islands[idx] = max(Islands[idx], coverage)
            else:
                Stone[idx] = max(Stone[idx], item.get('stone', 1.) * (1 - smooth(minimum_radius - stone_edge, minimum_radius + stone_edge, distance)))
                Wear[idx] = max(Wear[idx], 1 - smooth(minimum_radius + shoulder - grass_edge, minimum_radius + shoulder + grass_edge, distance))


for route in cfg['paths']:
    points = spline(route['points'])
    for a, b in zip(points, points[1:]):
        stroke(a, b, route['width'], route['stone'])
for item in cfg['clearings'] + cfg['rock_patches']:
    ellipse(item)
for item in cfg['grass_islands']:
    ellipse(item, grass=True)

raw, preview = bytearray(), bytearray()
totals, playable_totals = [0, 0, 0], [0, 0, 0]
playable_pixels = solid_stone_pixels = 0
px0, py0 = cfg['playable_bounds']['world_min']
px1, py1 = cfg['playable_bounds']['world_max']
colors = [(83, 104, 53), (148, 116, 75), (116, 124, 117)]
for j in range(N):
    raw.append(0)
    preview.append(0)
    y = oy + (j + .5) * step_y
    for i in range(N):
        idx = j * N + i
        x = ox + (i + .5) * step_x
        worn = min(1., Wear[idx]) * (1 - Islands[idx])
        stone = min(Stone[idx], Wear[idx]) * (1 - Islands[idx])
        # Dirt is the exact remainder, not another noisy texture overlay.
        weights = [round((1 - worn) * 255), 0, round(stone * 255)]
        weights[1] = 255 - weights[0] - weights[2]
        assert min(weights) >= 0 and sum(weights) == 255
        raw.extend(weights)
        for k, value in enumerate(weights):
            totals[k] += value
        if px0 <= x <= px1 and py0 <= y <= py1:
            playable_pixels += 1
            solid_stone_pixels += weights[2] >= 250
            for k, value in enumerate(weights):
                playable_totals[k] += value
        preview.extend(round(sum(weights[k] * colors[k][channel] / 255 for k in range(3))) for channel in range(3))


def png(filename, data):
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))
    filename.write_bytes(b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', N, N, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(data, 9)) + chunk(b'IEND', b''))


out = ROOT / 'Art/EnvironmentPolish'
out.mkdir(parents=True, exist_ok=True)
png(out / 'Lvl1_GroundWeights.png', raw)
png(out / 'Lvl1_LayoutPreview.png', preview)
report = {
    'version': cfg['version'], 'resolution': N, 'world_min': cfg['world_min'], 'world_size': cfg['world_size'],
    'weights': dict(zip(['grass', 'dirt', 'stone'], [value / (N * N * 255) for value in totals])),
    'playable_bounds': cfg['playable_bounds'],
    'playable_weights': dict(zip(['grass', 'dirt', 'stone'], [value / (playable_pixels * 255) for value in playable_totals])),
    'playable_solid_stone_area': solid_stone_pixels / playable_pixels,
    'routes': [route['name'] for route in cfg['paths']],
    'notes': 'Solid stone interiors; earth shoulders; clustered grass islands. All edge breakup is baked offline.'
}
(out / 'LayoutReport.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
