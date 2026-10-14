"""Measure eight atlas silhouettes without changing source pixels or alpha."""
from pathlib import Path
import hashlib
import json
from ground_image_data import read_png

ROOT = Path(__file__).resolve().parents[1]
atlases = ['Art/LayeredGround/RockStamps_BaseDark.png',
           'Art/LayeredGround/RockStamps_Additional.png']
result = {'version': 1, 'atlases': [], 'variants': []}
for atlas_index, relative in enumerate(atlases):
    path = ROOT / relative
    width, height, channels, pixels = read_png(path)
    assert channels == 4 and width % 2 == height % 2 == 0
    result['atlases'].append({'source': relative, 'width': width, 'height': height,
                             'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    # Locate the transparent gutter instead of assuming generation placed it
    # on the exact midpoint. The image and silhouettes are never cropped/repainted.
    xs = [x for x in range(int(width*.44),int(width*.57))
          if all(pixels[(y*width+x)*4+3] < 128 for y in range(height))]
    ys = [y for y in range(int(height*.44),int(height*.57))
          if all(pixels[(y*width+x)*4+3] < 128 for x in range(width))]
    assert xs and ys, 'Atlas needs clear gutters'
    split_x, split_y = xs[len(xs)//2], ys[len(ys)//2]
    for row in range(2):
        for col in range(2):
            left, right = (0,split_x) if col == 0 else (split_x,width)
            top, bottom = (0,split_y) if row == 0 else (split_y,height)
            opaque = [(x,y) for y in range(top,bottom) for x in range(left,right)
                      if pixels[(y*width+x)*4+3] >= 128]
            assert opaque, 'Empty atlas cell'
            x0,y0 = min(x for x,y in opaque), min(y for x,y in opaque)
            x1,y1 = max(x for x,y in opaque)+1, max(y for x,y in opaque)+1
            assert x0 > left and y0 > top and x1 < right and y1 < bottom, 'Rock crosses an atlas cell'
            x0,y0,x1,y1 = max(left,x0-2),max(top,y0-2),min(right,x1+2),min(bottom,y1+2)
            result['variants'].append({'variant': atlas_index*4+row*2+col, 'atlas': atlas_index,
                'paddedBoundsPx': [x0,y0,x1,y1], 'trimmedWidth': x1-x0, 'trimmedHeight': y1-y0,
                'normalizedUVRect': [x0/width,y0/height,(x1-x0)/width,(y1-y0)/height],
                'opaqueFraction': len(opaque)/((x1-x0)*(y1-y0))})
destination = ROOT / 'Art/LayeredGround/RockAtlasRegions.json'
destination.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'atlases': result['atlases'], 'variants': len(result['variants'])}))
