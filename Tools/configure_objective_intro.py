"""Persist the Blood Shrine's 30-second challenge in its saved Blueprint.

Run with UnrealEditor-Cmd -run=pythonscript -script=Tools/configure_objective_intro.py
and PythonScriptPlugin enabled. Existing mesh and balance settings are preserved.
"""
import json
import shutil
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
relative = Path('HeavensDivide/Blueprints/Objectives/BP_BloodShrine.uasset')
backup = root / 'Saved/ObjectiveIntro/Backup' / relative
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(root / 'Content' / relative, backup)

asset = '/Game/HeavensDivide/Blueprints/Objectives/BP_BloodShrine'
blueprint = u.load_asset(asset)
assert blueprint, asset
defaults = u.get_default_object(blueprint.generated_class())
defaults.set_editor_property('challenge_duration', 30.0)
defaults.set_editor_property('required_blood', 30)
u.BlueprintEditorLibrary.compile_blueprint(blueprint)
assert u.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
defaults = u.get_default_object(blueprint.generated_class())
assert defaults.get_editor_property('challenge_duration') == 30.0
assert defaults.get_editor_property('required_blood') == 30
baseline_path = root / 'Config/BloodshiftBalanceBaseline.json'
baseline = json.loads(baseline_path.read_text(encoding='utf-8'))
key = asset + '.Default__BP_BloodShrine_C'
if key in baseline:
    baseline[key]['ChallengeDuration'] = '30.000000'
    baseline[key]['RequiredBlood'] = '30'
    baseline_path.write_text(json.dumps(baseline, indent=2) + '\n', encoding='utf-8')
mesh = defaults.get_editor_property('shrine_mesh')
report = {'duration': 30, 'bloodbound_kills': 30,
          'shrine_mesh': mesh.get_editor_property('static_mesh').get_path_name(),
          'shrine_scale': str(mesh.get_editor_property('relative_scale3d')),
          'shrine_location': str(mesh.get_editor_property('relative_location'))}
(root / 'Saved/ObjectiveIntro/Configuration.json').write_text(json.dumps(report, indent=2))
u.log('OBJECTIVE_INTRO_CONFIGURED ' + json.dumps(report))
