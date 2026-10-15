from pathlib import Path
import json, unreal as u
root=Path(u.Paths.project_dir()).resolve(); out=root/'Saved/SurvivalMaps'; out.mkdir(parents=True,exist_ok=True)
groups={
 '/Game/Assets/Environment/Ancient_Ruins/Meshes/':['SM_Column','SM_Wall','SM_Wall_Half','SM_Wall_Quarter','SM_Wall_Pillar_Short','SM_Obelisk','SM_Obelisk_Pedestal','SM_Brick_Rubble_Wall','SM_Brick_Kit_2','SM_Rock_Medium_1','SM_Rock_Large_1','SM_Mountain','SM_Leafy_Foliage_1','SM_Red_Flower','SM_Lily_Pad','SM_Urn','SM_Wooden_Crate','SM_Central_Platform_Floor'],
 '/Game/Assets/Environment/GreenCave/Meshes/':['SM_MineArch','SM_Cart','SM_BeamSupport_a','SM_RockArray_a','SM_TotemRock_a','SM_TotemRock_b','SM_TotemRock_c','SM_StraightRail_a','SM_Box_a','SM_Barrel_a','SM_Bag_a','SM_Pickaxe','SM_Crystal_Blue_a','SM_SingleCrystal_Red_a','SM_SmallCrystals','SM_Bush','SM_LargeMushrooms_a','SM_SmallMushrooms_a','SM_WoodSupport','SM_Platform_a','SM_Arch'],
 '/Game/Assets/Environment/JapaneseShrine/Meshes/':['SM_TreeCedarTrunk01a','SM_TreeCedarLeaves01a','SM_TreeBlackPineTrunk01a','SM_TreeBlackPineLeaves01a','SM_KitRockCliff03a','SM_Rock_Ground_01a','SM_LampStone01a','SM_LampPost01a','SM_PlantainLily01a'],
 '/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/':['Tree_Medium_Green','Tree_Small_Red'],
 '/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Bushes/':['SM_Bush_01']}
data={}
sm=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
for prefix,names in groups.items():
 for name in names:
  mesh=u.load_asset(prefix+name)
  if not isinstance(mesh,u.StaticMesh):continue
  b=mesh.get_bounds()
  data[prefix+name]={'origin':list(b.origin.to_tuple()),'extent':list(b.box_extent.to_tuple()),'lods':sm.get_lod_count(mesh),'collision':str(mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')),'materials':[s.material_interface.get_path_name() if s.material_interface else None for s in mesh.get_editor_property('static_materials')]}
(out/'AssetInventory.json').write_text(json.dumps(data,indent=2))
u.log('SURVIVAL_ASSETS_INSPECTED '+str(len(data)))
