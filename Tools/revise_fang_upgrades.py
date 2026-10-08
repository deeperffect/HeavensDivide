"""Targeted Fang revision; preserve unrelated tuning, pool order and user presentation assets.

Run after compiling. -ValidateFangRevision checks saved assets without writing.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
validate = '-ValidateFangRevision' in u.SystemLibrary.get_command_line()
backup = root / 'Saved/Backups/FangRevision' / datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
folder = '/Game/HeavensDivide/Upgrades/Ninja/'
montages = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/'
controller = '/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
ninja_path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja'
rows = {r['id']: r for r in json.loads((root / 'Tools/fang_build_upgrades.json').read_text())}
copy = json.loads((root / 'Tools/card_descriptions.json').read_text())


def disk(path):
    return root / 'Content' / (path.split('.')[0].removeprefix('/Game/') + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def back_up(path):
    src = disk(path)
    if src.exists():
        dest = backup / src.relative_to(root)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)


def props(obj, **values):
    for key, value in values.items():
        obj.set_editor_property(key, value)


def card_path(uid):
    return folder + 'DA_Upgrade_Ninja' + uid


changed_ids = ['FangKillingEdge', 'FangFarstrider', 'FangPursuitChance', 'FangBurstPower']
allowed = {disk(card_path(uid)) for uid in changed_ids} | {disk(controller), disk(ninja_path), disk(montages + 'AM_FangAttack_Left')}
protected = {str(p): digest(p) for p in (root / 'Content/HeavensDivide').rglob('*.uasset') if validate or p not in allowed}
bp = u.load_asset(controller)
comp = u.get_default_object(bp.generated_class()).get_component_by_class(u.PlayerUpgradeComponent)
pool = list(comp.get_editor_property('upgrade_pool'))
old_pool = [a.get_path_name() for a in pool if a]
by_id = {str(a.get_editor_property('upgrade_id')): a for a in pool if a}
assert 'FangResonantReach' in by_id and 'FangResonance' in by_id

if not validate:
    for path in [controller, ninja_path, montages + 'AM_FangAttack_Left'] + [card_path(uid) for uid in changed_ids]:
        back_up(path)
    for uid in changed_ids:
        row = rows[uid]
        a = u.load_asset(card_path(uid)) if u.EditorAssetLibrary.does_asset_exist(card_path(uid)) else None
        if not a:
            factory = u.DataAssetFactory()
            factory.set_editor_property('data_asset_class', u.UpgradeDefinition)
            a = u.AssetToolsHelpers.get_asset_tools().create_asset('DA_Upgrade_Ninja' + uid, folder.rstrip('/'), u.UpgradeDefinition, factory)
            parent = by_id[row['art']]
            props(a, icon=parent.get_editor_property('icon'), card_artwork=parent.get_editor_property('card_artwork'))
        if uid == 'FangFarstrider':
            balance = {str(k): v for k, v in a.get_editor_property('balance_parameters').items()}
            balance['MaxDamageBonus'] = 4.0
            props(a, balance_parameters=balance)
        else:
            props(a, upgrade_id=uid, max_level=row['levels'],
                  category=u.UpgradeCategory.NINJA, investment_owner=u.UpgradeInvestmentOwner.NINJA,
                  role=u.UpgradeRole.MECHANIC if row['kind'] == 'one_time' else u.UpgradeRole.SUPPORT,
                  rarity=u.UpgradeRarity.RARE if row['kind'] == 'rare' else u.UpgradeRarity.COMMON,
                  uses_rolled_rarity=False, rarity_magnitudes=[],
                  prerequisite_upgrade_ids=['ReturningFang'] + row['requires'], prerequisite_requirements=[],
                  balance_parameters=row['balance'], stat_modifiers=[], special_effects=[],
                  requires_meta_unlock=False, unlocked_by_default=True)
            u.SystemLibrary.execute_console_command(None, 'setnopec ' + a.get_path_name() + ' bHasRuntimeBalance True')
        props(a, display_name=copy[uid]['name'], description=copy[uid]['description'], rolled_description_format='')
        assert u.EditorAssetLibrary.save_loaded_asset(a, False)
        if a not in pool:
            pool.append(a)
        by_id[uid] = a
    comp.set_editor_property('upgrade_pool', pool)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)

source = u.load_asset(montages + 'AM_FangAttack')
mirrored = u.load_asset(montages + 'AS_NinjaThrow_Left')
original_clip = u.load_asset('/Game/Assets/PlayerCharacters/Ninja/RetargetedAnimations/AS_Combo_Attack_Air_Wave_01_Seq')
assert source and mirrored and original_clip
original_segments = [s for slot in source.get_editor_property('slot_anim_tracks')
                     for s in slot.get_editor_property('anim_track').get_editor_property('anim_segments')]
assert len(original_segments) == 1 and original_segments[0].get_editor_property('anim_reference') == original_clip
if not validate:
    alternate = u.load_asset(montages + 'AM_FangAttack_Left') if u.EditorAssetLibrary.does_asset_exist(montages + 'AM_FangAttack_Left') else u.EditorAssetLibrary.duplicate_asset(source.get_path_name(), montages + 'AM_FangAttack_Left')
    slots = list(source.get_editor_property('slot_anim_tracks'))
    for i, slot in enumerate(slots):
        track = slot.get_editor_property('anim_track')
        segments = list(track.get_editor_property('anim_segments'))
        for j, segment in enumerate(segments):
            segment.set_editor_property('anim_reference', mirrored)
            segments[j] = segment
        track.set_editor_property('anim_segments', segments)
        slot.set_editor_property('anim_track', track)
        slots[i] = slot
    alternate.set_editor_property('slot_anim_tracks', slots)
    assert u.EditorAssetLibrary.save_loaded_asset(alternate, False)
else:
    alternate = u.load_asset(montages + 'AM_FangAttack_Left')

ninja = u.load_asset(ninja_path)
cdo = u.get_default_object(ninja.generated_class())
attack = cdo.get_component_by_class(u.AutoAttackComponent)
build = cdo.get_component_by_class(u.NinjaBuildComponent)
normal_montage = attack.get_editor_property('attack_montage')
normal_alternate = attack.get_editor_property('alternate_attack_montage')
normal_projectile = attack.get_editor_property('projectile_class')
fang_class = u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_NinjaProjectileFang').generated_class()
if not validate:
    props(attack, fang_montage=source, fang_alternate_montage=alternate)
    props(build, fang_projectile_class=fang_class)
    u.BlueprintEditorLibrary.compile_blueprint(ninja)
    assert u.EditorAssetLibrary.save_loaded_asset(ninja, False)
    cdo = u.get_default_object(ninja.generated_class())
    attack = cdo.get_component_by_class(u.AutoAttackComponent)
    build = cdo.get_component_by_class(u.NinjaBuildComponent)
assert attack.get_editor_property('fang_montage') == source
assert attack.get_editor_property('fang_alternate_montage') == alternate
assert build.get_editor_property('fang_projectile_class') == fang_class
assert attack.get_editor_property('attack_montage') == normal_montage
assert attack.get_editor_property('alternate_attack_montage') == normal_alternate
assert attack.get_editor_property('projectile_class') == normal_projectile
clips = [s.get_editor_property('anim_reference') for t in alternate.get_editor_property('slot_anim_tracks')
         for s in t.get_editor_property('anim_track').get_editor_property('anim_segments')]
assert clips == [mirrored]
assert alternate.get_editor_property('sequence_length') == source.get_editor_property('sequence_length')
assert len(u.AnimationLibrary.get_animation_notify_events(alternate)) == len(u.AnimationLibrary.get_animation_notify_events(source))
for frame in [0, 5, 10, 15, 20, 25]:
    a = u.AnimPoseExtensions.get_anim_pose_at_frame(original_clip, frame, u.AnimPoseEvaluationOptions())
    b = u.AnimPoseExtensions.get_anim_pose_at_frame(mirrored, frame, u.AnimPoseEvaluationOptions())
    for src, dst in [('RightHand', 'LeftHand'), ('LeftHand', 'RightHand')]:
        p = u.AnimPoseExtensions.get_bone_pose(a, src, u.AnimPoseSpaces.WORLD).translation
        q = u.AnimPoseExtensions.get_bone_pose(b, dst, u.AnimPoseSpaces.WORLD).translation
        assert ((p.x + q.x)**2 + (p.y - q.y)**2 + (p.z - q.z)**2)**.5 < 1.0

saved_pool = list(u.get_default_object(bp.generated_class()).get_component_by_class(u.PlayerUpgradeComponent).get_editor_property('upgrade_pool'))
saved_paths = [a.get_path_name() for a in saved_pool if a]
assert saved_paths[:len(old_pool)] == old_pool, 'Existing pool order changed'
ids = [str(a.get_editor_property('upgrade_id')) for a in saved_pool if a]
assert len(ids) == len(set(ids))
for uid in changed_ids:
    assert uid in ids
    a = u.load_asset(card_path(uid))
    assert a.get_editor_property('max_level') == rows[uid]['levels']
    assert str(a.get_editor_property('description')) == copy[uid]['description']
    for key, value in rows[uid]['balance'].items():
        # Preserve user-tuned Farstrider speed/distance, changing only its requested cap.
        if uid != 'FangFarstrider' or key == 'MaxDamageBonus':
            assert abs(a.get_editor_property('balance_parameters')[key] - value) < .0001
reach = by_id['FangResonantReach']
assert reach.get_editor_property('max_level') == 5 and reach.get_editor_property('rarity') == u.UpgradeRarity.RARE
assert set(map(str, reach.get_editor_property('prerequisite_upgrade_ids'))) == {'ReturningFang', 'FangResonance'}
assert all(digest(Path(p)) == h for p, h in protected.items()), 'Protected asset changed'
if not validate:
    (backup / 'protected_hashes.json').write_text(json.dumps(protected, indent=2))
u.log('FANG_REVISION_OK: saved upgrades, preserved pool, dedicated projectile, verified mirrored montage; validate=' + str(validate))
