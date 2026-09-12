"""Author 20 complete build families from the checked-in design catalog; preserve unrelated upgrades."""
import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/build_family_catalog.json').read_text())
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UpgradeDefinition)
def backup(path):
 rel=Path(path.removeprefix('/Game/')+'.uasset');src=root/'Content'/rel;dst=root/'Saved/Backups/BuildFamilies'/rel
 if src.exists() and not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def asset(path):
 backup(path)
 return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else asset_tools.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.UpgradeDefinition,factory)
def set_props(a,props):
 for k,v in props.items():a.set_editor_property(k,v)
def configure(r,uid,title,desc,role,synergy=False,scale=None):
 owner=r['owner'];folder='/Game/HeavensDivide/Upgrades/'+('Synergy' if synergy else owner)
 path=folder+('/DA_BuildSynergy_' if synergy else '/DA_Upgrade_'+owner)+uid
 a=asset(path)
 existing_art=a.get_editor_property('card_artwork')
 art=existing_art or unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/CardArt2/'+owner+('/BladeWave' if owner=='Samurai' else '/VenomousKunai'))
 legacy_evo=uid in ['RazorHalo','Starfall','BlackWeb','WitheringGarden']
 set_props(a,dict(upgrade_id=uid,display_name=title,description=desc,category=unreal.UpgradeCategory.SYNERGY if synergy else getattr(unreal.UpgradeCategory,owner.upper()),investment_owner=getattr(unreal.UpgradeInvestmentOwner,owner.upper()),role=unreal.UpgradeRole.EVOLUTION if legacy_evo else getattr(unreal.UpgradeRole,role.upper()),build_family_id=r['id'],max_level=5 if scale else 1,rarity=unreal.UpgradeRarity.LEGENDARY if legacy_evo else unreal.UpgradeRarity.RARE if role in ['Mechanic','Special'] else unreal.UpgradeRarity.COMMON,uses_rolled_rarity=bool(scale),prerequisite_upgrade_ids=[] if role=='Starter' else [r['id']],prerequisite_requirements=[],exclusivity_group='None',requires_meta_unlock=False,stat_modifiers=[],special_effects=[],card_artwork=art,icon=art))
 if legacy_evo:
  reqs=[]
  for suffix in ['Power','Area']:
   q=unreal.UpgradePrerequisiteRequirement();q.set_editor_property('upgrade_id',r['id']+suffix);q.set_editor_property('minimum_level',2);reqs.append(q)
  a.set_editor_property('prerequisite_requirements',reqs)
 if scale:
  values={'Power':[.2,.3,.45],'Area':[.12,.18,.25],'WideArc':[.12,.18,.25],'Haste':[.1,.15,.22]}[scale]
  mags=[]
  for rarity,v in zip([unreal.UpgradeRarity.COMMON,unreal.UpgradeRarity.RARE,unreal.UpgradeRarity.EPIC],values):
   m=unreal.UpgradeRarityMagnitude();m.set_editor_property('rarity',rarity);m.set_editor_property('magnitude',v);mags.append(m)
  a.set_editor_property('rarity_magnitudes',mags)
  fmt={'Power':'+{Percent}% family damage.','Area':'+{Percent}% family radius or reach.','WideArc':'+{Percent}% Blade Wave width and damage.','Haste':'+{Percent}% family recharge speed.'}[scale]
  if r['id']=='BladeWave' and scale=='Haste':fmt='+{Percent}% Blade Wave travel speed. Basic attack frequency is unchanged.'
  if r['id']=='NightThread' and scale=='Area':fmt='+{Percent}% jump distance; every 2 ranks adds 1 chain target.'
  a.set_editor_property('rolled_description_format',fmt)
 assert unreal.EditorAssetLibrary.save_loaded_asset(a,False),path
 return a
created=[]
for r in rows:
 desc=r['description']
 if r['kind']!='Legacy':desc+=f" Base damage {r['damage']}; radius/width {r['radius']} cm; recharge {r['cooldown']}s."
 created.append(configure(r,r['id'],r['name'],desc,'Starter'))
 for b in r['branches']:created.append(configure(r,b['id'],b['name'],b['description'],'Mechanic'))
 for suffix in r['scales']:
  uid='WideArc' if suffix=='WideArc' else r['id']+suffix
  label={'Power':'Force','Area':'Reach','WideArc':'Wide Arc','Haste':'Velocity' if r['id']=='BladeWave' else 'Rhythm'}[suffix]
  created.append(configure(r,uid,r['name']+': '+label,'Scale this ability independently.','Support',scale=suffix))
 y=r['synergy'];created.append(configure(r,y['id'],y['name'],y['description']+f" Follow-up: {int(y['factor']*100)}% of the preparing hit's damage"+(' split across the echoes' if y['kind']=='Echo' else '')+f"; {y['radius']} cm reach. Triggers at most once per "+('1 second' if y['kind']=='Recharge' else '0.35 seconds')+' per family. The same victim can be prepared again after 2 seconds.','Special',synergy=True))
assert len(created)==160
assert len({str(a.get_editor_property('upgrade_id')) for a in created})==160
path='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController';backup(path)
bp=unreal.load_asset(path);comp=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
# Replace by ID, preventing duplicate offers if an older asset used the same ID elsewhere.
new_by_id={str(a.get_editor_property('upgrade_id')):a for a in created}
pool=[];seen=set()
for old in comp.get_editor_property('upgrade_pool'):
 if not old:continue
 uid=str(old.get_editor_property('upgrade_id'))
 if uid in seen:continue
 pool.append(new_by_id.get(uid,old));seen.add(uid)
for uid,a in new_by_id.items():
 if uid not in seen:pool.append(a);seen.add(uid)
comp.set_editor_property('upgrade_pool',pool)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
unreal.log('BUILD_FAMILIES_ASSETS_PASS: 20 families, 60 branches, 60 scalables, 20 synergies; '+str(len(pool))+' unique pool entries')
