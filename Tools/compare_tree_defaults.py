"""Unsaved original versus repaired foliage test in the existing village."""
import json,unreal as u
assert L.load_level('/Game/Maps/Lvl_OldVillage')
M=u.MaterialEditingLibrary;rows=[]
for a in A.get_all_level_actors():
 for c in a.get_components_by_class(u.StaticMeshComponent):
  if not c.static_mesh or not c.static_mesh.get_name().startswith('Tree_'):continue
  row={'actor':a.get_actor_label(),'mesh':c.static_mesh.get_path_name(),'forced_lod':c.get_editor_property('forced_lod_model'),'wpo_disable_distance':c.get_editor_property('world_position_offset_disable_distance'),'materials':[]}
  for i in range(c.get_num_materials()):
   m=c.get_material(i)
   row['materials'].append(m.get_path_name() if m else None)
   if m and 'MI_TreeLeaves' in m.get_name():
    source='/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/_Materials/'+('MI_TreeLeaves_Red_2' if 'Red' in m.get_name() else 'MI_TreeLeaves')
    c.set_material(i,u.load_asset(source))
  rows.append(row)
(ROOT/'Saved/MapSceneryAudit/TreeComponentSettings.json').write_text(json.dumps(rows,indent=2))
