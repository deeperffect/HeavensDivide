"""Build objective beacons and texture-free, animated card rarity frames. Back up edited assets."""
from pathlib import Path
from datetime import datetime
import json, shutil, os
import unreal as u

E=u.EditorAssetLibrary; M=u.MaterialEditingLibrary
root=Path(u.Paths.project_dir()).resolve()
out=root/'Saved/ObjectivePresentation';out.mkdir(parents=True,exist_ok=True)
backup=out/'Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
changed=[]
expression_pool={}

def preserve(path):
    source=root/'Content'/(path.removeprefix('/Game/')+'.uasset')
    if source.exists():
        target=backup/source.relative_to(root/'Content');target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)

def material(path,ui=False):
    preserve(path)
    folder,name=path.rsplit('/',1)
    mat=u.load_asset(path) if E.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.Material,u.MaterialFactoryNew())
    # Constructor-loaded assets can have rooted expressions. Reuse graph nodes on
    # regeneration instead of asking the editor to garbage-collect rooted objects.
    expression_pool[mat.get_path_name()]=list(M.get_material_expressions(mat))
    mat.set_editor_property('material_domain',u.MaterialDomain.MD_UI if ui else u.MaterialDomain.MD_SURFACE)
    mat.set_editor_property('blend_mode',u.BlendMode.BLEND_ALPHA_COMPOSITE if ui else u.BlendMode.BLEND_ADDITIVE)
    if not ui:
        mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('two_sided',True)
    return mat

def node(mat,cls,**props):
    pool=expression_pool[mat.get_path_name()]
    candidates=[item for item in pool if isinstance(item,cls)]
    if 'parameter_name' in props:
        candidates=[item for item in candidates if str(item.get_editor_property('parameter_name'))==props['parameter_name']]
    if cls==u.MaterialExpressionComponentMask:
        candidates=[item for item in candidates if all(item.get_editor_property(key)==props[key] for key in ['r','g','b','a'])]
    n=next(iter(candidates),None)
    if n:pool.remove(n)
    else:n=M.create_material_expression(mat,cls)
    for key,value in props.items():n.set_editor_property(key,value)
    return n

def scalar(mat,name,value):
    return node(mat,u.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)

def vector(mat,name,color):
    return node(mat,u.MaterialExpressionVectorParameter,parameter_name=name,default_value=u.LinearColor(*color))

def finish(mat,inputs,code):
    custom=node(mat,u.MaterialExpressionCustom,output_type=u.CustomMaterialOutputType.CMOT_FLOAT4,code=code)
    pins=[]
    for name in inputs:
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    custom.set_editor_property('inputs',pins)
    for name,expr in inputs.items():assert M.connect_material_expressions(expr,'',custom,name)
    for alpha,prop in [(False,u.MaterialProperty.MP_EMISSIVE_COLOR),(True,u.MaterialProperty.MP_OPACITY)]:
        mask=node(mat,u.MaterialExpressionComponentMask,r=not alpha,g=not alpha,b=not alpha,a=alpha)
        assert M.connect_material_expressions(custom,'',mask,'')
        assert M.connect_material_property(mask,'',prop)
    for key in ['EmissiveColor','Opacity']:
        u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),key,'(UseConstant=False)')
    M.layout_material_expressions(mat);M.recompile_material(mat)
    assert E.save_loaded_asset(mat,False)
    changed.append(mat.get_path_name())

folder='/Game/HeavensDivide/Materials/Objectives/'
mat=material(folder+'M_ObjectiveBeaconAura')
finish(mat,{'UV':node(mat,u.MaterialExpressionTextureCoordinate),'Clock':node(mat,u.MaterialExpressionTime),
            'Tint':vector(mat,'Tint',(1,.65,.08,1))},r'''
float2 p=(UV-.5)*2;
float r=length(p),a=atan2(p.y,p.x);
float pulse=.78+.22*sin(Clock*3.6);
float ring=exp(-abs(r-.70)*115)+exp(-abs(r-.87)*135)*.5;
float glow=exp(-abs(r-.70)*20)*.28;
float phase=frac(Clock*.55);
float wave=exp(-abs(r-(.30+phase*.60))*90)*(1-phase)*.65;
float orbit=pow(saturate(.5+.5*cos(a*3-Clock*1.8)),12)*exp(-abs(r-.80)*100);
float mask=saturate((ring+glow+wave+orbit)*pulse)*(1-smoothstep(.93,1.,r));
return float4(Tint.rgb*(3.5+orbit*2),mask*.85);
''')

mat=material(folder+'M_ObjectiveBeaconArrow')
# The marker remains readable when a tall prop is between the objective and camera.
mat.set_editor_property('disable_depth_test',True)
finish(mat,{'UV':node(mat,u.MaterialExpressionTextureCoordinate),'Clock':node(mat,u.MaterialExpressionTime),
    'Tint':vector(mat,'Tint',(1,.65,.08,1)),
    'ArrowTex':node(mat,u.MaterialExpressionTextureObjectParameter,parameter_name='ArrowTexture',
        texture=u.load_asset('/Game/Assets/VFX/MarkersPointersV1/Textures/Sprites/T_Arrow_01'))},r'''
// Align local down with the projected world-space line back to the objective.
// Screen-down alone misses the landmark near the edges of a perspective camera.
float3 center=WSAddDemote(GetObjectWorldPosition(Parameters),GetPreViewTranslation(Parameters));
float4 projected=mul(float4(center,1),ResolvedView.TranslatedWorldToClip);
float4 target=mul(float4(center-float3(0,0,200),1),ResolvedView.TranslatedWorldToClip);
float2 down=(target.xy/max(target.w,.001)-projected.xy/max(projected.w,.001))*float2(ResolvedView.ViewSizeAndInvSize.x,-ResolvedView.ViewSizeAndInvSize.y);
down=dot(down,down)>.0001?normalize(down):float2(0,1);
float2 p=UV-.5;
float2 uv=float2(dot(p,float2(down.y,-down.x)),dot(p,down))+.5;
uv.y+=sin(Clock*3.7)*.09;
float2 arrowUV=(uv-float2(.5,.48))*float2(1.25,1.45)+.5;
float inside=step(0,arrowUV.x)*step(arrowUV.x,1)*step(0,arrowUV.y)*step(arrowUV.y,1);
float arrow=Texture2DSample(ArrowTex,ArrowTexSampler,saturate(arrowUV)).a*inside;
float glow=exp(-dot((uv-float2(.5,.48))*float2(4,4),(uv-float2(.5,.48))*float2(4,4)))*.22;
glow*=smoothstep(0,.15,min(min(UV.x,1-UV.x),min(UV.y,1-UV.y)));
float pulse=.85+.15*sin(Clock*3.7);
float3 color=lerp(Tint.rgb,Tint.rgb*.5+float3(.65,.65,.65),arrow*.35)*5;
return float4(color,saturate(arrow+glow)*pulse);
''')

if os.environ.get('HD_PRESENTATION_OBJECTIVES_ONLY')=='1':
    (out/'ObjectiveAssets.json').write_text(json.dumps({'assets':changed,'backup':str(backup)},indent=2))
    u.log('OBJECTIVE_BEACON_ASSETS_SAVED')
    raise SystemExit

folder='/Game/HeavensDivide/Blueprints/UI/UpgradeUI/Materials/'
existing_frame=u.load_asset(folder+'M_UI_UpgradeRarityGlow')
if existing_frame and any(isinstance(expr,u.MaterialExpressionCustom) and
                         'The widget includes an 18-unit gutter' in expr.get_editor_property('code')
                         for expr in M.get_material_expressions(existing_frame)):
    # Keep the installed UI graph intact on repeated objective edits; the card
    # animation is independent of the world markers being refreshed here.
    (out/'Assets.json').write_text(json.dumps({'assets':changed,'backup':str(backup),
        'rarity_frame':'existing procedural frame preserved'},indent=2))
    u.log('OBJECTIVE_ASSETS_SAVED_EXISTING_RARITY_FRAME_PRESERVED')
    raise SystemExit
mat=material(folder+'M_UI_UpgradeRarityGlow',True)
finish(mat,{'UV':node(mat,u.MaterialExpressionTextureCoordinate),
    'Clock':scalar(mat,'AnimationTime',0),'Intensity':scalar(mat,'Intensity',.55),
    'HaloWidth':scalar(mat,'HaloWidth',8),'Speed':scalar(mat,'Speed',.7),
    'Color':vector(mat,'Color',(.04,.36,1,1))},r'''
// The widget includes an 18-unit gutter on all sides of its 400 x 600 card.
float2 p=abs((UV-.5)*float2(436,636));
float2 q=p-float2(197,297);
float d=max(max(q.x,q.y),(q.x+q.y+23)*.70710678);
float edge=abs(d);
float angle=atan2((UV.y-.5)*1.5,UV.x-.5);
float pulse=.84+.16*sin(Clock*2.4);
float sweep=pow(saturate(.5+.5*cos(angle-Clock*Speed*2.0)),18);
float second=pow(saturate(.5+.5*cos(angle+Clock*Speed*1.5+2.5)),24)*saturate(Intensity-.65);
float rim=exp(-edge/1.65);
float halo=exp(-edge/HaloWidth)*.55+exp(-edge/(HaloWidth*2.4))*.15;
float shimmer=.75+.25*sin(angle*13-Clock*5);
float trace=(sweep+second)*(exp(-edge/5.5)+exp(-edge/(HaloWidth*1.5))*.4);
float alpha=saturate((halo*pulse+rim*(.65+.18*shimmer)+trace)*Intensity);
// Fade out well before the image boundary, so no rectangular cutoff surrounds the card.
float gutter=saturate(min(min(UV.x,1-UV.x)*436,min(UV.y,1-UV.y)*636)/6);
alpha*=gutter;
float3 tint=lerp(Color.rgb,float3(1,.96,.86),saturate(rim*.30+trace*.7));
return float4(tint*alpha*(.9+Intensity*.3),alpha);
''')
for name,color,intensity,width,speed in [
    ('Rare',(.025,.32,1,1),.62,7.,.75),
    ('Epic',(.60,.06,1,1),1.,11.,.95),
    ('Legendary',(1,.46,.025,1),1.55,16.,1.2)]:
    path=folder+'MI_UI_Upgrade'+name;preserve(path)
    inst=u.load_asset(path)
    assert inst,path
    M.set_material_instance_parent(inst,mat)
    for key,value in [('Intensity',intensity),('HaloWidth',width),('Speed',speed),('AnimationTime',0.)]:
        M.set_material_instance_scalar_parameter_value(inst,key,value)
    M.set_material_instance_vector_parameter_value(inst,'Color',u.LinearColor(*color))
    M.update_material_instance(inst);assert E.save_loaded_asset(inst,False)
    changed.append(path)
(out/'Assets.json').write_text(json.dumps({'assets':changed,'backup':str(backup)},indent=2))
u.log('OBJECTIVE_AND_RARITY_ASSETS_SAVED')
