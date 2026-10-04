import unreal as u,json
from pathlib import Path
out=Path(u.Paths.project_saved_dir())/'CombatAudio';out.mkdir(parents=True,exist_ok=True)
r={'objects':{},'upgrades':[],'api':{},'sounds':[]}
for cls in ['MetaSoundBuilderSubsystem','MetaSoundSourceBuilder','MetaSoundEditorSubsystem','MetasoundFrontendLiteral','MetasoundFrontendLiteralBlueprintAccess','MetasoundFrontendClassName']:
 c=getattr(u,cls,None)
 if c:r['api'][cls]={n:getattr(c,n).__doc__ for n in dir(c) if any(s in n for s in ['create_source','add_node_by','literal','connect_nodes','build_to_asset','find_node','set_node_input','get_node','set_node_location','convert'])}
ss=u.get_engine_subsystem(u.SubobjectDataSubsystem)
paths=u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints',True,False)
for path in paths:
 name=path.rsplit('/',1)[-1].split('.')[0]
 if not name.startswith('BP_') or not any(k in name for k in ['Samurai','Ninja','Enemy','Projectile','Pickup','Chest','Trial']):continue
 a=u.load_asset(path)
 if not isinstance(a,u.Blueprint):continue
 c=u.get_default_object(a.generated_class());objects=[c]
 try:
  for h in ss.k2_gather_subobject_data_for_blueprint(a):
   o=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(u.SubobjectDataBlueprintFunctionLibrary.get_data(h),a)
   if o and o!=c:objects.append(o)
 except:pass
 d={}
 for o in objects:
  props={}
  for k in dir(o):
   if any(t in k for t in ['sound','combo_ability','impact_feedback','montage','presentation']):
    try:props[k]=str(o.get_editor_property(k))
    except:pass
  if props:d[o.get_name()]=props
 r['objects'][path]=d
for path in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Upgrades',True,False):
 a=u.load_asset(path)
 if isinstance(a,u.UpgradeDefinition):r['upgrades'].append({'path':path,'id':str(a.get_editor_property('upgrade_id')),'runtime':a.get_editor_property('has_runtime_presentation'),'presentation':str(a.get_editor_property('presentation'))})
for path in u.EditorAssetLibrary.list_assets('/Game/Assets/Sounds',True,False):
 a=u.load_asset(path);r['sounds'].append({'path':path,'class':a.get_class().get_name()})
 if a.get_name()=='MS_Ninja_Impact':
  t=u.AssetExportTask();t.object=a;t.exporter=u.ObjectExporterT3D();t.filename=str(out/'ExistingMetaSound.copy');t.automated=True;t.prompt=False;t.replace_identical=True;u.Exporter.run_asset_export_task(t)
(out/'audit.json').write_text(json.dumps(r,indent=2));u.log('COMBAT_AUDIO_AUDIT_READY')
