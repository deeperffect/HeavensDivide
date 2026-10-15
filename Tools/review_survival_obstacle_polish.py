"""Rendered map review plus fitted-collision and pack-assembly checks."""
from pathlib import Path
import json,math,unreal as u
root=Path(u.Paths.project_dir()).resolve()
POLISH_OUT=root/'Saved/SurvivalMaps/ObstaclePolish'

def editor_checks(key,world,actors):
 result={}
 if key=='EmberQuarry':
  assert not any(a.get_actor_label()=='Layered sandstone collision' for a in actors)
  rocks=[a for a in actors if a.get_actor_label()=='Layered sandstone'];assert len(rocks)>=80
  for a in rocks:
   c=a.static_mesh_component
   assert c.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)==u.CollisionResponseType.ECR_BLOCK
   assert c.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY)==u.CollisionResponseType.ECR_BLOCK
   assert c.static_mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')==u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
   assert c.static_mesh.get_editor_property('complex_collision_mesh').get_name()=='SM_Sandstone_Collision'
  cliffs=[a for a in actors if a.get_actor_label()=='Canyon escarpment']
  assert all(a.get_actor_bounds(False)[1].z*2<=801 and a.actor_has_tag('CameraClearEscarpment') for a in cliffs)
  for a in cliffs:
   mb=a.static_mesh_component.static_mesh.get_bounds();scale=a.get_actor_scale3d()
   assert abs(mb.box_extent.x*2*scale.x-350)<.1 and abs(mb.box_extent.y*2*scale.y-1050)<.1
  result={'blocking_sandstone':len(rocks),'lowered_cliffs':len(cliffs)}
  reference=u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,-20000))
  reference.set_actor_label('Sandstone visual query reference');rc=reference.static_mesh_component
  rc.set_mobility(u.ComponentMobility.MOVABLE);assert rc.set_static_mesh(u.load_asset('/Game/Assets/Environment/GreenCave/Meshes/SM_StoneArray_a'))
  rc.set_collision_profile_name('OldVillageCanopy');rc.set_editor_property('can_ever_affect_navigation',False);rc.set_visibility(False);reference.set_actor_hidden_in_game(True)
 elif key=='MoonlitForest':
  heights=[]
  for a in actors:
   if a.get_actor_label()!='Woodland perimeter':continue
   for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
    heights += [c.static_mesh.get_bounds().box_extent.z*2*c.get_instance_transform(i,True).scale3d.z for i in range(c.get_instance_count())]
  assert heights and max(heights)<1121
  result={'lowered_perimeter_trees':len(heights),'max_height_cm':max(heights)}
 else:
  columns=[a for a in actors if a.get_actor_label()=='Cloister column'];assert len(columns)==20
  assert all(a.static_mesh_component.static_mesh.get_name().startswith('SM_Corner_Pillar_') for a in columns)
  topology=json.loads((POLISH_OUT/'Topology.json').read_text())['SM_Corner_Pillar']
  assert all(lod['boundary_edges']==0 for lod in topology)
  result={'closed_columns':len(columns),'closed_render_lods':4,'ruin_additions':sum(a.actor_has_tag('AncientRuinsDetailPass') for a in actors)}
 return result

def gameplay_checks(key,world):
 if key!='EmberQuarry':return {}
 rows=[];gaps=[];mismatches=0;unblocked=0
 build=json.loads((root/'Art/Environment/SurvivalMaps/Collision/CollisionBuild.json').read_text())
 assert build['selected']['triangles']==412 and build['selected']['surface_gap_max_cm']<25
 # Retain an independent full-detail source for comparison: the rendered mesh's
 # complex query now deliberately uses its reduced collision asset as well.
 reference=next(a for a in u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor) if a.get_actor_label()=='Sandstone visual query reference');rc=reference.static_mesh_component
 all_actors=u.GameplayStatics.get_all_actors_of_class(world,u.Actor)
 # Query the component itself so adjacent scenery cannot conceal a bad collider.
 # Compare cheap movement hulls against the rendered triangles at character height.
 for a in u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor):
  if a.get_actor_label()!='Layered sandstone':continue
  reference.set_actor_transform(a.get_actor_transform(),False,True)
  c=a.static_mesh_component;center,extent=a.get_actor_bounds(False);radius=math.hypot(extent.x,extent.y)+300
  for z in [85.,170.]:
   for j in range(16):
    t=j*math.tau/16;start=u.Vector(center.x+math.cos(t)*radius,center.y+math.sin(t)*radius,z);end=u.Vector(center.x,center.y,z)
    exact=rc.line_trace_component(start,end,True,False,False)
    simple=c.line_trace_component(start,end,False,False,False)
    if not exact:continue
    if not simple:
     mismatches+=1
     # A grazing zero-width ray can miss a simplified rim. The actual player
     # capsule must still be stopped, with every other actor ignored.
     hit=u.SystemLibrary.capsule_trace_single_by_profile(world,start,end,45,95,'Pawn',False,[actor for actor in all_actors if actor!=a],u.DrawDebugTrace.NONE,False)
     if not hit:unblocked+=1
     continue
    ep=next(v for v in exact if isinstance(v,u.Vector));sp=next(v for v in simple if isinstance(v,u.Vector))
    ed=math.dist(start.to_tuple(),ep.to_tuple());sd=math.dist(start.to_tuple(),sp.to_tuple());gap=max(0,ed-sd)
    gaps.append(gap)
    if gap>50:rows.append({'actor':a.get_name(),'angle':j,'height':z,'gap_cm':gap,'simple':list(sp.to_tuple()),'visual':list(ep.to_tuple())})
 assert gaps,'No sandstone surface probes'
 reference.destroy_actor()
 gaps.sort();result={'surface_comparisons':len(gaps),'grazing_ray_misses':mismatches,'unblocked_player_capsules':unblocked,'gap_p95_cm':gaps[int(len(gaps)*.95)],'gap_max_cm':max(gaps),'surface_gap_p95_cm':build['selected']['surface_gap_p95_cm'],'surface_gap_max_cm':build['selected']['surface_gap_max_cm'],'collision_triangles':412,'outliers':rows}
 (POLISH_OUT/'SandstoneProbes.json').write_text(json.dumps(result,indent=2))
 summary={k:v for k,v in result.items() if k!='outliers'}
 assert unblocked==0 and mismatches<=3,summary
 assert result['gap_p95_cm']<15 and result['gap_max_cm']<200,('Excessive sandstone collision gap',summary)
 return summary

EXTRA_EDITOR_VALIDATION=editor_checks
EXTRA_GAMEPLAY_VALIDATION=gameplay_checks
EXTRA_VIEWS={
 'MoonlitForest':[{'name':'Perimeter','position':(8090,0,1000),'target':(7350,0,100),'fov':72}],
 'EmberQuarry':[{'name':'Perimeter','position':(8800,-1800,1050),'target':(8070,-1800,100),'fov':72},{'name':'Sandstone','position':(3750,-2300,1850),'target':(2000,-2300,100),'fov':70}],
 'SunkenSanctuary':[{'name':'Cloister','position':(-2900,500,1450),'target':(-4700,800,250),'fov':70},{'name':'FloodedAltar','position':(-650,2100,1750),'target':(-2000,1350,80),'fov':75}]
}
SKIP_AUTOMATION=True
REPORT_NAME=globals().get('REPORT_NAME','ObstaclePolish/Validation.json')
exec(compile((root/'Tools/review_survival_maps.py').read_text(encoding='utf-8'),'review_survival_maps.py','exec'))
