"""Apply this pass's JSON settings to its instance, keeping a dated backup."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
source=root/'Content/HeavensDivide/Materials/ConnectedGround/MI_ConnectedGround.uasset'
backup=root/'Saved/ConnectedGround/Backups'/datetime.now().strftime('%Y%m%d_%H%M%S_Tone')/source.relative_to(root)
backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,backup)
lib=u.MaterialEditingLibrary
mat=u.load_asset('/Game/HeavensDivide/Materials/ConnectedGround/MI_ConnectedGround')
config=json.loads((root/'Tools/connected_ground.json').read_text())
for name,value in config.items():
    if isinstance(value,list):lib.set_material_instance_vector_parameter_value(mat,name,u.LinearColor(*value))
    else:lib.set_material_instance_scalar_parameter_value(mat,name,float(value))
lib.update_material_instance(mat)
assert u.EditorAssetLibrary.save_loaded_asset(mat,False)
u.log('CONNECTED_GROUND_TUNED '+str(backup))
