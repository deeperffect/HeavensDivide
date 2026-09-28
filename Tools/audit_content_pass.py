import json
from pathlib import Path
import unreal as u

out=Path(u.Paths.project_saved_dir())/'ContentPass';out.mkdir(parents=True,exist_ok=True)
registry=u.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
def val(v):
    if isinstance(v,u.Object):return v.get_path_name()
    if isinstance(v,(str,bool,int,float)) or v is None:return v
    if isinstance(v,(list,u.Array)):return [val(x) for x in v]
    return str(v)
def props(o,fields):
    result={}
    for k in fields.split():
        try:result[k]=val(o.get_editor_property(k))
        except Exception:pass
    return result
report={'upgrades':[],'vfx':[],'enemies':{},'players':{},'maps':{},'chests':{}}
for a in registry.get_assets_by_path('/Game',True):
    path=str(a.package_name);kind=str(a.asset_class_path.asset_name)
    if kind=='NiagaraSystem' and ('VFX' in path):report['vfx'].append(path)
    if kind=='UpgradeDefinition':
        obj=a.get_asset();row=props(obj,'upgrade_id display_name description max_level special_effect upgrade_role balance_parameters has_runtime_presentation character_stat_modifiers shared_stat_modifiers')
        row['path']=path
        row['presentation']=props(obj.get_editor_property('presentation'),'pulse_system line_system warning_system impact_system detonation_system override_family_visuals authored_radius scale lifetime_override')
        report['upgrades'].append(row)
    if kind=='Blueprint' and '/Blueprints/EnemyCharacters/' in path and '/BP_' in path:
        bp=a.get_asset();cdo=u.get_default_object(bp.generated_class())
        row=props(cdo,'drop_category xp_reward experience_per_pickup experience_pickup_class contact_damage_amount contact_damage_interval attack_damage attack_cooldown')
        for key in ['health_component','character_movement','death_presentation']:
            try:row[key]=props(cdo.get_editor_property(key),'max_health max_walk_speed death_niagara_system')
            except Exception:pass
        report['enemies'][path]=row
for name in ['Ninja','Samurai']:
    bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+name);cdo=u.get_default_object(bp.generated_class())
    report['players'][name]={}
    for c in cdo.get_components_by_class(u.ActorComponent):
        fields=[s for s in dir(c) if any(w in s.lower() for w in ['vfx','niagara','damage','cooldown','effect','system']) and not s.startswith('_')]
        report['players'][name][c.get_name()]=props(c,' '.join(fields))
for name in ['SK_Small_Treasure_Chest','SK_Medium_Treasure_Chest']:
    mesh=u.load_asset('/Game/Assets/Environment/Ancient_Ruins/Meshes/'+name)
    mod=u.SkeletonModifier();mod.set_skeletal_mesh(mesh)
    report['chests'][name]=[{'bone':str(n),'local':str(mod.get_bone_transform(n,False))} for n in mod.get_all_bone_names()]
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
for name in ['Lvl_B1_Lvl1','Lvl_B1_Lvl1-2','Lvl_B1_Lvl1-3']:
    assert levels.load_level('/Game/Maps/'+name)
    rows=[]
    for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
        if any(t in a.get_class().get_name() for t in ['Spawner','Objective','Bounds','SpawnArea','PlayerStart']):
            row={'name':a.get_name(),'class':a.get_class().get_path_name(),'location':str(a.get_actor_location())}
            row.update(props(a,'pressure_phases pressure_events enemy_spawn_entries absolute_hard_alive_cap'))
            rows.append(row)
    report['maps'][name]=rows
(out/'audit.json').write_text(json.dumps(report,indent=2))
u.log('CONTENT_PASS_AUDIT_READY')
