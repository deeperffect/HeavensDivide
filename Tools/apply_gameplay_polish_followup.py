import unreal as u
import pathlib, shutil, json
root=pathlib.Path(u.Paths.project_dir())
out=root/'Saved/GameplayPolishFollowup'
(out/'Backups').mkdir(parents=True,exist_ok=True)
report={}
for name in ['Ninja','Samurai']:
 path='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name
 source=root/'Content'/path.removeprefix('/Game/')
 source=source.with_suffix('.uasset')
 backup=out/'Backups'/source.name
 if not backup.exists(): shutil.copy2(source,backup)
 bp=u.load_asset(path)
 comp=u.get_default_object(bp.generated_class()).get_editor_property('swap_presentation')
 before=comp.get_editor_property('swap_freeze_duration')
 comp.set_editor_property('swap_freeze_duration',.62)
 u.BlueprintEditorLibrary.compile_blueprint(bp)
 assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
 value=u.get_default_object(bp.generated_class()).get_editor_property('swap_presentation').get_editor_property('swap_freeze_duration')
 assert abs(value-.62)<.0001
 report[name]={'before':before,'after':value}
(out/'AssetChanges.json').write_text(json.dumps(report,indent=2))
u.log('GAMEPLAY_FOLLOWUP_ASSETS_OK')
