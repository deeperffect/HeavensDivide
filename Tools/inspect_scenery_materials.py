from pathlib import Path
import json,unreal as u
R=Path(u.Paths.project_dir()).resolve();O=R/'Saved/MapSceneryAudit';M=u.MaterialEditingLibrary;D={}
for path in ['/Game/Assets/VFX/MagicProjectilesVol3/Models/SM_Plane01','/Game/Assets/VFX/StylizedOrbsVol1/Models/SM_Sphere01','/Game/Assets/Environment/Ancient_Ruins/Materials/MM_Leaves','/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/Tree_Medium_Green','/Game/Assets/Environment/Ancient_Ruins/Meshes/SM_Trim_Full']:
 o=u.load_asset(path);row={};D[path]=row
 if isinstance(o,u.StaticMesh):
  b=o.get_bounds();row['origin']=list(b.origin.to_tuple());row['extent']=list(b.box_extent.to_tuple())
  bs=o.get_editor_property('body_setup')
  if bs:
   ag=bs.get_editor_property('agg_geom');row['collision']={}
   for key in ['sphere_elems','box_elems','sphyl_elems','convex_elems']:
    vals=[]
    for el in ag.get_editor_property(key):
     er={}
     for p in ['center','x','y','z','radius','length','rotation','vertex_data']:
      try:er[p]=str(el.get_editor_property(p))
      except:pass
     vals.append(er)
    row['collision'][key]=vals
 elif isinstance(o,u.Material):
  seen={}
  def walk(n):
   if not n or n.get_name() in seen:return
   nr={'class':n.get_class().get_name()};seen[n.get_name()]=nr
   for p in ['code','parameter_name','default_value','material_function']:
    try:nr[p]=str(n.get_editor_property(p))
    except:pass
   ns=M.get_inputs_for_material_expression(o,n);nr['inputs']=[x.get_name() for x in ns if x]
   for nn in ns:walk(nn)
  for p in [u.MaterialProperty.MP_WORLD_POSITION_OFFSET,u.MaterialProperty.MP_OPACITY_MASK,u.MaterialProperty.MP_BASE_COLOR]:walk(M.get_material_property_input_node(o,p))
  row['graph']=seen
(O/'MaterialGeometry.json').write_text(json.dumps(D,indent=2))
