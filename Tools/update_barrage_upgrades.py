"""Replace Crossfire with Forking Projectiles and update Crescendo, preserving other tuning."""
import unreal,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve();backup=root/'Saved/Backups/BarrageRevision'/datetime.now().strftime('%Y%m%d_%H%M%S')
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
cross='/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaCrossfire'
crescendo='/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaCrescendo'
def disk(path):return root/'Content'/(path.removeprefix('/Game/').split('.')[0]+'.uasset')
def copy(path):
 src=disk(path);dest=backup/src.relative_to(root)
 if src.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
bp=unreal.load_asset(controller);comp=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool=list(comp.get_editor_property('upgrade_pool'))
hashes={str(disk(a.get_path_name())):hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest() for a in pool if str(a.get_editor_property('upgrade_id')) not in {'Crossfire','Crescendo'}}
for path in [controller,cross,crescendo]:copy(path)
if unreal.EditorAssetLibrary.does_asset_exist(cross):
 refs=set(str(r) for r in unreal.EditorAssetLibrary.find_package_referencers_for_asset(cross,True));assert not refs-{controller},refs
comp.set_editor_property('upgrade_pool',[a for a in pool if str(a.get_editor_property('upgrade_id'))!='Crossfire'])
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
if unreal.EditorAssetLibrary.does_asset_exist(cross):assert unreal.EditorAssetLibrary.delete_asset(cross)
# Author missing new card with the standard catalog workflow. Existing cards are not rewritten.
exec(compile((root/'Tools/configure_ninja_builds.py').read_text(),'configure_ninja_builds.py','exec'),{})
a=unreal.load_asset(crescendo);a.set_editor_property('max_level',5)
a.set_editor_property('description','Every 3 attacks add 1 temporary projectile. At the cap, the next attack restarts the buildup. Cap: 5, +2 per extra rank.')
balance=dict(a.get_editor_property('balance_parameters'))
for k,v in {'BaseCap':5.0,'CapPerRank':2.0,'AttacksPerProjectile':3.0}.items():
 if not any(str(key)==k for key in balance):balance[k]=v
a.set_editor_property('balance_parameters',balance)
unreal.SystemLibrary.execute_console_command(None,'setnopec '+a.get_path_name()+' bHasRuntimeBalance True')
assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
(backup/'preserved_hashes.json').write_text(json.dumps(hashes,indent=2))
unreal.log('BARRAGE_REVISION_OK: Crossfire removed; Forking added; Crescendo has 5 ranks; other tuning preserved')
