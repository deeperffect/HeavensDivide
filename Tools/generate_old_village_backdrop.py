"""Blender: a small static landscape mesh outside the authored playable island."""
import bpy,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from old_village_backdrop_shape import height
OUT=ROOT/'Art/Environment/OldVillage/Backdrop';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
N=101;step=1000;verts=[];faces=[]
for y in range(N):
    for x in range(N):
        e=(x-50)*step;n=(y-50)*step
        verts.append((e/100,n/100,height(e,n)/100))
for y in range(N-1):
    for x in range(N-1):
        a=y*N+x;faces.append((a,a+1,a+N+1,a+N))
mesh=bpy.data.meshes.new('Countryside');mesh.from_pydata(verts,[],faces);mesh.update()
obj=bpy.data.objects.new('SM_OV_Countryside',mesh);bpy.context.collection.objects.link(obj)
bpy.context.view_layer.objects.active=obj;obj.select_set(True)
for polygon in mesh.polygons:polygon.use_smooth=True
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_OV_Countryside.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Z',axis_up='Y',apply_unit_scale=True)
(OUT/'Geometry.json').write_text(json.dumps({'vertices':len(verts),'triangles':len(faces)*2,'collision':False},indent=2))
print('OLD_VILLAGE_BACKDROP_GEOMETRY_READY',flush=True)
