"""Record combat/UI defaults and back up the actor before its visual replacement."""
import unreal as u, json, shutil
from pathlib import Path
root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved/GraveCantorReplacement'
relative = 'HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_GraveCantor.uasset'
backup = out / 'Backup' / relative
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists(): shutil.copy2(root/'Content'/relative, backup)
bp = u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_GraveCantor')
cdo = u.get_default_object(bp.generated_class())
properties = ['move_speed','ability_damage','ability_cooldown','cast_range','windup','xp_reward','tactical_role',
    'support_radius','support_targets','heal_fraction','health_bar_draw_size','status_indicator_draw_size','status_stack_font_size']
data = {key:str(cdo.get_editor_property(key)) for key in properties}
mesh = cdo.get_editor_property('mesh')
data['mesh'] = {key:str(mesh.get_editor_property(key)) for key in ['skeletal_mesh_asset','anim_class','relative_location','relative_rotation','relative_scale3d']}
(out/'OriginalGameplay.json').write_text(json.dumps(data,indent=2))
assert abs(cdo.get_editor_property('windup')-1.5)<.01
u.log('GRAVE_CANTOR_BACKED_UP '+json.dumps(data))
