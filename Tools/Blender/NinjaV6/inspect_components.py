import bpy
import json
from pathlib import Path
from mathutils import Vector

root = Path(r"C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide")
out = root/'Saved/NinjaV6Replacement'
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(root/'Art/Characters/Ninja/NinjaV6_Textured/Bloodshift_NinjaV6_Textured.blend'))
obj = bpy.data.objects['NinjaV6_ReferenceTextured']
mesh = obj.data
parent = list(range(len(mesh.vertices)))
def find(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i
for edge in mesh.edges:
    a,b = map(find,edge.vertices)
    if a != b: parent[b] = a
groups = {}
for v in mesh.vertices:
    groups.setdefault(find(v.index), []).append(v.index)
groups = sorted(groups.values(), key=len, reverse=True)
report = []
for index, indices in enumerate(groups):
    points = [mesh.vertices[i].co for i in indices]
    report.append({'component': index, 'vertices': len(indices), 'vertex_indices': indices,
        'min': [min(p[i] for p in points) for i in range(3)],
        'max': [max(p[i] for p in points) for i in range(3)],
        'center': list(sum(points,Vector())/len(points))})
(out/'V6_Components.json').write_text(json.dumps(report,indent=2))
print('COMPONENTS',json.dumps([{k:v for k,v in c.items() if k!='vertex_indices'} for c in report]),flush=True)
