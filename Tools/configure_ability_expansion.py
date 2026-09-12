"""Create four ability families and their scalable support/evolution cards."""
import unreal,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
assets=unreal.AssetToolsHelpers.get_asset_tools()
def backup(path):
 rel=Path(path.removeprefix('/Game/').split('.')[0]+'.uasset'); src=root/'Content'/rel
 dst=root/'Saved/Backups/AbilityExpansion'/rel
 if src.exists() and not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def make_material(name,ring):
 path='/Game/HeavensDivide/Materials/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):
  m=unreal.load_asset(path)
  if ring:
   color=unreal.MaterialEditingLibrary.get_material_property_input_node(m,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
   mask=unreal.MaterialEditingLibrary.get_material_property_input_node(m,unreal.MaterialProperty.MP_OPACITY_MASK)
   assert color and mask
   if color.get_editor_property('desc')!='Ring luminance mask':
    backup(path)
    result=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionMultiply,-100,100)
    result.set_editor_property('desc','Ring luminance mask')
    assert unreal.MaterialEditingLibrary.connect_material_expressions(color,'',result,'A')
    assert unreal.MaterialEditingLibrary.connect_material_expressions(mask,'',result,'B')
    assert unreal.MaterialEditingLibrary.connect_material_property(result,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    data=m.get_editor_property('editor_only_data')
    unreal.SystemLibrary.execute_console_command(None,'setnopec '+data.get_path_name()+' EmissiveColor (UseConstant=False)')
    unreal.MaterialEditingLibrary.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
  return m
 m=assets.create_asset(name,'/Game/HeavensDivide/Materials',unreal.Material,unreal.MaterialFactoryNew())
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 m.set_editor_property('two_sided',True)
 m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if ring else unreal.BlendMode.BLEND_OPAQUE)
 color=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionVectorParameter,-600,0)
 color.set_editor_property('parameter_name','Tint');color.set_editor_property('default_value',unreal.LinearColor(3,1,0.2,1))
 strength=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionScalarParameter,-600,150)
 strength.set_editor_property('parameter_name','Intensity');strength.set_editor_property('default_value',1)
 mult=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionMultiply,-300,0)
 unreal.MaterialEditingLibrary.connect_material_expressions(color,'',mult,'A');unreal.MaterialEditingLibrary.connect_material_expressions(strength,'',mult,'B')
 unreal.MaterialEditingLibrary.connect_material_property(mult,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 data=m.get_editor_property('editor_only_data')
 unreal.SystemLibrary.execute_console_command(None,'setnopec '+data.get_path_name()+' EmissiveColor (UseConstant=False)')
 if ring:
  uv=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate,-600,300)
  mask=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionCustom,-300,300)
  inp=unreal.CustomInput();inp.set_editor_property('input_name','UV');mask.set_editor_property('inputs',[inp])
  mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
  mask.set_editor_property('code','float r=length(UV-float2(0.5,0.5)); return step(0.44,r)*step(r,0.49);')
  unreal.MaterialEditingLibrary.connect_material_expressions(uv,'',mask,'UV')
  unreal.MaterialEditingLibrary.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY_MASK)
  ring_color=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionMultiply,-100,100)
  ring_color.set_editor_property('desc','Ring luminance mask')
  unreal.MaterialEditingLibrary.connect_material_expressions(mult,'',ring_color,'A')
  unreal.MaterialEditingLibrary.connect_material_expressions(mask,'',ring_color,'B')
  unreal.MaterialEditingLibrary.connect_material_property(ring_color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  unreal.SystemLibrary.execute_console_command(None,'setnopec '+data.get_path_name()+' OpacityMask (UseConstant=False)')
 unreal.MaterialEditingLibrary.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
 return m
make_material('M_AbilityRing',True);make_material('M_AbilityStreak',False)
families=[
 ('Samurai','SteelTempest','Steel Tempest','Every 4 seconds, release a golden ring around Samurai for 24 base damage in a 260 cm radius.','RazorHalo','Razor Halo','Steel Tempest echoes after 0.3 seconds for 65% damage.','Area'),
 ('Samurai','Heavenfall','Heavenfall','Every 4 seconds, target the densest nearby crowd. After 0.5 seconds, strike a 280 cm radius for 42 base damage and apply Bleed. With Marked Blade, mark survivors.','Starfall','Starfall','Heavenfall strikes up to 3 separate nearby groups, prioritizing enemies outside earlier strike areas.','Deathblow'),
 ('Ninja','NightThread','Night Thread','Every 3.5 seconds, a shadow thread strikes up to 3 nearby enemies for 25 base damage each, jumping up to 360 cm between targets. Prioritizes bleeding enemies and applies Poison to them.','BlackWeb','Black Web','Night Thread leaves a small delayed burst at each hit position for 50% damage.','KunaisBounce'),
 ('Ninja','VenomGarden','Venom Garden','Plant a 340 cm field in a nearby crowd: 8 pulses of 4 base damage, 1.1 seconds apart. Poisons unpoisoned enemies. Samurai melee hits inside detonate the field for 150% remaining damage in a larger area and apply Bleed. 9 second recharge; one field at a time.','WitheringGarden','Withering Garden','Venom Garden lasts for 11 pulses, then bursts for triple pulse damage with 25% more radius. Samurai detonation includes this final burst in its remaining-damage payout.','VenomousKunai')]
factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UpgradeDefinition)
created=[]
for owner,family,title,desc,evo,evo_title,evo_desc,art in families:
 folder='/Game/HeavensDivide/Upgrades/'+owner
 # Reuse existing authored character artwork, keeping cards consistent with the current UI.
 art_path='/Game/HeavensDivide/Blueprints/UI/CardArt2/'+owner+'/'+art
 image=unreal.load_asset(art_path)
 if not image:image=unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/CardArt2/'+owner+('/BladeWave' if owner=='Samurai' else '/VenomousKunai'))
 entries=[('',title,desc,'Starter',1),('Power',title+': Force','Increase this ability\'s damage.','Support',5),('Area',title+': Reach','Increase this ability\'s radius or chain distance.','Support',5),('Haste',title+': Rhythm','Increase this ability\'s recharge speed.','Support',5),('Evolution',evo_title,evo_desc,'Evolution',1)]
 for suffix,name,description,role,maxlevel in entries:
  uid=evo if suffix=='Evolution' else family+suffix
  path=folder+'/DA_Upgrade_'+owner+uid;backup(path)
  a=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(path.rsplit('/',1)[1],folder,unreal.UpgradeDefinition,factory)
  settings={'upgrade_id':uid,'display_name':name,'description':description,'category':getattr(unreal.UpgradeCategory,owner.upper()),'investment_owner':getattr(unreal.UpgradeInvestmentOwner,owner.upper()),'role':getattr(unreal.UpgradeRole,role.upper()),'build_family_id':family,'max_level':maxlevel,'rarity':unreal.UpgradeRarity.LEGENDARY if suffix=='Evolution' else unreal.UpgradeRarity.COMMON,'uses_rolled_rarity':role=='Support','stat_modifiers':[],'special_effects':[],'prerequisite_upgrade_ids':[] if not suffix else [family],'prerequisite_requirements':[],'card_artwork':image,'icon':image}
  for k,v in settings.items():a.set_editor_property(k,v)
  if role=='Support':
   magnitudes={'Power':[.20,.30,.45],'Area':[.12,.18,.25],'Haste':[.10,.15,.22]}[suffix]
   mags=[]
   for rarity,value in zip([unreal.UpgradeRarity.COMMON,unreal.UpgradeRarity.RARE,unreal.UpgradeRarity.EPIC],magnitudes):
    entry=unreal.UpgradeRarityMagnitude();entry.set_editor_property('rarity',rarity);entry.set_editor_property('magnitude',value);mags.append(entry)
   a.set_editor_property('rarity_magnitudes',mags)
   text={'Power':'+{Percent}% ability damage.','Area':'+{Percent}% ability radius.','Haste':'+{Percent}% ability recharge speed. Independent of basic attack speed.'}[suffix]
   if family=='NightThread' and suffix=='Area':text='+{Percent}% jump distance. Every 2 ranks also adds 1 target.'
   a.set_editor_property('rolled_description_format',text)
  if suffix=='Evolution':
   reqs=[]
   for support in ['Power','Area']:
    req=unreal.UpgradePrerequisiteRequirement();req.set_editor_property('upgrade_id',family+support);req.set_editor_property('minimum_level',2);reqs.append(req)
   a.set_editor_property('prerequisite_requirements',reqs)
  assert unreal.EditorAssetLibrary.save_loaded_asset(a,False);created.append(a)
controller_path='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController';backup(controller_path)
bp=unreal.load_asset(controller_path);cdo=unreal.get_default_object(bp.generated_class());comp=cdo.get_editor_property('player_upgrade_component')
pool=[a for a in comp.get_editor_property('upgrade_pool') if a];ids={str(a.get_editor_property('upgrade_id')) for a in pool}
for a in created:
 if str(a.get_editor_property('upgrade_id')) not in ids:pool.append(a);ids.add(str(a.get_editor_property('upgrade_id')))
comp.set_editor_property('upgrade_pool',pool)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
unreal.log('ABILITY_EXPANSION_ASSETS_PASS: '+str(len(created))+' cards; pool='+str(len(pool)))

