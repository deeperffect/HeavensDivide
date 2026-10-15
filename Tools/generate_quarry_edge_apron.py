"""Ten-triangle visual ground extension under the lower quarry border."""
from pathlib import Path
import bpy,sys,math
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'Tools'))
from survival_map_layout import boundary
points=boundary('EmberQuarry');vertices=[(0,0,0)]
for i,p in enumerate(points):
 prev=points[i-1];nxt=points[(i+1)%len(points)]
 def normal(a,b):
  dx=b[0]-a[0];dy=b[1]-a[1];length=math.hypot(dx,dy);return dy/length,-dx/length
 a=normal(prev,p);b=normal(p,nxt);d=900/(1+a[0]*b[0]+a[1]*b[1])
 vertices.append((p[0]+(a[0]+b[0])*d,-p[1]-(a[1]+b[1])*d,0))
faces=[(0,(i+1)%len(points)+1,i+1) for i in range(len(points))]
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
mesh=bpy.data.meshes.new('SM_Quarry_EdgeApron');mesh.from_pydata(vertices,[],faces);mesh.update()
obj=bpy.data.objects.new('SM_Quarry_EdgeApron',mesh);bpy.context.collection.objects.link(obj);obj.select_set(True);bpy.context.view_layer.objects.active=obj
out=root/'Art/Environment/SurvivalMaps/Terrain/SM_Quarry_EdgeApron.fbx'
bpy.ops.export_scene.fbx(filepath=str(out),use_selection=True,apply_unit_scale=True,global_scale=.01,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE')
print('QUARRY_EDGE_APRON_READY',len(faces))
