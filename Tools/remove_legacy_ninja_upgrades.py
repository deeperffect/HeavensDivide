"""Retire old Ninja cards, preserve clones/tuning, and migrate Wide Orbit to growth.

Run with Unreal Python. -ValidateNinjaCleanup checks without writing.
"""
import hashlib,json,shutil
from datetime import datetime
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/retired_ninja_upgrades.json').read_text())
ids={r['id'] for r in rows};paths={r['path'] for r in rows}
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
growth_path='/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaWideOrbit'
validate='-ValidateNinjaCleanup' in unreal.SystemLibrary.get_command_line()
backup=root/'Saved/Backups/LegacyNinjaCleanup'/datetime.now().strftime('%Y%m%d_%H%M%S')
def disk(path):return root/'Content'/(path.removeprefix('/Game/').split('.')[0]+'.uasset')
def copy(path):
 src=disk(path);dest=backup/src.relative_to(root)
 if src.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
bp=unreal.load_asset(controller)
comp=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
retained=[a for a in comp.get_editor_property('upgrade_pool') if a and str(a.get_editor_property('upgrade_id')) not in ids]
hashes={str(disk(a.get_path_name())):hashlib.sha256(disk(a.get_path_name()).read_bytes()).hexdigest() for a in retained if str(a.get_editor_property('upgrade_id'))!='WideOrbit'}
growth=unreal.load_asset(growth_path)
if not validate:
 for path in paths:
  if unreal.EditorAssetLibrary.does_asset_exist(path):
   refs=set(str(r) for r in unreal.EditorAssetLibrary.find_package_referencers_for_asset(path,True))
   assert not refs-paths-{controller},(path,refs)
   copy(path)
 copy(controller);copy(growth_path)
 comp.set_editor_property('upgrade_pool',retained)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
 growth.set_editor_property('display_name','Growing Shuriken')
 growth.set_editor_property('description','The shuriken grows as it travels, reaching double size after 1000 cm.')
 balance=dict(growth.get_editor_property('balance_parameters'))
 balance.setdefault('GrowthDistance',1000.0);balance.setdefault('MaxSizeMultiplier',2.0)
 growth.set_editor_property('balance_parameters',balance)
 unreal.SystemLibrary.execute_console_command(None,'setnopec '+growth.get_path_name()+' bHasRuntimeBalance True')
 assert unreal.EditorAssetLibrary.save_loaded_asset(growth,False)
 # Clear references between retiring assets before deleting them.
 for path in paths:
  if unreal.EditorAssetLibrary.does_asset_exist(path):
   a=unreal.load_asset(path);a.set_editor_property('prerequisite_upgrade_ids',[]);a.set_editor_property('prerequisite_requirements',[])
   assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
 for path in paths:
  if unreal.EditorAssetLibrary.does_asset_exist(path):assert unreal.EditorAssetLibrary.delete_asset(path)
 assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items()),'Retained tuning changed'
 (backup/'retained_asset_hashes.json').write_text(json.dumps(hashes,indent=2))
actual={str(a.get_editor_property('upgrade_id')) for a in retained}
assert len(actual)==len(retained)==57 and not ids.intersection(actual)
assert {'ShadowStep','MultipleStrikes','AfterimageFrenzy'}.issubset(actual)
for a in retained:
 required={str(v) for v in a.get_editor_property('prerequisite_upgrade_ids')}|{str(v.get_editor_property('upgrade_id')) for v in a.get_editor_property('prerequisite_requirements')}
 assert required.issubset(actual),(a.get_path_name(),required-actual)
for path in paths:assert not unreal.EditorAssetLibrary.does_asset_exist(path),path
assert str(growth.get_editor_property('display_name'))=='Growing Shuriken'
unreal.log('NINJA_CLEANUP_OK: 12 retired cards removed; all 3 clone cards preserved; growth card updated; 57 unique cards; retained tuning unchanged')
