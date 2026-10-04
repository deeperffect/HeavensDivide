"""Bake the Ninja throw's left-hand counterpart with Unreal's mirror modifier."""
import unreal as u
import json
from pathlib import Path

folder='/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja'
assets=u.AssetToolsHelpers.get_asset_tools()
original=u.load_asset(folder+'/AM_AutoAttackNinja')
source=u.load_asset('/Game/Assets/PlayerCharacters/Ninja/RetargetedAnimations/AS_Combo_Attack_Air_Wave_01_Seq')
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja')
mesh=u.get_default_object(bp.generated_class()).get_editor_property('mesh')
skeleton=mesh.get_editor_property('skeletal_mesh_asset').get_editor_property('skeleton')
table=u.new_object(u.MirrorDataTable)
assert u.SwapVFXSetupLibrary.set_property_text(table,'RowStruct',"/Script/CoreUObject.ScriptStruct'/Script/Engine.MirrorTableRow'")
table.set_editor_property('skeleton',skeleton)
assert u.SwapVFXSetupLibrary.set_property_text(table,'MirrorAxis','X')
bones=[str(mesh.get_bone_name(i)) for i in range(mesh.get_num_bones())]
rows=[]
for bone in bones:
    partner=bone.replace('Left','Right') if 'Left' in bone else bone.replace('Right','Left')
    assert partner in bones, (bone,partner)
    rows.append({'Name':bone,'MirroredName':partner,'MirrorEntryType':'Bone','bEnabled':True})
assert table.fill_from_json_string(json.dumps(rows))
assert len(table.get_row_names())==len(bones)
sequence_path=folder+'/AS_NinjaThrow_Left'
montage_path=folder+'/AM_AutoAttackNinja_Left'
assert not u.EditorAssetLibrary.does_asset_exist(sequence_path), 'Mirrored sequence already exists; inspect before re-baking.'
sequence=assets.duplicate_asset('AS_NinjaThrow_Left',folder,source)
assert sequence
modifier=u.new_object(u.MirrorModifier)
modifier.set_editor_property('mirror_data_table',table)
modifier.set_editor_property('update_notifies',False)
modifier.on_apply(sequence)
assert u.EditorAssetLibrary.save_loaded_asset(sequence,False)
montage=assets.duplicate_asset('AM_AutoAttackNinja_Left',folder,original)
assert montage
slots=montage.get_editor_property('slot_anim_tracks')
changed=0
for slot_index, slot in enumerate(slots):
    track=slot.get_editor_property('anim_track')
    segments=track.get_editor_property('anim_segments')
    for segment_index, segment in enumerate(segments):
        if segment.get_editor_property('anim_reference')==source:
            segment.set_editor_property('anim_reference',sequence)
            segments[segment_index] = segment
            changed+=1
    track.set_editor_property('anim_segments',segments)
    slot.set_editor_property('anim_track',track)
    slots[slot_index] = slot
assert changed==1, changed
montage.set_editor_property('slot_anim_tracks',slots)
assert u.EditorAssetLibrary.save_loaded_asset(montage,False)
report={'source':source.get_path_name(),'sequence':sequence.get_path_name(),'montage':montage.get_path_name(),
        'mirror_pairs':rows,'bone_pose_api':u.AnimPoseExtensions.get_bone_pose.__doc__,
        'socket_names':[str(n) for n in mesh.get_all_socket_names()]}
Path(u.Paths.project_saved_dir(),'NinjaLeftThrowCreation.json').write_text(json.dumps(report,indent=2))
u.log('NINJA_LEFT_THROW_CREATED')
