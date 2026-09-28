"""Apply the September content pass. Run in Unreal; VERIFY_ONLY=True checks saved assets."""
import json, shutil
from pathlib import Path
from datetime import datetime
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
BASE=json.loads((ROOT/'Tools/enemy_wave_schedule.json').read_text())
VERIFY_ONLY=globals().get('VERIFY_ONLY',False)
BACKUP=ROOT/'Saved/Backups/ContentPass'/datetime.now().strftime('%Y%m%d_%H%M%S')
REPORT={'upgrades':{},'maps':{},'characters':{},'balance':{}}
def get(o,k): return o.get_editor_property(k)
def setv(o,**kw):
    for k,v in kw.items(): o.set_editor_property(k,v)
    return o
def st(cls,**kw): return setv(cls(),**kw)
def backup(path,ext='.uasset'):
    if VERIFY_ONLY:return
    rel=Path(path.removeprefix('/Game/')+ext)
    dst=BACKUP/rel
    if not dst.exists():
        dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/'Content'/rel,dst)
def save(asset):
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False),asset.get_path_name()
def load(path):
    result=u.load_asset(path)
    assert result,path
    return result

# Distinct finite bursts from the existing VFX library. Modifier cards inherit their source attack.
SMOKE='/Game/Assets/VFX/StylizedSmokeV1/Particles/NiagaraSystems/'
IMPACT='/Game/Assets/VFX/HitsImpactsV2/Particles/NiagaraSystems/'
EFFECTS={
 'OverkillBurst':(SMOKE+'NS_StylizedSmoke_Impact_v01',180,(1,.35,.03),.9,.12),
 'BloodTransfer':(SMOKE+'NS_StylizedSmoke_Impact_v03',230,(.7,.025,.07),.8,.15),
 'BleedingEdge':(IMPACT+'NS_Impact_SamuraiAttack',100,(.8,.025,.05),.55,.10),
 'VenomousKunai':(SMOKE+'NS_StylizedSmoke_Impact_v02',180,(.15,.65,.015),.6,.12),
 'EmbeddedBlades':(IMPACT+'NS_Impact_NinjaAttackProj',100,(.65,.2,1),.55,.1),
 'GrandEntrance':(SMOKE+'NS_StylizedSmoke_Impact_v01',250,(1,.45,.08),.9,.15),
 'TagTeam':(IMPACT+'NS_Impact_NinjaAttackProj',130,(.5,.15,.85),.5,.12),
}
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
def smoke_variant(key,source,color):
    folder='/Game/HeavensDivide/VFX/Upgrades';name='NS_'+key
    path=folder+'/'+name
    if u.EditorAssetLibrary.does_asset_exist(path):
        obj=load(path)
        if not VERIFY_ONLY:
            backup(path)
            assert u.SwapVFXSetupLibrary.tint_smoke_system(obj,u.LinearColor(*color,1))
            save(obj)
        return obj
    assert not VERIFY_ONLY,'Missing variant '+path
    obj=u.AssetToolsHelpers.get_asset_tools().duplicate_asset(name,folder,load(source))
    assert obj and u.SwapVFXSetupLibrary.tint_smoke_system(obj,u.LinearColor(*color,1))
    save(obj);return obj

for data in registry.get_assets_by_path('/Game/HeavensDivide/Upgrades',recursive=True):
    if str(data.asset_class_path.asset_name)!='UpgradeDefinition':continue
    card=data.get_asset(); key=str(get(card,'upgrade_id')); p=get(card,'presentation')
    if key in EFFECTS:
        path,radius,color,lifetime,interval=EFFECTS[key]
        slot='line_system' if key=='TagTeam' else 'pulse_system'
        current=get(p,slot)
        managed=current is None or current.get_path_name().split('.')[0] in (path,'/Game/HeavensDivide/VFX/Upgrades/NS_'+key)
        if not VERIFY_ONLY and managed:
            backup(str(data.package_name))
            is_smoke=path.startswith(SMOKE)
            effect=smoke_variant(key,path,color) if is_smoke else load(path)
            setv(p,**{slot:effect},authored_radius=float(radius),lifetime_override=lifetime,
                 minimum_spawn_interval=interval,override_color=True,color=u.LinearColor(*color,1),
                 scale_system_to_radius=(key!='TagTeam'),radius_parameter='None',duration_parameter='None',
                 color_parameter='None' if is_smoke else 'User.Color 1',
                 system_scale_parameter='User._Scale' if is_smoke else 'User.Scale')
            if key=='TagTeam':setv(p,scale=u.Vector(.45,.45,.45))
            setv(card,presentation=p)
            assert u.SwapVFXSetupLibrary.set_property_text(card,'bHasRuntimePresentation','True')
            save(card)
        assert get(get(card,'presentation'),slot),key+' missing VFX'
        if managed:assert str(get(get(card,'presentation'),'system_scale_parameter')) in ('User._Scale','User.Scale')
        REPORT['upgrades'][key]='Assigned '+get(get(card,'presentation'),slot).get_path_name()
    elif key.startswith('Global') or key in ['SamuraiArea','SamuraiHeavyBlade','SamuraiTempo','Handoff','SynergySwapRestoresDashCharge','MarkedBlade']:
        REPORT['upgrades'][key]='Stat/resource/status modifier; existing attack, HUD or mark presentation'
    else:
        REPORT['upgrades'][key]='Existing weapon/projectile/clone presentation or inherited source-attack modifier'

classes={}
for name,e in BASE['enemies'].items():
    bp=load(e['path']);classes[name]=bp.generated_class()
    cdo=u.get_default_object(classes[name])
    hp=cdo.get_component_by_class(u.HealthComponent)
    # Reduce sponge health, reward threatening enemies, preserve the early fodder breakpoints.
    tuning={'Ogre':(260,12),'Gorilla':(1400,120),'DevilRanged':(55,5),'GoblinBomb':(35,5)}
    if name in tuning:
        health,xp=tuning[name]
        if not VERIFY_ONLY:
            backup(e['path']);cdo.modify();hp.modify()
            setv(hp,max_health=float(health));setv(cdo,xp_reward=xp)
            if name=='Gorilla':setv(cdo,drop_category=u.EnemyDropCategory.ELITE)
            u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
        cdo=u.get_default_object(bp.generated_class());hp=cdo.get_component_by_class(u.HealthComponent)
        assert get(hp,'max_health')==health and get(cdo,'xp_reward')==xp,name
        if name=='Gorilla':assert get(cdo,'drop_category')==u.EnemyDropCategory.ELITE
    REPORT['balance'][name]={'health':get(hp,'max_health'),'xp':get(cdo,'xp_reward')}

for character in ['Samurai','Ninja']:
    path='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+character
    bp=load(path);cdo=u.get_default_object(bp.generated_class());ability=get(cdo,'combo_ability')
    if not get(ability,'vfx') and not VERIFY_ONLY:
        backup(path)
        setv(ability,vfx=smoke_variant('SamuraiActive',SMOKE+'NS_StylizedSmoke_Impact_v01',(1,.4,.04)),
             vfx_radius_parameter='User._Scale',vfx_reference_radius=200.,vfx_scale=u.Vector(1,1,1),
             vfx_duration_parameter='None',spawn_vfx_every_pulse=True,vfx_visibility_duration=.65,
             vfx_offset=u.Vector(0,0,-80))
        setv(cdo,combo_ability=ability);u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    assert get(get(u.get_default_object(bp.generated_class()),'combo_ability'),'vfx'),character+' ability VFX'
    REPORT['characters'][character+' active ability']=str(get(ability,'vfx'))
    if character=='Ninja':
        attack=cdo.get_component_by_class(u.AutoAttackComponent)
        projectile=get(attack,'projectile_class')
        obj=u.get_default_object(projectile);feedback=get(obj,'impact_feedback')
        if not get(feedback,'hit_sound') and not VERIFY_ONLY:
            pp=projectile.get_path_name().split('.')[0];backup(pp)
            setv(feedback,hit_sound=load('/Game/Assets/Sounds/Ninja/MS_Ninja_Impact'))
            if not get(feedback,'hit_niagara_system'):setv(feedback,hit_niagara_system=load(IMPACT+'NS_Impact_NinjaAttackProj'))
            setv(obj,impact_feedback=feedback)
            pb=load(pp);u.BlueprintEditorLibrary.compile_blueprint(pb);save(pb)
        assert get(get(u.get_default_object(projectile),'impact_feedback'),'hit_sound')

# Per-map overrides, without changing terrain, objectives, navmeshes or spawner Blueprint defaults.
PROFILES=[
 {'map':'Lvl_B1_Lvl1','name':'First steps','scale':.90,'cadence':1.05,'events':(55.,70.),'arc':100.,
  'mixes':[('Grunt','Creature'),('GoblinCrawler','Grunt'),('Bear','Grunt'),('FloatingSkeleton','Creature'),('Fish','Grunt'),('Creature','Bear')],
  'rushes':[('GruntAdvance','Grunt',8,150),('CrawlerFlank','GoblinCrawler',7,270)]},
 {'map':'Lvl_B1_Lvl1-2','name':'The pursuit','scale':1.,'cadence':1.,'events':(38.,50.),'arc':45.,
  'mixes':[('Grunt','GoblinCrawler'),('GoblinCrawler','Creature'),('Fish','Grunt'),('FloatingSkeleton','GoblinCrawler'),('Creature','Fish'),('Bear','GoblinCrawler')],
  'rushes':[('CrawlerRush','GoblinCrawler',12,120),('FishRush','Fish',10,210),('SkeletonFlank','FloatingSkeleton',10,330)]},
 {'map':'Lvl_B1_Lvl1-3','name':'The siege','scale':.85,'cadence':1.10,'events':(48.,62.),'arc':75.,
  'mixes':[('Grunt','Bear'),('Creature','Grunt'),('Bear','FloatingSkeleton'),('Creature','Bear'),('FloatingSkeleton','Creature'),('Bear','Grunt')],
  'rushes':[('ShieldWall','Bear',9,150),('CreatureEscort','Creature',12,240),('SkeletonSupport','FloatingSkeleton',8,330)]},
]
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for pi,profile in enumerate(PROFILES):
    path='/Game/Maps/'+profile['map']
    entries=[]
    for name,e in BASE['enemies'].items():
        delay=list(e['respawn_delay'])
        if name=='Gorilla':delay=[90,115]
        if name=='Ogre':delay=[40,55]
        unlock=e['unlock_seconds']
        if name=='Gorilla':unlock=[360,360,300][pi]
        entries.append(st(u.EnemySpawnEntry,enemy_class=classes[name],enabled=True,
            minimum_run_time=float(unlock),maximum_run_time=0.,
            pressure_spawn_mode=u.EnemyPressureSpawnMode.MAINTAIN_POPULATION if e['role']=='basic' else u.EnemyPressureSpawnMode.TIMED_THREAT,
            health_scaling_per_minute=min(e['health_per_minute'],.04),
            min_respawn_delay_after_death=float(delay[0]),max_respawn_delay_after_death=float(delay[1])))
    phases=[];rows=[]
    for i,p in enumerate(BASE['phases']):
        # Every third minute is a recovery window. Late-game pressure still climbs.
        recovery=i%3==2
        desired=round(p['desired_population']*profile['scale']*(.88 if recovery else 1))
        main,secondary=profile['mixes'][i%6]
        main_count=round(desired*.72);counts={main:main_count,secondary:desired-main_count}
        populations=[];threats={}
        for name,e in BASE['enemies'].items():
            if e['role']=='basic':
                n=counts.get(name,0)
                # A positive cap makes off-wave classes eligible for authored rushes only.
                populations.append(st(u.EnemyPopulationPhaseEntry,enemy_class=classes[name],desired_population=n,max_population=max(n,1),refill_priority=1.))
            else:
                cap=p['threat_caps'].get(name,0)
                if name=='Gorilla':cap=1 if i>=([6,6,5][pi]) else 0
                if pi==2 and name=='Ogre' and i>=5:cap=2
                if recovery and name in ('DevilRanged','GoblinBomb'):cap=max(1,cap-1) if cap else 0
                threats[name]=cap
                populations.append(st(u.EnemyPopulationPhaseEntry,enemy_class=classes[name],desired_population=0,max_population=cap,refill_priority=1.))
        cap=min(140,desired+sum(threats.values())+10)
        intervals=[float(x*profile['cadence']*(1.12 if recovery else 1)) for x in p['intervals']]
        phases.append(st(u.EnemyPressurePhase,phase_name=profile['name']+' '+str(i+1),
            start_time_seconds=float(p['start_seconds']),end_time_seconds=float(p['end_seconds']),
            global_max_alive=cap,normal_spawn_interval=intervals[0],accelerated_spawn_interval=intervals[1],emergency_spawn_interval=intervals[2],
            enemy_population_entries=populations,events_enabled=i>=2 and not recovery,
            event_interval_min=profile['events'][0],event_interval_max=profile['events'][1]))
        rows.append({'minute':i+1,'population':counts,'threat_caps':threats,'cap':cap,'rushes':i>=2 and not recovery,'intervals':intervals})
        assert desired+sum(threats.values())<=cap and cap+12<=160
    events=[]
    for name,enemy,count,start in profile['rushes']:
        events.append(st(u.EnemyPressureEventDefinition,event_name=name,enabled=True,minimum_run_time=float(start),weight=1.,
            event_cooldown_min=65.,event_cooldown_max=85.,spawn_arc_degrees=profile['arc'],spawn_distance_min=1700.,spawn_distance_max=2300.,
            delay_between_members=.16,delay_after_first_member=.6,allow_temporary_population_overflow=True,
            event_population_overflow_allowance=12,ignore_threat_death_cooldown=False,
            enemy_entries=[st(u.EnemyPressureEventEnemyEntry,enemy_class=classes[enemy],count=count,class_overflow_allowance=count)]))
    assert levels.load_level(path),path
    placed=[a for a in actors.get_all_level_actors() if isinstance(a,u.EnemySpawner)]
    assert len(placed)==1,(path,len(placed))
    spawner=placed[0]
    if not VERIFY_ONLY:
        backup(path,'.umap');spawner.modify()
        setv(spawner,enemy_spawn_entries=entries,pressure_phases=phases,pressure_events=events,
            absolute_hard_alive_cap=160,normal_max_batch_size=2,accelerated_max_batch_size=3,emergency_max_batch_size=4,
            timed_threat_spawn_slot_chance=.12,directional_bias_enabled=True,directional_bias_chance=[.45,.65,.5][pi])
        assert levels.save_current_level(),path
    actual=get(spawner,'pressure_phases')
    assert len(actual)==12 and len(get(spawner,'pressure_events'))==len(events)
    def same_fields(a,b,fields):
        for field in fields.split():
            x,y=get(a,field),get(b,field)
            assert abs(x-y)<.0001 if isinstance(x,float) else x==y,(path,field,x,y)
    for a,b in zip(actual,phases):
        same_fields(a,b,'phase_name global_max_alive events_enabled start_time_seconds end_time_seconds normal_spawn_interval accelerated_spawn_interval emergency_spawn_interval event_interval_min event_interval_max')
        assert len(get(a,'enemy_population_entries'))==len(get(b,'enemy_population_entries'))
        for x,y in zip(get(a,'enemy_population_entries'),get(b,'enemy_population_entries')):
            same_fields(x,y,'enemy_class desired_population max_population refill_priority')
    for a,b in zip(get(spawner,'enemy_spawn_entries'),entries):
        same_fields(a,b,'enemy_class enabled minimum_run_time maximum_run_time pressure_spawn_mode health_scaling_per_minute min_respawn_delay_after_death max_respawn_delay_after_death')
    for a,b in zip(get(spawner,'pressure_events'),events):
        same_fields(a,b,'event_name enabled minimum_run_time event_cooldown_min event_cooldown_max spawn_arc_degrees spawn_distance_min spawn_distance_max delay_between_members event_population_overflow_allowance ignore_threat_death_cooldown')
        for x,y in zip(get(a,'enemy_entries'),get(b,'enemy_entries')):
            same_fields(x,y,'enemy_class count class_overflow_allowance')
    REPORT['maps'][profile['map']]={'identity':profile['name'],'waves':rows,'rushes':profile['rushes']}

# User authorized reverting the generated cloth if the visual cause cannot be established.
# Remove ONLY our named clothing assets; do not replace either imported two-material mesh.
for mesh in ['/Game/Assets/PlayerCharacters/Samurai/fdsafdsa','/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3']:
    backup(mesh)
if not VERIFY_ONLY:assert u.CharacterSimulationSetupCommandlet.remove_generated_cloth()
for mesh in ['/Game/Assets/PlayerCharacters/Samurai/fdsafdsa','/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3']:
    obj=load(mesh)
    assert len(get(obj,'materials'))==2
    assert not [x for x in get(obj,'mesh_clothing_assets') if '_Clothes_' in x.get_name()]
    REPORT['characters'][mesh]='Generated cloth removed; imported mesh/materials/skeleton preserved'

out=ROOT/'Saved/ContentPass';out.mkdir(parents=True,exist_ok=True)
(out/('verified.json' if VERIFY_ONLY else 'applied.json')).write_text(json.dumps(REPORT,indent=2))
u.log('CONTENT_PASS_VERIFIED' if VERIFY_ONLY else 'CONTENT_PASS_APPLIED')
