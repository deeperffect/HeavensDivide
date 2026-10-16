"""Read-only saved-map provenance, foliage and collision inventory."""
from pathlib import Path
import json, unreal as u
ROOT=Path(u.Paths.project_dir()).resolve(); OUT=ROOT/'Saved/MapSceneryAudit'; OUT.mkdir(parents=True,exist_ok=True)
E=u.EditorAssetLibrary; A=u.get_editor_subsystem(u.EditorActorSubsystem); L=u.get_editor_subsystem(u.LevelEditorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
D={'maps':{},'meshes':{},'materials':{},'candidates':{}}
def mat(m):
 if not m or m.get_path_name() in D['materials']:return
 p=m.get_path_name(); row={'class':m.get_class().get_name()};D['materials'][p]=row
 if isinstance(m,u.MaterialInstanceConstant):
  par=m.get_editor_property('parent');row['parent']=par.get_path_name();mat(par)
  row['textures']={str(n):str(u.MaterialEditingLibrary.get_material_instance_texture_parameter_value(m,n).get_path_name()) for n in u.MaterialEditingLibrary.get_texture_parameter_names(m) if u.MaterialEditingLibrary.get_material_instance_texture_parameter_value(m,n)}
  row['scalars']={str(n):u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(m,n) for n in u.MaterialEditingLibrary.get_scalar_parameter_names(m)}
  row['overrides']=str(m.get_editor_property('base_property_overrides'))
 elif isinstance(m,u.Material):
  for key in ['blend_mode','two_sided','shading_model','dithered_lod_transition','use_material_attributes','opacity_mask_clip_value']:
   try:row[key]=str(m.get_editor_property(key))
   except:pass
def mesh(m):
 p=m.get_path_name()
 if p in D['meshes']:return
 b=m.get_bounds();bs=m.get_editor_property('body_setup')
 row={'origin':list(b.origin.to_tuple()),'extent':list(b.box_extent.to_tuple()),'lods':SM.get_lod_count(m),'vertices':[SM.get_number_verts(m,i) for i in range(SM.get_lod_count(m))],'materials':[s.material_interface.get_path_name() if s.material_interface else None for s in m.get_editor_property('static_materials')],'collision':str(bs.get_editor_property('collision_trace_flag')) if bs else None,'simple_shapes':SM.get_simple_collision_count(m),'source':m.get_editor_property('asset_import_data').get_first_filename() if m.get_editor_property('asset_import_data') else ''}
 D['meshes'][p]=row
 for s in m.get_editor_property('static_materials'):mat(s.material_interface)
for key in ['OldVillage','MoonlitForest','SunkenSanctuary','EmberQuarry']:
 assert L.load_level('/Game/Maps/Lvl_'+key);rows=[]
 for a in A.get_all_level_actors():
  for c in a.get_components_by_class(u.StaticMeshComponent):
   m=c.static_mesh
   if not m:continue
   mesh(m);center,extent,_=u.SystemLibrary.get_component_bounds(c)
   row={'actor':a.get_name(),'label':a.get_actor_label(),'component':c.get_name(),'folder':str(a.get_folder_path()),'mesh':m.get_path_name(),'location':list(a.get_actor_location().to_tuple()),'scale':list(a.get_actor_scale3d().to_tuple()),'yaw':a.get_actor_rotation().yaw,'center':list(center.to_tuple()),'extent':list(extent.to_tuple()),'visible':c.is_visible() and not a.get_editor_property('hidden'),'profile':str(c.get_collision_profile_name()),'collision':str(c.get_collision_enabled()),'pawn':str(c.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN)),'enemy':str(c.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY)),'nav':c.get_editor_property('can_ever_affect_navigation'),'tags':[str(t) for t in a.tags],'materials':[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]}
   if isinstance(c,u.InstancedStaticMeshComponent):row['instances']=[{'p':list(c.get_instance_transform(i,True).translation.to_tuple()),'s':list(c.get_instance_transform(i,True).scale3d.to_tuple())} for i in range(c.get_instance_count())]
   for i in range(c.get_num_materials()):mat(c.get_material(i))
   rows.append(row)
 D['maps'][key]=rows
 (OUT/'Inventory.json').write_text(json.dumps(D,indent=2))
for path in ['/Game/Assets/Environment/GreenCave/Meshes/SM_WaterPlane','/Game/Assets/Environment/JapaneseShrine/Meshes/SM_DirtGround01a','/Game/Assets/Environment/JapaneseShrine/Meshes/SM_DirtGround02a','/Game/Assets/Environment/Ancient_Ruins/Meshes/SM_Floor','/Game/Assets/Environment/Ancient_Ruins/Meshes/SM_Tower_Base','/Game/Assets/Environment/JapaneseShrine/Meshes/SM_KitRockCliff03a']:
 m=u.load_asset(path);mesh(m);D['candidates'][path]=D['meshes'][m.get_path_name()]
 t=u.AssetExportTask();t.object=m;t.filename=str(OUT/(m.get_name()+'.fbx'));t.exporter=u.StaticMeshExporterFBX();t.automated=True;t.prompt=False;t.replace_identical=True;assert u.Exporter.run_asset_export_task(t)
(OUT/'Inventory.json').write_text(json.dumps(D,indent=2));u.log('MAP_SCENERY_AUDIT_COMPLETE')

