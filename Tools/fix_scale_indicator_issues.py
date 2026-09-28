"""Targeted fixes; preserve current gameplay tuning and user-authored colors/scales."""
import unreal as u, shutil
from pathlib import Path
from datetime import datetime
root=Path(u.Paths.project_dir()).resolve()
backup=root/'Saved/Backups/ScaleIndicatorFix'/datetime.now().strftime('%Y%m%d_%H%M%S')
def protect(path):
 rel=Path(path.removeprefix('/Game/')+'.uasset');dest=backup/rel
 dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/'Content'/rel,dest)
def save(obj):assert u.EditorAssetLibrary.save_loaded_asset(obj,False)
for sub in ['Samurai/DA_Upgrade_SamuraiOverkillBurst','Samurai/DA_Upgrade_SamuraiBloodTransfer','Synergy/DA_Synergy_GrandEntrance','Ninja/DA_Upgrade_NinjaVenomousKunai']:
 path='/Game/HeavensDivide/Upgrades/'+sub;protect(path);card=u.load_asset(path);p=card.get_editor_property('presentation')
 if str(p.get_editor_property('system_scale_parameter'))=='User._Scale':
  p.set_editor_property('multiply_authored_system_scale',True)
  card.set_editor_property('presentation',p);save(card)
path='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai';protect(path)
bp=u.load_asset(path);c=u.get_default_object(bp.generated_class());p=c.get_editor_property('combo_ability')
p.set_editor_property('vfx_radius_parameter_is_scale',True)
c.set_editor_property('combo_ability',p);u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
circle=u.load_asset('/Game/HeavensDivide/Materials/M_AttackIndicatorCircle')
protect('/Game/HeavensDivide/Materials/M_AttackIndicatorCircle')
custom=next(e for e in u.MaterialEditingLibrary.get_material_expressions(circle) if isinstance(e,u.MaterialExpressionCustom))
code=custom.get_editor_property('code')
code=code.replace('rgb += Edge.rgb * rim * impactGate * (0.4 + impactBeat * 1.8);','rgb += Edge.rgb * rim * impactGate * (1.0-saturate(Continuous)) * (0.4 + impactBeat * 1.8);')
custom.set_editor_property('code',code);u.MaterialEditingLibrary.recompile_material(circle);save(circle)
for enemy,field in [('Elites/BP_EnemyGorilla','contact_aura_material'),('Mobs/BP_EnemyGoblinBomb','attack_telegraph_material')]:
 path='/Game/HeavensDivide/Blueprints/EnemyCharacters/'+enemy;protect(path);bp=u.load_asset(path)
 c=u.get_default_object(bp.generated_class());c.set_editor_property(field,circle)
 u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
u.log('SCALE_INDICATOR_FIX_SAVED')
