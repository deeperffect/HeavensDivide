"""Read-only validation of authored encounters, map dependencies, and cooking references."""
import unreal as u
from pathlib import Path
import json
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/EncounterExpansion'
setup=json.loads((out/'Setup.json').read_text());reg=u.AssetRegistryHelpers.get_asset_registry();reg.search_all_assets(True)
maps=sorted(str(a.package_name) for a in reg.get_assets_by_class(u.TopLevelAssetPath('/Script/Engine','World'),True) if str(a.package_name).startswith('/Game/'))
assert maps==sorted(setup['kept_maps']),(maps,setup['kept_maps'])
report={'maps':maps,'spawn_phases':[],'levels':{},'materials':{},'balance_overrides':{}}
for name in ['M_EncounterWarning','M_EnemyRoleRing','M_EnemyRim']:
    material=u.load_asset('/Game/HeavensDivide/Materials/'+name);assert material
    report['materials'][name]=str(material.get_editor_property('material_domain'))
    assert u.MaterialEditingLibrary.get_material_property_input_node(material,u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert u.MaterialEditingLibrary.get_material_property_input_node(material,u.MaterialProperty.MP_OPACITY)
    if name=='M_EncounterWarning':assert material.get_editor_property('material_domain')==u.MaterialDomain.MD_DEFERRED_DECAL
sp=u.get_default_object(u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner').generated_class())
entries=sp.get_editor_property('enemy_spawn_entries');assert len(entries)==20
phases=sp.get_editor_property('pressure_phases');assert len(phases)==12
for i,p in enumerate(phases):
    assert p.start_time_seconds==i*60
    assert p.end_time_seconds==((i+1)*60 if i<11 else 0)
    assert p.global_max_alive<=160
    pop=p.enemy_population_entries;assert len(pop)==16
    assert len([e for e in pop if e.desired_population>0])==2
    assert len({e.enemy_class.get_path_name() for e in pop})==len(pop)
    report['spawn_phases'].append({'start':p.start_time_seconds,'end':p.end_time_seconds,'cap':p.global_max_alive,
        'normal_target':sum(e.desired_population for e in pop),'special_cap':sum(e.max_population for e in pop if e.desired_population==0)})
for row in setup['enemies']:
    name=row['name'];bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_'+name);assert bp
    cdo=u.get_default_object(bp.generated_class())
    assert cdo.get_health_component().get_editor_property('max_health')==row['health']
    assert cdo.get_editor_property('ability_damage')==row['damage']
    assert any(e.enemy_class==bp.generated_class() for e in entries)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for p in ['/Game/Maps/Lvl_MainMenu','/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/TestMap','/Game/Maps/Lvl_B1_Arena']:
    assert level.load_level(p),p
    all_actors=actors.get_all_level_actors()
    if p.endswith('Lvl1'):
        directors=[a for a in all_actors if isinstance(a,u.EncounterDirector)];assert len(directors)==1
        placed=[a for a in all_actors if isinstance(a,u.EnemySpawner)];assert len(placed)==1
        assert len(placed[0].get_editor_property('enemy_spawn_entries'))==20
        assert len(placed[0].get_editor_property('pressure_phases'))==12
        assert directors[0].get_editor_property('marching_enemy_class')
        assert directors[0].get_editor_property('healing_urn_class')
        grades=[a for a in all_actors if a.get_actor_label()=='Bloodshift Combat Grade'];assert len(grades)==1
    if p.endswith('Arena'):
        bosses=[a for a in all_actors if isinstance(a,u.FinalBossBase)];assert len(bosses)==1
        assert isinstance(bosses[0],u.CinderOracleBoss)
    report['levels'][p]={'loaded':True,'actors':len(all_actors)}
for config in (root/'Saved/Config').rglob('*TesterBalance.ini'):
    report['balance_overrides'][str(config.relative_to(root))]=config.read_text(encoding='utf-8-sig')
(out/'Validation.json').write_text(json.dumps(report,indent=2))
u.log('ENCOUNTER_ASSETS_VALIDATION_PASS')
