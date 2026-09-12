"""Seed only missing runtime keys. Re-running never resets designer balance or VFX edits."""
import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();values=json.loads((root/'Tools/upgrade_tuning_defaults.json').read_text())
bp=unreal.load_asset('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')
cdo=unreal.get_default_object(bp.generated_class());upgrades=cdo.get_editor_property('player_upgrade_component')
# Preserve the actual Blueprint's previously-authored Blade Wave numbers on first migration.
manager=cdo.get_editor_property('character_manager');samurai_class=manager.get_editor_property('samurai_class')
samurai=unreal.get_default_object(samurai_class);attack=samurai.get_component_by_class(unreal.AutoAttackComponent)
if attack:
 for key,prop in [('WaveWidth','blade_wave_base_width'),('WaveDamageMultiplier','blade_wave_damage_multiplier'),('WaveTravelDistance','blade_wave_travel_distance'),('WaveSpeed','blade_wave_speed')]:
  values['BladeWave'][key]=attack.get_editor_property(prop)
 values['CrossingBlades']['SideAngle']=attack.get_editor_property('crossing_blade_side_angle')
 wave_class=attack.get_editor_property('blade_wave_class')
 if wave_class:
  wave=unreal.get_default_object(wave_class)
  for key,prop in [('WaveThickness','wave_thickness'),('WaveHeight','wave_height'),('WaveVFXAuthoredDuration','vfx_authored_duration')]:values['BladeWave'][key]=wave.get_editor_property(prop)
count=0;seen=set()
for a in upgrades.get_editor_property('upgrade_pool'):
 if not a:continue
 uid=str(a.get_editor_property('upgrade_id'))
 if uid not in values:continue
 seen.add(uid);path=a.get_path_name().split('.')[0];rel=Path(path.removeprefix('/Game/')+'.uasset')
 backup=root/'Saved/Backups/UpgradeTuning'/rel
 if not backup.exists():backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/'Content'/rel,backup)
 current=dict(a.get_editor_property('balance_parameters'));names={str(k) for k in current}
 for key,v in values[uid].items():
  if key not in names:current[key]=float(v)
 a.set_editor_property('balance_parameters',current)
 unreal.SystemLibrary.execute_console_command(None,'setnopec '+a.get_path_name()+' bHasRuntimeBalance '+('True' if current else 'False'))
 unreal.SystemLibrary.execute_console_command(None,'setnopec '+a.get_path_name()+' bHasRuntimePresentation True')
 assert a.get_editor_property('has_runtime_balance')==bool(current)
 assert a.get_editor_property('has_runtime_presentation')
 assert unreal.EditorAssetLibrary.save_loaded_asset(a,False),path
 count+=1
assert seen==set(values),str(set(values)-seen)
unreal.log('UPGRADE_TUNING_ASSETS_PASS: '+str(count)+' assets seeded without replacing existing tuning or presentation')
