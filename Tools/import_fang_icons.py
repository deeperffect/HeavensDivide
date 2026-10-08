"""Import the generated Fang icons. -ValidateFangIcons loads/checks without saving assets."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib,json,re,shutil,struct
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
art=root/'Art/UpgradeIcons'
rows=json.loads((art/'fang_generation.json').read_text())['cards']
definitions={r['id']:r for r in json.loads((root/'Tools/fang_build_upgrades.json').read_text())}
# Later support cards intentionally share their parent illustrations.
for uid, parent in {'FangPursuitChance':'FangKillingEdge', 'FangBurstPower':'FangResonance'}.items():
 row=dict(next(r for r in rows if r['id']==parent));row.update(id=uid,shared_icon_from=parent);rows.append(row)
assert len(rows)==len(definitions)==26 and {r['id'] for r in rows}==set(definitions)
validate='-ValidateFangIcons' in u.SystemLibrary.get_command_line()
backup=root/'Saved/Backups/FangIcons'/datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
work=root/'Saved/FangArtValidation';work.mkdir(parents=True,exist_ok=True)
def disk(p):return root/'Content'/(p.split('.')[0].removeprefix('/Game/')+'.uasset')
def asset_path(uid):return '/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_'+('' if uid.startswith('Ninja') else 'Ninja')+uid+('Stance' if uid=='ReturningFang' else '')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def export(card,stage):
 task=u.AssetExportTask();task.object=card;task.filename=str(work/(str(card.get_editor_property('upgrade_id'))+'_'+stage+'.copy'))
 task.exporter=u.ObjectExporterT3D();task.automated=True;task.prompt=False;task.replace_identical=True
 assert u.Exporter.run_asset_export_task(task)
 data=Path(task.filename).read_bytes();text=data.decode('utf-16') if data.startswith((b'\xff\xfe',b'\xfe\xff')) else data.decode('utf-8-sig')
 return hashlib.sha256('\n'.join(line.rstrip() for line in text.splitlines() if not re.match(r'^\s*(?:Icon|CardArtwork)=',line)).encode()).hexdigest()
targets={disk(r['texture']) for r in rows}|{disk(asset_path(r['id'])) for r in rows}
protected=set((root/'Content/HeavensDivide/Upgrades').rglob('*.uasset'))|set((root/'Content/HeavensDivide/Blueprints/UI').rglob('*.uasset'))|{disk('/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController')}
if not validate:protected-=targets
before={str(p):digest(p) for p in protected}
if not validate:
 backup.mkdir(parents=True)
 for p in targets|{art/'manifest.json',art/'tuning_baseline.json'}:
  if p.exists():
   dest=backup/p.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
 (backup/'protected_hashes.json').write_text(json.dumps(before,indent=2))
manifest=json.loads((art/'manifest.json').read_text());by_id={r['id']:r for r in manifest['cards']}
report=[]
for row in rows:
 uid=row['id'];source=root/row['file'];assert digest(source)==row['source_sha256'],uid
 data=source.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n'
 w,h=struct.unpack('>II',data[16:24]);assert w==h and w>=512,uid
 card=u.load_asset(asset_path(uid));assert card,uid
 gameplay=export(card,'Before')
 if not validate:
  task=u.AssetImportTask();task.filename=str(source);task.destination_path,task.destination_name=row['texture'].rsplit('/',1)
  task.automated=True;task.replace_existing=True;task.save=False
  u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);assert task.imported_object_paths,uid
  texture=u.load_asset(row['texture'])
  for k,v in dict(srgb=True,lod_group=u.TextureGroup.TEXTUREGROUP_UI,compression_settings=u.TextureCompressionSettings.TC_EDITOR_ICON,
    mip_gen_settings=u.TextureMipGenSettings.TMGS_NO_MIPMAPS,address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP,
    never_stream=True,max_texture_size=512).items():texture.set_editor_property(k,v)
  assert u.EditorAssetLibrary.save_loaded_asset(texture,False)
  card.set_editor_property('icon',texture);card.set_editor_property('card_artwork',texture)
  assert export(card,'After')==gameplay,uid+' gameplay changed during art import'
  assert u.EditorAssetLibrary.save_loaded_asset(card,False)
 else:texture=u.load_asset(row['texture'])
 assert card.get_editor_property('icon')==texture and card.get_editor_property('card_artwork')==texture,uid
 assert texture.get_editor_property('max_texture_size')==512 and texture.get_editor_property('never_stream'),uid
 definition=definitions[uid]
 if not validate:
  entry=by_id.get(uid,{})
  entry.update(id=uid,name=definition['name'],description=definition['description'],format=definition['format'],asset=asset_path(uid),
   in_pool=True,category='CURSED' if definition['kind']=='shrine' else 'NINJA',role=str(card.get_editor_property('role')),
   requirements=[str(x) for x in card.get_editor_property('prerequisite_upgrade_ids')],artwork=texture.get_path_name(),icon=texture.get_path_name(),
   file=row['file'],texture=row['texture'],prompt=row['prompt'],mode=row['mode'],source_sha256=row['source_sha256'],
   subject=row['prompt'].split('Subject: ',1)[-1])
  if uid not in by_id:manifest['cards'].append(entry);by_id[uid]=entry
 report.append(dict(id=uid,source_sha256=row['source_sha256'],gameplay_preserved=True))
for p,h in before.items():assert digest(Path(p))==h,'Unrelated asset changed: '+p
if not validate:
 for uid in ('CuttingReturn','FinalPursuit'):by_id[uid]['in_pool']=False
 manifest['total']=len(manifest['cards'])
 (art/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (art/'fang_import_report.json').write_text(json.dumps(dict(cards=report,backup=str(backup)),indent=2)+'\n')
u.log('FANG_ICONS_'+('VALIDATION' if validate else 'IMPORT')+'_OK: 24 unique icons, gameplay and unrelated assets preserved')
