from pathlib import Path
import unreal as u,json
root=Path(u.Paths.project_dir()).resolve()
prefix='/Game/Assets/Environment/'
groups={
 'JapaneseShrine/Meshes/':['SM_ShrineLittle01a','SM_ShrineBody01a','SM_ShrineRoof01a','SM_Pond_01a','SM_FenceRed01a','SM_FenceBamboo01a','SM_StatueJizo01a','SM_Well01a','SM_Cart01','SM_RockTiles01a'],
 'Ancient_Ruins/Meshes/':['SM_Central_Platform','SM_Central_Platform_Floor','SM_Tower_Base','SM_Tower_Corner','SM_Tower_C_Wall','SM_Tower_Door_Frame','SM_Tower_Topper','SM_Roof_Large','SM_Roof_Small','SM_Curved_Wall','SM_Fountain_Head','SM_Tree_1_Trunk','SM_Tree_1_Leaves','SM_Bush','SM_Root_2','SM_Wall_Variant'],
 'GreenCave/Meshes/':['SM_Entrance','SM_LargeArch','SM_Arch','SM_BeamSupport_a','SM_RockArray_a','SM_StoneArray_a','SM_LargeWall_b','SM_MineArch','SM_Table','SM_Toolshelf','SM_StandingLantern','SM_StraightRail_a','SM_StraightRail_b','SM_ForkedRail_2_a','SM_DeadTree_a','SM_Fern_a','SM_GlowingPlant_a'],
 'Stylized_Village/Assets/Foliage/Trees/':['Tree_Medium_Green','Tree_Large_Green','Tree_Small_Green','Tree_Small_Red'],
 'Stylized_Village/Assets/Foliage/Bushes/':['SM_Bush_01']}
data={}
for folder,names in groups.items():
 for name in names:
  path=prefix+folder+name;m=u.load_asset(path)
  if isinstance(m,u.StaticMesh):
   b=m.get_bounds();data[path]={'size':[round(v*2,1) for v in b.box_extent.to_tuple()],'origin':list(b.origin.to_tuple())}
(root/'Saved/SurvivalMaps/ReworkAssets.json').write_text(json.dumps(data,indent=2))
u.log('REWORK_ASSETS_READY')
