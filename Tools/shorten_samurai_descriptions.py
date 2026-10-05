"""Compatibility entry point for current upgrade copy; audit mode remains read-only."""
import json
import runpy
from pathlib import Path
import unreal
root = Path(unreal.Paths.project_dir()).resolve()
if '-AuditSamuraiText' in unreal.SystemLibrary.get_command_line():
    bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
    pool = unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool')
    rows = [dict(id=str(a.get_editor_property('upgrade_id')), name=str(a.get_editor_property('display_name')), description=str(a.get_editor_property('description')), format=str(a.get_editor_property('rolled_description_format'))) for a in pool if a and '/Samurai/' in a.get_path_name()]
    out = root / 'Saved/SamuraiCardTextAudit.json'
    out.write_text(json.dumps(rows, indent=2), encoding='utf-8')
    unreal.log('SAMURAI_TEXT_AUDIT: ' + str(out))
else:
    runpy.run_path(str(root / 'Tools/publish_upgrade_copy.py'))
