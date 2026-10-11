"""Mesh-UV stone for vertical breakable walls; preserves the source texture color."""
from pathlib import Path
import shutil, unreal as u
folder='/Game/HeavensDivide/Materials/EnvironmentPolish';path=folder+'/M_EncounterSealStone'
lib=u.MaterialEditingLibrary
source=u.load_asset(folder+'/MI_Lvl1_Ground')
stone=lib.get_material_instance_texture_parameter_value(source,'StoneTexture');assert stone
if u.EditorAssetLibrary.does_asset_exist(path):
 material=u.load_asset(path)
else:
 material=u.AssetToolsHelpers.get_asset_tools().create_asset('M_EncounterSealStone',folder,u.Material,u.MaterialFactoryNew())
 uv=lib.create_material_expression(material,u.MaterialExpressionTextureCoordinate,-600,0)
 texture=lib.create_material_expression(material,u.MaterialExpressionTextureSampleParameter2D,-400,0)
 texture.set_editor_property('parameter_name','StoneTexture');texture.set_editor_property('texture',stone)
 assert lib.connect_material_expressions(uv,'',texture,'')
 assert lib.connect_material_property(texture,'RGB',u.MaterialProperty.MP_BASE_COLOR)
 glow=lib.create_material_expression(material,u.MaterialExpressionMultiply,-180,160);glow.set_editor_property('const_b',.12)
 assert lib.connect_material_expressions(texture,'RGB',glow,'A')
 assert lib.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
 for name,value,prop in [('Roughness',.9,u.MaterialProperty.MP_ROUGHNESS),('Specular',.1,u.MaterialProperty.MP_SPECULAR)]:
  node=lib.create_material_expression(material,u.MaterialExpressionScalarParameter,-400,300 if name=='Roughness' else 450)
  node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',value)
  assert lib.connect_material_property(node,'',prop)
 lib.layout_material_expressions(material);lib.recompile_material(material)
 assert u.EditorAssetLibrary.save_loaded_asset(material,False)
u.log('ENCOUNTER_WALL_MATERIAL_OK '+stone.get_path_name())

# The old smoke template can contain a missing texture or default renderer material.
# Give the new departure its own validated translucent material.
smoke_path='/Game/HeavensDivide/VFX/Swap/M_NinjaDepartureSmoke'
smoke=u.load_asset(smoke_path) if u.EditorAssetLibrary.does_asset_exist(smoke_path) else u.EditorAssetLibrary.duplicate_asset('/Game/HeavensDivide/VFX/Swap/M_SwapSmoke',smoke_path)
root=Path(u.Paths.project_dir()).resolve()
source=root/'Content/HeavensDivide/VFX/Swap/M_SwapSmoke.uasset';backup=root/'Saved/GameplayPolish/Backups/M_SwapSmoke.uasset'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(source,backup)
for smoke_material in [smoke,u.load_asset('/Game/HeavensDivide/VFX/Swap/M_SwapSmoke')]:
 # A soft procedural puff has no missing texture dependency or sprite-sheet edges.
 lib.delete_all_material_expressions(smoke_material)
 uv=lib.create_material_expression(smoke_material,u.MaterialExpressionTextureCoordinate,-500,0)
 particle=lib.create_material_expression(smoke_material,u.MaterialExpressionParticleColor,-500,180)
 puff=lib.create_material_expression(smoke_material,u.MaterialExpressionCustom,-200,0)
 puff.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
 puff.set_editor_property('code','''float2 p=(UV-.5)*2;
float r=length(p), a=atan2(p.y,p.x);
float edge=.72+.07*sin(a*5)+.04*sin(a*9+1.3);
float density=1-smoothstep(edge-.4,edge+.05,r);
density*=.84+.16*sin(p.x*8+sin(p.y*6))*sin(p.y*7);
float3 ink=float3(.028,.006,.065);
float3 color=lerp(Tint*.7,ink,saturate(density*1.2));
return float4(color,saturate(density)*Opacity*.65);''')
 pins=[]
 for name in ['UV','Tint','Opacity']:
  pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
 puff.set_editor_property('inputs',pins)
 assert lib.connect_material_expressions(uv,'',puff,'UV')
 assert lib.connect_material_expressions(particle,'RGB',puff,'Tint')
 assert lib.connect_material_expressions(particle,'A',puff,'Opacity')
 for alpha,prop in [(False,u.MaterialProperty.MP_EMISSIVE_COLOR),(True,u.MaterialProperty.MP_OPACITY)]:
  mask=lib.create_material_expression(smoke_material,u.MaterialExpressionComponentMask,80,120 if alpha else 0)
  for channel in ['r','g','b']:mask.set_editor_property(channel,not alpha)
  mask.set_editor_property('a',alpha)
  assert lib.connect_material_expressions(puff,'',mask,'')
  assert lib.connect_material_property(mask,'',prop)
 smoke_material.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT)
 smoke_material.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
 lib.set_material_usage(smoke_material,u.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
 lib.recompile_material(smoke_material);assert u.EditorAssetLibrary.save_loaded_asset(smoke_material,False)
fx=u.load_asset('/Game/HeavensDivide/VFX/Swap/NS_NinjaDeparture')
if fx:
 for emitter in u.SwapVFXSetupLibrary.get_emitters(fx):
  for renderer in emitter.get_editor_property('RendererProperties'):renderer.set_editor_property('Material',smoke)
 assert u.SwapVFXSetupLibrary.rebuild_system(fx,u.LinearColor(.45,.12,1,1));assert u.EditorAssetLibrary.save_loaded_asset(fx,False)
u.log('NINJA_DEPARTURE_MATERIAL_OK procedural soft puff')
