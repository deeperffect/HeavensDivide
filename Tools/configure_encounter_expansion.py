"""Author the encounter expansion. Original assets/maps are backed up once before edits."""
import unreal as u
from pathlib import Path
import json, shutil, math

root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/EncounterExpansion';out.mkdir(parents=True,exist_ok=True)
assets=u.AssetToolsHelpers.get_asset_tools();lib=u.MaterialEditingLibrary
reg=u.AssetRegistryHelpers.get_asset_registry();reg.search_all_assets(True)
report={'enemies':[],'levels':{},'abp_dependencies':{},'visuals':[]}
folder='/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical'

def backup(path,ext='.uasset'):
    src=root/'Content'/(path.removeprefix('/Game/').split('.')[0]+ext)
    dst=out/'Backup'/src.relative_to(root/'Content')
    if src.exists() and not dst.exists():
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def save(obj):
    assert u.EditorAssetLibrary.save_loaded_asset(obj,False),obj.get_path_name()
def setp(obj,**fields):
    for k,v in fields.items():obj.set_editor_property(k,v)
def bp(name,parent,directory=folder):
    path=directory+'/'+name;backup(path)
    if u.EditorAssetLibrary.does_asset_exist(path):obj=u.load_asset(path)
    else:
        f=u.BlueprintFactory();f.set_editor_property('parent_class',parent)
        obj=assets.create_asset(name,directory,u.Blueprint,f)
    assert obj,path
    u.BlueprintEditorLibrary.compile_blueprint(obj)
    return obj,u.get_default_object(obj.generated_class())
def finish(obj):u.BlueprintEditorLibrary.compile_blueprint(obj);save(obj)
def material(name):
    path='/Game/HeavensDivide/Materials/'+name;backup(path)
    m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,path.rsplit('/',1)[0],u.Material,u.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    setp(m,blend_mode=u.BlendMode.BLEND_TRANSLUCENT,shading_model=u.MaterialShadingModel.MSM_UNLIT,two_sided=True)
    return m
def node(m,typ,x=0,y=0):return lib.create_material_expression(m,typ,x,y)
def scalar(m,name,value,y=0):
    n=node(m,u.MaterialExpressionScalarParameter,-500,y);setp(n,parameter_name=name,default_value=value);return n
def custom(m,code,inputs):
    n=node(m,u.MaterialExpressionCustom)
    ins=[]
    for name,src in inputs.items():
        i=u.CustomInput();i.set_editor_property('input_name',name);ins.append(i)
    setp(n,inputs=ins,code=code,output_type=u.CustomMaterialOutputType.CMOT_FLOAT4)
    for name,src in inputs.items():assert lib.connect_material_expressions(src,'',n,name)
    for color,prop in [(True,u.MaterialProperty.MP_EMISSIVE_COLOR),(False,u.MaterialProperty.MP_OPACITY)]:
        mask=node(m,u.MaterialExpressionComponentMask,200,0 if color else 150)
        setp(mask,r=color,g=color,b=color,a=not color)
        assert lib.connect_material_expressions(n,'',mask,'')
        assert lib.connect_material_property(mask,'',prop)
    lib.recompile_material(m);save(m);return m

m=material('M_EncounterWarning')
setp(m,material_domain=u.MaterialDomain.MD_DEFERRED_DECAL,shading_model=u.MaterialShadingModel.MSM_DEFAULT_LIT)
warning=custom(m,r'''
float2 p=(UV-.5)*2;
float r=length(p);
float d=Shape<.5 ? r : (Shape<1.5 ? max(abs(p.x),abs(p.y)) : r);
float inner=Shape>1.5 ? saturate(Inner) : 0;
float inside=step(d,.985)*(Shape>1.5 ? step(inner,r) : 1);
float edge=1-smoothstep(.025,.055,abs(d-.95));
if(Shape>1.5) edge=max(edge,1-smoothstep(.015,.04,abs(r-inner)));
float sweep=step(d,lerp(inner,1,saturate(Fill)));
float stripes=.5+.5*sin((p.x+p.y)*34);
float3 ink=float3(.015,.006,.012);
float3 red=float3(1.7,.055,.025);
float3 gold=float3(2.5,.65,.12);
float3 color=lerp(ink,red,sweep*.7+edge*.3);
color=lerp(color,gold,edge*(.45+Impact*.55));
color=lerp(color,float3(2.4,.28,.045),Impact*(.7+.3*stripes));
return float4(color,inside*(.3+edge*.6+sweep*.22+Impact*.25));
''',{'UV':node(m,u.MaterialExpressionTextureCoordinate,-500,-200),
     'Shape':scalar(m,'Shape',0),'Inner':scalar(m,'Inner',0,100),'Fill':scalar(m,'Fill',0,200),'Impact':scalar(m,'Impact',0,300)})
m=material('M_EnemyRoleRing');t=node(m,u.MaterialExpressionVectorParameter,-500,100);setp(t,parameter_name='Tint',default_value=u.LinearColor(1,.2,.05,1))
ring=custom(m,r'''float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);
float rim=step(.81,r)*step(r,.94);float ticks=step(.6,cos(a*8))*step(.65,r)*step(r,.8);
return float4(Tint.rgb*1.3,max(rim,ticks)*.8);''',{'UV':node(m,u.MaterialExpressionTextureCoordinate,-500,0),'Tint':t})
m=material('M_EnemyRim');setp(m,used_with_skeletal_mesh=True)
t=node(m,u.MaterialExpressionVectorParameter,-500,100);setp(t,parameter_name='Tint',default_value=u.LinearColor(1,.18,.06,1))
f=node(m,u.MaterialExpressionFresnel,-500,0);setp(f,exponent=3.5,base_reflect_fraction=.02)
rim=custom(m,'return float4(Tint.rgb*1.35,saturate(.12+Edge*.55));',{'Tint':t,'Edge':f})

# The existing ogre weapon material has an empty imported color sample.
weapon=u.load_asset('/Game/Assets/EnemyCharacters/Ogre/OgreWeaponMat')
backup(weapon.get_path_name())
for expr in lib.get_material_expressions(weapon):
    if isinstance(expr,u.MaterialExpressionTextureSample) and not expr.get_editor_property('texture'):
        expr.set_editor_property('texture',u.load_asset('/Game/Assets/EnemyCharacters/Ogre/EnemyOgreWeapon_lambert1_BaseColor'))
lib.recompile_material(weapon);save(weapon)

base='/Game/HeavensDivide/Blueprints/EnemyCharacters/'
def source(name):
    path=base+('Elites/' if name in ['Ogre','Gorilla'] else 'Mobs/')+'BP_Enemy'+name
    return u.get_default_object(u.load_asset(path).generated_class())
def copy_look(dst,src,scale=1):
    dm=dst.get_component_by_class(u.SkeletalMeshComponent);sm=src.get_component_by_class(u.SkeletalMeshComponent)
    dm.set_editor_property('receives_decals',False)
    for k in ['skeletal_mesh_asset','anim_class','relative_location','relative_rotation','relative_scale3d','override_materials']:
        dm.set_editor_property(k,sm.get_editor_property(k))
    s=dm.get_editor_property('relative_scale3d');dm.set_editor_property('relative_scale3d',u.Vector(s.x*scale,s.y*scale,s.z*scale))
    cap=dst.get_component_by_class(u.CapsuleComponent);sc=src.get_component_by_class(u.CapsuleComponent)
    for k in ['capsule_half_height','capsule_radius']:cap.set_editor_property(k,sc.get_editor_property(k)*scale)
    p=dm.get_editor_property('relative_location');dm.set_editor_property('relative_location',u.Vector(p.x*scale,p.y*scale,p.z*scale))
    for k in ['experience_pickup_class','health_bar_widget_class','mark_indicator_widget_class','hit_flash_material']:
        try:dst.set_editor_property(k,src.get_editor_property(k))
        except Exception:pass
    abp=sm.get_editor_property('anim_class')
    if abp:
        p=abp.get_path_name().split('.')[0]
        report['abp_dependencies'][p]=[str(x) for x in reg.get_dependencies(p,u.AssetRegistryDependencyOptions(include_hard_package_references=True)) if 'Blueprints/EnemyCharacters' in str(x)]

# name, role, art, health, speed, damage, cooldown, range, warning, first seconds, color
rows=[
('AshSeer','ASH_SEER','DevilRanged',70,180,14,5.2,900,1.15,60,(1,.23,.035)),
('HexSniper','HEX_SNIPER','FloatingSkeleton',85,165,19,6,1100,1.35,180,(.85,.1,1)),
('MireWeaver','MIRE_WEAVER','Fish',100,155,9,7,850,1.25,270,(.25,1,.15)),
('HornLancer','HORN_LANCER','Grunt',85,205,19,5.4,700,1.1,90,(1,.075,.045)),
('FangStalker','FANG_STALKER','GoblinCrawler',60,255,13,6.5,750,.9,240,(1,.55,.06)),
('GraveCantor','GRAVE_CANTOR','FloatingSkeleton',160,155,0,6.5,850,1.5,300,(.15,1,.65)),
('WarDrummer','WAR_DRUMMER','Bear',240,170,0,7,650,1.4,420,(1,.78,.2)),
('OgreWarden','OGRE_WARDEN','Ogre',520,160,26,5.8,450,1.4,210,(1,.2,.35)),
('StormGorilla','STORM_GORILLA','Gorilla',1500,155,28,6.4,700,1.55,450,(.15,.65,1)),
('FrostOracle','FROST_ORACLE','Creature',145,185,15,6,850,1.2,360,(.3,.9,1))]
classes={}
for name,role,art,hp,speed,damage,cooldown,cast_range,windup,start,color in rows:
    asset,cdo=bp('BP_'+name,u.TacticalEnemy);copy_look(cdo,source(art))
    setp(cdo,tactical_role=getattr(u.TacticalEnemyRole,role),enemy_name=''.join(' '+c if c.isupper() else c for c in name).strip(),
         move_speed=speed,ability_damage=damage,ability_cooldown=cooldown,cast_range=cast_range,windup=windup,role_color=u.LinearColor(*color,1),xp_reward=10 if hp>300 else 5)
    cdo.get_health_component().set_editor_property('max_health',hp)
    cdo.get_editor_property('role_ring').set_material(0,ring)
    # Only use a matching skeleton montage, whose old gameplay notify is type-filtered.
    src=source(art)
    try:montage=src.get_editor_property('attack_montage')
    except Exception:montage=None
    if montage:cdo.set_editor_property('cast_montage',montage)
    if hp>300:cdo.set_editor_property('drop_category',u.EnemyDropCategory.ELITE)
    finish(asset);classes[name]=asset.generated_class()
    report['enemies'].append({'name':name,'health':hp,'damage':damage,'speed':speed,'introduced_seconds':start})

asset,cdo=bp('BP_AshenMarcher',u.TacticalEnemy);copy_look(cdo,source('Grunt'),.9)
setp(cdo,tactical_role=u.TacticalEnemyRole.HORN_LANCER,enemy_name='Ashen Marcher',ability_damage=16,move_speed=250,role_color=u.LinearColor(1,.4,.05,1),xp_reward=2)
cdo.get_health_component().set_editor_property('max_health',28);finish(asset);march=asset.generated_class()

asset,cdo=bp('BP_CinderOracle',u.CinderOracleBoss,base+'Bosses/CinderOracle')
copy_look(cdo,source('DevilRanged'),2.5)
setp(cdo,boss_display_name='KAGUTSU - THE CINDER ORACLE',boss_max_health=32000,boss_move_speed=150,spell_damage=22,
     guardian_class=classes['AshSeer'],oracle_cast_montage=source('DevilRanged').get_editor_property('attack_montage'),phase2_health_threshold=0.)
finish(asset);boss=asset.generated_class()

asset,cdo=bp('BP_HealingUrn',u.HealingUrn,'/Game/HeavensDivide/Blueprints/Encounters')
pot=u.load_asset('/Game/Assets/JapaneseShrine/Meshes/SM_PotUnSoiled10a');assert pot
mesh=cdo.get_editor_property('pot');mesh.set_static_mesh(pot)
size=pot.get_bounding_box().max-pot.get_bounding_box().min
factor=90/max(size.z,1);mesh.set_editor_property('relative_scale3d',u.Vector(factor,factor,factor))
mesh.set_editor_property('relative_location',u.Vector(0,0,-48-pot.get_bounding_box().min.z*factor))
cdo.get_health_component().set_editor_property('max_health',18)
cdo.set_editor_property('healing_class',u.load_asset('/Game/HeavensDivide/Blueprints/BP_HealingPickup').generated_class())
finish(asset);urn=asset.generated_class()
audio=u.load_asset('/Game/HeavensDivide/Audio/DA_CombatAudio');backup(audio.get_path_name())
sounds=dict(audio.get_editor_property('sounds'));sounds['UrnBreak']=u.load_asset('/Game/HeavensDivide/Audio/Waves/W_DeathSkeleton')
audio.set_editor_property('sounds',sounds);save(audio)

asset,cdo=bp('BP_EncounterDirector',u.EncounterDirector,'/Game/HeavensDivide/Blueprints/Encounters')
setp(cdo,marching_enemy_class=march,healing_urn_class=urn);finish(asset);director=asset.generated_class()

spawn_bp=u.load_asset(base+'BP_EnemySpawner');assert spawn_bp
backup(spawn_bp.get_path_name());spawn_cdo=u.get_default_object(spawn_bp.generated_class())
old_entries=list(spawn_cdo.get_editor_property('enemy_spawn_entries'))
entries=[e for e in old_entries if '/Tactical/' not in e.enemy_class.get_path_name()]
for e in entries:
    elite='Elites' in e.enemy_class.get_path_name()
    e.health_scaling_per_minute=.095 if elite else .075
for row in rows:
    e=u.EnemySpawnEntry();setp(e,enemy_class=classes[row[0]],minimum_run_time=row[9],health_scaling_per_minute=.08,
        pressure_spawn_mode=u.EnemyPressureSpawnMode.TIMED_THREAT,min_respawn_delay_after_death=12 if row[3]<300 else 28,max_respawn_delay_after_death=20 if row[3]<300 else 40)
    entries.append(e)
basic=['Grunt','GoblinCrawler','Creature','Fish','Bear','FloatingSkeleton']
phases=[]
for i in range(12):
    start=i*60;total=40+i*7
    p=u.EnemyPressurePhase();setp(p,phase_name='MixedPressure_%02d'%(i+1),start_time_seconds=start,end_time_seconds=(i+1)*60 if i<11 else 0,
        global_max_alive=min(156,total+24),normal_spawn_interval=max(.38,.67-i*.021),accelerated_spawn_interval=max(.24,.36-i*.007),emergency_spawn_interval=.19,
        events_enabled=i>=2,event_interval_min=58,event_interval_max=85)
    population=[]
    weights=[(basic[i%6],.72),(basic[(i+1)%6],.28)]
    for art,weight in weights:
        e=u.EnemyPopulationPhaseEntry();setp(e,enemy_class=source(art).get_class(),desired_population=round(total*weight),max_population=round(total*weight),refill_priority=1.)
        population.append(e)
    for row in rows:
        e=u.EnemyPopulationPhaseEntry();active=start+59>=row[9]
        cap=(2 if start>=480 and row[3]<200 else 1) if active else 0
        setp(e,enemy_class=classes[row[0]],desired_population=0,max_population=cap,refill_priority=1.4)
        population.append(e)
    for old in entries[:10]:
        if old.pressure_spawn_mode==u.EnemyPressureSpawnMode.TIMED_THREAT:
            e=u.EnemyPopulationPhaseEntry();setp(e,enemy_class=old.enemy_class,desired_population=0,max_population=1 if start+59>=old.minimum_run_time else 0,refill_priority=1.)
            population.append(e)
    p.set_editor_property('enemy_population_entries',population);phases.append(p)
def tune_spawner(obj):setp(obj,enemy_spawn_entries=entries,pressure_phases=phases,absolute_hard_alive_cap=160,timed_threat_spawn_slot_chance=.3)
tune_spawner(spawn_cdo);finish(spawn_bp)

level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for path in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena']:
    backup(path,'.umap');assert level.load_level(path)
    all_actors=actors.get_all_level_actors();info=[]
    for a in all_actors:
        if isinstance(a,u.EnemySpawner):tune_spawner(a);info.append('Spawner updated')
    if path.endswith('Lvl1'):
        existing=[a for a in all_actors if isinstance(a,u.EncounterDirector)]
        if not existing:
            a=actors.spawn_actor_from_class(director,u.Vector(0,0,100));a.set_actor_label('Encounter Director - Marches and Healing Urns')
        info.append('Director installed')
    else:
        old=[a for a in all_actors if isinstance(a,u.FinalBossBase)]
        assert len(old)==1,[(a.get_actor_label(),a.get_class().get_name()) for a in old]
        if not isinstance(old[0],u.CinderOracleBoss):
            transform=old[0].get_actor_transform();actors.destroy_actor(old[0])
            a=actors.spawn_actor_from_class(boss,transform.translation,transform.rotation.rotator());a.set_actor_label('Kagutsu - The Cinder Oracle')
        info.append('Caster boss installed')
    # A subtle inexpensive grade shared by gameplay and arena; no additional realtime lights.
    grading=next((a for a in actors.get_all_level_actors() if a.get_actor_label()=='Bloodshift Combat Grade'),None)
    if not grading:grading=actors.spawn_actor_from_class(u.PostProcessVolume,u.Vector(0,0,0));grading.set_actor_label('Bloodshift Combat Grade')
    setp(grading,unbound=True,priority=25,blend_weight=.7)
    settings=grading.get_editor_property('settings')
    for k,v in {'color_saturation':u.Vector4(.94,.95,.98,1),'color_contrast':u.Vector4(1.07,1.06,1.05,1),
                'color_gain_shadows':u.Vector4(.96,1.015,1.065,1),'bloom_intensity':.22,'vignette_intensity':.18,'scene_fringe_intensity':0.,'motion_blur_amount':0.}.items():
        settings.set_editor_property('override_'+k,True);settings.set_editor_property(k,v)
    grading.set_editor_property('settings',settings)
    assert level.save_current_level();report['levels'][path]=info

report['kept_maps']=['/Game/Maps/Lvl_MainMenu','/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/TestMap','/Game/Maps/Lvl_B1_Arena',
    '/Game/Assets/Environment/Stylized_Village/Maps/Stylized_Village_Fall/Stylized_Village_Fall_Landscape',
    '/Game/Assets/Environment/Stylized_Village/Maps/Stylized_Village_Fall/Stylized_Village_Fall_Lighting']
(out/'Setup.json').write_text(json.dumps(report,indent=2))
u.log('ENCOUNTER_EXPANSION_SETUP_PASS')
