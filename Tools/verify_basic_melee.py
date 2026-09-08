"""Read-only verification of saved basic/special attack defaults after migration."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/basic_melee_original_defaults.json').read_text())
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
  else:assert c.get_editor_property(key)==value,(row['path'],key)
 # Load and compile the actual assigned animation Blueprint too.
 anim_class=c.get_editor_property('mesh').get_editor_property('anim_class')
 if anim_class:
  abp=unreal.load_asset(anim_class.get_path_name().split('.')[0])
  if isinstance(abp,unreal.Blueprint):unreal.BlueprintEditorLibrary.compile_blueprint(abp)
assert not (root/'Content/HeavensDivide/Blueprints/EnemyCharacters/Montages/AM_EnemyGruntAttack.uasset').exists()
unreal.log('BASIC_MELEE_SAVED_ASSETS_PASS: 7 basics, 3 specials; balance preserved')
