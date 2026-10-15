"""Create the lightweight selected-card retainer dissolve material."""
from pathlib import Path
import unreal as u
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary
folder='/Game/HeavensDivide/Materials/UI';name='M_UpgradeCardSelectionDissolve';path=folder+'/'+name
mat=u.load_asset(path) if E.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.Material,u.MaterialFactoryNew())
mat.set_editor_property('material_domain',u.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode',u.BlendMode.BLEND_ALPHA_COMPOSITE)
M.delete_all_material_expressions(mat)
tex=M.create_material_expression(mat,u.MaterialExpressionTextureObjectParameter,-650,-150)
tex.set_editor_property('parameter_name','CardTexture');tex.set_editor_property('texture',u.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
uv=M.create_material_expression(mat,u.MaterialExpressionTextureCoordinate,-650,100)
progress=M.create_material_expression(mat,u.MaterialExpressionScalarParameter,-650,300)
progress.set_editor_property('parameter_name','Progress');progress.set_editor_property('default_value',0.)
custom=M.create_material_expression(mat,u.MaterialExpressionCustom,-250,0)
custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
pins=[]
for key in ['Card','UV','Progress']:
 pin=u.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
custom.set_editor_property('inputs',pins)
custom.set_editor_property('code','''
float4 card=Texture2DSample(Card,CardSampler,UV);
float grain=frac(sin(dot(floor(UV*float2(96,144)),float2(12.9898,78.233)))*43758.5453);
float waves=.5+.5*sin(UV.x*27+sin(UV.y*19)*2);
float field=(1-UV.y)*.82+waves*.12+grain*.06;
float front=lerp(-.12,1.12,saturate(Progress));
float mask=smoothstep(front-.025,front+.025,field);
float edge=(1-smoothstep(.008,.055,abs(field-front)))*mask;
// Retainer textures use premultiplied color; keep the dissolve premultiplied too.
return float4(card.rgb*mask+float3(1.0,.58,.12)*edge*card.a*.85,card.a*mask);
''')
for node,key,out in [(tex,'Card',''),(uv,'UV',''),(progress,'Progress','')]:M.connect_material_expressions(node,out,custom,key)
rgb=M.create_material_expression(mat,u.MaterialExpressionComponentMask,100,0)
for key,value in [('r',True),('g',True),('b',True),('a',False)]:rgb.set_editor_property(key,value)
alpha=M.create_material_expression(mat,u.MaterialExpressionComponentMask,100,150)
for key,value in [('r',False),('g',False),('b',False),('a',True)]:alpha.set_editor_property(key,value)
assert M.connect_material_expressions(custom,'',rgb,''), 'RGB mask input connection failed'
assert M.connect_material_expressions(custom,'',alpha,''), 'Alpha mask input connection failed'
M.connect_material_property(rgb,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
M.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
for key in ['EmissiveColor','Opacity']:u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),key,'(UseConstant=False)')
M.recompile_material(mat);assert E.save_loaded_asset(mat,False)
u.log('UPGRADE_SELECTION_DISSOLVE_SAVED')
