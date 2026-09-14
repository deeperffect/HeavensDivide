"""Move the saved Ninja component's shuriken speed into the upgrade DA."""
import unreal,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve()
bp=unreal.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja')
speed=900.0
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
 data=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
 obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data,bp)
 if isinstance(obj,unreal.NinjaBuildComponent):
  try:speed=float(obj.get_editor_property('giant_shuriken_travel_speed'))
  except Exception:pass # Migration can also run after the component property is retired.
  break
path='/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaGreatShuriken'
card=unreal.load_asset(path);assert card
source=root/'Content/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaGreatShuriken.uasset'
backup=root/'Saved/Backups/ShurikenSpeed'/datetime.now().strftime('%Y%m%d_%H%M%S')/source.name
backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,backup)
balance=dict(card.get_editor_property('balance_parameters'))
if not any(str(key)=='TravelSpeed' for key in balance):balance['TravelSpeed']=max(1.0,speed)
card.set_editor_property('balance_parameters',balance)
unreal.SystemLibrary.execute_console_command(None,'setnopec '+card.get_path_name()+' bHasRuntimeBalance True')
assert unreal.EditorAssetLibrary.save_loaded_asset(card,False)
unreal.log('SHURIKEN_SPEED_MIGRATED: '+str(card.get_editor_property('balance_parameters')))
