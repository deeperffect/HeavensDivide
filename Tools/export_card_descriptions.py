"""Export saved upgrade copy and rarity variants for the card copy audit."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
component = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool = {a.get_path_name() for a in component.get_editor_property('upgrade_pool') if a}
rows = []
for path in unreal.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Upgrades', recursive=True):
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.UpgradeDefinition):
        continue
    rows.append(dict(path=asset.get_path_name(), active=asset.get_path_name() in pool,
        id=str(asset.get_editor_property('upgrade_id')),
        description=str(asset.get_editor_property('description')),
        format=str(asset.get_editor_property('rolled_description_format')),
        variants=[dict(magnitude=v.get_editor_property('magnitude'), description=str(v.get_editor_property('description_override')))
                  for v in asset.get_editor_property('rarity_magnitudes')]))
(root / 'Saved/CardDescriptionsBefore.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
unreal.log('CARD_COPY_EXPORT_OK: ' + str(len(rows)))
