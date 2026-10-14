"""Configure level-up VFX with backups; joint combo VFX live in their data assets."""
import unreal as u
from pathlib import Path
from datetime import datetime
import shutil
root=Path(u.Paths.project_dir()).resolve()
backup=root/'Saved/Backups/LevelUpAndSamuraiVFX'/datetime.now().strftime('%Y%m%d_%H%M%S')
def before(a):
 src=root/'Content'/(a.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset')
 dst=backup/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def save(bp):
 u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController');before(bp)
cdo=u.get_default_object(bp.generated_class())
fx=u.load_asset('/Game/Assets/VFX/VerticalBeamsV1/Particles/NiagaraSystems/NS_VerticalBeam22_Cosmic');assert isinstance(fx,u.NiagaraSystem)
cdo.set_editor_property('level_up_vfx',fx);save(bp)
u.log('LEVEL_UP_VFX_OK')
