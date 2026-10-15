"""Build and measure a low triangle-count collider without filling rock crevices."""
from pathlib import Path
import bpy,bmesh,json,math,re
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parents[1];out=root/'Saved/SurvivalMaps/ObstaclePolish';dest=root/'Art/Environment/SurvivalMaps/Collision';dest.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(out/'SM_StoneArray_a.fbx'))
obj=next(o for o in bpy.context.scene.objects if o.name=='SM_StoneArray_a_LOD0')
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bm.to_mesh(obj.data);bm.free()
def tree(mesh):return BVHTree.FromPolygons([v.co for v in mesh.vertices],[tuple(p.vertices) for p in mesh.polygons],all_triangles=False)
exact=tree(obj.data)
rocks=[r for r in json.loads((out/'Inspection.json').read_text())['maps']['EmberQuarry'] if r['label']=='Layered sandstone']
rays=[]
for a in rocks:
 center=a['center'];extent=a['extent'];radius=math.hypot(extent[0],extent[1])+300;s=a['scale'][0];yaw=math.radians(float(re.search(r'yaw: ([\d.\-]+)',a['rotation']).group(1)));cy,sy=math.cos(yaw),math.sin(yaw)
 def local(p):
  x=(p[0]-a['location'][0])/s;y=(p[1]-a['location'][1])/s;z=(p[2]-a['location'][2])/s
  return Vector(((x*cy+y*sy)/100,-(-x*sy+y*cy)/100,z/100))
 for z in [85.,170.]:
  for j in range(16):
   t=j*math.tau/16;start=local((center[0]+math.cos(t)*radius,center[1]+math.sin(t)*radius,z));end=local((center[0],center[1],z));delta=end-start
   hit=exact.ray_cast(start,delta.normalized(),delta.length)
   if hit[0] is not None:rays.append((start,delta,hit[3],s,a['name'],j,z))
report={'source_triangles':sum(len(p.vertices)-2 for p in obj.data.polygons),'trials':[]}
for ratio in [.2,.3,.4,.5]:
 candidate=obj.copy();candidate.data=obj.data.copy();bpy.context.collection.objects.link(candidate);bpy.ops.object.select_all(action='DESELECT');candidate.select_set(True);bpy.context.view_layer.objects.active=candidate
 modifier=candidate.modifiers.new('Low cost static collision','DECIMATE');modifier.ratio=ratio;bpy.ops.object.modifier_apply(modifier=modifier.name)
 # 2 cm outward margin covers the tiny inward errors of simplification.
 for v in candidate.data.vertices:v.co+=v.normal*.02
 bv=tree(candidate.data);gaps=[];surface_gaps=[];misses=[]
 for start,delta,d,s,name,angle,z in rays:
  hit=bv.ray_cast(start,delta.normalized(),delta.length)
  if hit[0] is None:misses.append([name,angle,z]);continue
  gaps.append(max(0,(d-hit[3])*s*100))
  surface_gaps.append(exact.find_nearest(hit[0])[3]*s*100)
 gaps.sort();surface_gaps.sort();stats={'ratio':ratio,'triangles':sum(len(p.vertices)-2 for p in candidate.data.polygons),'rays':len(rays),'misses':misses,'p95_cm':gaps[int(len(gaps)*.95)],'max_cm':max(gaps),'surface_gap_p95_cm':surface_gaps[int(len(surface_gaps)*.95)],'surface_gap_max_cm':max(surface_gaps)};report['trials'].append(stats)
 # Ray length can jump across a deep crevice after grazing a rim. Actual distance
 # to the visible surface is the meaningful clearance metric at those angles.
 if len(misses)<=3 and stats['p95_cm']<15 and stats['surface_gap_p95_cm']<10 and stats['surface_gap_max_cm']<25:
  candidate.name='SM_Sandstone_Collision';bpy.ops.export_scene.fbx(filepath=str(dest/'SM_Sandstone_Collision.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,global_scale=1.,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE')
  report['selected']=stats;break
 bpy.data.objects.remove(candidate,do_unlink=True)
assert 'selected' in report,report
(dest/'CollisionBuild.json').write_text(json.dumps(report,indent=2));print('SANDSTONE_COLLISION_BUILT',report['selected'])
