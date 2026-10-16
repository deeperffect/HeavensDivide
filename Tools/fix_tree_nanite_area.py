"""Keep Nanite tree canopies from thinning; preserve supplied source packages."""
from pathlib import Path
import sys,json,unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
import repair_map_scenery as r
changed=[]
# The texture experiment did not address geometry simplification. Restore the
# vendor texture; only the prepared mesh copies need the distance correction.
original=u.load_asset('/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/_Textures/T_Leaves_BC')
for path in r.E.list_assets(r.DEST+'/Materials',True,False):
 m=u.load_asset(path)
 if not isinstance(m,u.MaterialInstanceConstant):continue
 for n in r.M.get_texture_parameter_names(m):
  t=r.M.get_material_instance_texture_parameter_value(m,n)
  if t and t.get_name()=='T_Leaves_BC_Coverage':
   r.M.set_material_instance_texture_parameter_value(m,n,original);r.M.update_material_instance(m);r.E.save_loaded_asset(m,False)
for path in r.E.list_assets(r.DEST+'/Meshes',True,False):
 m=u.load_asset(path)
 if not isinstance(m,u.StaticMesh) or not m.get_name().startswith('Tree_'):continue
 settings=r.SM.get_nanite_settings(m)
 if not settings.enabled:continue
 settings.set_editor_property('shape_preservation',u.NaniteShapePreservation.PRESERVE_AREA)
 r.SM.set_nanite_settings(m,settings,True)
 assert r.E.save_loaded_asset(m,False)
 changed.append(m.get_path_name())
(r.OUT/'TreeNaniteRepair.json').write_text(json.dumps({'shape_preservation':'PreserveArea','meshes':changed},indent=2))
assert r.L.load_level('/Game/Maps/Lvl_OldVillage')
