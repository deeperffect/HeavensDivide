"""Set the saved boss and arena instance to the doubled rush speed, with backups."""
from pathlib import Path
from datetime import datetime
import json, shutil
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/BossChargeFollowup';out.mkdir(parents=True,exist_ok=True)
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
bp_path='/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle'
map_path='/Game/Maps/Lvl_B1_Arena'
for asset,extension in [(bp_path,'.uasset'),(map_path,'.umap')]:
    relative=Path(asset.removeprefix('/Game/')+extension)
    target=backup/relative;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/'Content'/relative,target)
bp=u.load_asset(bp_path);cdo=u.get_default_object(bp.generated_class())
report={'backup':str(backup),'default_before':cdo.get_editor_property('charge_speed')}
cdo.set_editor_property('charge_speed',3600.)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level(map_path)
bosses=[actor for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(actor,u.CinderOracleBoss)]
assert len(bosses)==1
report['placed_before']=bosses[0].get_editor_property('charge_speed')
bosses[0].set_editor_property('charge_speed',3600.)
assert level.save_current_level()
report['default_after']=u.get_default_object(bp.generated_class()).get_editor_property('charge_speed')
report['placed_after']=bosses[0].get_editor_property('charge_speed')
assert report['default_after']==3600. and report['placed_after']==3600.
(out/'Assets.json').write_text(json.dumps(report,indent=2))
u.log('BOSS_CHARGE_SPEED_SAVED')
