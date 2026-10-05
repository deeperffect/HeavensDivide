"""Author the Barrage tree. -ValidateBarrage is strictly asset-read-only.

Only the 21 Barrage definitions and the controller pool are saved.
Existing balance values are preserved; new values are seeded from barrage_build_upgrades.json.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, shutil
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
rows=json.loads((root/'Tools/barrage_build_upgrades.json').read_text())
folder='/Game/HeavensDivide/Upgrades/Ninja/'
controller='/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController'
validate='-ValidateBarrage' in unreal.SystemLibrary.get_command_line()
backup=root/'Saved/Backups/BarrageBuild'/datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
bp=unreal.load_asset(controller)
comp=unreal.get_default_object(bp.generated_class()).get_editor_property('player_upgrade_component')
pool=list(comp.get_editor_property('upgrade_pool'))
by_id={str(a.get_editor_property('upgrade_id')):a for a in pool if a}
changed={r['id'] for r in rows}
def path(r):return folder+'DA_Upgrade_'+('' if r['id'].startswith('Ninja') else 'Ninja')+r['id']
def disk(p):return root/'Content'/(p.split('.')[0].removeprefix('/Game/')+'.uasset')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def props(a,**kw):
 for k,v in kw.items():a.set_editor_property(k,v)
def back_up(p):
 f=disk(p)
 if f.exists():
  dest=backup/f.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
protected={str(f):digest(f) for f in (root/'Content/HeavensDivide/Upgrades').rglob('*.uasset') if f not in {disk(path(r)) for r in rows}}
if validate: protected[str(disk(controller))]=digest(disk(controller))
else:
 back_up(controller)
 for r in rows:
  p=path(r);back_up(p)
  a=unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else None
  if not a:
   factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.UpgradeDefinition)
   a=unreal.AssetToolsHelpers.get_asset_tools().create_asset(p.rsplit('/',1)[1],folder.rstrip('/'),unreal.UpgradeDefinition,factory)
   source=by_id.get(r['art'],by_id['BarrageStance']);art=source.get_editor_property('card_artwork')
   props(a,icon=art,card_artwork=art)
  balance={str(k):v for k,v in a.get_editor_property('balance_parameters').items()}
  for k,v in r['balance'].items():balance.setdefault(k,float(v))
  mags=[]
  for rarity,value in zip([unreal.UpgradeRarity.COMMON,unreal.UpgradeRarity.RARE,unreal.UpgradeRarity.EPIC],r['magnitudes']):
   m=unreal.UpgradeRarityMagnitude();props(m,rarity=rarity,magnitude=value);mags.append(m)
  requires=([] if r['kind'] in ('stance','basic') else ['BarrageStance'])+r['requires']
  props(a,upgrade_id=r['id'],display_name=r['name'],description=r['description'],max_level=r['levels'],
   category=unreal.UpgradeCategory.CURSED if r['kind']=='shrine' else unreal.UpgradeCategory.NINJA,
   investment_owner=unreal.UpgradeInvestmentOwner.NINJA,role=unreal.UpgradeRole.STARTER if r['kind']=='stance' else unreal.UpgradeRole.SUPPORT if r['levels']>1 else unreal.UpgradeRole.MECHANIC,
   rarity=unreal.UpgradeRarity.RARE if r['kind'] in ('rare','shrine','stance') else unreal.UpgradeRarity.COMMON,
   uses_rolled_rarity=bool(mags),rarity_magnitudes=mags,rolled_description_format=r['format'],
   prerequisite_upgrade_ids=requires,prerequisite_requirements=[],balance_parameters=balance,
   exclusivity_group='NinjaWeaponStance' if r['kind']=='stance' else 'None',
   stat_modifiers=[],special_effects=[],requires_meta_unlock=False,unlocked_by_default=True)
  unreal.SystemLibrary.execute_console_command(None,'setnopec '+a.get_path_name()+' bHasRuntimeBalance True')
  assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
  by_id[r['id']]=a
 pool=[a for a in pool if a and str(a.get_editor_property('upgrade_id')) not in changed|{'FocusedVolley','Crescendo'}]+[by_id[r['id']] for r in rows]
 comp.set_editor_property('upgrade_pool',pool);unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
 (backup/'protected_hashes.json').write_text(json.dumps(protected,indent=2))
ids=[str(a.get_editor_property('upgrade_id')) for a in pool]
assert len(ids)==len(set(ids))
assert 'FocusedVolley' not in ids and 'Crescendo' not in ids
for r in rows:
 a=next(a for a in pool if str(a.get_editor_property('upgrade_id'))==r['id'])
 assert a.get_editor_property('max_level')==r['levels']
 assert bool(a.get_editor_property('uses_rolled_rarity'))==bool(r['magnitudes'])
 assert a.get_editor_property('card_artwork') and a.get_editor_property('icon')
 assert set(str(x) for x in a.get_editor_property('prerequisite_upgrade_ids'))==set(([] if r['kind'] in ('stance','basic') else ['BarrageStance'])+r['requires'])
 assert (a.get_editor_property('category')==unreal.UpgradeCategory.CURSED)==(r['kind']=='shrine')
for p,h in protected.items():assert digest(Path(p))==h,'Unrelated asset changed: '+p
assert {k:sum(r['kind']==k for r in rows) for k in ('one_time','normal','rare','shrine')}==dict(one_time=6,normal=5,rare=6,shrine=3)
unreal.log('BARRAGE_BUILD_OK: 6 / 5 / 6 / 3, three convertible basic investments, '+str(len(ids))+' unique pool cards; protected assets unchanged')
