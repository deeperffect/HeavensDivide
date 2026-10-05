"""Use the current autoattack slash as a centered 360-degree Double Cut burst."""
from pathlib import Path
import shutil
import unreal as u

lib=u.AnimationLibrary
base='/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/'
normal=u.load_asset(base+'AM_AutoAttackSamurai')
montage=u.load_asset(base+'AM_DoubleCutBloodStance')
source_notify=next(e.get_editor_property('notify') for e in lib.get_animation_notify_events(normal)
                  if isinstance(e.get_editor_property('notify'),u.AnimNotify_SpawnSamuraiSlashNiagara)
                  and e.get_editor_property('notify').get_editor_property('attach_to_character_mesh'))
events=list(lib.get_animation_notify_events(montage))
trace=next(e for e in events if isinstance(e.get_editor_property('notify'),u.AnimNotify_PerformAutoAttackTrace))
time=lib.get_anim_notify_event_trigger_time(trace)
source=Path(u.Paths.project_content_dir(),'HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_DoubleCutBloodStance.uasset')
backup=Path(u.Paths.project_saved_dir(),'Backups/DoubleCut360',source.name)
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists(): shutil.copy2(source,backup)
old=[e for e in events if isinstance(e.get_editor_property('notify_state_class'),u.AnimNotifyState_SamuraiSlashNiagara)
     or isinstance(e.get_editor_property('notify'),u.AnimNotify_SpawnSamuraiSlashNiagara)]
assert len(old)==1
event=old[0]
track=next(t for t in lib.get_animation_notify_track_names(montage)
           if any(e.get_editor_property('notify_name')==event.get_editor_property('notify_name')
                  for e in lib.get_animation_notify_events_for_track(montage,t)))
notify=u.new_object(u.AnimNotify_SpawnSamuraiSlashNiagara,outer=montage)
for field in ['niagara_system','scale','rotation_offset','area_bonus_scale_multiplier',
              'area_scaled_offset_parameter','area_scaled_float_parameters']:
 notify.set_editor_property(field,source_notify.get_editor_property(field))
offset=source_notify.get_editor_property('location_offset')
notify.set_editor_property('location_offset',u.Vector(0,0,offset.z))
notify.set_editor_property('skeleton_socket_name','None')
notify.set_editor_property('attach_to_character_mesh',True)
notify.set_editor_property('radial_copies',4)
assert lib.remove_animation_notify_events_by_name(montage,event.get_editor_property('notify_name'))==1
lib.add_animation_notify_event_object(montage,time,notify,track)
assert len(lib.get_animation_notify_events(montage))==len(events)
assert u.EditorAssetLibrary.save_loaded_asset(montage,False)
u.log('DOUBLE_CUT_360_PASS: centered four-arc slash uses the autoattack effect and scaling at the damage frame')
