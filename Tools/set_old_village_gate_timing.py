"""Set the authored boss gate to ten minutes without rebuilding either map."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
backup = root / 'Saved/OldVillage/GateTimingBackups' / datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True, exist_ok=True)
maps = ['Lvl_OldVillage', 'Lvl_OldVillage_Blockout']
bp_path = '/Game/HeavensDivide/Blueprints/BP_BossToriiGate'
for rel in ['Content/HeavensDivide/Blueprints/BP_BossToriiGate.uasset'] + ['Content/Maps/'+m+'.umap' for m in maps]:
    shutil.copy2(root / rel, backup / Path(rel).name)

level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
report = {'seconds': 600, 'maps': {}, 'backup': str(backup)}
bp = u.load_asset(bp_path)
cdo = u.get_default_object(bp.generated_class())
report['blueprint_before'] = cdo.get_editor_property('unlock_run_time_seconds')
cdo.set_editor_property('unlock_run_time_seconds', 600.)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)

for name in maps:
    assert level.load_level('/Game/Maps/' + name)
    gates = [a for a in actors.get_all_level_actors() if isinstance(a, u.BossToriiGate)]
    assert len(gates) == 1, name
    gate = gates[0]
    report['maps'][name] = {'before': gate.get_editor_property('unlock_run_time_seconds')}
    gate.set_editor_property('unlock_run_time_seconds', 600.)
    assert level.save_current_level()

# Reload the saved packages, so assertions check disk values rather than dirty actors.
assert level.load_level('/Engine/Maps/Entry')
for name in maps:
    assert level.load_level('/Game/Maps/' + name)
    gate = next(a for a in actors.get_all_level_actors() if isinstance(a, u.BossToriiGate))
    seconds = gate.get_editor_property('unlock_run_time_seconds')
    assert seconds == 600., (name, seconds)
    report['maps'][name]['saved_seconds'] = seconds
assert u.get_default_object(bp.generated_class()).get_editor_property('unlock_run_time_seconds') == 600.
(root / 'Saved/OldVillage/GateTiming.json').write_text(json.dumps(report, indent=2))
u.log('OLD_VILLAGE_GATE_TIMING_SAVED ' + json.dumps(report))
