"""Apply Veshra pressure tuning, preserving every damage value and health budget."""
from pathlib import Path
from datetime import datetime
import json, shutil, unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved/BossSpellPressure'
out.mkdir(parents=True, exist_ok=True)
backup = out / 'Backups' / datetime.now().strftime('%Y%m%d_%H%M%S')
tuning = json.loads((root / 'Tools/boss_pressure_tuning.json').read_text())
bp_path = '/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle'
map_path = '/Game/Maps/Lvl_B1_Arena'
damage_fields = ['spell_damage', 'boss_base_damage', 'forward_attack_damage', 'aoe_damage',
                 'dash_damage', 'ground_circle_damage', 'phase2_forward_cleave_damage_multiplier',
                 'phase2_aoe_damage_multiplier']
protected = damage_fields + ['boss_max_health', 'phase_one_warning', 'phase_two_warning',
    'eclipse_warning', 'ground_duration', 'burst_active_time',
    'ground_count', 'guardians_per_cast', 'guardian_limit',
    'nova_safe_radius', 'eclipse_inner_radius']
assert not set(tuning).intersection(protected)

def preserve(path, extension):
    source = root / 'Content' / (path.removeprefix('/Game/') + extension)
    target = backup / source.relative_to(root)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)

horde_classes = [u.load_class(None, '/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/' + name + '.' + name + '_C') for name in ['BP_EnemyGrunt', 'BP_EnemyFloatingSkeleton']]
assert all(horde_classes)

def snapshot(obj):
    # The mixed-case native AoEDamage name is not exposed as aoe_damage in Python.
    native_names = {'aoe_damage': 'AoEDamage',
                    'phase2_aoe_damage_multiplier': 'Phase2AOEDamageMultiplier'}
    values = {key: obj.get_editor_property(native_names.get(key, key)) for key in [*tuning, *protected]}
    values['horde_classes'] = [c.get_path_name() for c in obj.get_editor_property('horde_classes')]
    scale = obj.get_component_by_class(u.SkeletalMeshComponent).get_editor_property('relative_scale3d')
    values['mesh_scale'] = [scale.x, scale.y, scale.z]
    values['health_component_max_health'] = obj.get_health_component().get_editor_property('max_health')
    return values

def tune(obj, before):
    obj.modify(True)
    obj.set_editor_property('horde_classes', horde_classes)
    for key, value in tuning.items():
        obj.set_editor_property(key, value)
    after = snapshot(obj)
    for key in [*protected, 'health_component_max_health', 'mesh_scale']:
        assert after[key] == before[key], (obj.get_path_name(), key, before[key], after[key])
    for key, expected in tuning.items():
        assert abs(after[key] - expected) < .001, (key, after[key], expected)
    return after

preserve(bp_path, '.uasset')
preserve(map_path, '.umap')
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert level.load_level(map_path)
placed = [a for a in actors.get_all_level_actors() if isinstance(a, u.FinalBossBase)]
assert len(placed) == 1 and isinstance(placed[0], u.CinderOracleBoss), 'Unexpected arena boss'
placed_before = snapshot(placed[0])
bp = u.load_asset(bp_path)
cdo = u.get_default_object(bp.generated_class())
cdo_before = snapshot(cdo)
tune(cdo, cdo_before)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
cdo_after = tune(u.get_default_object(bp.generated_class()), cdo_before)
placed = [a for a in actors.get_all_level_actors() if isinstance(a, u.CinderOracleBoss)]
assert len(placed) == 1
placed_after = tune(placed[0], placed_before)
assert level.save_current_level()
# Reload the saved arena to verify overrides survive level loading.
assert level.load_level('/Game/Maps/Lvl_B1_Lvl1')
assert level.load_level(map_path)
placed = [a for a in actors.get_all_level_actors() if isinstance(a, u.CinderOracleBoss)]
assert len(placed) == 1
reloaded = snapshot(placed[0])
assert reloaded == placed_after
report = {'blueprint': {'before': cdo_before, 'after': cdo_after},
          'arena': {'before': placed_before, 'after': reloaded},
          'damage_unchanged': True, 'size_unchanged': True, 'backup': str(backup)}
(out / 'TuningReport.json').write_text(json.dumps(report, indent=2))
baseline_path = root / 'Config/BloodshiftBalanceBaseline.json'
baseline = json.loads(baseline_path.read_text(encoding='utf-8-sig'))
shutil.copy2(baseline_path, backup / 'BloodshiftBalanceBaseline.json')
values = baseline.setdefault(u.get_default_object(bp.generated_class()).get_path_name(), {})
for key, value in tuning.items():
    native = 'bGroundFillsScreen' if key == 'ground_fills_screen' else ''.join(part.title() for part in key.split('_'))
    values[native] = ('True' if value else 'False') if isinstance(value, bool) else str(value)
values['HordeClasses'] = '(' + ','.join("BlueprintGeneratedClass'" + c.get_path_name() + "'" for c in horde_classes) + ')'
baseline_path.write_text(json.dumps(baseline, indent=2) + '\n')
u.log('BOSS_SPELL_PRESSURE_APPLIED')
