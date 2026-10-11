import unreal as u, json
from pathlib import Path
out = Path(u.Paths.project_saved_dir()) / 'CombatPolish'
out.mkdir(parents=True, exist_ok=True)
report = {}
def props(obj, keys):
    r = {}
    for k in keys:
        try: r[k] = str(obj.get_editor_property(k))
        except Exception: pass
    return r
paths = list(u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/EnemyCharacters', True, False))
paths.extend(u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/PlayerCharacters', True, False))
for path in paths:
    if '/BP_' not in path: continue
    bp = u.load_asset(path)
    if not isinstance(bp, u.Blueprint): continue
    c = u.get_default_object(bp.generated_class())
    r = {'class': str(c.get_class().get_super_class()) if hasattr(c.get_class(), 'get_super_class') else str(c.get_class())}
    r.update(props(c, ['move_speed', 'cast_montage', 'oracle_cast_montage', 'attack_montage', 'b_enable_lightweight_movement', 'tactical_role']))
    for comp in c.get_components_by_class(u.SceneComponent):
        r[comp.get_name()] = props(comp, ['relative_location','relative_rotation','relative_scale3d','capsule_half_height','capsule_radius','skeletal_mesh_asset','anim_class','animation_mode','receives_decals'])
    try:
        swap = c.get_editor_property('swap_presentation')
        r['swap'] = props(swap, ['arrival_montage','arrival_drop_height','arrival_drop_duration','arrival_play_rate','b_enable_ninja_arrival_drop'])
    except Exception: pass
    report[path] = r
for path in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/EnemyCharacters', True, False):
    if '/ABP_' not in path: continue
    a = u.load_asset(path)
    task = u.AssetExportTask(); task.object = a; task.exporter = u.ObjectExporterT3D()
    task.filename = str(out / (a.get_name() + '.copy')); task.automated=True; task.prompt=False; task.replace_identical=True
    u.Exporter.run_asset_export_task(task)
destination=out/('LatestAssetAudit.json' if (out/'AssetAudit.json').exists() else 'AssetAudit.json')
destination.write_text(json.dumps(report, indent=2))
u.log('COMBAT_POLISH_AUDIT_COMPLETE')
