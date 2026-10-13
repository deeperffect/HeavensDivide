"""Targeted saved-asset migration for the October gameplay polish pass. Run in UE Python."""
from pathlib import Path
from datetime import datetime
import json, shutil, runpy, unreal as u
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/GameplayPolish';out.mkdir(parents=True,exist_ok=True)
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
report={'backup':str(backup),'characters':{},'spawners':[]}
lib=u.MaterialEditingLibrary; folder='/Game/HeavensDivide/Materials/EnvironmentPolish'
def preserve(asset,ext='.uasset'):
 p=root/'Content'/(asset.removeprefix('/Game/')+ext)
 if p.exists():
  b=backup/p.relative_to(root);b.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,b)
def save(obj):assert u.EditorAssetLibrary.save_loaded_asset(obj,False)
for name in ['T_Lvl1_GroundWeights','M_Lvl1_Ground','MI_Lvl1_Ground']:preserve(folder+'/'+name)
mat=u.load_asset(folder+'/M_Lvl1_Ground')
reference_ground=mat and any(isinstance(e,u.MaterialExpressionCustom) and 'REFERENCE_GROUND' in e.get_editor_property('code') for e in lib.get_material_expressions(mat))
if reference_ground:
 # Keep the latest ground shader, texture overrides and scale when rerunning this older pass.
 runpy.run_path(str(root/'Tools/apply_reference_ground.py'))
 report['ground']='Reference ground refreshed with the current focused ground pipeline.'
else:
 task=u.AssetImportTask()
 for k,v in dict(filename=str(root/'Art/EnvironmentPolish/Lvl1_GroundWeights.png'),destination_path=folder,destination_name='T_Lvl1_GroundWeights',automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
 u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 mask=u.load_asset(folder+'/T_Lvl1_GroundWeights');mask.set_editor_property('srgb',False)
 mask.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
 mask.set_editor_property('address_x',u.TextureAddress.TA_CLAMP);mask.set_editor_property('address_y',u.TextureAddress.TA_CLAMP);save(mask)
 mat=u.load_asset(folder+'/M_Lvl1_Ground')
 expr=next(e for e in lib.get_material_expressions(mat) if isinstance(e,u.MaterialExpressionCustom) and 'automaticWeights' in e.get_editor_property('code'))
 code=expr.get_editor_property('code')
 start=code.index('float3 rgb =') if 'float3 rgb =' in code else code.index('// Original stone color')
 code=code[:start]+'''// Original stone color: only vegetation and soil receive the material grade.
 float variation = 1.0 + (n.noise(macro * .47 + 12.0) - .5) * MacroVariation;
 float3 gradedGrass = lerp(dot(grass,float3(.2126,.7152,.0722)).xxx,grass,Saturation)*Brightness*variation;
 float3 gradedDirt = lerp(dot(dirt,float3(.2126,.7152,.0722)).xxx,dirt,Saturation)*Brightness*variation;
 float3 originalStone = Texture2DSample(StoneTexture,StoneTextureSampler,p/max(StoneTileSize,1.0)).rgb;
 return max(0.0,gradedGrass*weights.x+gradedDirt*weights.y+originalStone*weights.z);
 '''
 expr.set_editor_property('code',code);lib.recompile_material(mat);save(mat)
 mi=u.load_asset(folder+'/MI_Lvl1_Ground');lib.set_material_instance_vector_parameter_value(mi,'StoneTint',u.LinearColor(1,1,1,1));lib.update_material_instance(mi);save(mi)
 report['ground']='Stone samples bypass material tint, saturation, brightness and macro darkening; natural lighting still applies.'
runpy.run_path(str(root/'Tools/create_encounter_wall_material.py'))

vfx=runpy.run_path(str(root/'Tools/create_swap_vfx.py'))
for character,source in [('Ninja','NS_SwapVioletSmoke'),('Samurai','NS_SwapInkSlash')]:
 effect_path='/Game/HeavensDivide/VFX/Swap/NS_'+character+'Departure'
 preserve(effect_path)
 effect=u.load_asset(effect_path) if u.EditorAssetLibrary.does_asset_exist(effect_path) else u.EditorAssetLibrary.duplicate_asset('/Game/HeavensDivide/VFX/Swap/'+source,effect_path)
 emitter=u.SwapVFXSetupLibrary.get_emitters(effect)[0]
 if character=='Ninja':
  for renderer in emitter.get_editor_property('RendererProperties'):renderer.set_editor_property('Material',u.load_asset('/Game/HeavensDivide/VFX/Swap/M_NinjaDepartureSmoke'))
 modules={m.get_class().get_name().replace('NiagaraStatelessModule_',''):m for m in emitter.get_editor_property('Modules')}
 vfx['range_value'](modules['InitializeParticle'],'LifetimeDistribution',.25,.5)
 vfx['distribution'](modules['AddVelocity'],'LinearVelocityDistribution',[-250,-250,80,250,250,210] if character=='Ninja' else [-90,-90,40,90,90,130],'NON_UNIFORM_RANGE')
 vfx['distribution'](modules['InitializeParticle'],'SpriteSizeDistribution',[75,75,130,130] if character=='Ninja' else [190,60,290,85],'NON_UNIFORM_RANGE')
 color=u.LinearColor(.45,.12,1,1) if character=='Ninja' else u.LinearColor(1,.12,.08,1)
 assert u.SwapVFXSetupLibrary.rebuild_system(effect,color);save(effect)
 bp_path='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+character;preserve(bp_path)
 bp=u.load_asset(bp_path);component=u.get_default_object(bp.generated_class()).get_editor_property('swap_presentation')
 keys=['entrance_play_rate','departure_play_rate','swap_freeze_duration']
 before={key:component.get_editor_property(key) for key in keys}
 for key,value in dict(entrance_play_rate=1.35,departure_play_rate=1.5,swap_freeze_duration=.62,freeze_ease_in_duration=.08,departure_vfx=effect,enable_departure_trail=True,departure_trail_lifetime=.22).items():component.set_editor_property(key,value)
 u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
 report['characters'][character]={'before':before,'after':{key:component.get_editor_property(key) for key in keys},'departure':effect_path}

def tune(spawner):
 for key,value in dict(stale_grunt_minimum_distance=2400.0,max_grunts_recycled_per_evaluation=6,replacement_sector_randomness=.1).items():spawner.set_editor_property(key,value)
 report['spawners'].append(spawner.get_path_name())
for asset in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints',True,False):
 if 'Spawn' not in asset:continue
 bp=u.load_asset(asset)
 if not isinstance(bp,u.Blueprint):continue
 obj=u.get_default_object(bp.generated_class())
 if not isinstance(obj,u.EnemySpawner):continue
 preserve(asset.split('.')[0]);tune(obj);u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
map_path='/Game/Maps/Lvl_B1_Lvl1';preserve(map_path,'.umap');assert level.load_level(map_path)
for actor in actors.get_all_level_actors():
 if isinstance(actor,u.EnemySpawner):tune(actor)
assert level.save_current_level()
(out/'AssetChanges.json').write_text(json.dumps(report,indent=2))
u.log('GAMEPLAY_POLISH_ASSETS_OK')
