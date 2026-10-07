"""Replace level-up VFX and restore authored Samurai ability presentation, with backups."""
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
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai');before(bp)
cdo=u.get_default_object(bp.generated_class());settings=cdo.get_editor_property('combo_ability')
original=settings.get_editor_property('vfx');assert original
settings.set_editor_property('attach_vfx_to_character',False)
settings.set_editor_property('spawn_vfx_every_pulse',False)
settings.set_editor_property('vfx_radius_parameter','None')
settings.set_editor_property('vfx_radius_parameter_is_scale',False)
settings.set_editor_property('vfx_duration_parameter','None')
settings.set_editor_property('vfx_scale',u.Vector(1,1,1))
settings.set_editor_property('vfx_visibility_duration',0.0)
cdo.set_editor_property('combo_ability',settings);save(bp)
assert settings.get_editor_property('vfx')==original
u.log('LEVEL_UP_SAMURAI_VFX_OK '+settings.export_text())
