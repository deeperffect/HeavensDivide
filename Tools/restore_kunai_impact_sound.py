import unreal,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
bp=unreal.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja')
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
 d=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h);a=unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(d,bp)
 if isinstance(a,unreal.AutoAttackComponent):
  cls=a.get_editor_property('projectile_class');unreal.log('KUNAI_CLASS: '+cls.get_path_name())
  cdo=unreal.get_default_object(cls);feedback=cdo.get_editor_property('impact_feedback')
  path=cls.get_path_name().split('.')[0];asset=unreal.load_asset(path)
  src=root/'Content'/(path.removeprefix('/Game/')+'.uasset');dest=root/'Saved/Backups/NinjaPresentation'/src.relative_to(root)
  if not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
  feedback.hit_sound=unreal.load_asset('/Game/Assets/Sounds/Ninja/MS_Ninja_Impact');assert feedback.hit_sound
  cdo.set_editor_property('impact_feedback',feedback)
  assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
  unreal.log('KUNAI_SOUND_RESTORED')
  break
else:raise RuntimeError('Ninja attack template missing')
