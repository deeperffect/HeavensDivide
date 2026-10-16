"""Save the gold selection / ember-red banish retainer material, with a backup."""
from pathlib import Path
from datetime import datetime
import json, shutil
import unreal as u
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary
folder='/Game/HeavensDivide/Materials/UI';name='M_UpgradeCardSelectionDissolve';path=folder+'/'+name
root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/CardDissolve';out.mkdir(parents=True,exist_ok=True)
source=root/'Content'/(path.removeprefix('/Game/')+'.uasset')
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')/source.name
if source.exists():
 backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,backup)
mat=u.load_asset(path) if E.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.Material,u.MaterialFactoryNew())
mat.set_editor_property('material_domain',u.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode',u.BlendMode.BLEND_ALPHA_COMPOSITE)
# Reuse expressions: constructor-loaded materials can contain rooted graph nodes.
pool=list(M.get_material_expressions(mat))
def node(cls,x,y,**props):
 matches=[n for n in pool if isinstance(n,cls) and all((str(n.get_editor_property(k)) if k=='parameter_name' else n.get_editor_property(k))==v for k,v in props.items())]
 n=next(iter(matches),None)
 if n:pool.remove(n)
 else:n=M.create_material_expression(mat,cls,x,y)
 for key,value in props.items():n.set_editor_property(key,value)
 return n
tex=node(u.MaterialExpressionTextureObjectParameter,-650,-150,parameter_name='CardTexture')
tex.set_editor_property('parameter_name','CardTexture');tex.set_editor_property('texture',u.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
uv=node(u.MaterialExpressionTextureCoordinate,-650,100)
progress=node(u.MaterialExpressionScalarParameter,-650,300,parameter_name='Progress')
progress.set_editor_property('parameter_name','Progress');progress.set_editor_property('default_value',0.)
burn=node(u.MaterialExpressionScalarParameter,-650,450,parameter_name='Burn')
burn.set_editor_property('default_value',0.)
custom=node(u.MaterialExpressionCustom,-250,0)
custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
pins=[]
for key in ['Card','UV','Progress','Burn']:
 pin=u.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
custom.set_editor_property('inputs',pins)
custom.set_editor_property('code','''
float4 card=Texture2DSample(Card,CardSampler,UV);
float grain=frac(sin(dot(floor(UV*float2(96,144)),float2(12.9898,78.233)))*43758.5453);
float waves=.5+.5*sin(UV.x*27+sin(UV.y*19)*2);
float burn=saturate(Burn);
float tongues=sin(UV.x*51+sin(UV.x*17)*3+Progress*9)*.026;
float field=(1-UV.y)*.82+waves*.12+grain*.06+burn*tongues;
float front=lerp(-.12,1.12,saturate(Progress));
float mask=smoothstep(front-.025,front+.025,field);
float distance=abs(field-front);
float halo=(1-smoothstep(.012,.095,distance))*mask;
float edge=(1-smoothstep(.006,.040,distance))*mask;
float core=(1-smoothstep(.002,.013,distance))*mask;
// Banishes char just ahead of the advancing flame, with a flickering ember rim.
float charBand=(1-smoothstep(.025,.14,field-front))*mask*burn;
float flicker=lerp(1.,.78+.22*sin(UV.x*91+Progress*93),burn);
float3 rim=lerp(float3(1.,.70,.035),float3(1.,.075,.003),burn);
float3 hot=lerp(float3(1.,.95,.48),float3(1.,.48,.035),burn);
float3 ink=card.rgb*lerp(1.,.13,charBand);
// Retainer textures use premultiplied color; keep the dissolve premultiplied too.
float3 glow=(rim*(halo*.55+edge*1.8)+hot*core*1.4)*flicker;
return float4(ink*mask+glow*card.a,card.a*mask);
''')
for expr,key in [(tex,'Card'),(uv,'UV'),(progress,'Progress'),(burn,'Burn')]:M.connect_material_expressions(expr,'',custom,key)
rgb=node(u.MaterialExpressionComponentMask,100,0,r=True,g=True,b=True,a=False)
for key,value in [('r',True),('g',True),('b',True),('a',False)]:rgb.set_editor_property(key,value)
alpha=node(u.MaterialExpressionComponentMask,100,150,r=False,g=False,b=False,a=True)
for key,value in [('r',False),('g',False),('b',False),('a',True)]:alpha.set_editor_property(key,value)
assert M.connect_material_expressions(custom,'',rgb,''), 'RGB mask input connection failed'
assert M.connect_material_expressions(custom,'',alpha,''), 'Alpha mask input connection failed'
M.connect_material_property(rgb,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
M.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
for key in ['EmissiveColor','Opacity']:u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),key,'(UseConstant=False)')
M.recompile_material(mat);assert E.save_loaded_asset(mat,False)
(out/'Assets.json').write_text(json.dumps({'material':path,'backup':str(backup),'selection_dissolve_seconds':.64,'banish_dissolve_seconds':.64},indent=2))
u.log('UPGRADE_SELECTION_DISSOLVE_SAVED')
