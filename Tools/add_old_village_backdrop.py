"""Add a lightweight countryside and horizon without moving playable scenery."""
from pathlib import Path
from datetime import datetime
import sys,json,math,random,shutil
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from import_old_village import import_file
from old_village_backdrop_shape import height,outside_arena,R
from fix_old_village_bamboo import repair_current_level
from polish_old_village import placement
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary
ACT=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
DEST='/Game/HeavensDivide/Environment/OldVillage/Backdrop';TAG='OldVillageBackdrop'
def put(obj,**values):
    for key,value in values.items():obj.set_editor_property(key,value)

def material(name,code,sky=False):
    path=DEST+'/Materials/'+name
    mat=E.load_asset(path) if E.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST+'/Materials',u.Material,u.MaterialFactoryNew())
    M.delete_all_material_expressions(mat)
    put(mat,two_sided=sky,shading_model=u.MaterialShadingModel.MSM_UNLIT if sky else u.MaterialShadingModel.MSM_DEFAULT_LIT)
    if sky:put(mat,is_sky=True)
    custom=M.create_material_expression(mat,u.MaterialExpressionCustom)
    put(custom,code=code,output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins=[]
    for key,cls,output in [('P',u.MaterialExpressionWorldPosition,'XYZ'),('C',u.MaterialExpressionCameraPositionWS,''),('N',u.MaterialExpressionVertexNormalWS,'')]:
        pin=u.CustomInput();put(pin,input_name=key);pins.append(pin)
    if not sky:
        pin=u.CustomInput();put(pin,input_name='T');pins.append(pin)
    put(custom,inputs=pins)
    for key,cls,output in [('P',u.MaterialExpressionWorldPosition,'XYZ'),('C',u.MaterialExpressionCameraPositionWS,''),('N',u.MaterialExpressionVertexNormalWS,'')]:
        node=M.create_material_expression(mat,cls);M.connect_material_expressions(node,output,custom,key)
    if not sky:
        texture=M.create_material_expression(mat,u.MaterialExpressionTextureObject)
        put(texture,texture=u.load_asset('/Game/HeavensDivide/Materials/StylizedGround/T_StylizedGround_Grass'))
        M.connect_material_expressions(texture,'',custom,'T')
    if sky:M.connect_material_property(custom,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        M.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
        glow=M.create_material_expression(mat,u.MaterialExpressionMultiply);put(glow,const_b=.12)
        M.connect_material_expressions(custom,'',glow,'A');M.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        rough=M.create_material_expression(mat,u.MaterialExpressionConstant);put(rough,r=.95);M.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    for key in ['BaseColor','EmissiveColor','Roughness']:u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),key,'(UseConstant=False)')
    M.recompile_material(mat);assert E.save_loaded_asset(mat,False)
    return mat

def scenic_component(c):
    c.set_collision_profile_name('NoCollision');c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    put(c,can_ever_affect_navigation=False,generate_overlap_events=False,cast_shadow=False,affect_distance_field_lighting=False,visible_in_ray_tracing=False)
    c.set_component_tick_enabled(False)

def scenery(mesh,label,pos,scale,mat=None,yaw=0):
    a=ACT.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*pos),u.Rotator(pitch=0,yaw=yaw,roll=0))
    a.set_actor_label(label);a.set_folder_path('95_Backdrop');put(a,tags=[TAG]);a.set_actor_scale3d(u.Vector(*scale))
    c=a.static_mesh_component;c.set_static_mesh(mesh);scenic_component(c)
    if mat:c.set_material(0,mat)
    return a

def forest():
    SDS=u.get_engine_subsystem(u.SubobjectDataSubsystem);SD=u.SubobjectDataBlueprintFunctionLibrary
    a=ACT.spawn_actor_from_class(u.Actor,u.Vector(0,0,0));a.set_actor_label('Distant woodland - instanced');a.set_folder_path('95_Backdrop');put(a,tags=[TAG]);a.set_actor_tick_enabled(False)
    handles=SDS.k2_gather_subobject_data_for_instance(a);actor_handle=next(h for h in handles if SD.is_actor(SD.get_data(h)))
    root,error=SDS.add_new_subobject(u.AddNewSubobjectParams(parent_handle=actor_handle,new_class=u.SceneComponent,blueprint_context=None));assert not str(error),str(error)
    put(SD.get_associated_object(SD.get_data(root)),mobility=u.ComponentMobility.STATIC)
    rng=random.Random(718);positions=[]
    # Irregular groves start well beyond the solid perimeter walls.
    for _ in range(180):
        e=rng.uniform(-26000,28000);n=rng.uniform(-24000,30000);z=height(e,n)
        if outside_arena(e,n,950) and z>80 and e+n>-3500:positions.append((e,n,z))
    # Denser irregular groves close to the walls, where the game camera sees out.
    for side in range(4):
        for j in range(34):
            along=-8500+j*520+rng.uniform(-170,170);across=R+rng.uniform(650,2300)
            e,n=[(along,across),(across,along),(along,-across),(-across,along)][side]
            z=height(e,n)
            if z>-80:positions.append((e,n,z))
    for i,name in enumerate(['Tree_Medium_Green','Tree_Small_Red']):
        mesh=u.load_asset('/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/'+name)
        b=mesh.get_bounds()
        handle,error=SDS.add_new_subobject(u.AddNewSubobjectParams(parent_handle=root,new_class=u.HierarchicalInstancedStaticMeshComponent,blueprint_context=None));assert not str(error),str(error)
        c=SD.get_associated_object(SD.get_data(handle));c.set_static_mesh(mesh);scenic_component(c)
        # Billboard LODs need vertex positioning to face the camera. Disable wind
        # in isolated material instances instead of disabling all vertex offsets.
        put(c,mobility=u.ComponentMobility.STATIC,evaluate_world_position_offset=True)
        for slot in [1,2]:
            source=mesh.get_material(slot)
            if not isinstance(source,u.MaterialInstanceConstant):continue
            path=DEST+'/Materials/'+source.get_name()+'_Distant'
            mat=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(source.get_path_name(),path)
            scalar_names={str(n) for n in M.get_scalar_parameter_names(mat)}
            for key,value in {'WindIntensity':0.,'WindWeight':0.,'Debug Grid':0.,'DebugBoost':1.,'BaseColorBrightness':.65}.items():
                if key in scalar_names:M.set_material_instance_scalar_parameter_value(mat,key,value)
            vectors={str(n) for n in M.get_vector_parameter_names(mat)}
            tint=u.LinearColor(.10,.22,.065,1) if i==0 else u.LinearColor(.32,.10,.055,1)
            for key,value in {'Base Foliage Tint':tint,'Subsurface Color':tint,'Vertex Paint Color (R)':u.LinearColor(.45,.5,.35,1),'Vertex Paint Color (G)':u.LinearColor(.4,.5,.45,1),'BaseColor Multiplier':u.LinearColor(.65,.8,.65,1)}.items():
                if key in vectors:M.set_material_instance_vector_parameter_value(mat,key,value)
            M.update_material_instance(mat);assert E.save_loaded_asset(mat,False);c.set_material(slot,mat)
        transforms=[]
        for j,(e,n,z) in enumerate(positions):
            if (j%7==0)!=(i==1):continue
            s=rng.uniform(850,1400)/(2*b.box_extent.z)
            transforms.append(u.Transform(location=u.Vector(n,e,z-(b.origin.z-b.box_extent.z)*s-65),rotation=u.Rotator(pitch=0,yaw=rng.uniform(0,360),roll=0),scale=u.Vector(s,s,s)))
        c.add_instances(transforms,True,True,False)
    return len(positions)

def add_backdrop():
    for a in list(ACT.get_all_level_actors()):
        if a.actor_has_tag(TAG):ACT.destroy_actor(a)
    ground=E.load_asset(DEST+'/Meshes/SM_OV_Countryside')
    if not ground:ground=import_file(ROOT/'Art/Environment/OldVillage/Backdrop/SM_OV_Countryside.fbx',DEST+'/Meshes','SM_OV_Countryside',True)
    landmat=material('M_Countryside', '''
float broad=.5+.5*sin(P.x*.0006+sin(P.y*.00045))*cos(P.y*.0007);
float3 painted=Texture2DSample(T,TSampler,P.xy/700).rgb;
float3 grass=painted*lerp(float3(.55,.73,.46),float3(.82,.89,.59),broad);
float slope=saturate((.96-abs(N.z))*5);
float3 col=lerp(grass,float3(.23,.255,.245),slope);
float haze=smoothstep(14000,45000,length(P.xy-C.xy))*.68;
return lerp(col,float3(.30,.43,.46),haze);
''')
    stone=material('M_DistantStone', '''
float cells=.5+.5*sin(P.z*.032+sin(P.x*.006+P.y*.007));
return lerp(float3(.11,.13,.13),float3(.21,.23,.22),cells);
''')
    sky=material('M_PaintedHorizon', '''
float3 d=normalize(P-C);float t=saturate(d.z);
float3 col=lerp(float3(.43,.56,.59),float3(.14,.29,.43),pow(t,.6));
float clouds=pow(saturate(sin(d.x*17+sin(d.y*12))*cos(d.y*21+d.z*8)),5)*smoothstep(.06,.3,t)*(1-smoothstep(.5,.8,t));
return col+clouds*float3(.10,.10,.085);
''',True)
    scenery(ground,'Rolling countryside beyond the walls',(0,0,0),(1,1,1),landmat,90)
    scenery(u.load_asset('/Engine/BasicShapes/Plane'),'Water horizon',(0,0,-205),(4000,4000,1),u.load_asset('/Game/HeavensDivide/Environment/OldVillage/Materials/M_OV_Water'))
    scenery(u.load_asset('/Engine/BasicShapes/Sphere'),'Painted sky horizon',(0,0,0),(10000,10000,10000),sky)
    count=forest()
    # Small neighboring settlements give the landscape a purpose beyond the arena.
    for mesh_name,label,e,n,size in [('SM_OV_MainShrine','Hilltop sanctuary',-3500,14000,.75),('SM_OV_ThatchedCottage','Outlying farmhouse',4800,12500,1.),('SM_OV_ThatchedCottage','Outlying farmhouse',6800,13700,.85)]:
        source=u.load_asset('/Game/HeavensDivide/Environment/OldVillage/Meshes/Buildings/'+mesh_name)
        b=source.get_bounds();z=height(e,n)-(b.origin.z-b.box_extent.z)*size-40
        building=scenery(source,label,(n,e,z),(size,size,size),yaw=135)
        center,extent=building.get_actor_bounds(False)
        levels=[height(center.y+dy,center.x+dx) for dx in [-extent.x,extent.x] for dy in [-extent.y,extent.y]]
        top=max(levels)+15;bottom=min(levels)-80
        p=building.get_actor_location();p.z+=top-(center.z-extent.z);building.set_actor_location(p,False,True)
        scenery(u.load_asset('/Engine/BasicShapes/Cube'),'Distant stone terrace',(center.x,center.y,(top+bottom)/2),((extent.x*2+80)/100,(extent.y*2+80)/100,(top-bottom)/100),stone)
    rock=u.load_asset('/Game/Assets/Environment/JapaneseShrine/Meshes/SM_KitRockCliff03a');rb=rock.get_bounds()
    for i,(e,n,h) in enumerate([(-11800,-11200,1800),(-14500,-9700,1100),(-10500,-14800,900)]):
        s=h/(2*rb.box_extent.z)
        scenery(rock,'Coastal sea stack',(n,e,-330-(rb.origin.z-rb.box_extent.z)*s),(s*1.25,s*1.25,s),yaw=i*75)
    fog=ACT.spawn_actor_from_class(u.ExponentialHeightFog,u.Vector(0,0,-200));fog.set_actor_label('Distant coastal haze');fog.set_folder_path('95_Backdrop');put(fog,tags=[TAG])
    fc=fog.get_component_by_class(u.ExponentialHeightFogComponent)
    put(fc,fog_density=.015,fog_height_falloff=.2,start_distance=12000,fog_max_opacity=.65,fog_inscattering_luminance=u.LinearColor(.32,.44,.47,1),enable_volumetric_fog=False)
    return {'trees':count,'forest_components':2,'terrain_triangles':20000,'volumetric_fog':False}

def run():
    backup=ROOT/'Saved/OldVillage/Backups'/(datetime.now().strftime('%Y%m%d_%H%M%S')+'_BambooBackdrop');backup.mkdir(parents=True,exist_ok=True)
    report={}
    for name in ['Lvl_OldVillage','Lvl_OldVillage_Blockout']:
        shutil.copy2(ROOT/'Content/Maps'/(name+'.umap'),backup/(name+'.umap'))
        assert L.load_level('/Game/Maps/'+name)
        before={a.get_name():placement(a) for a in ACT.get_all_level_actors() if not a.actor_has_tag(TAG)}
        report[name]={'bamboo':repair_current_level(),'backdrop':add_backdrop()}
        assert all(placement(a)==before[a.get_name()] for a in ACT.get_all_level_actors() if a.get_name() in before)
        assert L.save_current_level()
    (ROOT/'Saved/OldVillage/BambooBackdropReport.json').write_text(json.dumps(report,indent=2))
    u.log('OLD_VILLAGE_BAMBOO_BACKDROP_SAVED')
if __name__=='__main__':run()
