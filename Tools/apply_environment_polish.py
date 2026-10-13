"""Apply the authored ground and daylight grade to the two gameplay maps.
Run through Unreal Python. Backs up both maps and existing pass-owned assets.
Re-running updates this pass only; shared/vendor textures and materials stay intact.
"""
from pathlib import Path
from datetime import datetime
import hashlib,json,shutil,runpy,unreal as u
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/EnvironmentPolish';out.mkdir(parents=True,exist_ok=True)
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir(parents=True,exist_ok=True)
folder='/Game/HeavensDivide/Materials/EnvironmentPolish';source='/Game/HeavensDivide/Materials/StylizedGround'
lib=u.MaterialEditingLibrary;assets=u.AssetToolsHelpers.get_asset_tools();level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
report={'backup':str(backup),'maps':{},'assets':[]}
def disk(p,ext='.uasset'):return root/'Content'/(p.split('.')[0].removeprefix('/Game/')+ext)
def preserve(p,ext='.uasset'):
 f=disk(p,ext)
 if f.exists():
  dest=backup/f.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
def put(o,**kw):
 if isinstance(o,u.Object):o.modify(True)
 for k,v in kw.items():o.set_editor_property(k,v)
def save(a):assert u.EditorAssetLibrary.save_loaded_asset(a,False),a.get_path_name()
shared={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Content/HeavensDivide/Materials/StylizedGround').glob('*.uasset')}
for p in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena']:preserve(p,'.umap')
for name in ['T_Lvl1_GroundWeights','M_Lvl1_Ground','MI_Lvl1_Ground','MI_Arena_Stone']:preserve(folder+'/'+name)
mat=u.load_asset(folder+'/M_Lvl1_Ground') if u.EditorAssetLibrary.does_asset_exist(folder+'/M_Lvl1_Ground') else None
reference_ground=mat and any(isinstance(e,u.MaterialExpressionCustom) and 'REFERENCE_GROUND' in e.get_editor_property('code') for e in lib.get_material_expressions(mat))
if reference_ground:
 # Refresh the current Level 1 ground without restoring this pass's older shader or tuning.
 runpy.run_path(str(root/'Tools/apply_reference_ground.py'))
 mat=u.load_asset(folder+'/M_Lvl1_Ground')
else:
 task=u.AssetImportTask();put(task,filename=str(root/'Art/EnvironmentPolish/Lvl1_GroundWeights.png'),destination_path=folder,destination_name='T_Lvl1_GroundWeights',automated=True,replace_existing=True,save=False)
 assets.import_asset_tasks([task]);mask=u.load_asset(folder+'/T_Lvl1_GroundWeights');assert mask
 put(mask,srgb=False,compression_settings=u.TextureCompressionSettings.TC_MASKS,address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP,lod_group=u.TextureGroup.TEXTUREGROUP_WORLD,max_texture_size=1024);save(mask)
 mat=u.load_asset(folder+'/M_Lvl1_Ground') if u.EditorAssetLibrary.does_asset_exist(folder+'/M_Lvl1_Ground') else u.EditorAssetLibrary.duplicate_asset(source+'/M_StylizedGround',folder+'/M_Lvl1_Ground');assert mat
 expressions=lib.get_material_expressions(mat)
 blend=next(e for e in expressions if isinstance(e,u.MaterialExpressionCustom) and 'automaticWeights' in e.get_editor_property('code'))
 # Start from the unmodified lit parent code on every run.
 original=next(e for e in lib.get_material_expressions(u.load_asset(source+'/M_StylizedGround')) if isinstance(e,u.MaterialExpressionCustom) and 'automaticWeights' in e.get_editor_property('code')).get_editor_property('code')
 insert='''float2 layoutUV = (p - LayoutOrigin.xy) / max(LayoutSize.xy, 1.0);
 float2 breakup = float2(n.noise(p/85.0 + 7.0), n.noise(p/85.0 + 47.0)) - .5;
 float3 authored = Texture2DSample(LayoutTexture, LayoutTextureSampler, layoutUV + breakup * LayoutEdgeVariation / max(LayoutSize.xy, 1.0)).rgb;
 weights = authored / max(dot(authored,1.0), .001);
 '''
 original=original.replace('float3 grass =',insert+'float3 grass =')
 # Blend a rotated sample at a different frequency to soften repeating brush strokes.
 variation='''float2 alternate = float2(p.x*.866-p.y*.5, p.x*.5+p.y*.866);
 float detailBlend = .25 + .5*n.noise(p/620.0 + 31.0);
 grass = lerp(grass, Texture2DSample(GrassTexture, GrassTextureSampler, alternate*1.17/max(GrassTileSize,1.0)+.37).rgb*GrassTint.rgb, detailBlend);
 dirt = lerp(dirt, Texture2DSample(DirtTexture, DirtTextureSampler, alternate*1.23/max(DirtTileSize,1.0)+.63).rgb*DirtTint.rgb, detailBlend);
 '''
 original=original.replace('float3 rgb =',variation+'float3 rgb =')
 original=original[:original.index('float3 rgb =')]+'''// Original stone color: grade vegetation and soil only.
 float variation = 1.0 + (n.noise(macro * .47 + 12.0) - .5) * MacroVariation;
 float3 gradedGrass = lerp(dot(grass,float3(.2126,.7152,.0722)).xxx,grass,Saturation)*Brightness*variation;
 float3 gradedDirt = lerp(dot(dirt,float3(.2126,.7152,.0722)).xxx,dirt,Saturation)*Brightness*variation;
 float3 originalStone = Texture2DSample(StoneTexture,StoneTextureSampler,p/max(StoneTileSize,1.0)).rgb;
 return max(0.0,gradedGrass*weights.x+gradedDirt*weights.y+originalStone*weights.z);
 '''
 blend.set_editor_property('code',original);blend.set_editor_property('description','Level 1 authored routes: RGB = grass, dirt, stone')
 params=[('LayoutTexture',u.MaterialExpressionTextureObjectParameter,mask),('LayoutOrigin',u.MaterialExpressionVectorParameter,u.LinearColor(-7600,-7600,0,0)),('LayoutSize',u.MaterialExpressionVectorParameter,u.LinearColor(15200,15200,0,0)),('LayoutEdgeVariation',u.MaterialExpressionScalarParameter,24.)]
 nodes_to_connect=[]
 pins=list(blend.get_editor_property('inputs'));pin_names={str(p.get_editor_property('input_name')) for p in pins}
 for index,(name,cls,value) in enumerate(params):
  node=next((e for e in expressions if isinstance(e,cls) and str(e.get_editor_property('parameter_name'))==name),None)
  if not node:node=lib.create_material_expression(mat,cls,-1400,2050+index*190)
  put(node,parameter_name=name,group='Level layout')
  if name=='LayoutTexture':put(node,texture=value,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
  else:put(node,default_value=value)
  if name not in pin_names:
   pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
  nodes_to_connect.append((node,name))
 blend.set_editor_property('inputs',pins)
 for node,name in nodes_to_connect:assert lib.connect_material_expressions(node,'',blend,name)
 lib.recompile_material(mat);save(mat)
def instance(name,parent,scalars,vectors):
 p=folder+'/'+name
 obj=u.load_asset(p) if u.EditorAssetLibrary.does_asset_exist(p) else assets.create_asset(name,folder,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
 lib.set_material_instance_parent(obj,parent)
 # UE 5.8 setters return false even after a successful write; verify readback.
 for k,v in scalars.items():
  lib.set_material_instance_scalar_parameter_value(obj,k,v)
  assert abs(lib.get_material_instance_scalar_parameter_value(obj,k)-v)<.0001,k
 for k,v in vectors.items():
  lib.set_material_instance_vector_parameter_value(obj,k,u.LinearColor(*v))
  got=lib.get_material_instance_vector_parameter_value(obj,k)
  assert all(abs(a-b)<.0001 for a,b in zip((got.r,got.g,got.b,got.a),v)),k
 lib.update_material_instance(obj);save(obj);return obj
if reference_ground:
 lvl_mat=u.load_asset(folder+'/MI_Lvl1_Ground');assert lvl_mat
else:
 lvl_mat=instance('MI_Lvl1_Ground',mat,{'Brightness':1.10,'Saturation':.88,'MacroVariation':.11,'GrassTileSize':350,'DirtTileSize':750,'StoneTileSize':800,'Roughness':.88,'Specular':.14},{'GrassTint':(.9,1.03,.87,1),'DirtTint':(1.00,1.02,1.03,1),'StoneTint':(.95,1.01,1.07,1)})
arena_mat=instance('MI_Arena_Stone',u.load_asset(source+'/M_StylizedGround'),{'UsePaint':0,'GrassCoverage':0,'StoneCoverage':1,'StoneTileSize':600,'Brightness':.95,'Saturation':.22,'MacroVariation':.12,'Roughness':.88,'Specular':.16},{'StoneTint':(.68,.76,.88,1)})
lighting_source='/Game/Assets/Environment/Stylized_Village/Maps/Stylized_Village_Fall/Stylized_Village_Fall_Lighting'
lighting_path='/Game/Maps/Lvl_B1_Lvl1_Lighting'
lighting_hash=hashlib.sha256(disk(lighting_source,'.umap').read_bytes()).hexdigest()
preserve(lighting_path,'.umap')
if not u.EditorAssetLibrary.does_asset_exist(lighting_path):
 duplicate=assets.duplicate_asset('Lvl_B1_Lvl1_Lighting','/Game/Maps',u.load_asset(lighting_source));assert duplicate
 assert u.EditorLoadingAndSavingUtils.save_map(duplicate,lighting_path)
for map_path in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena']:
 assert level.load_level(map_path);is_field=map_path.endswith('Lvl1')
 if is_field:
  world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
  inherited=[v for v in u.EditorLevelUtils.get_levels(world) if v.get_path_name().startswith(lighting_source+'.')]
  if inherited:
   assert len(inherited)==1
   assert u.EditorLevelUtils.remove_level_from_world(inherited[0])
   assert u.EditorLevelUtils.add_level_to_world(world,lighting_path,u.LevelStreamingAlwaysLoaded)
  assert level.set_current_level_by_name(map_path.rsplit('/',1)[-1])
 all_actors=actors.get_all_level_actors()
 def signature(a):
  p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d();return (a.get_class().get_name(),p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z)
 before={a.get_name():signature(a) for a in all_actors if not isinstance(a,(u.Light,u.SkyLight,u.ExponentialHeightFog,u.PostProcessVolume))}
 floor=[a for a in all_actors if a.get_actor_label()=='Plane' and isinstance(a,u.StaticMeshActor)];assert len(floor)==1
 floor[0].modify(True);floor[0].static_mesh_component.modify(True);floor[0].static_mesh_component.set_material(0,lvl_mat if is_field else arena_mat)
 # The existing floor stays in place: collision, navigation and actor layout are preserved.
 suns=[a for a in all_actors if isinstance(a,u.DirectionalLight)];skies=[a for a in all_actors if isinstance(a,u.SkyLight)];fogs=[a for a in all_actors if isinstance(a,u.ExponentialHeightFog)]
 assert len(suns)==len(skies)==len(fogs)==1
 sun=suns[0];sun.modify(True);sun.set_actor_rotation(u.Rotator(pitch=-43 if is_field else -48,yaw=-38 if is_field else 130,roll=0),False)
 lc=sun.get_component_by_class(u.DirectionalLightComponent)
 put(lc,intensity=7.5 if is_field else 7.0,light_color=u.Color(r=255,g=255,b=255,a=255),use_temperature=True,temperature=5800 if is_field else 5100,indirect_lighting_intensity=1.0,volumetric_scattering_intensity=.35,light_source_angle=2.0,contact_shadow_length=.025)
 sky=skies[0].get_component_by_class(u.SkyLightComponent)
 put(sky,intensity=1.75 if is_field else 1.3,light_color=u.Color(r=225,g=236,b=255,a=255),indirect_lighting_intensity=1.0)
 fog=fogs[0];fog.modify(True);fog.set_actor_location(u.Vector(0,0,-150),False,False)
 fc=fog.get_component_by_class(u.ExponentialHeightFogComponent)
 secondary=fc.get_editor_property('second_fog_data');secondary.set_editor_property('fog_density',0.);put(fc,second_fog_data=secondary)
 put(fc,enable_volumetric_fog=False,directional_inscattering_luminance=u.LinearColor(.12,.11,.095,1),fog_density=.005 if is_field else .007,fog_height_falloff=.3,fog_inscattering_luminance=u.LinearColor(.24,.27,.3,1),start_distance=6500.,fog_max_opacity=.12)
 grade=next(a for a in all_actors if isinstance(a,u.PostProcessVolume) and a.get_actor_label()=='Bloodshift Combat Grade')
 for a in all_actors:
  if isinstance(a,u.PostProcessVolume) and a!=grade and a.get_editor_property('unbound'):put(a,enabled=False)
 put(grade,enabled=True,unbound=True,priority=50.,blend_weight=1.)
 settings=grade.get_editor_property('settings')
 settings_values={'auto_exposure_min_brightness':1.,'auto_exposure_max_brightness':1.,'auto_exposure_bias':.8,'color_saturation':u.Vector4(.98,.98,.98,1),'color_contrast':u.Vector4(1.045,1.045,1.045,1),'color_gain_shadows':u.Vector4(.97,1.0,1.045,1),'color_gain_highlights':u.Vector4(1.025,1.01,.98,1),'bloom_intensity':.16,'bloom_threshold':1.2,'vignette_intensity':.15,'motion_blur_amount':0.,'scene_fringe_intensity':0.,'lens_flare_intensity':0.,'ambient_occlusion_intensity':.6,'ambient_occlusion_radius':100.}
 for k,v in settings_values.items():put(settings,**{'override_'+k:True,k:v})
 put(grade,settings=settings)
 if is_field:
  for lamp in [a for a in all_actors if any(c.static_mesh and 'SM_LampPost01a' in c.static_mesh.get_name() for c in a.get_components_by_class(u.StaticMeshComponent))]:
   label='Polish Lantern - '+lamp.get_name();light=next((a for a in all_actors if a.get_actor_label()==label),None)
   p=lamp.get_actor_location()+u.Vector(0,0,185)
   if not light:light=actors.spawn_actor_from_class(u.PointLight,p)
   light.set_actor_label(label);light.set_folder_path('Art Direction/Lanterns');light.set_actor_location(p,False,False)
   comp=light.get_component_by_class(u.PointLightComponent)
   put(comp,mobility=u.ComponentMobility.MOVABLE,intensity_units=u.LightUnits.LUMENS,intensity=130.,attenuation_radius=370.,light_color=u.Color(r=255,g=192,b=109,a=255),cast_shadows=False,indirect_lighting_intensity=.3,source_radius=8.)
 sky.recapture_sky()
 after={a.get_name():signature(a) for a in actors.get_all_level_actors() if a.get_name() in before}
 assert before==after,'Gameplay actor transforms changed'
 if is_field:
  assert level.set_current_level_by_name(lighting_path.rsplit('/',1)[-1])
  assert level.save_current_level()
  assert level.set_current_level_by_name(map_path.rsplit('/',1)[-1])
 assert level.save_current_level()
 report['maps'][map_path]={'floor':floor[0].get_path_name(),'material':(lvl_mat if is_field else arena_mat).get_path_name(),'sun_lux':lc.get_editor_property('intensity'),'sky_intensity':sky.get_editor_property('intensity'),'grade':grade.get_actor_label(),'fixed_ev100':1.,'exposure_bias':.8,'original_gameplay_actor_count':len(before)}
assert all(hashlib.sha256(p.read_bytes()).hexdigest()==h for p,h in shared.items()),'Shared ground assets were modified'
report['assets']=[folder+'/'+n for n in ['T_Lvl1_GroundWeights','M_Lvl1_Ground','MI_Lvl1_Ground','MI_Arena_Stone']]
assert hashlib.sha256(disk(lighting_source,'.umap').read_bytes()).hexdigest()==lighting_hash
report['shared_ground_unchanged']=True
report['vendor_lighting_unchanged']=True
report['lighting_sublevel']=lighting_path
(out/'Applied.json').write_text(json.dumps(report,indent=2));u.log('ENVIRONMENT_POLISH_APPLIED '+str(backup))
