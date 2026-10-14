"""Bake deterministic, non-overlapping decorative rock quads; no Unreal required.

The eight atlas silhouettes are selected per instance. All placement checks run
offline: exact rotated quad bounds, circle clearances, expanded route corridors,
western prop exclusion, grass-core rejection, and separating-axis overlap tests.
Counts scale with area; rock sizes remain fixed. --extent-scale 2 builds a 4x-area layout.
Only the resulting layout JSON is written; an existing output is backed up.
"""
from pathlib import Path
from datetime import datetime
import argparse
import collections
import hashlib
import json
import math
import random
import shutil

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--extent-scale', type=float, default=1.0)
parser.add_argument('--output-root', default='Art/LayeredGround')
args = parser.parse_args()
assert args.extent_scale >= 1.0
CONFIG = ROOT / 'Tools/layered_ground.json'
cfg = json.loads(CONFIG.read_text())
terrain = json.loads((ROOT / cfg['routes']).read_text())
atlas_regions = json.loads((ROOT / cfg['atlas_regions']).read_text())
variants = {v['variant']: v for v in atlas_regions['variants']}
# Extent changes affect placement bounds and broad composition, never rock lengths,
# texture tile sizes, or gaps. The same per-square-meter density is retained.
scale = args.extent_scale
for key in ('world_min', 'world_max'):
    cfg[key] = [v * scale for v in cfg[key]]
area_ratio = math.prod(b-a for a,b in zip(cfg['world_min'],cfg['world_max'])) / cfg['reference_area_cm2']
for key in ('cluster_count', 'target_count'):
    cfg[key] = round(cfg[key] * area_ratio)
cfg['count_range'] = [round(v*area_ratio) for v in cfg['count_range']]
terrain['world_min'] = [v*scale for v in terrain['world_min']]
terrain['world_size'] = [v*scale for v in terrain['world_size']]
for route in terrain['paths']:
    route['points'] = [[v*scale for v in p] for p in route['points']]
for item in cfg['western_props']:
    for key in ('world_min','world_max'):
        item[key] = [v*scale for v in item[key]]
cfg['start']['center'] = [v*scale for v in cfg['start']['center']]
cfg['objectives']['centers'] = [[v*scale for v in p] for p in cfg['objectives']['centers']]
cfg['preferred_start_clusters'] = [[v*scale for v in p] for p in cfg['preferred_start_clusters']]
cfg['output'] = str(Path(args.output_root) / 'RockScatterLayout.json')

rng = random.Random(cfg['seed'])


from ground_image_data import read_png


mask_width, mask_height, mask_channels, mask_pixels = read_png(ROOT / cfg['mask'])


def weights(x, y):
    u = (x - terrain['world_min'][0]) / terrain['world_size'][0]
    v = (y - terrain['world_min'][1]) / terrain['world_size'][1]
    i = min(mask_width - 1, max(0, int(u * mask_width)))
    j = min(mask_height - 1, max(0, int(v * mask_height)))
    offset = (j * mask_width + i) * mask_channels
    return tuple(mask_pixels[offset + k] / 255 for k in range(3))


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1]


def rectangle(x, y, yaw, width, height):
    angle = math.radians(yaw)
    ux, uy = (math.cos(angle), math.sin(angle)), (-math.sin(angle), math.cos(angle))
    hx, hy = width * .5, height * .5
    corners = [(x + ux[0] * a + uy[0] * b, y + ux[1] * a + uy[1] * b)
               for a, b in [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)]]
    return dict(x=x, y=y, ux=ux, uy=uy, hx=hx, hy=hy, corners=corners,
                radius=math.hypot(hx, hy), minimum=(min(p[0] for p in corners), min(p[1] for p in corners)),
                maximum=(max(p[0] for p in corners), max(p[1] for p in corners)))


def overlaps(a, b, padding=0):
    delta = (b['x'] - a['x'], b['y'] - a['y'])
    if dot(delta, delta) > (a['radius'] + b['radius'] + padding) ** 2:
        return False
    for axis in (a['ux'], a['uy'], b['ux'], b['uy']):
        reach = sum(item['hx'] * abs(dot(item['ux'], axis)) + item['hy'] * abs(dot(item['uy'], axis)) for item in (a, b))
        if abs(dot(delta, axis)) > reach + padding:
            return False
    return True


def circle_overlap(rect, center, radius):
    offset = (center[0] - rect['x'], center[1] - rect['y'])
    dx = max(0., abs(dot(offset, rect['ux'])) - rect['hx'])
    dy = max(0., abs(dot(offset, rect['uy'])) - rect['hy'])
    return dx * dx + dy * dy <= radius * radius


def segment_hits_expanded_quad(rect, a, b, padding):
    # A square expansion is conservative around quad corners; it can never
    # admit a slab into the requested circular corridor around the spline.
    t0, t1 = 0., 1.
    for axis, half in ((rect['ux'], rect['hx']), (rect['uy'], rect['hy'])):
        start = dot((a[0] - rect['x'], a[1] - rect['y']), axis)
        end = dot((b[0] - rect['x'], b[1] - rect['y']), axis)
        delta, extent = end - start, half + padding
        if abs(delta) < 1.e-9:
            if abs(start) > extent:
                return False
            continue
        near, far = sorted(((-extent - start) / delta, (extent - start) / delta))
        t0, t1 = max(t0, near), min(t1, far)
        if t0 > t1:
            return False
    return True


def spline(points):
    result = []
    for k in range(len(points) - 1):
        p0, p1, p2, p3 = points[max(0, k - 1)], points[k], points[k + 1], points[min(len(points) - 1, k + 2)]
        for j in range(count := max(3, int(math.dist(p1, p2) / 110))):
            t = j / count
            result.append(tuple(.5 * (2 * p1[d] + (-p0[d] + p2[d]) * t
                + (2 * p0[d] - 5 * p1[d] + 4 * p2[d] - p3[d]) * t * t
                + (-p0[d] + 3 * p1[d] - 3 * p2[d] + p3[d]) * t * t * t) for d in range(2)))
    return result + [points[-1]]


segments = []
for route in terrain['paths']:
    points, clearance = spline(route['points']), cfg['route_clearance'][route['name']]
    for a, b in zip(points, points[1:]):
        segments.append((a, b, clearance, (min(a[0], b[0]) - clearance, min(a[1], b[1]) - clearance),
                         (max(a[0], b[0]) + clearance, max(a[1], b[1]) + clearance)))
western_quads = [rectangle(*[(a + b) * .5 for a, b in zip(prop['world_min'], prop['world_max'])],
                          0, *[b - a for a, b in zip(prop['world_min'], prop['world_max'])])
                 for prop in cfg['western_props']]
exclusions = [(cfg['start']['center'], cfg['start']['radius'])]
exclusions += [(point, cfg['objectives']['radius']) for point in cfg['objectives']['centers']]
rejections = collections.Counter()
placements, quads, clusters = [], [], []
quad_cells = collections.defaultdict(list)


def cells(rect, padding=0):
    for y in range(math.floor((rect['minimum'][1] - padding) / 1000), math.floor((rect['maximum'][1] + padding) / 1000) + 1):
        for x in range(math.floor((rect['minimum'][0] - padding) / 1000), math.floor((rect['maximum'][0] + padding) / 1000) + 1):
            yield x, y


def invalid_reason(rect, include_existing=True):
    margin = cfg['boundary_margin']
    if any(rect['minimum'][i] < cfg['world_min'][i] + margin or rect['maximum'][i] > cfg['world_max'][i] - margin for i in range(2)):
        return 'arena_boundary'
    if any(overlaps(rect, prop) for prop in western_quads):
        return 'western_props'
    if any(circle_overlap(rect, center, radius) for center, radius in exclusions):
        return 'start_or_objective'
    if include_existing:
        nearby = {index for cell in cells(rect, cfg['quad_gap']) for index in quad_cells[cell]}
        if any(overlaps(rect, quads[index], cfg['quad_gap']) for index in nearby):
            return 'other_rock'
    if weights(rect['x'], rect['y'])[0] > cfg['grass_center_limit']:
        return 'grass_center'
    for x, y in rect['corners']:
        if weights(rect['x'] + (x - rect['x']) * .6, rect['y'] + (y - rect['y']) * .6)[0] > cfg['grass_core_limit']:
            return 'grass_core'
    for a, b, clearance, minimum, maximum in segments:
        if any(rect['minimum'][i] > maximum[i] or rect['maximum'][i] < minimum[i] for i in range(2)):
            continue
        if segment_hits_expanded_quad(rect, a, b, clearance):
            return 'route_center'
    return None


def place(cluster, kind, x, y):
    # Select the silhouette first so quad dimensions preserve its trimmed aspect.
    neighbors = sorted(placements, key=lambda p: (p['x'] - x) ** 2 + (p['y'] - y) ** 2)[:2]
    pool = cfg['hero_variants'] if kind == 'hero' else list(variants)
    choices = [v for v in pool if v not in [p['variant'] for p in neighbors]]
    if not choices:
        choices = [v for v in pool if not neighbors or v != neighbors[0]['variant']] or pool
    variant = rng.choice(choices)
    low, high = cfg['hero_length'] if kind == 'hero' else cfg['companion_length']
    # A triangular distribution produces deliberate size hierarchy, not equal tiles.
    width = round(rng.triangular(low, high, low + (high - low) * .42), 2)
    native_aspect = variants[variant]['trimmedHeight'] / variants[variant]['trimmedWidth']
    height = round(width * native_aspect * rng.uniform(*cfg['aspect_jitter']), 2)
    yaw = round((cluster['yaw'] + rng.uniform(-cfg['cluster_yaw_variation'], cfg['cluster_yaw_variation'])) % 360, 2)
    x, y = round(x, 2), round(y, 2)
    quad = rectangle(x, y, yaw, width, height)
    reason = invalid_reason(quad)
    if reason:
        rejections[reason] += 1
        return False
    placements.append(dict(id=len(placements), cluster=cluster['id'], kind=kind, variant=variant,
                           x=x, y=y, z=cfg['z'], yaw=yaw, width=width, height=height))
    quads.append(quad)
    for cell in cells(quad):
        quad_cells[cell].append(len(quads) - 1)
    cluster['count'] += 1
    return True


# First reserve broad slabs across distinct areas, including the first gameplay view.
for index in range(cfg['cluster_count']):
    cluster = dict(id=index, x=0, y=0, yaw=rng.uniform(0, 360), radius=rng.uniform(*cfg['cluster_radius']), count=0)
    for attempt in range(5000):
        if index < len(cfg['preferred_start_clusters']) and attempt < 300:
            anchor = cfg['preferred_start_clusters'][index]
            x, y = anchor[0] + rng.uniform(-240, 240), anchor[1] + rng.uniform(-240, 240)
        else:
            x, y = (rng.uniform(cfg['world_min'][i] + 450, cfg['world_max'][i] - 450) for i in range(2))
        if any(math.hypot(x - c['x'], y - c['y']) < cfg['cluster_min_separation'] for c in clusters):
            continue
        grass, dirt, stone = weights(x, y)
        if grass > cfg['grass_center_limit'] or rng.random() > .35 + .65 * min(1., stone + dirt):
            continue
        if place(cluster, 'hero', x, y):
            cluster['x'], cluster['y'] = placements[-1]['x'], placements[-1]['y']
            clusters.append(cluster)
            break
    else:
        raise RuntimeError('Cannot fit the requested number of distinct clusters: %d' % index)

# Reserve a second hero where the clearing fits it before adding smaller pieces.
for cluster in rng.sample(clusters, len(clusters)):
    for attempt in range(900):
        angle, distance = rng.uniform(0, math.tau), rng.uniform(850, cluster['radius'])
        if place(cluster, 'hero', cluster['x'] + math.cos(angle) * distance, cluster['y'] + math.sin(angle) * distance):
            break

# Add clustered companions in rounds rather than filling one cluster first.
for pass_index in range(12):
    order = list(clusters)
    rng.shuffle(order)
    for cluster in order:
        if len(placements) >= cfg['target_count']:
            break
        if cluster['count'] >= cfg['cluster_max_count']:
            continue
        if sum(p['cluster'] == cluster['id'] and p['kind'] == 'companion' for p in placements) >= cfg['cluster_max_companions']:
            continue
        for attempt in range(600):
            angle = rng.uniform(0, math.tau)
            distance = rng.triangular(550, cluster['radius'], cluster['radius'] * .65)
            if place(cluster, 'companion', cluster['x'] + math.cos(angle) * distance, cluster['y'] + math.sin(angle) * distance):
                break
    if len(placements) >= cfg['target_count']:
        break
assert cfg['count_range'][0] <= len(placements) <= cfg['count_range'][1], 'Insufficient valid space: %d stamps, %.2f%% quad coverage' % (len(placements), 100 * sum(p['width'] * p['height'] for p in placements) / math.prod(b - a for a, b in zip(cfg['world_min'], cfg['world_max'])))

# Validate serialized transforms, not intermediate unrounded candidates.
for index, rock in enumerate(placements):
    quad = rectangle(rock['x'], rock['y'], rock['yaw'], rock['width'], rock['height'])
    assert invalid_reason(quad, include_existing=False) is None, rock
    assert all(not overlaps(quad, other, cfg['quad_gap']) for other in quads[:index]), rock
assert len(clusters) == cfg['cluster_count'] and all(1 <= cluster['count'] <= cfg['cluster_max_count'] for cluster in clusters)
assert all(sum(p['cluster'] == c['id'] and p['kind'] == 'companion' for p in placements) <= cfg['cluster_max_companions'] for c in clusters)
area = math.prod(b - a for a, b in zip(cfg['world_min'], cfg['world_max']))
quad_area = sum(rock['width'] * rock['height'] for rock in placements)
assert cfg['quad_coverage_range'][0] <= quad_area / area <= cfg['quad_coverage_range'][1], 'Quad coverage outside the authored target'
report = dict(version=cfg['version'], seed=cfg['seed'], map='/Game/Maps/Lvl_B1_Lvl1',
              units='centimeters', extent_scale=scale, effective_config=cfg, terrain_world_min=terrain['world_min'], terrain_world_size=terrain['world_size'], atlas_variants=len(variants), floor_z=1., stamp_z=cfg['z'],
              mask_sha256=hashlib.sha256((ROOT / cfg['mask']).read_bytes()).hexdigest(),
              atlas_regions_sha256=hashlib.sha256((ROOT / cfg['atlas_regions']).read_bytes()).hexdigest(),
              config_sha256=hashlib.sha256(CONFIG.read_bytes()).hexdigest(),
              counts=dict(total=len(placements), clusters=len(clusters),
                          kinds=dict(collections.Counter(p['kind'] for p in placements)),
                          variants=dict(collections.Counter(p['variant'] for p in placements))),
              coverage=dict(quad_area_cm2=round(quad_area, 2), arena_area_cm2=round(area, 2),
                            quad_fraction=quad_area / area,
                            estimated_opaque_fraction=sum(p['width'] * p['height'] * variants[p['variant']]['opaqueFraction'] for p in placements) / area,
                            target_range=cfg['quad_coverage_range'],
                            note='Conservative non-overlapping quad coverage; actual atlas rock coverage is lower.'),
              validation=dict(passed=True, full_rotated_bounds=True, boundary_margin_cm=cfg['boundary_margin'],
                              player_and_objectives_clear=True, western_props_clear=True,
                              route_centers_clear=True, grass_cores_avoided=True,
                              rectangle_sat_non_overlap=True, minimum_quad_gap_cm=cfg['quad_gap']),
              rejected_candidates=dict(rejections), clusters=clusters, placements=placements)
destination = ROOT / cfg['output']
if destination.exists():
    backup = ROOT / 'Saved/LayeredGround/Backups' / (datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '_RockScatter')
    backup.mkdir(parents=True, exist_ok=True)
    shutil.copy2(destination, backup / destination.name)
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: report[key] for key in ('seed', 'counts', 'coverage', 'validation')}))
