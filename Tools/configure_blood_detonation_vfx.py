"""Assign the requested Cascade effect; preserve timing and all unrelated tuning."""
import unreal as u
from pathlib import Path
from datetime import datetime
import shutil
root=Path(u.Paths.project_dir()).resolve()
backup=root/'Saved/Backups/BloodDetonationVFX'/datetime.now().strftime('%Y%m%d_%H%M%S')
def before(a):
 src=root/'Content'/(a.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset')
 dst=backup/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai');before(bp)
component=u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
fx=u.load_asset('/Game/Assets/VFX/MagicProjectilesVol4/AoE_MagicAbilities/Particles/P_AoE_BloodBall01_BloodDetonation')
assert isinstance(fx,u.ParticleSystem)
component.set_editor_property('blood_detonation_vfx',fx)
u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
card=u.load_asset('/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiBloodDetonation');before(card)
p=card.get_editor_property('presentation');p.set_editor_property('pulse_system',None);card.set_editor_property('presentation',p)
assert u.EditorAssetLibrary.save_loaded_asset(card,False)
u.log('BLOOD_DETONATION_VFX_OK: '+fx.get_path_name()+' Delay='+str(component.get_editor_property('blood_detonation_explosion_delay')))
# Read-only export for checking Cascade lifetime/looping during authoring.
task=u.AssetExportTask();task.object=fx;task.filename=str(root/'Saved/StanceVFXAudit/BloodDetonationCascade.copy');task.automated=True;task.prompt=False
u.Exporter.run_asset_export_task(task)
