"""Migrate stance balance caps/text without changing tuning. -ValidateSamuraiBalance is read-only."""
import ast
from datetime import datetime
import hashlib
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
validate = '-ValidateSamuraiBalance' in unreal.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/SamuraiBalance' / datetime.now().strftime('%Y%m%d_%H%M%S')
changed = {
    'DoubleCutFrequency', 'IaijutsuDoubleCutFrequency', 'CrescentDoubleCutFrequency',
    'IaijutsuMarkDamage', 'IaijutsuMarkPact', 'IaijutsuDashPact',
    'CrescentSlow', 'CrescentEruptionPact', 'CrescentFieldPower',
}
# Read the authoring rows as data without executing the broad authoring workflows.
expected = {}
for filename in ['overhaul_blood_stance.py', 'overhaul_iaijutsu.py', 'overhaul_crescent.py']:
    tree = ast.parse((root / 'Tools' / filename).read_text(encoding='utf-8'))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'rows' for t in node.targets):
            for row in ast.literal_eval(node.value):
                if row[0] in changed:
                    expected[row[0]] = (row[2], row[-1])
assert set(expected) == changed

bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
pool = list(unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component').get_editor_property('upgrade_pool'))
cards = {str(card.get_editor_property('upgrade_id')): card for card in pool if card}
assert len(cards) == len(pool) == 106

def disk(card):
    return root / 'Content' / (card.get_path_name().removeprefix('/Game/').split('.')[0] + '.uasset')

preserved = {disk(card): hashlib.sha256(disk(card).read_bytes()).hexdigest()
             for uid, card in cards.items() if uid not in changed}
for uid, (cap, description) in expected.items():
    card = cards[uid]
    if not validate:
        source = disk(card)
        destination = backup / source.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        card.set_editor_property('max_level', cap)
        card.set_editor_property('description', description)
        assert unreal.EditorAssetLibrary.save_loaded_asset(card, False)
    assert card.get_editor_property('max_level') == cap, uid
    assert str(card.get_editor_property('description')) == description, uid
for path, digest in preserved.items():
    assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Unrelated asset changed: ' + str(path)
if not validate:
    unreal.log('SAMURAI_BALANCE_BACKUP: ' + str(backup))
unreal.log('SAMURAI_BALANCE_OK: Three-rank frequency caps and tradeoff/field descriptions verified; unrelated cards preserved')
