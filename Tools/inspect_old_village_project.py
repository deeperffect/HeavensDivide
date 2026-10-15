"""Read-only inventory of existing environment helpers and current map setup."""
from pathlib import Path
import json, unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/OldVillage';OUT.mkdir(parents=True,exist_ok=True)
names=['SM_TreeBlackPineTrunk01a','SM_TreeBlackPineLeaves01a','SM_TreeBamboo01a','SM_TreeBamboo02a',
 'SM_TreeCedarTrunk01a','SM_TreeCedarLeaves01a','SM_LampPost01a','SM_LampStone01a','SM_LampPond01a',
 'SM_Grass_01a','SM_Pond_01a','SM_PondRock01a','SM_PondRock02a','SM_PondRock03a',
 'SM_Bench_01a','SM_Cart01','SM_Katana_Holder','SM_PotUnSoiled01a','SM_Plant_WaterLilies_Group_01a',
 'SM_Rock_Ground_01a','SM_RockCliff01a','SM_RockTile01a','SM_RockTile02a','SM_Lantern_01a',
 'SM_ShrineStairs01a','SM_ShrineFence03a','SM_Sack_01','SM_RockPebble01a','SM_PlantainLily01a']
paths=['/Game/Assets/Environment/JapaneseShrine/Meshes/'+n for n in names]
paths+=['/Game/Assets/Environment/Stylized_Village/Assets/Props/Furniture/SM_SM_Barrel_01',
 '/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Bushes/SM_Bush_01',
 '/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Grass/SM_SimpleGrass_Green']
data={'assets':[],'api':{},'map':[]}
editor=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
for p in paths:
    a=u.load_asset(p)
    if not isinstance(a,u.StaticMesh):continue
    b=a.get_bounds()
    data['assets'].append(dict(path=p,origin=list(b.origin.to_tuple()),extent=list(b.box_extent.to_tuple()),
       materials=[s.material_interface.get_path_name() if s.material_interface else None for s in a.get_editor_property('static_materials')]))
for cls,names in [(u.StaticMeshEditorSubsystem,['set_convex_decomposition_collisions','add_simple_collisions','get_number_triangles','set_lods']),
 (u.NavigationSystemV1,['find_path_to_location_synchronously','project_point_to_navigation']),
 (u.LevelEditorSubsystem,['new_level','save_current_level','build_light_maps']),
 (u.EditorLevelLibrary,['editor_play_simulate'])]:
    for name in names:
        fn=getattr(cls,name,None)
        data['api'][cls.__name__+'.'+name]=str(getattr(fn,'__doc__',None))
level=u.get_editor_subsystem(u.LevelEditorSubsystem);assert level.load_level('/Game/Maps/Lvl_B1_Lvl1')
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a,(u.PlayerStart,u.NavMeshBoundsVolume)) or any(s in a.get_class().get_name() for s in ['Spawner','Director','Camera','Samurai','Ninja','GameMode']):
        data['map'].append(dict(label=a.get_actor_label(),class_path=a.get_class().get_path_name(),location=list(a.get_actor_location().to_tuple()),rotation=str(a.get_actor_rotation()),scale=list(a.get_actor_scale3d().to_tuple())))
data['world_settings']=str(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_world_settings().get_editor_property('default_game_mode'))
(OUT/'ProjectInventory.json').write_text(json.dumps(data,indent=2));u.log('OLD_VILLAGE_PROJECT_INSPECTED')
