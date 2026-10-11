import unreal as u, json, re
from pathlib import Path
out=Path(u.Paths.project_saved_dir())/'CombatPolish'
audit=json.loads((out/'AssetAudit.json').read_text()); sizes=json.loads((out/'EnemySizes.json').read_text())
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
u.EditorLoadingAndSavingUtils.new_blank_map(False)
actors=u.get_editor_subsystem(u.EditorActorSubsystem); report={'enemies':[], 'players':{},'warnings':[]}
for row in sizes:
    path=next(p for p in audit if p.rsplit('/',1)[1].split('.')[0]=='BP_'+row['enemy'])
    bp=u.load_asset(path); actor=actors.spawn_actor_from_class(bp.generated_class(),u.Vector(len(report['enemies'])*600,0,600))
    cap=actor.get_component_by_class(u.CapsuleComponent);actual=cap.get_scaled_capsule_half_height()*2
    assert abs(actual-row['capsule_height'])<.02,(path,actual,row)
    mesh=actor.get_component_by_class(u.SkeletalMeshComponent)
    assert mesh.get_editor_property('skeletal_mesh_asset'),path
    assert mesh.get_editor_property('anim_class'),path
    report['enemies'].append({'name':row['enemy'],'collision_height':actual,'scale':str(actor.get_actor_scale3d())})
for name in ['Ninja','Samurai']:
    bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name)
    c=u.get_default_object(bp.generated_class()); swap=c.get_component_by_class(u.SwapPresentationComponent)
    montage=swap.get_editor_property('entrance_montage')
    report['players'][name]={'montage':montage.get_path_name() if montage else None,'mesh_offset':str(c.get_component_by_class(u.SkeletalMeshComponent).get_editor_property('relative_location'))}
    if montage:
        t=u.AssetExportTask();t.object=montage;t.exporter=u.ObjectExporterT3D();t.filename=str(out/(name+'Arrival.copy'));t.automated=True;t.prompt=False;t.replace_identical=True;u.Exporter.run_asset_export_task(t)
projectile=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_FireballProjectile')
p=actors.spawn_actor_from_class(projectile.generated_class(),u.Vector(0,1000,100))
report['projectile_effects']=[str(c.get_asset()) for c in p.get_components_by_class(u.NiagaraComponent)]
assert report['projectile_effects'],'Enemy fireball has no Niagara visual'
for path in ['/Game/HeavensDivide/Blueprints/Encounters/BP_MagnetUrn','/Game/HeavensDivide/Blueprints/Encounters/BP_EncounterDirector']:
    c=u.get_default_object(u.load_asset(path).generated_class())
    assert c.get_editor_property('magnet_class' if path.endswith('Urn') else 'magnet_urn_class'),path
for name in ['M_EncounterSurface','M_EncounterEnergy']:
    m=u.load_asset('/Game/HeavensDivide/Materials/'+name)
    assert m.get_editor_property('material_domain')==u.MaterialDomain.MD_SURFACE
    assert m.get_editor_property('shading_model')==u.MaterialShadingModel.MSM_UNLIT
(out/'Validation.json').write_text(json.dumps(report,indent=2));u.log('COMBAT_POLISH_VALIDATION_COMPLETE')
