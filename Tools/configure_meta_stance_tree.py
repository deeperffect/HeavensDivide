"""Install the stance-aware skill catalog after compiling. Preserves other balance rules.
Run through Unreal Python; never opens or edits the player's progression save.
"""
from pathlib import Path
from datetime import datetime
import json,shutil,unreal as u
root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/ProgressionExpansion';out.mkdir(parents=True,exist_ok=True)
asset=u.load_asset('/Game/HeavensDivide/Balance/DA_GlobalBalance');assert asset
source=root/'Content/HeavensDivide/Balance/DA_GlobalBalance.uasset'
backup=out/'AssetBackups'/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir(parents=True,exist_ok=True)
shutil.copy2(source,backup/source.name)
properties=['cadence_upgrade_rank_cap','max_active_hazards','minimum_warning','persistent_damage_interval','hazard_vertical_tolerance','player_collision_padding_fraction','seconds_per_ember','reward_time_cap','victory_embers']
original={p:asset.get_editor_property(p) for p in properties}
def describe(node):
 return {k:str(node.get_editor_property(k)) for k in ['id','name','description','effect','required_stance','page','target','map_position','max_rank','base_cost','per_rank','passive_effect']}
previous=[describe(n) for n in asset.get_editor_property('skills')]
defaults=list(u.get_default_object(u.BloodshiftBalanceData).get_editor_property('skills'))
assert len(defaults)==45,'Compile the redesigned catalog before applying'
asset.modify(True);asset.set_editor_property('skills',defaults)
assert all(asset.get_editor_property(p)==v for p,v in original.items())
assert u.EditorAssetLibrary.save_loaded_asset(asset,False)
result={'backup':str(backup),'before':previous,'after':[describe(n) for n in asset.get_editor_property('skills')],'other_rules_preserved':original}
(out/'CatalogMigration.json').write_text(json.dumps(result,indent=2))
u.log('META_STANCE_CATALOG_INSTALLED 45 skills; non-skill balance rules preserved')
