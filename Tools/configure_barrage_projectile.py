"""Assign the project-owned Barrage effect; preserve the source system and back up the Blueprint."""
import shutil
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir())
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_NinjaProjectile')
fx=u.load_asset('/Game/HeavensDivide/VFX/Stances/NS_BarrageProjectile')
assert bp and isinstance(fx,u.NiagaraSystem)
source=root/'Content/HeavensDivide/Blueprints/PlayerCharacters/BP_NinjaProjectile.uasset'
backup=root/'Saved/Backups/SteamBarrage/BP_NinjaProjectile.uasset'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(source,backup)
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
count=0
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(u.SubobjectDataBlueprintFunctionLibrary.get_data(h),bp)
    if isinstance(obj,u.NiagaraComponent):
        obj.set_editor_property('asset',fx)
        count+=1
assert count==1,count
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
u.log('BARRAGE_PROJECTILE_ASSIGNED_OK')
