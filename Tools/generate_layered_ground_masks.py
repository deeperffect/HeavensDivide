"""Bake silhouette-based grass fringes/contact shade at a fixed cm-per-texel.

This creates numerical data, not replacement artwork. --layout-root selects a
larger generated layout; mask resolution grows with the extent, never rock size.
"""
from array import array
from pathlib import Path
import argparse
import hashlib
import json
import math
from ground_image_data import read_png, write_rgba

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--layout-root', default='Art/LayeredGround')
args = parser.parse_args()
folder = ROOT / args.layout_root
layout_path = folder / 'RockScatterLayout.json'
layout = json.loads(layout_path.read_text())
cfg = layout['effective_config']
regions = json.loads((ROOT/cfg['atlas_regions']).read_text())
images = [read_png(ROOT/a['source']) for a in regions['atlases']]
origin, span = layout['terrain_world_min'], layout['terrain_world_size']
size = 2**math.ceil(math.log2(max(span)/cfg['mask_world_texel_cm']))
assert size <= 8192, 'Split very large worlds into mask tiles instead of reducing detail density'
step = span[0]/size
assert span[0] == span[1]
inside = bytearray(size*size)
for p in layout['placements']:
    r = regions['variants'][p['variant']]
    w,h,channels,data = images[r['atlas']]
    x0,y0,x1,y1 = r['paddedBoundsPx']
    co,si = math.cos(math.radians(p['yaw'])),math.sin(math.radians(p['yaw']))
    reach = math.hypot(p['width'],p['height'])/2
    for j in range(max(0,int((p['y']-reach-origin[1])/step)),min(size,int((p['y']+reach-origin[1])/step)+1)):
        dy = origin[1]+(j+.5)*step-p['y']
        for i in range(max(0,int((p['x']-reach-origin[0])/step)),min(size,int((p['x']+reach-origin[0])/step)+1)):
            dx = origin[0]+(i+.5)*step-p['x']
            lx,ly = (dx*co+dy*si)/p['width'],(-dx*si+dy*co)/p['height']
            # Engine BasicShapes/Plane UV0 is verified against PlaneUV.json.
            u,v = lx+.5,ly+.5
            if 0 <= u < 1 and 0 <= v < 1:
                ax,ay = int(x0+u*(x1-x0)),int(y0+v*(y1-y0))
                if data[(ay*w+ax)*channels+3] >= 128:
                    inside[j*size+i] = 1

# Chamfer distance in world centimeters follows the alpha silhouette, including
# notches and open seams. It is independent of the enclosing rectangular quad.
dist = array('f',(0 if solid else 1.e6 for solid in inside))
diag = step*math.sqrt(2)
for y in range(size):
    for x in range(size):
        k = y*size+x
        d = dist[k]
        if x: d=min(d,dist[k-1]+step)
        if y:
            d=min(d,dist[k-size]+step)
            if x: d=min(d,dist[k-size-1]+diag)
            if x+1<size: d=min(d,dist[k-size+1]+diag)
        dist[k]=d
for y in range(size-1,-1,-1):
    for x in range(size-1,-1,-1):
        k=y*size+x
        d=dist[k]
        if x+1<size: d=min(d,dist[k+1]+step)
        if y+1<size:
            d=min(d,dist[k+size]+step)
            if x: d=min(d,dist[k+size-1]+diag)
            if x+1<size: d=min(d,dist[k+size+1]+diag)
        dist[k]=d

def hash2(x,y):
    n=(x*374761393+y*668265263+20261014)&0xffffffff
    n=((n^(n>>13))*1274126177)&0xffffffff
    return (n^(n>>16))/4294967295

def noise(x,y,scale):
    x,y=x/scale,y/scale
    ix,iy=math.floor(x),math.floor(y)
    fx,fy=x-ix,y-iy
    fx,fy=fx*fx*(3-2*fx),fy*fy*(3-2*fy)
    a,b,c,d=hash2(ix,iy),hash2(ix+1,iy),hash2(ix,iy+1),hash2(ix+1,iy+1)
    return (a+(b-a)*fx)*(1-fy)+(c+(d-c)*fx)*fy

pixels=bytearray(size*size*4)
fringe_count=shadow_count=0
for j in range(size):
    y=origin[1]+(j+.5)*step
    for i in range(size):
        x=origin[0]+(i+.5)*step
        k=j*size+i
        broad=noise(x,y,1300)
        broken=noise(x,y,140)
        fine=noise(x,y,42)
        reach=cfg['grass_fringe_cm']*(.45+broken*.85)
        fringe=max(0,1-dist[k]/reach)**.7 * (.45+.55*broken)
        shadow=max(0,1-dist[k]/cfg['contact_shadow_cm'])**1.3
        pixels[k*4:k*4+4]=bytes([round(255*fringe),round(255*shadow),round(255*broad),round(255*(.65*broken+.35*fine))])
        fringe_count += not inside[k] and fringe>.05
        shadow_count += not inside[k] and shadow>.05
path=folder/'GroundBlend.png'
write_rgba(path,size,size,pixels)
report={'passed': True,'size':size,'world_origin':origin,'world_size':span,'cm_per_texel':step,
        'grass_fringe_cm':cfg['grass_fringe_cm'],'contact_shadow_cm':cfg['contact_shadow_cm'],
        'exterior_grass_pixels':fringe_count,'exterior_shadow_pixels':shadow_count,
        'layout_sha256':hashlib.sha256(layout_path.read_bytes()).hexdigest(),
        'atlas_regions_sha256':hashlib.sha256((ROOT/cfg['atlas_regions']).read_bytes()).hexdigest(),
        'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'channels':{'R':'silhouette grass fringe','G':'contact shade','B':'broad world variation','A':'broken transition noise'}}
assert fringe_count and shadow_count
path.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
