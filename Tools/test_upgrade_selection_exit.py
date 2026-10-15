"""Real-time PIE check: card exit completes while the gameplay world is frozen."""
from pathlib import Path
import json,time,traceback,re,shutil
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UpgradeSelection';OUT.mkdir(parents=True,exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
u.EditorPythonScripting.set_keep_python_script_alive(True)
S={'stage':'load','start':time.monotonic()};R={'passed':False}

def finish():
 (OUT/'Validation.json').write_text(json.dumps(R,indent=2));L.editor_request_end_play()
 u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()

def tick(dt):
 if S.get('busy'):return
 S['busy']=True
 try:
  now=time.monotonic();assert now-S['start']<300,'Upgrade selection check timed out'
  if S['stage']=='load':
   assert L.load_level('/Game/Maps/Lvl_OldVillage');L.editor_request_begin_play();S.update(stage='ready',at=now);return
  if S['stage']=='automation':
   if now-S['at']<5:return
   log=(ROOT/'Saved/OldVillage_test_upgrade_selection_exit.log').read_text(encoding='utf-8',errors='replace')
   results=re.findall(r'Test Completed\. Result=\{([^}]+)\} Name=\{([^}]+)\}',log[S['log_start']:])
   if len(results)<3:return
   R['automation']=[{'name':n,'result':v} for v,n in results]
   for name in ['RestoredCategoryUpgrades','UpgradeSelectionHold','UpgradeSelectionDissolve']:
    src=ROOT/'Saved'/(name+'.png')
    if src.exists():shutil.copy2(src,OUT/(name+'.png'))
   assert all(v=='Success' for v,n in results),R['automation']
   R['passed']=True;u.log('UPGRADE_SELECTION_EXIT_VALIDATED');finish();return
  if S['stage']=='ended':
   if L.is_in_play_in_editor():return
   assert L.load_level('/Engine/Maps/Entry')
   S.update(stage='automation',at=now,log_start=len((ROOT/'Saved/OldVillage_test_upgrade_selection_exit.log').read_text(encoding='utf-8',errors='replace')))
   world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
   u.SystemLibrary.execute_console_command(world,'Automation RunTests HeavensDivide.UI.LevelUpPresentation+HeavensDivide.UI.UpgradeCardLayout+HeavensDivide.Upgrades.DraftUnlocks')
   return
  world=u.EditorLevelLibrary.get_game_world()
  if not world:return
  pc=u.GameplayStatics.get_player_controller(world,0)
  for character in u.GameplayStatics.get_all_actors_of_class(world,u.CharacterBase):
   health=character.get_component_by_class(u.HealthComponent)
   if health:health.set_damage_enabled(False)
   attack=character.get_component_by_class(u.AutoAttackComponent)
   if attack:attack.set_auto_attack_enabled(False)
  if S['stage']=='ready' and pc.get_run_time_seconds()>=2.5:
   xp=pc.get_experience_component();xp.add_xp(xp.get_xp_to_next_level());S.update(stage='offer',at=now);return
  if S['stage']=='offer':
   if not pc.get_editor_property('level_up_selection_active'):return
   widget=pc.get_editor_property('level_up_widget')
   if not widget:return
   assert widget.select_category_choice(0)
   S.update(widget=widget,stage='select',at=now);return
  if S['stage']=='select' and now-S['at']>.7:
   widget=S['widget']
   # SelectUpgradeChoice is the shared handler used by mouse and controller.
   assert widget.select_upgrade_choice(0);assert not widget.select_upgrade_choice(1)
   assert pc.get_editor_property('level_up_selection_active'),'Input released before the exit animation'
   R['before']={'real_time':now,'run_time':pc.get_run_time_seconds(),'dilation':u.GameplayStatics.get_global_time_dilation(world)}
   assert R['before']['dilation']<.001
   S.update(stage='exit',at=now,samples=[]);return
  if S['stage']=='exit':
   age=now-S['at'];S['samples'].append({'age':age,'selecting':pc.get_editor_property('level_up_selection_active'),'dilation':u.GameplayStatics.get_global_time_dilation(world)})
   if pc.get_editor_property('level_up_selection_active'):
    assert age<2,'UMG exit failed to advance while world was frozen'
    return
   R['exit_seconds']=age;R['samples']=S['samples'];R['after_dilation']=u.GameplayStatics.get_global_time_dilation(world)
   assert .4<age<1.2,(age,'unexpected real-time exit duration')
   assert R['after_dilation']>.99,'Gameplay speed was not restored'
   L.editor_request_end_play();S.update(stage='ended',at=now)
 except Exception:R['error']=traceback.format_exc();u.log_error(R['error']);finish()
 finally:S['busy']=False
S['handle']=u.register_slate_post_tick_callback(tick)
