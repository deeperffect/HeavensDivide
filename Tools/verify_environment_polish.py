"""Read-only checks for the saved environment pass; run with Unreal Python."""
from pathlib import Path
import json,re,unreal as u
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/EnvironmentPolish';original=json.loads((out/'Audit.json').read_text())
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem);lib=u.MaterialEditingLibrary
folder='/Game/HeavensDivide/Materials/EnvironmentPolish';checks=[]
def xyz(s):return tuple(float(v) for v in re.search(r'x: ([\d.-]+), y: ([\d.-]+), z: ([\d.-]+)',s).groups())
def rot(s):return tuple(float(v) for v in re.search(r'pitch: ([\d.-]+), yaw: ([\d.-]+), roll: ([\d.-]+)',s).groups())
def near(a,b):return all(abs(x-y)<.001 for x,y in zip(a,b))
for map_path in ['/Game/Maps/Lvl_B1_Lvl1','/Game/Maps/Lvl_B1_Arena']:
 assert level.load_level(map_path);all_actors=actors.get_all_level_actors();by_name={a.get_name():a for a in all_actors}
 for old in original[map_path]:
  assert old['name'] in by_name,old['label'];a=by_name[old['name']]
  assert a.get_class().get_name()==old['class']
  if old.get('lighting') or old.get('settings'):continue
  p=a.get_actor_location();s=a.get_actor_scale3d();r=a.get_actor_rotation()
  assert near((p.x,p.y,p.z),xyz(old['location'])),old['label']
  assert near((s.x,s.y,s.z),xyz(old['scale'])),old['label']
  assert near((r.pitch,r.yaw,r.roll),rot(old['rotation'])),old['label']
 checks.append(map_path+': original non-lighting actors and transforms intact')
 assert not any(a.get_actor_label().startswith('TEMP Environment') for a in all_actors)
 planes=[a for a in all_actors if a.get_actor_label()=='Plane'];assert len(planes)==1
 mi=planes[0].get_component_by_class(u.StaticMeshComponent).get_material(0)
 expected='MI_Lvl1_Ground' if map_path.endswith('Lvl1') else 'MI_Arena_Stone'
 assert mi.get_path_name()==folder+'/'+expected+'.'+expected
 assert mi.get_base_material().get_editor_property('shading_model')==u.MaterialShadingModel.MSM_DEFAULT_LIT
 if map_path.endswith('Lvl1'):
  ground_code='\n'.join(e.get_editor_property('code') for e in lib.get_material_expressions(mi.get_base_material()) if isinstance(e,u.MaterialExpressionCustom))
  reference_ground='REFERENCE_GROUND' in ground_code
  expected_scalars=({'GrassTileSize':1000,'DirtTileSize':1500,'StoneTileSize':2200,'LayoutEdgeVariation':0} if reference_ground else {'GrassTileSize':350})
  for key,value in expected_scalars.items():assert abs(lib.get_material_instance_scalar_parameter_value(mi,key)-value)<.001,key
  if reference_ground:
   for layer in ['Grass','Dirt','Stone']:
    texture=lib.get_material_instance_texture_parameter_value(mi,layer+'Texture')
    assert texture and texture.get_path_name().startswith('/Game/HeavensDivide/Materials/ReferenceGround/T_ReferenceGround_'+layer)
  for key,expected_value in [('LayoutOrigin',(-7600,-7600)),('LayoutSize',(15200,15200))]:
   value=lib.get_material_instance_vector_parameter_value(mi,key)
   assert near((value.r,value.g),expected_value),key
  texture=lib.get_material_instance_texture_parameter_value(mi,'LayoutTexture')
  assert texture and texture.get_path_name()==folder+'/T_Lvl1_GroundWeights.T_Lvl1_GroundWeights'
 checks.append(map_path+': correct lit ground material')
 sun=next(a for a in all_actors if isinstance(a,u.DirectionalLight))
 sc=sun.get_component_by_class(u.DirectionalLightComponent)
 assert abs(sun.get_actor_rotation().pitch-(-43 if map_path.endswith('Lvl1') else -48))<.01
 assert abs(sc.get_editor_property('intensity')-(7.5 if map_path.endswith('Lvl1') else 7))<.01
 fog=next(a for a in all_actors if isinstance(a,u.ExponentialHeightFog))
 fc=fog.get_component_by_class(u.ExponentialHeightFogComponent)
 assert fc.get_editor_property('fog_max_opacity')<.13
 assert fc.get_editor_property('second_fog_data').get_editor_property('fog_density')==0
 if map_path.endswith('Lvl1'):assert sun.get_path_name().startswith('/Game/Maps/Lvl_B1_Lvl1_Lighting.')
 checks.append(map_path+': daylight direction and reduced haze persist after reload')
 volumes=[a for a in all_actors if isinstance(a,u.PostProcessVolume) and a.get_editor_property('enabled') and a.get_editor_property('unbound')]
 assert len(volumes)==1 and volumes[0].get_actor_label()=='Bloodshift Combat Grade'
 grade=volumes[0];s=grade.get_editor_property('settings')
 assert grade.get_editor_property('blend_weight')==1 and grade.get_editor_property('priority')==50
 assert s.get_editor_property('auto_exposure_min_brightness')==s.get_editor_property('auto_exposure_max_brightness')==1
 assert s.get_editor_property('motion_blur_amount')==0 and s.get_editor_property('scene_fringe_intensity')==0
 assert s.get_editor_property('bloom_intensity')<.2 and s.get_editor_property('vignette_intensity')<.2
 checks.append(map_path+': single stable-exposure grade, restrained bloom and no blur')
 lanterns=[a for a in all_actors if a.get_actor_label().startswith('Polish Lantern - ')]
 assert len(lanterns)==(4 if map_path.endswith('Lvl1') else 0)
 for a in lanterns:assert not a.get_component_by_class(u.PointLightComponent).get_editor_property('cast_shadows')
 checks.append(map_path+': expected lightweight lantern lighting')
mask=u.load_asset(folder+'/T_Lvl1_GroundWeights');assert mask and not mask.get_editor_property('srgb')
assert mask.get_editor_property('address_x')==mask.get_editor_property('address_y')==u.TextureAddress.TA_CLAMP
mat=u.load_asset(folder+'/M_Lvl1_Ground');code='\n'.join(e.get_editor_property('code') for e in lib.get_material_expressions(mat) if isinstance(e,u.MaterialExpressionCustom))
assert 'LayoutTextureSampler' in code and 'p - LayoutOrigin.xy' in code
checks.append('Linear world-space layout mask imported and used')
stats=lib.get_statistics(mat)
material_stats={k:stats.get_editor_property(k) for k in ['num_pixel_shader_instructions','num_samplers','num_pixel_texture_samples']}
(out/'Verification.json').write_text(json.dumps({'passed':True,'checks':checks,'material_stats':material_stats},indent=2));u.log('ENVIRONMENT_VERIFY_PASS '+str(len(checks)))
