"""Connect the mirrored clip to the alternate throw; -ValidateNinjaAlternate is read-only."""
from datetime import datetime
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
folder = '/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/'
validate = '-ValidateNinjaAlternate' in u.SystemLibrary.get_command_line()
original = u.load_asset(folder + 'AM_AutoAttackNinja')
alternate = u.load_asset(folder + 'AM_AutoAttackNinja_Left')
mirrored = u.load_asset(folder + 'AS_NinjaThrow_Left')
source = u.load_asset('/Game/Assets/PlayerCharacters/Ninja/RetargetedAnimations/AS_Combo_Attack_Air_Wave_01_Seq')
assert original and alternate and mirrored and source
if not validate:
    backup = root / 'Saved/Backups/NinjaAlternateMontage' / datetime.now().strftime('%Y%m%d_%H%M%S')
    backup.mkdir(parents=True, exist_ok=True)
    disk = root / 'Content/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_AutoAttackNinja_Left.uasset'
    shutil.copy2(disk, backup / disk.name)
    slots = list(alternate.get_editor_property('slot_anim_tracks'))
    count = 0
    for si, slot in enumerate(slots):
        track = slot.get_editor_property('anim_track')
        segments = list(track.get_editor_property('anim_segments'))
        for ai, segment in enumerate(segments):
            if segment.get_editor_property('anim_reference') in [source, mirrored]:
                segment.set_editor_property('anim_reference', mirrored)
                segments[ai] = segment
                count += 1
        track.set_editor_property('anim_segments', segments)
        slot.set_editor_property('anim_track', track)
        slots[si] = slot
    assert count == 1, count
    alternate.set_editor_property('slot_anim_tracks', slots)
    assert u.EditorAssetLibrary.save_loaded_asset(alternate, False)
    u.log('NINJA_ALTERNATE_BACKUP: ' + str(backup))

clips = [segment.get_editor_property('anim_reference')
         for slot in alternate.get_editor_property('slot_anim_tracks')
         for segment in slot.get_editor_property('anim_track').get_editor_property('anim_segments')]
assert clips == [mirrored], 'Alternate montage does not reference the mirrored clip'
assert alternate.get_editor_property('sequence_length') == original.get_editor_property('sequence_length')
assert len(u.AnimationLibrary.get_animation_notify_events(alternate)) == len(u.AnimationLibrary.get_animation_notify_events(original))
# Verify the clip itself, not just its name, at several points through the throw.
options = u.AnimPoseEvaluationOptions()
errors = []
for frame in [0, 5, 10, 15, 20, 25]:
    a = u.AnimPoseExtensions.get_anim_pose_at_frame(source, frame, options)
    b = u.AnimPoseExtensions.get_anim_pose_at_frame(mirrored, frame, options)
    for src, dst in [('RightHand', 'LeftHand'), ('LeftHand', 'RightHand')]:
        p = u.AnimPoseExtensions.get_bone_pose(a, src, u.AnimPoseSpaces.WORLD).translation
        q = u.AnimPoseExtensions.get_bone_pose(b, dst, u.AnimPoseSpaces.WORLD).translation
        errors.append(((p.x + q.x)**2 + (p.y - q.y)**2 + (p.z - q.z)**2)**.5)
assert max(errors) < 1.0, errors
u.log('NINJA_ALTERNATE_OK: alternate montage uses verified mirrored poses')
