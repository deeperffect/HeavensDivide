"""Give Ninja's montage exclusive VFX ownership and freeze-aware slash notifies."""
import unreal as u
from pathlib import Path
from datetime import datetime
import shutil,hashlib
root=Path(u.Paths.project_dir()).resolve();backup=root/'Saved/Backups/NinjaAbilityVFX'/datetime.now().strftime('%Y%m%d_%H%M%S')
def file(a):return root/'Content'/(a.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset')
def before(a):
 src=file(a);dst=backup/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def save(a):assert u.EditorAssetLibrary.save_loaded_asset(a,False)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja');before(bp)
cdo=u.get_default_object(bp.generated_class());settings=cdo.get_editor_property('combo_ability')
settings.set_editor_property('enable_code_vfx',False);cdo.set_editor_property('combo_ability',settings)
u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
montage=settings.get_editor_property('montage');before(montage)
lib=u.AnimationLibrary
records=[(track,e) for track in lib.get_animation_notify_track_names(montage) for e in lib.get_animation_notify_events_for_track(montage,track)]
old=[(track,e) for track,e in records if isinstance(e.get_editor_property('notify'),u.AnimNotify_PlayNiagaraEffect)]
assert len(old)==4,'Expected four slash notifies'
source=u.load_asset('/Game/Assets/VFX/CrossSlashesV1/Particles/NiagaraSystems/NS_CrossSlash03')
digest=hashlib.sha256(file(source).read_bytes()).hexdigest()
path='/Game/HeavensDivide/VFX/Abilities/NS_NinjaAbilitySlash'
fx=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.EditorAssetLibrary.duplicate_asset(source.get_path_name().split('.')[0],path)
assert fx
u.SwapVFXSetupLibrary.prepare_portal_local_space(fx);save(fx)
# Rebuild only these events; retain authored timing, tracks, transforms, and all other notifies.
for name in set(str(e.get_editor_property('notify_name')) for _,e in old):
 assert sum(str(e.get_editor_property('notify_name'))==name for _,e in records)==sum(str(e.get_editor_property('notify_name'))==name for _,e in old)
 lib.remove_animation_notify_events_by_name(montage,name)
for track,event in old:
 original=event.get_editor_property('notify')
 notify=u.new_object(u.AnimNotify_AbilityNiagara,outer=montage)
 for prop in ['location_offset','rotation_offset','scale','socket_name']:
  notify.set_editor_property(prop,original.get_editor_property(prop))
 notify.set_editor_property('template',fx)
 notify.set_editor_property('attached',False);notify.set_editor_property('absolute_scale',True)
 time=lib.get_anim_notify_event_trigger_time(event)
 lib.add_animation_notify_event_object(montage,time,notify,track)
 u.log('NINJA_NOTIFY '+str(time)+' '+str(notify.get_editor_property('scale')))
save(montage)
assert hashlib.sha256(file(source).read_bytes()).hexdigest()==digest
u.log('NINJA_ABILITY_VFX_OK: four montage notifies; code VFX disabled; vendor asset unchanged')
