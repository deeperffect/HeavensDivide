"""Apply authored basic-mob waves to the Blueprint and survival map.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Use validate_enemy_waves.py in a fresh commandlet to check saved assets.
This replaces configure_enemy_pressure_v1.py as the current spawn setup.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path

import unreal

CONFIG = json.loads(Path(__file__).with_name('enemy_wave_schedule.json').read_text())
VALIDATE_ONLY = globals().get('VALIDATE_ONLY', False)
SAVED = Path(unreal.Paths.project_saved_dir()).resolve()
CONTENT = Path(unreal.Paths.project_content_dir()).resolve()


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def read(obj, name):
    return obj.get_editor_property(name)


def assign(obj, values):
    for name, value in values.items():
        obj.set_editor_property(name, value)
    return obj


def struct(kind, **values):
    return assign(kind(), values)


def validate_schedule():
    enemies, phases = CONFIG['enemies'], CONFIG['phases']
    check(len(enemies) == 10, 'Expected exactly ten survival enemies')
    seen = set()
    last_cap = 0
    for i, p in enumerate(phases):
        check(p['start_seconds'] == (phases[i - 1]['end_seconds'] if i else 0), 'Wave gap or overlap')
        check(p['end_seconds'] > p['start_seconds'] or (i == len(phases) - 1 and p['end_seconds'] == 0), 'Invalid wave end')
        check(enemies[p['basic']]['role'] == 'basic', 'Threat selected as basic wave')
        check(p['desired_population'] > 0, 'Empty basic wave')
        check(last_cap <= p['global_cap'] <= CONFIG['hard_cap'], 'Non-progressive or unreachable global cap')
        check(p['desired_population'] + sum(p['threat_caps'].values()) <= p['global_cap'], 'No room for threats')
        check(p['intervals'][0] >= p['intervals'][1] >= p['intervals'][2] > 0, 'Invalid refill cadence')
        seen.add(p['basic'])
        for name, cap in p['threat_caps'].items():
            check(enemies[name]['role'] == 'threat', 'Basic mob in threat list')
            if cap > 0 and (p['end_seconds'] == 0 or enemies[name]['unlock_seconds'] < p['end_seconds']):
                seen.add(name)
        last_cap = p['global_cap']
    check(seen == set(enemies), 'An enemy cannot spawn in any wave')
    # Every transition, including the exact boundary, must resolve to one wave.
    for p in phases:
        for second in [max(0, p['start_seconds'] - .001), p['start_seconds'], p['start_seconds'] + .001]:
            matches = [q for q in phases if q['start_seconds'] <= second and (q['end_seconds'] == 0 or second < q['end_seconds'])]
            check(len(matches) == 1, 'Ambiguous wave boundary')


def canonical(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (bool, int, str)) or value is None:
        return value
    if isinstance(value, float):
        return round(value, 5)
    return str(value)


ENTRY_FIELDS = 'enemy_class enabled minimum_run_time maximum_run_time pressure_spawn_mode health_scaling_per_minute min_respawn_delay_after_death max_respawn_delay_after_death'.split()
PHASE_FIELDS = 'phase_name start_time_seconds end_time_seconds global_max_alive normal_spawn_interval accelerated_spawn_interval emergency_spawn_interval events_enabled'.split()
POP_FIELDS = 'enemy_class desired_population max_population refill_priority'.split()


def snapshot(obj):
    result = {name: canonical(read(obj, name)) for name in SETTINGS if name not in ('enemy_spawn_entries', 'pressure_phases', 'pressure_events')}
    result['entries'] = [{n: canonical(read(e, n)) for n in ENTRY_FIELDS} for e in read(obj, 'enemy_spawn_entries')]
    result['phases'] = []
    for p in read(obj, 'pressure_phases'):
        row = {n: canonical(read(p, n)) for n in PHASE_FIELDS}
        row['population'] = [{n: canonical(read(e, n)) for n in POP_FIELDS} for e in read(p, 'enemy_population_entries')]
        result['phases'].append(row)
    result['event_count'] = len(read(obj, 'pressure_events'))
    return result


validate_schedule()
blueprints = {n: unreal.load_asset(e['path']) for n, e in CONFIG['enemies'].items()}
check(all(blueprints.values()), 'Missing enemy Blueprint')
classes = {n: bp.generated_class() for n, bp in blueprints.items()}
entries = []
for name, data in CONFIG['enemies'].items():
    entries.append(struct(unreal.EnemySpawnEntry,
        enemy_class=classes[name], enabled=True, spawn_weight=1.0,
        minimum_run_time=float(data['unlock_seconds']), maximum_run_time=0.0,
        spawn_cost=1, max_alive_of_this_type=0,
        health_scaling_per_minute=data['health_per_minute'],
        pressure_spawn_mode=(unreal.EnemyPressureSpawnMode.MAINTAIN_POPULATION if data['role'] == 'basic' else unreal.EnemyPressureSpawnMode.TIMED_THREAT),
        min_respawn_delay_after_death=float(data['respawn_delay'][0]),
        max_respawn_delay_after_death=float(data['respawn_delay'][1])))

phases = []
for p in CONFIG['phases']:
    population = [struct(unreal.EnemyPopulationPhaseEntry,
        enemy_class=classes[p['basic']], desired_population=p['desired_population'],
        max_population=p['desired_population'], refill_priority=1.0)]
    for name, cap in p['threat_caps'].items():
        population.append(struct(unreal.EnemyPopulationPhaseEntry,
            enemy_class=classes[name], desired_population=0, max_population=cap, refill_priority=1.0))
    phases.append(struct(unreal.EnemyPressurePhase,
        phase_name=p['name'], start_time_seconds=float(p['start_seconds']), end_time_seconds=float(p['end_seconds']),
        global_max_alive=p['global_cap'], normal_spawn_interval=p['intervals'][0],
        accelerated_spawn_interval=p['intervals'][1], emergency_spawn_interval=p['intervals'][2],
        enemy_population_entries=population, events_enabled=False))

SETTINGS = {
    'enemy_spawn_entries': entries, 'pressure_phases': phases, 'pressure_events': [],
    'absolute_hard_alive_cap': CONFIG['hard_cap'], 'normal_max_batch_size': 2,
    'accelerated_max_batch_size': 3, 'emergency_max_batch_size': 4,
    'timed_threat_spawn_slot_chance': .12, 'spawning_enabled': True,
    'directional_bias_enabled': True, 'directional_bias_chance': .55,
    # This existing recycler is class-specific. Its phase guard only allows
    # Grunt replacements while a Grunt wave is actually active.
    'spatial_pressure_grunt_class': classes['Grunt'],
}

spawner_bp = unreal.load_asset(CONFIG['spawner'])
check(spawner_bp is not None, 'Missing spawner Blueprint')
spawner_cdo = unreal.get_default_object(spawner_bp.generated_class())
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
check(levels.load_level(CONFIG['level']), 'Could not load survival level')
placed = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_class() == spawner_bp.generated_class()]
check(len(placed) == 1, 'Expected exactly one placed survival spawner')

if not VALIDATE_ONLY:
    backup = SAVED / 'Backups' / 'EnemyWaves' / datetime.now().strftime('%Y%m%d_%H%M%S')
    paths = [(CONFIG['spawner'], '.uasset'), (CONFIG['level'], '.umap')]
    paths += [(e['path'], '.uasset') for e in CONFIG['enemies'].values() if e['role'] == 'basic']
    for path, extension in paths:
        relative = Path(path.removeprefix('/Game/') + extension)
        target = backup / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(CONTENT / relative, target)
    (backup / 'spawner_before.json').write_text(json.dumps([snapshot(spawner_cdo), snapshot(placed[0])], indent=2))
    unreal.log('ENEMY_WAVES_BACKUP: ' + str(backup))

    for name, data in CONFIG['enemies'].items():
        if data['role'] != 'basic':
            continue
        bp = blueprints[name]
        cdo = unreal.get_default_object(bp.generated_class())
        hp = cdo.get_component_by_class(unreal.HealthComponent)
        cdo.modify()
        hp.modify()
        cdo.set_editor_property('move_speed', float(data['speed']))
        hp.set_editor_property('max_health', float(data['health']))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        check(unreal.EditorAssetLibrary.save_loaded_asset(bp, False), 'Could not save ' + name)

    assign(spawner_cdo, SETTINGS)
    unreal.BlueprintEditorLibrary.compile_blueprint(spawner_bp)
    check(unreal.EditorAssetLibrary.save_loaded_asset(spawner_bp, False), 'Could not save spawner defaults')
    # Compilation may reinstance actors. Reacquire the actual level instance.
    placed = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_class() == spawner_bp.generated_class()]
    check(len(placed) == 1, 'Placed spawner missing after compile')
    placed[0].modify()
    assign(placed[0], SETTINGS)
    check(levels.save_current_level(), 'Could not save survival level')

# Compare loaded data against separately constructed expected structs. The
# validation entry point repeats this in a new process to catch unsaved defaults.
expected = {name: canonical(v) for name, v in SETTINGS.items() if name not in ('enemy_spawn_entries', 'pressure_phases', 'pressure_events')}
expected['entries'] = [{n: canonical(read(e, n)) for n in ENTRY_FIELDS} for e in entries]
expected['phases'] = []
for p in phases:
    row = {n: canonical(read(p, n)) for n in PHASE_FIELDS}
    row['population'] = [{n: canonical(read(e, n)) for n in POP_FIELDS} for e in read(p, 'enemy_population_entries')]
    expected['phases'].append(row)
expected['event_count'] = 0
spawner_cdo = unreal.get_default_object(spawner_bp.generated_class())
for obj in [spawner_cdo] + placed:
    check(snapshot(obj) == expected, 'Saved spawner differs from authored schedule: ' + obj.get_path_name())

tuning = {}
for name, data in CONFIG['enemies'].items():
    cdo = unreal.get_default_object(blueprints[name].generated_class())
    hp = cdo.get_component_by_class(unreal.HealthComponent)
    tuning[name] = {'health': read(hp, 'max_health'), 'speed': read(cdo, 'move_speed')}
    if data['role'] == 'basic':
        check(tuning[name] == {'health': data['health'], 'speed': data['speed']}, 'Health/speed did not persist: ' + name)

report = {'validation_only': VALIDATE_ONLY, 'enemies': tuning, 'spawners_checked': [a.get_path_name() for a in [spawner_cdo] + placed], 'schedule': expected}
(SAVED / 'enemy_waves_validation.json').write_text(json.dumps(report, indent=2))
unreal.log('ENEMY_WAVES_OK: 10 enemies, 12 ordered waves, Blueprint and placed spawner match; health/speed verified')
