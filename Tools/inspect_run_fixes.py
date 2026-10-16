from pathlib import Path
import json
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved/RunFixes'
out.mkdir(parents=True, exist_ok=True)
report = {}
def props(obj, names):
    return {n: str(obj.get_editor_property(n)) for n in names}
wave = u.load_asset('/Game/HeavensDivide/Audio/Music/W_MitkoIgra')
report['music'] = props(wave, ['looping','duration','virtualization_mode'])
for name in ['Samurai','Ninja']:
    cdo = u.get_default_object(u.load_class(None, '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name+'.BP_'+name+'_C'))
    swap = cdo.get_component_by_class(u.SwapPresentationComponent)
    attack = cdo.get_component_by_class(u.AutoAttackComponent)
    report[name] = {'swap': props(swap, ['swap_freeze_duration','entrance_play_rate','departure_play_rate','entrance_montage','departure_montage','arrival_drop_duration','arrival_walk_duration']),
                    'attack': props(attack,['attack_radius','attack_forward_offset','attack_montage','double_cut_montage'])}
    for key in ['attack_montage','double_cut_montage']:
        montage = attack.get_editor_property(key)
        if montage:
            report[name][key] = []
            for event in u.AnimationLibrary.get_animation_notify_events(montage):
                notify = event.get_editor_property('notify')
                if notify:
                    row={'class':notify.get_class().get_name(),'time':event.get_editor_property('display_time') if hasattr(event,'display_time') else str(event)}
                    if isinstance(notify,u.AnimNotify_SpawnSamuraiSlashNiagara):
                        row.update(props(notify,['niagara_system','scale','location_offset','rotation_offset','radial_copies','area_bonus_scale_multiplier']))
                    report[name][key].append(row)
rig = u.get_default_object(u.load_class(None,'/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_PlayerCameraRig.BP_PlayerCameraRig_C'))
report['camera']=props(rig,['enable_swap_focus','swap_zoom_amount'])
fx = u.load_asset('/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_BloodStance')
task=u.AssetExportTask();task.object=fx;task.exporter=u.ObjectExporterT3D();task.filename=str(out/'BloodSlash.txt');task.automated=True;task.prompt=False
u.Exporter.run_asset_export_task(task)
(out/'Before.json').write_text(json.dumps(report,indent=2))
u.log('RUN_FIXES_INSPECTED')
