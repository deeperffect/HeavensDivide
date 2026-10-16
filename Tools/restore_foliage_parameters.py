"""Keep source shader divisors; disable only actual wind animation controls."""
from pathlib import Path
import json,hashlib,sys,unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
import repair_map_scenery as r
D=json.loads((r.OUT/'Inventory.json').read_text());changed=[]
for path,data in D['materials'].items():
 if data['class']!='MaterialInstanceConstant':continue
 name=path.rsplit('/',1)[-1].split('.')[0]
 target=r.DEST+'/Materials/'+name+'_'+hashlib.sha1(path.encode()).hexdigest()[:6]+'_Stationary'
 if not r.E.does_asset_exist(target):continue
 source=u.load_asset(path);m=u.load_asset(target)
 for n in r.M.get_scalar_parameter_names(source):
  value=r.M.get_material_instance_scalar_parameter_value(source,n)
  if any(t in str(n).lower().replace(' ','') for t in ['windintensity','windweight','windspeed']):value=0.
  r.M.set_material_instance_scalar_parameter_value(m,n,value)
 r.E.set_metadata_tag(m,'SceneryFoliageSource',path);r.M.update_material_instance(m);r.E.save_loaded_asset(m,False);changed.append(target)
(r.OUT/'FoliageParameters.json').write_text(json.dumps({'restored_instances':changed},indent=2))
