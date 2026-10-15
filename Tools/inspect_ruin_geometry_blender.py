"""Inspect exported pack pieces, their open boundaries, and assembly axes."""
from pathlib import Path
import bpy,bmesh,json,math
root=Path(__file__).resolve().parents[1];out=root/'Saved/SurvivalMaps/ObstaclePolish'
data={}
for path in sorted(out.glob('*.fbx')):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 bpy.ops.import_scene.fbx(filepath=str(path))
 objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];rows=[]
 for o in objects:
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001)
  open_edges=[e for e in bm.edges if e.is_boundary]
  coords=[tuple(o.matrix_world@v.co) for v in bm.verts]
  rows.append({'name':o.name,'vertices':len(bm.verts),'faces':len(bm.faces),'boundary_edges':len(open_edges),'bounds':[[min(p[i] for p in coords),max(p[i] for p in coords)] for i in range(3)],'boundary_vertices':list({tuple(round(c,5) for c in o.matrix_world@v.co) for e in open_edges for v in e.verts})[:100]})
  bm.free()
 data[path.stem]=rows
(out/'Topology.json').write_text(json.dumps(data,indent=2))
print('RUIN_TOPOLOGY_READY')
