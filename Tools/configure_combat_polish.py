"""Targeted visual assets and species sizing. Backups make the size pass idempotent."""
import unreal as u, json, shutil, re
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve(); out=root/'Saved/CombatPolish'; out.mkdir(parents=True,exist_ok=True)
lib=u.MaterialEditingLibrary; assets=u.AssetToolsHelpers.get_asset_tools()
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
def backup(path):
    src=root/'Content'/(path.removeprefix('/Game/').split('.')[0]+'.uasset'); dst=out/'Backup'/src.relative_to(root/'Content')
    if src.exists() and not dst.exists(): dst.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(src,dst)
def save(a): assert u.EditorAssetLibrary.save_loaded_asset(a,False),a.get_path_name()
def node(m,typ): return lib.create_material_expression(m,typ)
def material(name,code,parameters):
    path='/Game/HeavensDivide/Materials/'+name; backup(path)
    if u.EditorAssetLibrary.does_asset_exist(path):
        # Native constructors retain these assets. Update existing expressions
        # in place instead of deleting objects held by the engine's root set.
        m=u.load_asset(path)
        changed=False
        if m.get_editor_property('translucency_pass')!=u.MaterialTranslucencyPass.MTP_BEFORE_DOF:
            m.set_editor_property('translucency_pass',u.MaterialTranslucencyPass.MTP_BEFORE_DOF);changed=True
        for expr in lib.get_material_expressions(m):
            if isinstance(expr,u.MaterialExpressionCustom) and expr.get_editor_property('code')!=code:
                expr.set_editor_property('code',code);changed=True
        if changed:lib.recompile_material(m);save(m)
        return
    m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,path.rsplit('/',1)[0],u.Material,u.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    for k,v in dict(blend_mode=u.BlendMode.BLEND_TRANSLUCENT,shading_model=u.MaterialShadingModel.MSM_UNLIT,two_sided=True,translucency_pass=u.MaterialTranslucencyPass.MTP_BEFORE_DOF).items(): m.set_editor_property(k,v)
    inputs={'UV':node(m,u.MaterialExpressionTextureCoordinate)}
    for name,value in parameters.items():
        n=node(m,u.MaterialExpressionVectorParameter if isinstance(value,tuple) else u.MaterialExpressionScalarParameter)
        n.set_editor_property('parameter_name',name); n.set_editor_property('default_value',u.LinearColor(*value,1) if isinstance(value,tuple) else value); inputs[name]=n
    custom=node(m,u.MaterialExpressionCustom); ins=[]
    for name in inputs:
        i=u.CustomInput();i.set_editor_property('input_name',name);ins.append(i)
    custom.set_editor_property('inputs',ins);custom.set_editor_property('code',code);custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
    for name,n in inputs.items(): assert lib.connect_material_expressions(n,'',custom,name)
    for color,prop in [(True,u.MaterialProperty.MP_EMISSIVE_COLOR),(False,u.MaterialProperty.MP_OPACITY)]:
        mask=node(m,u.MaterialExpressionComponentMask)
        for k,v in dict(r=color,g=color,b=color,a=not color).items():mask.set_editor_property(k,v)
        assert lib.connect_material_expressions(custom,'',mask,'');assert lib.connect_material_property(mask,'',prop)
    lib.recompile_material(m);save(m)

material('M_EncounterSurface',r'''
float2 p=(UV-.5)*2;
float r=length(p), d=Shape>.5&&Shape<1.5?max(abs(p.x),abs(p.y)):r;
float inner=Shape>1.5?saturate(Inner):0;
float mask=(1-smoothstep(.985,1,d))*(Shape>1.5?smoothstep(inner-.008,inner,r):1);
float rim=1-smoothstep(.018,.045,abs(d-.965));
if(Shape>1.5) rim=max(rim,1-smoothstep(.012,.032,abs(r-inner)));
float progress=step(d,lerp(inner,1,saturate(Fill)));
float stripe=step(.62,frac((p.x+p.y)*7));
float pulse=.6+.4*sin(Pulse*18-r*18);
float3 ink=float3(.016,.011,.02);
float3 gold=float3(2.4,1.2,.26);
float3 col=lerp(ink,Tint.rgb*1.3,progress*.60+stripe*.09);
col=lerp(col,gold,rim*.95);
col=lerp(col,Tint.rgb*(2+1.8*pulse)+rim*gold,Impact);
return float4(col,mask*saturate(.55+rim*.4+progress*.15+Impact*.22)*Fade);
''',{'Shape':0.,'Inner':0.,'Fill':0.,'Impact':0.,'Fade':1.,'Pulse':0.,'Tint':(1,.19,.035)})
material('M_EncounterEnergy',r'''
float edge=pow(abs(UV.y*2-1),.7);
float bands=.55+.45*pow(abs(sin(UV.x*75)),8);
float a=lerp((1-edge)*bands*.55,.92,Solid)*Fade;
return float4(Tint.rgb*(1.8+bands*1.2),saturate(a));
''',{'Fade':1.,'Solid':0.,'Tint':(1,.25,.05)})

audit=json.loads((out/'AssetAudit.json').read_text())
factors={'EnemyGoblinCrawler':.72,'EnemyGoblinBomb':.8,'EnemyFish':.84,'EnemyFloatingSkeleton':.94,'EnemyCreature':1.02,
 'EnemyGrunt':1.08,'EnemyDevilRanged':1.12,'EnemyBear':1.22,'EnemyOgre':1.4,'EnemyGorilla':1.58,
 'AshenMarcher':.96,'FangStalker':.78,'MireWeaver':.94,'HexSniper':1.04,'AshSeer':1.2,'HornLancer':1.3,
 'FrostOracle':1.17,'GraveCantor':1.32,'WarDrummer':1.43,'OgreWarden':1.58,'StormGorilla':1.78,
 'CinderOracle':1.06,'SamuraiBoss':.94}
report=[]
for path,old in audit.items():
    name=path.rsplit('/',1)[1].split('.')[0].removeprefix('BP_')
    if name not in factors:continue
    backup(path); bp=u.load_asset(path); c=u.get_default_object(bp.generated_class()); cap=c.get_component_by_class(u.CapsuleComponent)
    old_scale=old['CollisionCylinder']['relative_scale3d']
    xyz=[float(re.search(k+r': ([\-\d.]+)',old_scale).group(1)) for k in ['x','y','z']]
    factor=factors[name];cap.set_editor_property('relative_scale3d',u.Vector(*[v*factor for v in xyz]))
    u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    report.append({'enemy':name,'size_multiplier':factor,'capsule_height':float(old['CollisionCylinder']['capsule_half_height'])*2*xyz[2]*factor})
(out/'EnemySizes.json').write_text(json.dumps(report,indent=2))

# Available only after the new native classes have been built.
if hasattr(u,'MagnetPickup'):
    pickpath='/Game/HeavensDivide/Blueprints/Encounters/BP_MagnetPickup'
    if u.EditorAssetLibrary.does_asset_exist(pickpath): pickup=u.load_asset(pickpath)
    else:
        f=u.BlueprintFactory();f.set_editor_property('parent_class',u.MagnetPickup)
        pickup=assets.create_asset('BP_MagnetPickup',pickpath.rsplit('/',1)[0],u.Blueprint,f)
    u.BlueprintEditorLibrary.compile_blueprint(pickup);save(pickup)
    urnpath='/Game/HeavensDivide/Blueprints/Encounters/BP_MagnetUrn'
    urn=u.load_asset(urnpath) if u.EditorAssetLibrary.does_asset_exist(urnpath) else u.EditorAssetLibrary.duplicate_asset('/Game/HeavensDivide/Blueprints/Encounters/BP_HealingUrn',urnpath)
    c=u.get_default_object(urn.generated_class());c.set_editor_property('drops_magnet',True)
    c.set_editor_property('magnet_class',pickup.generated_class())
    u.BlueprintEditorLibrary.compile_blueprint(urn);save(urn)
    path='/Game/HeavensDivide/Blueprints/Encounters/BP_EncounterDirector';backup(path)
    bp=u.load_asset(path);c=u.get_default_object(bp.generated_class());c.set_editor_property('magnet_urn_class',urn.generated_class())
    u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
u.log('COMBAT_POLISH_ASSETS_COMPLETE')
