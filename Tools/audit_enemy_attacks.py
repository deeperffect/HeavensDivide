"""Read every enemy Blueprint's saved attack family and tuning; never saves assets."""
import json
from pathlib import Path
import unreal as u

registry = u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
rows = []
for data in registry.get_assets_by_path('/Game', recursive=True):
    if str(data.asset_class_path.asset_name) != 'Blueprint':
        continue
    # Generated-class ancestry is available without loading unrelated Blueprints.
    native_parent = str(data.get_tag_value('NativeParentClass'))
    if not any(name in native_parent for name in ('Enemy', 'Boss')):
        continue
    bp = data.get_asset()
    cls = bp.generated_class() if bp else None
    cdo = u.get_default_object(cls) if cls else None
    if not isinstance(cdo, u.EnemyBase):
        continue
    row = {'path': bp.get_path_name(), 'native_parent': native_parent}
    for prop in ('move_speed', 'attack_range', 'attack_damage', 'attack_interval',
                 'attack_windup', 'attack_recovery', 'attack_hit_radius',
                 'attack_hit_forward_offset', 'use_procedural_attack_motion',
                 'attack_lunge_distance', 'attack_lunge_out_duration',
                 'attack_lunge_return_duration', 'attack_wobble_angle',
                 'attack_montage', 'contact_damage', 'contact_damage_radius',
                 'contact_damage_interval', 'tactical_role', 'windup',
                 'ability_cooldown', 'cast_montage', 'charge_duration'):
        try:
            value = cdo.get_editor_property(prop)
        except Exception:
            continue
        row[prop] = value.get_path_name() if isinstance(value, u.Object) else (
            value if value is None or isinstance(value, (str, int, float, bool)) else str(value))
    mesh = cdo.get_component_by_class(u.SkeletalMeshComponent)
    anim_class = mesh.get_editor_property('anim_class') if mesh else None
    row['animation'] = anim_class.get_path_name() if anim_class else None
    if isinstance(cdo, u.GoblinBombEnemy):
        family = 'bomb_fuse'
    elif isinstance(cdo, u.TankMeleeEnemyBase):
        family = 'montage_slam' if row.get('attack_montage') else 'contact_aura'
    elif isinstance(cdo, u.MontageMeleeEnemyBase):
        family = 'montage_melee'
    elif isinstance(cdo, u.MeleeEnemyBase):
        family = 'basic_melee'
    elif isinstance(cdo, u.TacticalEnemy):
        family = 'tactical_ability'
    elif isinstance(cdo, u.RangedEnemyBase):
        family = 'ranged_projectile'
    else:
        family = 'boss'
    row['family'] = family
    rows.append(row)
rows.sort(key=lambda row: row['path'])
out = Path(u.Paths.project_saved_dir()) / 'EnemyAttackAudit.json'
out.write_text(json.dumps(rows, indent=2), encoding='utf-8')
for row in rows:
    u.log('ENEMY_ATTACK_AUDIT ' + json.dumps(row))
u.log('ENEMY_ATTACK_AUDIT_COMPLETE count=' + str(len(rows)))
