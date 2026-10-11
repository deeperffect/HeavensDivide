"""Read-only verification of saved wobble profiles and preserved special attacks."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/basic_melee_original_defaults.json').read_text())
profiles=json.loads((root/'Tools/basic_melee_profiles.json').read_text())
for row in rows:
 bp=unreal.load_asset(row['path']); assert bp,row['path']
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 c=unreal.get_default_object(bp.generated_class())
 assert isinstance(c,unreal.MontageMeleeEnemyBase)==row['special'],row['path']
 for key,value in row['props'].items():
  if key=='attack_montage':
   if row['special']:
    m=c.get_editor_property(key)
    assert (m.get_path_name() if m else None)==value,row['path']
   else:
    try:c.get_editor_property(key)
    except Exception:pass
    else:raise AssertionError('Basic montage property remains')
  else:
   expected=profiles.get(row['path'].split('.')[0],{}).get(key,value)
   assert abs(c.get_editor_property(key)-expected)<0.0001,(row['path'],key)
 # Load and compile the actual assigned animation Blueprint too.
 anim_class=c.get_editor_property('mesh').get_editor_property('anim_class')
 if anim_class:
  abp=unreal.load_asset(anim_class.get_path_name().split('.')[0])
  if isinstance(abp,unreal.Blueprint):unreal.BlueprintEditorLibrary.compile_blueprint(abp)
assert not (root/'Content/HeavensDivide/Blueprints/EnemyCharacters/Montages/AM_EnemyGruntAttack.uasset').exists()
for path,values in profiles.items():
 bp=unreal.load_asset(path);assert bp,path
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 c=unreal.get_default_object(bp.generated_class())
 assert isinstance(c,unreal.MeleeEnemyBase) and not isinstance(c,unreal.MontageMeleeEnemyBase),path
 try:c.get_editor_property('attack_montage')
 except Exception:pass
 else:raise AssertionError('Basic enemy exposes montage playback: '+path)
 assert c.get_editor_property('use_procedural_attack_motion'),path
 assert abs(c.get_editor_property('attack_wobble_angle')-12.0)<0.0001,path
 for key,value in values.items():
  assert abs(c.get_editor_property(key)-value)<0.0001,(path,key)
 assert values['attack_interval']>values['attack_windup']+values['attack_recovery'],path
 # An authored Blueprint override must not silently restore montage attacks.
 for graph in unreal.BlueprintEditorLibrary.list_graphs(bp):
  for node in unreal.ObjectIterator(unreal.K2Node):
   if node.get_outer()==graph:
    title=unreal.BlueprintEditorLibrary.get_node_title(node)
    assert not ('montage' in title.lower() and 'play' in title.lower()),(path,title)
for slow in profiles.values():
 for fast in profiles.values():
  if slow['move_speed']<fast['move_speed']:
   assert slow['attack_windup']<fast['attack_windup']
   assert slow['attack_interval']<fast['attack_interval']
unreal.log('BASIC_MELEE_SAVED_ASSETS_PASS: 7 combat profiles, dummy and 3 special attack defaults verified')
