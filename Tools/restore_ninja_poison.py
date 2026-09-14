"""Restore only Venomous Kunai from the cleanup backup, preserving other tuning."""
import hashlib,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
relative=Path('Content/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaVenomousKunai.uasset')
path='/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaVenomousKunai'
if not (root/relative).exists():
 backups=sorted((root/'Saved/Backups/LegacyNinjaCleanup').glob('*/'+relative.as_posix()))
 assert backups,'Poison card backup missing'
 shutil.copy2(backups[-1],root/relative)
 unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/HeavensDivide/Upgrades/Ninja'],True)
card=unreal.load_asset(path);assert card
controller=unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
comp=unreal.get_default_object(controller.generated_class()).get_editor_property('player_upgrade_component')
pool=list(comp.get_editor_property('upgrade_pool'))
def disk(a):return root/'Content'/(a.get_path_name().removeprefix('/Game/').split('.')[0]+'.uasset')
hashes={disk(a):hashlib.sha256(disk(a).read_bytes()).hexdigest() for a in pool if a!=card}
if not any(str(a.get_editor_property('upgrade_id'))=='VenomousKunai' for a in pool):pool.append(card)
card.set_editor_property('description','Ninja attacks apply Poison.')
assert unreal.EditorAssetLibrary.save_loaded_asset(card,False)
comp.set_editor_property('upgrade_pool',pool)
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
assert unreal.EditorAssetLibrary.save_loaded_asset(controller,False)
assert len(pool)==len({str(a.get_editor_property('upgrade_id')) for a in pool})==57
assert all(hashlib.sha256(p.read_bytes()).hexdigest()==h for p,h in hashes.items())
unreal.log('NINJA_POISON_RESTORED: 57 cards; other card tuning unchanged')
