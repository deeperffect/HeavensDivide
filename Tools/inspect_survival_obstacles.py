"""Read the source geometry and placed obstacles before targeted corrections."""
from pathlib import Path
import json, unreal as u
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/SurvivalMaps/ObstaclePolish';out.mkdir(parents=True,exist_ok=True)
E=u.EditorAssetLibrary;A=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
R='/Game/Assets/Environment/Ancient_Ruins/Meshes/'
paths=[R+n for n in ['SM_Column','SM_Corner_Pillar','SM_Corner_Pillar_Detailed_L','SM_Tower_Corner','SM_Tower_Wall_Window','SM_Tower_Door_Frame','SM_Curved_Wall','SM_Curved_Wall_Trim','SM_Fountain_Head','SM_Circular_Steps','SM_Central_Platform','SM_Central_Platform_Floor','SM_Central_Platform_Bricks','SM_Stairs_Type_1','SM_Floor_Variant_2_Detailed','SM_Royal_Crest','SM_Root_2','SM_Root_7','SM_Tree_2_Trunk','SM_Tree_2_Leaves','SM_Vine_1','SM_Wall_Detail_1','SM_Wall_Detail_2','SM_Wall_Detail_3']]
paths+=['/Game/Assets/Environment/GreenCave/Meshes/SM_StoneArray_a']
data={'assets':{},'maps':{},'api':{}}
for path in paths:
 m=u.load_asset(path);b=m.get_bounds()
 data['assets'][m.get_name()]={'path':path,'origin':list(b.origin.to_tuple()),'extent':list(b.box_extent.to_tuple()),'lods':SM.get_lod_count(m),'materials':[s.material_interface.get_path_name() if s.material_interface else None for s in m.get_editor_property('static_materials')]}
 task=u.AssetExportTask();task.object=m;task.filename=str(out/(m.get_name()+'.fbx'));task.exporter=u.StaticMeshExporterFBX();task.automated=True;task.prompt=False;task.replace_identical=True
 if not Path(task.filename).exists():assert u.Exporter.run_asset_export_task(task),path
for key in ['MoonlitForest','EmberQuarry','SunkenSanctuary']:
 assert L.load_level('/Game/Maps/Lvl_'+key);actors=[]
 for a in A.get_all_level_actors():
  label=a.get_actor_label()
  if not any(n in label for n in ['Canyon escarpment','Woodland perimeter','Layered sandstone','Cloister column']):continue
  c,e=a.get_actor_bounds(False)
  row={'label':label,'name':a.get_name(),'folder':str(a.get_folder_path()),'center':list(c.to_tuple()),'extent':list(e.to_tuple()),'location':list(a.get_actor_location().to_tuple()),'rotation':str(a.get_actor_rotation()),'scale':list(a.get_actor_scale3d().to_tuple())}
  if isinstance(a,u.StaticMeshActor):
   mesh=a.static_mesh_component.static_mesh;row.update(mesh=mesh.get_path_name(),profile=str(a.static_mesh_component.get_collision_profile_name()),trace=str(mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')))
  actors.append(row)
 data['maps'][key]=actors
for cls,names in [(u.StaticMeshEditorSubsystem,['set_convex_decomposition_collisions','get_convex_collision_count','remove_collisions']), (u.GeometryScript_AssetUtils,['copy_mesh_from_static_mesh','copy_mesh_to_static_mesh']),(u.GeometryScript_MeshQueries,['get_mesh_info_string'])]:
 for name in names:data['api'][cls.__name__+'.'+name]=str(getattr(getattr(cls,name,None),'__doc__',None))
data['ruin_levels']=list(E.list_assets('/Game/Assets/Environment/Ancient_Ruins/Levels',recursive=True,include_folder=False))
(out/'Inspection.json').write_text(json.dumps(data,indent=2))
u.log('SURVIVAL_OBSTACLES_INSPECTED')
