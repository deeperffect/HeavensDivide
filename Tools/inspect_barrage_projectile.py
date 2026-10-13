import json
from pathlib import Path
import unreal as u
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_NinjaProjectile')
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
rows=[]
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    data=u.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    obj=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data,bp)
    row={'name':obj.get_name(),'class':obj.get_class().get_name()}
    for prop in ['relative_location','relative_rotation','relative_scale3d','asset','auto_activate']:
        try: row[prop]=str(obj.get_editor_property(prop))
        except: pass
    rows.append(row)
cdo=u.get_default_object(bp.generated_class())
rows.append({'fade':cdo.get_editor_property('impact_trail_fade_duration')})
out=Path(u.Paths.project_dir())/'Saved/SteamBarrage';out.mkdir(parents=True,exist_ok=True)
(out/'ProjectileAssets.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
u.log('BARRAGE_INSPECT_OK')
