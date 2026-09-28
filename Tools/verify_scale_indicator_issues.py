"""Render color inheritance, transparent aura interior and continuous animation."""
from pathlib import Path
import json
import unreal as u
root=Path(u.Paths.project_dir()).resolve()

lib=u.MaterialEditingLibrary
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
out=root/'Saved/ScaleIndicatorFix/Render';out.mkdir(parents=True,exist_ok=True)
report={}
original=json.loads((root/'Saved/ScaleIndicatorFix/audit.json').read_text())
for name in ['M_AttackIndicatorRectangle','M_AttackIndicatorRectangle_Decal','M_AttackIndicatorCircle','M_AttackIndicatorCircle_Decal']:
    m=u.load_asset('/Game/HeavensDivide/Materials/'+name)
    for e in lib.get_material_expressions(m):
        if isinstance(e,u.MaterialExpressionVectorParameter):
            key=str(e.get_editor_property('parameter_name'))
            old=original[name]['parameters'][key].split('{',1)[1]
            now=str(e.get_editor_property('default_value')).split('{',1)[1]
            assert old==now,(name,key,'User color changed')
for enemy,field in [('Elites/BP_EnemyGorilla','contact_aura_material'),('Mobs/BP_EnemyGoblinBomb','attack_telegraph_material')]:
    bp=u.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/'+enemy)
    assert u.get_default_object(bp.generated_class()).get_editor_property(field).get_name()=='M_AttackIndicatorCircle'
bp=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
assert u.get_default_object(bp.generated_class()).get_editor_property('combo_ability').get_editor_property('vfx_radius_parameter_is_scale')
for name in ['M_AttackIndicatorRectangle','M_AttackIndicatorCircle']:
    material=u.load_asset('/Game/HeavensDivide/Materials/'+name)
    # In-memory test clock only; never save this test expression to the material.
    custom=next(e for e in lib.get_material_expressions(material) if isinstance(e,u.MaterialExpressionCustom))
    clock=lib.create_material_expression(material,u.MaterialExpressionScalarParameter,-900,1500)
    clock.set_editor_property('parameter_name','TestClock')
    assert lib.connect_material_expressions(clock,'',custom,'Clock')
    lib.recompile_material(material)
    mid=u.MaterialLibrary.create_dynamic_material_instance(world,material)
    mid.set_scalar_parameter_value('LaneAspect',1.)
    target=u.RenderingLibrary.create_render_target2d(world,256,256,u.TextureRenderTargetFormat.RTF_RGBA8)
    def render():
        u.RenderingLibrary.clear_render_target2d(world,target,u.LinearColor(.1,.1,.1,1))
        u.RenderingLibrary.draw_material_to_render_target(world,target,mid)
        return [(p.r,p.g,p.b,p.a) for p in u.RenderingLibrary.read_render_target(world,target,False)]
    mid.set_scalar_parameter_value('FillAmount',1.)
    authored=render()[128*256+128]
    mid.set_vector_parameter_value('FillColor',u.LinearColor(0,1,0,1))
    green=render()[128*256+128]
    assert green[1]>green[0]*3 and green[1]>green[2]*3,(name,green)
    mid.set_vector_parameter_value('FillColor',u.LinearColor(0,0,1,1))
    blue=render()[128*256+128]
    assert blue[2]>blue[0]*3 and blue[2]>blue[1]*3,(name,blue)
    u.RenderingLibrary.export_render_target(world,target,str(out),name+'_blue.png')
    mid.set_scalar_parameter_value('FillAmount',.93)
    mid.set_scalar_parameter_value('ImpactWarningStrength',1.)
    gentle=render()
    mid.set_scalar_parameter_value('ImpactWarningStrength',3.)
    strong=render()
    assert sum(a!=b for a,b in zip(gentle,strong))>100,'Triple ripple must visibly change the outline'
    report[name]={'authored_center':authored,'green_center':green,'blue_center':blue}
    if name=='M_AttackIndicatorCircle':
        mid.set_scalar_parameter_value('FillAmount',0.)
        mid.set_scalar_parameter_value('OutlineOnly',0.)
        mid.set_scalar_parameter_value('ContinuousRipple',1.)
        a=render()
        center=a[128*256+128]
        assert max(center[:3])<=2,'Gorilla interior must stay black'
        mid.set_scalar_parameter_value('TestClock',.17)
        b=render()
        assert b[128*256+128]==center,'Ripple must not animate the black interior'
        assert sum(x!=y for x,y in zip(a,b))>100,'Aura must ripple without fill progression'
        u.RenderingLibrary.export_render_target(world,target,str(out),'GorillaAuraRing.png')
        report[name]['ring_center']=center
assert report['M_AttackIndicatorRectangle']['authored_center']==report['M_AttackIndicatorCircle']['authored_center'], 'Authored fill colors must render identically'
(out/'verification.json').write_text(json.dumps(report,indent=2))
u.log('SCALE_INDICATOR_RENDER_PASS '+json.dumps(report))
