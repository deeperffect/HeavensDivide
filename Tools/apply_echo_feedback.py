"""Persist the inspected Echoing Slash timing and its independent blue tint."""
from pathlib import Path
from datetime import datetime
import json, shutil
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/FeedbackPolish';out.mkdir(parents=True,exist_ok=True)
asset='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai'
source=root/'Content'/('HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.uasset')
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True,exist_ok=True);shutil.copy2(source,backup/source.name)
bp=u.load_asset(asset)
attack=u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
old_delay=attack.get_editor_property('blood_echo_delay')
attack.set_editor_property('blood_echo_delay',.25)
attack.set_editor_property('blood_echo_color',u.LinearColor(.025,.20,1.,1.))
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
attack=u.get_default_object(bp.generated_class()).get_component_by_class(u.AutoAttackComponent)
assert abs(attack.get_editor_property('blood_echo_delay')-.25)<.0001
(out/'Applied.json').write_text(json.dumps({'previous_loaded_delay':old_delay,'delay':.25,
    'increase_from_inspected_delay':.1,'color':[.025,.20,1.,1.],
    'effect':attack.get_editor_property('blood_echo_vfx').get_path_name(),'backup':str(backup)},indent=2))
u.log('ECHO_FEEDBACK_SAVED')
