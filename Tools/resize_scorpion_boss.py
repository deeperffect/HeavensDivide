"""Save the scorpion at 2x its initial size and export its saved ability settings.

Run through Unreal Editor Python with the project editor closed. Backs up the
Blueprint/arena, edits only mesh size/offset and capsule size/floor placement,
then reloads the saved assets to check geometry and unchanged combat settings.
"""
from datetime import datetime
from pathlib import Path
import json
import re
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/ScorpionBoss/SizeCorrection'
OUT.mkdir(parents=True, exist_ok=True)
BP = '/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle'
MAP = '/Game/Maps/Lvl_B1_Arena'
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)


def snake(name):
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()


def numeric_fields(header):
    source = (ROOT / 'Source/HeavensDivide' / header).read_text()
    return [snake(name) for name in re.findall(
        r'UPROPERTY\(Edit(?:Anywhere|DefaultsOnly)[^\n]*\)\s*(?:float|int32)\s+(\w+)\s*=', source)]


ORACLE_FIELDS = numeric_fields('CinderOracleBoss.h')
PROTECTED = ORACLE_FIELDS + [
    'boss_max_health', 'boss_move_speed', 'boss_base_damage',
    'forward_attack_damage', 'AoEDamage', 'dash_damage', 'ground_circle_damage',
    'phase2_health_threshold', 'pushback_multiplier', 'xp_reward',
    'guardian_class', 'oracle_cast_montage', 'death_montage', 'boss_display_name',
]


def value(item):
    if isinstance(item, u.Object):
        return item.get_path_name()
    if item is None or isinstance(item, (bool, int, float, str)):
        return item
    return str(item)


def snapshot(actor):
    data = {field: value(actor.get_editor_property(field)) for field in PROTECTED}
    data['health_component_max_health'] = actor.get_health_component().get_editor_property('max_health')
    return data


def geometry(actor):
    mesh = actor.get_component_by_class(u.SkeletalMeshComponent)
    capsule = actor.get_component_by_class(u.CapsuleComponent)
    scale = mesh.get_editor_property('relative_scale3d')
    location = mesh.get_editor_property('relative_location')
    return dict(mesh_scale=[scale.x, scale.y, scale.z],
                mesh_location=[location.x, location.y, location.z],
                capsule_radius=capsule.get_unscaled_capsule_radius(),
                capsule_half_height=capsule.get_unscaled_capsule_half_height())


def resize(actor):
    actor.modify(True)
    mesh = actor.get_component_by_class(u.SkeletalMeshComponent)
    capsule = actor.get_component_by_class(u.CapsuleComponent)
    mesh.modify(True)
    capsule.modify(True)
    capsule.set_capsule_size(300, 400, False)
    location = mesh.get_editor_property('relative_location')
    mesh.set_editor_property('relative_location', u.Vector(location.x, location.y, -400))
    mesh.set_editor_property('relative_scale3d', u.Vector(1.30, 1.30, 1.30))


def verify_size(actor):
    data = geometry(actor)
    assert all(abs(component - 1.30) < .001 for component in data['mesh_scale']), data
    assert abs(data['mesh_location'][2] + 400) < .001, data
    assert abs(data['capsule_radius'] - 300) < .001, data
    assert abs(data['capsule_half_height'] - 400) < .001, data


assert len(ORACLE_FIELDS) >= 41, ORACLE_FIELDS
assert LEVEL.load_level(MAP)
placed = [actor for actor in ACTORS.get_all_level_actors() if isinstance(actor, u.CinderOracleBoss)]
assert len(placed) == 1
boss = placed[0]
bp = u.load_asset(BP)
cdo = u.get_default_object(bp.generated_class())
before_arena, before_cdo = snapshot(boss), snapshot(cdo)
before_geometry = dict(arena=geometry(boss), blueprint=geometry(cdo))
actor_scale = boss.get_actor_scale3d()

backup = OUT / 'Backups' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
for relative in [BP.removeprefix('/Game/') + '.uasset', MAP.removeprefix('/Game/') + '.umap']:
    source = ROOT / 'Content' / relative
    destination = backup / 'Content' / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)

resize(cdo)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert 'ERROR' not in str(bp.get_editor_property('status'))
assert snapshot(u.get_default_object(bp.generated_class())) == before_cdo
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
boss = next(actor for actor in ACTORS.get_all_level_actors() if isinstance(actor, u.CinderOracleBoss))
resize(boss)
floor = next(actor for actor in ACTORS.get_all_level_actors() if actor.get_actor_label() == 'Plane')
location = boss.get_actor_location()
location.z = floor.get_actor_location().z + boss.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
boss.set_actor_location(location, False, False)
assert boss.get_actor_scale3d() == actor_scale
assert snapshot(boss) == before_arena
assert LEVEL.save_current_level()

assert LEVEL.load_level('/Game/Maps/Lvl_B1_Lvl1')
assert LEVEL.load_level(MAP)
boss = next(actor for actor in ACTORS.get_all_level_actors() if isinstance(actor, u.CinderOracleBoss))
cdo = u.get_default_object(u.load_asset(BP).generated_class())
verify_size(boss)
verify_size(cdo)
assert snapshot(boss) == before_arena and snapshot(cdo) == before_cdo
assert boss.get_actor_scale3d() == actor_scale
floor = next(actor for actor in ACTORS.get_all_level_actors() if actor.get_actor_label() == 'Plane')
foot_z = boss.get_actor_location().z - boss.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
assert abs(foot_z - floor.get_actor_location().z) < .01

guardian_class = boss.get_editor_property('guardian_class')
guardian = u.get_default_object(guardian_class) if guardian_class else None
guardian_settings = {field: value(guardian.get_editor_property(field))
                     for field in numeric_fields('TacticalEnemy.h')} if guardian else {}
if guardian:
    guardian_settings.update(tactical_role=str(guardian.get_editor_property('tactical_role')),
                             health=guardian.get_health_component().get_editor_property('max_health'))
balance = u.load_asset('/Game/HeavensDivide/Balance/DA_GlobalBalance')
hazard_settings = {field: balance.get_editor_property(field) for field in [
    'max_active_hazards', 'minimum_warning', 'persistent_damage_interval',
    'hazard_vertical_tolerance', 'player_collision_padding_fraction']}
report = dict(passed=True, backup=str(backup), size_multiplier_since_initial_rig=2,
              before=before_geometry, after=dict(arena=geometry(boss), blueprint=geometry(cdo)),
              actor_scale=[actor_scale.x, actor_scale.y, actor_scale.z], floor_aligned=True,
              unchanged_combat=True, blueprint_settings=snapshot(cdo), arena_settings=snapshot(boss),
              guardian_settings=guardian_settings, hazard_settings=hazard_settings)
(OUT / 'Verification.json').write_text(json.dumps(report, indent=2))
u.log('SCORPION_SIZE_CORRECTION_VERIFIED ' + json.dumps(report))
