"""Apply absolute enemy/spawner targets without changing attacks or rewards."""
import json,shutil,unreal as u
from datetime import datetime
from pathlib import Path
root=Path(u.Paths.project_dir());out=root/'Saved/WallPressureFollowup'
targets=json.loads((root/'Tools/enemy_pressure_tuning.json').read_text())['targets']
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
report={'enemies':{},'spawners':{}}
baseline_path=root/'Config/BloodshiftBalanceBaseline.json'
baseline=json.loads(baseline_path.read_text(encoding='utf-8-sig'))
def copy_backup(path,ext='.uasset'):
 src=root/'Content'/(path.removeprefix('/Game/').split('.')[0]+ext)
 dst=backup/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def damage_values(cdo):
 values={}
 if isinstance(cdo,(u.MeleeEnemyBase,u.RangedEnemyBase)):values['attack_damage']=cdo.get_editor_property('attack_damage')
 if isinstance(cdo,u.TacticalEnemy):values['ability_damage']=cdo.get_editor_property('ability_damage')
 return values
for path,target in targets['enemies'].items():
 copy_backup(path);bp=u.load_asset(path);assert bp
 cdo=u.get_default_object(bp.generated_class());health=cdo.get_health_component()
 before=health.get_editor_property('max_health');damage=damage_values(cdo);xp=cdo.get_editor_property('xp_reward')
 health.modify(True);health.set_editor_property('max_health',target['health'])
 assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
 assert abs(health.get_editor_property('max_health')-target['health'])<.001
 assert damage_values(cdo)==damage and cdo.get_editor_property('xp_reward')==xp
 baseline.setdefault(health.get_path_name(),{})['MaxHealth']='%.6f'%target['health']
 report['enemies'][path]={'before':before,'after':target['health'],'damage_preserved':True,'xp_preserved':True}
def tune_spawner(obj,target):
 obj.modify(True);phases=list(obj.get_editor_property('pressure_phases'))
 assert len(phases)==len(target['phases'])
 for p,t in zip(phases,target['phases']):
  assert str(p.phase_name)==t['name']
  for field,key in [('global_max_alive','cap'),('normal_spawn_interval','normal'),('accelerated_spawn_interval','accelerated'),('emergency_spawn_interval','emergency'),('event_interval_min','event_min'),('event_interval_max','event_max')]:p.set_editor_property(field,t[key])
  rows=list(p.enemy_population_entries);assert len(rows)==len(t['populations'])
  for row,value in zip(rows,t['populations']):
   assert row.enemy_class.get_path_name()==value['class']
   row.set_editor_property('desired_population',value['desired']);row.set_editor_property('max_population',value['max'])
  p.set_editor_property('enemy_population_entries',rows)
 obj.set_editor_property('pressure_phases',phases);obj.set_editor_property('absolute_hard_alive_cap',target['hard_cap'])
 return phases
path='/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner';copy_backup(path)
bp=u.load_asset(path);cdo=u.get_default_object(bp.generated_class());phases=tune_spawner(cdo,targets['spawners']['blueprint'])
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
values=baseline.setdefault(cdo.get_path_name(),{});values['PressurePhases']='('+','.join(p.export_text() for p in phases)+')';values['AbsoluteHardAliveCap']=str(targets['spawners']['blueprint']['hard_cap'])
report['spawners']['blueprint']={'phase_count':len(phases),'hard_cap':cdo.get_editor_property('absolute_hard_alive_cap')}
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for path,target in targets['spawners'].items():
 if path=='blueprint':continue
 copy_backup(path,'.umap');assert level.load_level(path)
 spawners=[a for a in actors.get_all_level_actors() if isinstance(a,u.EnemySpawner)];assert len(spawners)==1
 phases=tune_spawner(spawners[0],target);assert level.save_current_level()
 report['spawners'][path]={'phase_count':len(phases),'hard_cap':spawners[0].get_editor_property('absolute_hard_alive_cap')}
backup.mkdir(parents=True,exist_ok=True);shutil.copy2(baseline_path,backup/'BloodshiftBalanceBaseline.json')
baseline_path.write_text(json.dumps(baseline,indent=2)+'\n')
(out/'Applied.json').write_text(json.dumps(report,indent=2))
u.log('ENEMY_PRESSURE_TUNING_APPLIED')
