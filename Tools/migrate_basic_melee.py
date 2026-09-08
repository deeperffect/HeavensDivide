"""Resave basic melee Blueprints after moving montage support to the special branch.
Run after building the editor. Preserves every existing balance value.
"""
import unreal,json,shutil,re
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/basic_melee_original_defaults.json').read_text())
report=[]
for row in rows:
 bp=unreal.load_asset(row['path']);assert bp,row
 c=unreal.get_default_object(bp.generated_class())
 special=isinstance(c,unreal.MontageMeleeEnemyBase)
 assert special==row['special'],row['path']
 for key,value in row['props'].items():
  if key=='attack_montage':continue
  assert c.get_editor_property(key)==value,(row['path'],key,value,c.get_editor_property(key))
 if special:
  montage=c.get_editor_property('attack_montage')
  old=row['props']['attack_montage']
  assert (montage is None and old is None) or (montage and montage.get_path_name() in old),row
  unreal.BlueprintEditorLibrary.compile_blueprint(bp)
  report.append({'path':row['path'],'action':'special preserved'})
  continue
 graph_nodes=[]
 for graph in unreal.BlueprintEditorLibrary.list_graphs(bp):
  for node in unreal.ObjectIterator(unreal.K2Node):
   if node.get_outer()==graph:
    graph_nodes.append(unreal.BlueprintEditorLibrary.get_node_title(node))
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 c=unreal.get_default_object(bp.generated_class())
 try:c.get_editor_property('attack_montage')
 except Exception:pass
 else:raise AssertionError('Basic enemy still has montage property '+row['path'])
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
 report.append({'path':row['path'],'action':'basic migrated','graph_nodes':graph_nodes})
# Report the obsolete basic montage for removal after the commandlet exits.
# Unreal cannot reliably unload this montage in the same process as BP migration.
old='/Game/HeavensDivide/Blueprints/EnemyCharacters/Montages/AM_EnemyGruntAttack'
if unreal.EditorAssetLibrary.does_asset_exist(old):
 refs=unreal.EditorAssetLibrary.find_package_referencers_for_asset(old,True)
 report.append({'old_montage_referencers':list(refs)})
 if not refs:
  rel=Path(old.removeprefix('/Game/')+'.uasset'); backup=root/'Saved/Backups/BasicMelee'/rel
  backup.parent.mkdir(parents=True,exist_ok=True)
  if not backup.exists():shutil.copy2(root/'Content'/rel,backup)
  report.append({'obsolete_montage_ready_for_removal':old})
(root/'Saved/basic_melee_migration.json').write_text(json.dumps(report,indent=2))
unreal.log('BASIC_MELEE_MIGRATION_OK')


