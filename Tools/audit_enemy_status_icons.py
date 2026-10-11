"""Read enemy status icon assignments across all project enemy Blueprints."""
import unreal as u,json
from pathlib import Path
out=Path(u.Paths.project_saved_dir()).resolve()/'StatusIconFix';out.mkdir(parents=True,exist_ok=True)
rows=[]
for path in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/EnemyCharacters',True,False):
    asset=u.load_asset(path)
    if not isinstance(asset,u.Blueprint):continue
    cls=asset.generated_class()
    if not cls:continue
    cdo=u.get_default_object(cls)
    if not isinstance(cdo,u.EnemyBase):continue
    row={'blueprint':path}
    for prop in ['bleed_status_icon','poison_status_icon']:
        value=cdo.get_editor_property(prop);row[prop]=value.get_path_name() if value else None
    rows.append(row)
(out/'IconAssignments.json').write_text(json.dumps(rows,indent=2))
u.log('ENEMY_STATUS_ICON_AUDIT '+json.dumps(rows))
