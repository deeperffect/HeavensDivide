"""Fill missing stance VFX using project-owned copies of vendor assets.
-ValidateStanceVFX verifies assignments read-only. Existing artist assignments are preserved.
"""
from pathlib import Path
from datetime import datetime
import hashlib, json, shutil
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
validate='-ValidateStanceVFX' in u.SystemLibrary.get_command_line()
backup=root/'Saved/Backups/StanceVFX'/datetime.now().strftime('%Y%m%d_%H%M%S')
folder='/Game/HeavensDivide/VFX/Stances'
report=[]
def disk(a):return root/'Content'/(a.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset')
def save(a):
 assert u.EditorAssetLibrary.save_loaded_asset(a,False),a.get_path_name()
def before(a):
 if validate:return
 src=disk(a)
 if src.exists():
  dst=backup/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def effect(name,source,color,pool=False):
 path=folder+'/'+name
 a=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
 if not a:
  assert not validate,'Missing effect '+path
  a=u.EditorAssetLibrary.duplicate_asset(source,path);assert a,source
  assert u.SwapVFXSetupLibrary.configure_stance_effect(a,u.LinearColor(*color,1),pool)
  save(a)
 if not validate and '-RetuneStanceVFX' in u.SystemLibrary.get_command_line() and pool:
  before(a);assert u.SwapVFXSetupLibrary.configure_stance_effect(a,u.LinearColor(*color,1),pool);save(a)
 report.append({'effect':path,'source':source})
 return a
vendor_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Content/Assets/VFX').rglob('*.uasset')}
# Flatten smoke mesh vertices after particle rotation, preserving the vendor material.
material_path=folder+'/M_PoisonGround'
if not u.EditorAssetLibrary.does_asset_exist(material_path):
 assert not validate,'Missing ground smoke material'
 m=u.EditorAssetLibrary.duplicate_asset('/Game/Assets/VFX/StylizedSmokeV1/Materials/Original/M_SmokeShader',material_path);assert m
 lib=u.MaterialEditingLibrary
 pos=lib.create_material_expression(m,u.MaterialExpressionWorldPosition,-700,600)
 pos.set_editor_property('world_position_shader_offset',u.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
 center=lib.create_material_expression(m,u.MaterialExpressionParticlePositionWS,-700,800)
 sub=lib.create_material_expression(m,u.MaterialExpressionSubtract,-450,600)
 lib.connect_material_expressions(center,'',sub,'A');lib.connect_material_expressions(pos,'',sub,'B')
 axis=lib.create_material_expression(m,u.MaterialExpressionConstant3Vector,-450,800)
 axis.set_editor_property('constant',u.LinearColor(0,0,.995,1))
 mul=lib.create_material_expression(m,u.MaterialExpressionMultiply,-200,600)
 lib.connect_material_expressions(sub,'',mul,'A');lib.connect_material_expressions(axis,'',mul,'B')
 lib.connect_material_property(mul,'',u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
 lib.recompile_material(m);save(m)
mi_path=folder+'/MI_PoisonGround'
if not u.EditorAssetLibrary.does_asset_exist(mi_path):
 assert not validate,'Missing ground smoke instance'
 mi=u.EditorAssetLibrary.duplicate_asset('/Game/Assets/VFX/StylizedSmokeV1/Materials/Mi_StylizedSmoke01',mi_path);assert mi
 u.MaterialEditingLibrary.set_material_instance_parent(mi,u.load_asset(material_path));save(mi)
impact='/Game/Assets/VFX/HitsImpactsV2/Particles/NiagaraSystems/NS_Impact_26'
hit='/Game/Assets/VFX/HitsImpactsV2/Particles/NiagaraSystems/NS_Impact_NinjaAttackProj'
slash='/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_10'
cross='/Game/Assets/VFX/CrossSlashesV1/Particles/NiagaraSystems/NS_CrossSlash01'
smoke='/Game/Assets/VFX/StylizedSmokeV1/Particles/NiagaraSystems/'
red=(2.0,.05,.09);purple=(.9,.12,2.0);gold=(2.0,1.0,.08);green=(.15,1.2,.025)
fx={
 'blood':effect('NS_BloodCritical',impact,red),
 'ninja':effect('NS_NinjaCritical',impact,purple),
 'poison':effect('NS_PoisonHit',hit,green),
 'pressure':effect('NS_NinjaPressure',hit,purple),
 'grind':effect('NS_GrindingHalt',impact,gold),
 'bloodrush':effect('NS_BloodRush',slash,red),
 'viperrush':effect('NS_ViperRush',slash,green),
 'burst':effect('NS_NinjaRadialBurst',slash,purple),
 'toxic':effect('NS_ToxicGround',smoke+'NS_StylizedSmoke_Loop_v09_Green',green,True),
 'bloom':effect('NS_VenomBloom',smoke+'NS_StylizedSmoke_Loop_v10_Arcane',purple,True),
}
reg=u.AssetRegistryHelpers.get_asset_registry();reg.scan_paths_synchronous(['/Game/HeavensDivide/Upgrades'],True)
cards={str(a.get_editor_property('upgrade_id')):a for d in reg.get_assets_by_path('/Game/HeavensDivide/Upgrades',True) if isinstance((a:=d.get_asset()),u.UpgradeDefinition)}
# Every entry is wired to an actual gameplay trigger. Scalable ranks inherit their mechanic's effect.
assignments={
 'BloodCritical':('blood',100,.12),
 'BloodRush':('bloodrush',150,1.0), 'FangDeadeye':('ninja',100,.12),
 'RelentlessFang':('pressure',100,.3), 'FangSplinter':('ninja',140,.15),
 'GrindingHalt':('grind',150,.25), 'ShurikenHunger':('pressure',160,.3),
 'SerratedEdge':('pressure',100,.3), 'BarrageStance':('poison',100,.15),
 'BarrageCritical':('ninja',100,.12),
 'ForkingProjectiles':('pressure',100,.15),
}
for id,(key,radius,interval) in assignments.items():
 a=cards[id];p=a.get_editor_property('presentation')
 if not p.get_editor_property('pulse_system'):
  assert not validate,'Missing card VFX '+id
  before(a);p.set_editor_property('pulse_system',fx[key]);p.set_editor_property('authored_radius',radius)
  p.set_editor_property('system_scale_parameter','User.Scale');p.set_editor_property('multiply_authored_system_scale',True)
  p.set_editor_property('minimum_spawn_interval',interval);p.set_editor_property('show_fallback_with_niagara',False)
  p.set_editor_property('lifetime_override',.65)
  a.set_editor_property('presentation',p);save(a)
 report.append({'card':id,'slot':'Presentation > Pulse System','effect':p.get_editor_property('pulse_system').get_path_name()})
# Viper's Rush grants movement speed, never an attack slash.
a=cards['BarrageRush'];p=a.get_editor_property('presentation')
if p.get_editor_property('pulse_system'):
 assert not validate,'Viper rush must not have an attack slash'
 before(a);p.set_editor_property('pulse_system',None);a.set_editor_property('presentation',p);save(a)
# Newer cards hide Runtime VFX until this authoring flag is enabled.
for id in [*assignments,'IaijutsuAOE']:
 a=cards[id]
 if not a.get_editor_property('has_runtime_presentation'):
  assert not validate,'Hidden Runtime VFX settings: '+id
  before(a);assert u.SwapVFXSetupLibrary.set_property_text(a,'bHasRuntimePresentation','True');save(a)
 assert a.get_editor_property('has_runtime_presentation')
for name in ['Ninja','Samurai']:
 bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name);assert bp
 cdo=u.get_default_object(bp.generated_class());component=cdo.get_component_by_class(u.NinjaBuildComponent if name=='Ninja' else u.AutoAttackComponent)
 settings={'toxic_ground_vfx':fx['toxic'],'venom_bloom_vfx':fx['bloom'],'fang_return_burst_vfx':fx['burst'],'shuriken_burst_vfx':fx['burst']} if name=='Ninja' else {'blood_echo_vfx':u.load_asset('/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_BloodStance')}
 changed=False;before(bp)
 for prop,value in settings.items():
  assert value
  existing=component.get_editor_property(prop)
  if not existing or (not validate and prop in ['fang_return_burst_vfx','shuriken_burst_vfx'] and existing.get_path_name().startswith(folder+'/NS_NinjaWheelBurst.')):
   assert not validate,'Missing component VFX '+prop
   component.set_editor_property(prop,value);changed=True
   if prop in ['toxic_ground_vfx','venom_bloom_vfx']:
    component.set_editor_property('poison_pool_vfx_reference_radius',100.0)
    component.set_editor_property('poison_pool_vfx_height_scale',.005)
  report.append({'blueprint':name,'slot':prop,'effect':component.get_editor_property(prop).get_path_name()})
 if name=='Ninja':
  # World-local meshes form a thin floor layer, so normal depth testing keeps enemies in front.
  if abs(component.get_editor_property('poison_pool_vfx_height_scale')-.005)>1e-6:
   assert not validate,'Pool VFX must remain at ground height'
   component.set_editor_property('poison_pool_vfx_height_scale',.005);changed=True
 if changed:u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
 combo=cdo.get_editor_property('combo_ability')
 report.append({'blueprint':name,'existing_combo':combo.export_text()})
# Retire the initial sparse cross-slash prototype only when no saved asset uses it.
old=folder+'/NS_NinjaWheelBurst'
if not validate and u.EditorAssetLibrary.does_asset_exist(old) and not u.EditorAssetLibrary.find_package_referencers_for_asset(old,True):
 before(u.load_asset(old));assert u.EditorAssetLibrary.delete_asset(old)
for path,digest in vendor_hashes.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Vendor asset modified: '+path
out=root/'Saved/StanceVFXAudit';out.mkdir(parents=True,exist_ok=True)
(out/'assignments.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('STANCE_VFX_ASSIGNMENTS_OK: '+str(len(assignments))+' card hooks; vendor assets unchanged')

