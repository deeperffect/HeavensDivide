"""Ancient_Ruins kit assemblies, preserving wide combat lanes and level floor."""
import math
import unreal as u
from survival_map_layout import playable, reserved

R='/Game/Assets/Environment/Ancient_Ruins/Meshes/'
TAG='AncientRuinsDetailPass'

def add_details(b):
 A=b.ACT;before={a.get_name() for a in A.get_all_level_actors()};sources=set()
 def prop(model,label,x,y,scale=1,yaw=0,z=0,solid=True,folder='35_Ancient ruins'):
  sources.add(model)
  return b.prop(R+model,label,x,y,scale=scale,yaw=yaw,z=z,solid=solid,folder=folder)
 # Finish the existing tower silhouettes with real windowed walls, not solid boxes.
 for x,y,s in [(-7100,-5200,1.25),(-7000,5700,1.25),(7200,5700,1.45),(7200,-5100,1.15)]:
  for dx,dy,yaw in [(0,-330,0),(330,0,90),(0,330,180),(-330,0,270)]:
   prop('SM_Tower_Wall_Window','Ruined tower window',x+dx*s,y+dy*s,(1.55*s,2*s,s),yaw,180*s,folder='35_Ancient ruins/Tower chambers')
  prop('SM_Royal_Crest','Weathered tower crest',x,y-375*s,.7*s,z=230*s,solid=False,folder='35_Ancient ruins/Tower chambers')
 # Matched trim completes the existing curved shrine walls. Shared kit pivots matter.
 for wall in list(A.get_all_level_actors()):
  if wall.get_actor_label()!='Broken circular shrine':continue
  loc=wall.get_actor_location();s=wall.get_actor_scale3d();loc.z+=450*s.z
  sources.add('SM_Curved_Wall_Trim')
  b.mesh_actor(b.copied_mesh(R+'SM_Curved_Wall_Trim'),'Shrine coping',loc.x,loc.y,loc.z,(s.x,s.y,s.z),wall.get_actor_rotation().yaw,folder='35_Ancient ruins/Shrine gardens',solid=False,seat=False)
 # An ornate island altar uses three matching platform layers with their shared pivot.
 base=prop('SM_Central_Platform','Reservoir island altar',-2000,800,1.2,z=-45,solid=False,folder='35_Ancient ruins/Flooded altar')
 loc=base.get_actor_location()
 for model,label in [('SM_Central_Platform_Floor','Altar inset floor'),('SM_Central_Platform_Bricks','Altar broken masonry')]:
  sources.add(model)
  b.mesh_actor(b.copied_mesh(R+model),label,loc.x,loc.y,loc.z,1.2,folder='35_Ancient ruins/Flooded altar',solid=False,seat=False)
 plinth=prop('SM_Obelisk_Pedestal','Altar pedestal',-2000,800,1.25,z=113,solid=False,folder='35_Ancient ruins/Flooded altar')
 c,e=plinth.get_actor_bounds(False)
 prop('SM_Obelisk','Flooded waystone',-2000,800,1.75,z=c.z+e.z,solid=False,folder='35_Ancient ruins/Flooded altar')
 # Fountain heads are wall-mounted pieces: mount them onto masonry facing the water.
 for x,y in [(-2000,3090),(2200,-2110)]:
  folder='35_Ancient ruins/Reservoir fountains'
  prop('SM_Wall','Fountain retaining wall',x,y,(2.6,5.,1.3),folder=folder)
  for dx in [-380,380]:prop('SM_Corner_Pillar','Fountain pier',x+dx,y,(2.0,2.0,1.4),folder=folder)
  prop('SM_Fountain_Head','Carved fountain mask',x,y-65,4.5,z=150,solid=False,folder=folder)
  prop('SM_Trim_Full','Fountain cornice',x,y,(2.6,3.5,2.),z=390,solid=False,folder=folder)
 # The pack's paired trunk and leaf meshes share a pivot. Never centre them separately.
 for x,y,yaw in [(-7700,3400,25),(4400,6800,150),(7700,-2800,-20)]:
  s=.46;folder='35_Ancient ruins/Root gardens'
  trunk=b.copied_mesh(R+'SM_Tree_2_Trunk',True);leaf=b.copied_mesh(R+'SM_Tree_2_Leaves',True)
  sources.update(['SM_Tree_2_Trunk','SM_Tree_2_Leaves'])
  a=b.mesh_actor(trunk,'Ancient garden tree trunk',x,y,0,s,yaw,folder,False)
  loc=a.get_actor_location()
  b.mesh_actor(leaf,'Ancient garden tree canopy',loc.x,loc.y,loc.z,s,yaw,folder,False,seat=False)
  b.proxy('Ancient garden trunk collision',x,y,130,130,500,round=True)
  for dx,dy in [(-190,50),(200,30)]:prop('SM_Root_7','Roots around garden masonry',x+dx,y+dy,1.7,yaw=yaw+dx,solid=True,folder=folder)
  for dx,dy in [(-300,-160),(260,190)]:prop('SM_Rock_Medium_2','Root garden stones',x+dx,y+dy,1.25,yaw=yaw,folder=folder)
 # Decorative floor mosaics sit < 1 cm above the existing collision floor.
 # Four HISM batches replace many individual actors; no collision, nav or shadows.
 for i,(x,y) in enumerate([(-5700,-3000),(-5700,2200),(1600,1200),(6000,5200)]):
  model='SM_Floor_Variant_2_Detailed' if i%2==0 else 'SM_Floor_Corner_Var_2_Detailed';sources.add(model)
  mesh=b.copied_mesh(R+model);bounds=mesh.get_bounds();positions=[]
  for dx in range(-2,3):
   for dy in range(-2,3):
    if abs(dx)==2 and abs(dy)==2:continue
    px=x+dx*285;py=y+dy*285
    if playable('SunkenSanctuary',px,py,150):positions.append((px,py,.3))
  a=b.instances(mesh,'Inset ruin courtyard paving',positions,[1]*len(positions),folder='35_Ancient ruins/Inset paving')
  c=a.get_component_by_class(u.InstancedStaticMeshComponent)
  for j,(px,py,z) in enumerate(positions):
   sx=280/(bounds.box_extent.x*2);sy=280/(bounds.box_extent.y*2);sz=.6/(bounds.box_extent.z*2)
   c.update_instance_transform(j,u.Transform(location=u.Vector(px-bounds.origin.x*sx,py-bounds.origin.y*sy,z-(bounds.origin.z-bounds.box_extent.z)*sz),scale=u.Vector(sx,sy,sz)),True,True,True)
  a.set_editor_property('tags',['SurvivalInsetPaving'])
 # Wall buttresses and hanging ivy are attached to existing architecture.
 for wall in list(A.get_all_level_actors()):
  if wall.get_actor_label()!='Cloister wall':continue
  c,e=wall.get_actor_bounds(False);yaw=wall.get_actor_rotation().yaw;r=math.radians(yaw)
  x=c.x+math.sin(r)*65;y=c.y-math.cos(r)*65
  if not playable('SunkenSanctuary',x,y,60) or reserved('SunkenSanctuary',x,y,-150):continue
  prop('SM_Wall_Detail_1','Masonry buttress',x,y,(2.5,2.,1.5),yaw,folder='35_Ancient ruins/Wall gardens')
  prop('SM_Vine_1','Ivy spilling down masonry',x+math.cos(r)*110,y+math.sin(r)*110,.6,yaw,z=60,solid=False,folder='35_Ancient ruins/Wall gardens')
 # Replace cave-pack camp storage with the ruin pack's crates, barrels and bags.
 for a in list(A.get_all_level_actors()):
  if not isinstance(a,u.StaticMeshActor) or str(a.get_folder_path())!='30_Districts/Archaeologists camp':continue
  mesh=a.static_mesh_component.static_mesh
  model='SM_Wooden_Crate' if 'SM_Box_' in mesh.get_name() else 'SM_Barrel' if 'SM_Barrel_' in mesh.get_name() else 'SM_Burlap_Bag' if 'SM_Bag_' in mesh.get_name() else None
  if not model:continue
  center,extent=a.get_actor_bounds(False);smesh=b.copied_mesh(R+model);mb=smesh.get_bounds();yaw=a.get_actor_rotation().yaw
  # Match the original stack's height and local footprint, retaining its proxy.
  old=mesh.get_bounds();scale=a.get_actor_scale3d()
  a.static_mesh_component.set_static_mesh(smesh);a.set_actor_scale3d(u.Vector(old.box_extent.x*scale.x/mb.box_extent.x,old.box_extent.y*scale.y/mb.box_extent.y,old.box_extent.z*scale.z/mb.box_extent.z))
  nc,ne=a.get_actor_bounds(False);loc=a.get_actor_location();loc.x+=center.x-nc.x;loc.y+=center.y-nc.y;loc.z+=center.z-extent.z-(nc.z-ne.z);a.set_actor_location(loc,False,True)
  for slot in range(a.static_mesh_component.get_num_materials()):a.static_mesh_component.set_material(slot,b.fade_material(smesh.get_material(slot),b.fade_cache))
  sources.add(model)
 added=[a for a in A.get_all_level_actors() if a.get_name() not in before]
 for a in added:
  tags=list(a.get_editor_property('tags'));a.set_editor_property('tags',tags+[TAG])
 return {'new_actors':len(added),'source_models':sorted(sources),'inset_tiles':sum(c.get_instance_count() for a in added for c in a.get_components_by_class(u.InstancedStaticMeshComponent))}
