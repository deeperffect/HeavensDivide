"""Run with Blender --background --python; builds new map floors and horizons."""
import bpy,sys,json,math
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'Tools'))
from survival_map_layout import MAPS,playable,backdrop_height,inside_polygon,boundary
out=root/'Art/Environment/SurvivalMaps/Terrain';out.mkdir(parents=True,exist_ok=True)
report={}
def export(name,vertices,faces):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 # The FBX conversion reflects Blender Y into Unreal's left-handed frame.
 # Compensate here so authored asymmetric water holes match Unreal placements.
 vertices=[(x,-y,z) for x,y,z in vertices];faces=[tuple(reversed(f)) for f in faces]
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
 uv=mesh.uv_layers.new(name='TerrainUV')
 for polygon in mesh.polygons:
  for index in polygon.loop_indices:
   co=mesh.vertices[mesh.loops[index].vertex_index].co
   uv.data[index].uv=(co.x/1000,(co.y+co.z)/1000)
 obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj);obj.select_set(True);bpy.context.view_layer.objects.active=obj
 bpy.ops.export_scene.fbx(filepath=str(out/(name+'.fbx')),use_selection=True,apply_unit_scale=True,global_scale=.01,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,bake_anim=False,mesh_smooth_type='FACE')
 report[name]={'vertices':len(vertices),'triangles':sum(len(f)-2 for f in faces)}
for key,cfg in MAPS.items():
 verts=[];faces=[];lookup={}
 def vertex(x,y,z=0):
  p=(x,y,z)
  if p not in lookup:lookup[p]=len(verts);verts.append(p)
  return lookup[p]
 step=200;w,h=cfg['size']
 for x in range(-w//2,w//2,step):
  for y in range(-h//2,h//2,step):
   if not playable(key,x+step/2,y+step/2):continue
   faces.append(tuple(vertex(px,py) for px,py in [(x,y),(x+step,y),(x+step,y+step),(x,y+step)]))
 # Vertical cut faces make the combat floor read as solid land above water.
 edges={}
 for face in faces:
  for a,b in zip(face,face[1:]+face[:1]):
   pair=tuple(sorted((a,b)));edges[pair]=edges.get(pair,0)+1
 for (a,b),count in edges.items():
  if count==1:
   ax,ay,_=verts[a];bx,by,_=verts[b]
   faces.append((a,vertex(ax,ay,-450),vertex(bx,by,-450),b))
 export('SM_'+key+'_Ground',verts,faces)
 verts=[];faces=[];step=600;n=101
 for ix in range(n):
  x=(ix-(n-1)/2)*step
  for iy in range(n):
   y=(iy-(n-1)/2)*step
   verts.append((x,y,backdrop_height(key,x,y)))
 for ix in range(n-1):
  for iy in range(n-1):
   x=(ix-(n-1)/2+.5)*step;y=(iy-(n-1)/2+.5)*step
   if inside_polygon(x,y,boundary(key)):continue
   a=ix*n+iy;faces.append((a,a+n,a+n+1,a+1))
 export('SM_'+key+'_Surroundings',verts,faces)
(out/'Geometry.json').write_text(json.dumps(report,indent=2))
