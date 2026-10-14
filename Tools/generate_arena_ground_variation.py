"""Bake numerical ground-blend masks from the existing slab placement data.

No source artwork or placement is changed. R is soft, irregular grass around
rock footprints; G/B are world-space broad/fine variation. Run with Python.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
SIZE = 1024
ORIGIN = -7600.0
SPAN = 15200.0
layout_path = ROOT / "Art/ReferenceGround/RockScatterLayout.json"
layout = json.loads(layout_path.read_text())

def hash2(x, y):
    value = ((x * 374761393 + y * 668265263 + 20261014) & 0xffffffff)
    value = ((value ^ (value >> 13)) * 1274126177) & 0xffffffff
    return (value ^ (value >> 16)) / 4294967295.0

def noise(x, y, scale):
    x, y = x / scale, y / scale
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = hash2(ix, iy), hash2(ix + 1, iy)
    c, d = hash2(ix, iy + 1), hash2(ix + 1, iy + 1)
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy

def smooth(a, b, value):
    v = min(1, max(0, (value - a) / (b - a)))
    return v * v * (3 - 2 * v)

pixels = bytearray(SIZE * SIZE * 4)
for j in range(SIZE):
    y = ORIGIN + (j + .5) * SPAN / SIZE
    for i in range(SIZE):
        x = ORIGIN + (i + .5) * SPAN / SIZE
        k = (j * SIZE + i) * 4
        pixels[k + 1] = round(255 * noise(x, y, 1000))
        pixels[k + 2] = round(255 * (.7 * noise(x, y, 260) + .3 * noise(x, y, 95)))
        pixels[k + 3] = 255

for p in layout['placements']:
    angle = math.radians(p['yaw'])
    co, si = math.cos(angle), math.sin(angle)
    hx, hy = p['width'] * .54, p['height'] * .54
    radius = math.hypot(hx, hy) * 1.4
    bounds = [max(0, int((p[v] - radius - ORIGIN) / SPAN * SIZE)) for v in ('x','y')]
    ends = [min(SIZE, int((p[v] + radius - ORIGIN) / SPAN * SIZE) + 1) for v in ('x','y')]
    for j in range(bounds[1], ends[1]):
        dy = ORIGIN + (j + .5) * SPAN / SIZE - p['y']
        for i in range(bounds[0], ends[0]):
            dx = ORIGIN + (i + .5) * SPAN / SIZE - p['x']
            k = (j * SIZE + i) * 4
            radius = math.hypot((dx * co + dy * si) / hx, (-dx * si + dy * co) / hy)
            edge = (pixels[k + 2] / 255 - .5) * .45
            weight = 1 - smooth(.73, 1.21, radius + edge)
            # Vary edge strength so individual slabs do not have identical halos.
            strength = .82 + .18 * pixels[k + 1] / 255
            pixels[k] = max(pixels[k], round(weight * strength * 255))

path = ROOT / 'Art/EnvironmentPolish/Lvl1_GroundVariation.png'
path.parent.mkdir(parents=True, exist_ok=True)
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
raw = b''.join(b'\x00' + pixels[y*SIZE*4:(y+1)*SIZE*4] for y in range(SIZE))
path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE,SIZE,8,6,0,0,0)) + chunk(b'IDAT', zlib.compress(raw,9)) + chunk(b'IEND', b''))
metadata = dict(size=SIZE, world_origin=[ORIGIN,ORIGIN], world_size=[SPAN,SPAN],
                placements=len(layout['placements']), channels=dict(R='rock-edge grass', G='broad variation', B='blend-edge variation'),
                layout_sha256=hashlib.sha256(layout_path.read_bytes()).hexdigest(),
                texture_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
path.with_suffix('.json').write_text(json.dumps(metadata,indent=2))
print(json.dumps(metadata))
