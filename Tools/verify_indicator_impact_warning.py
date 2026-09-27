"""Render comparisons: only late charges animate; early charges and full auras do not."""
from pathlib import Path
import json
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
exec((root/'Tools/apply_indicator_impact_warning.py').read_text())
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
out = root/'Saved/AttackIndicators/ImpactWarning'
out.mkdir(parents=True, exist_ok=True)
report = {}
for name in ['M_AttackIndicatorRectangle', 'M_AttackIndicatorCircle', 'M_SamuraiLaneIndicator']:
    material = u.load_asset('/Game/HeavensDivide/Materials/' + name)
    mid = u.MaterialLibrary.create_dynamic_material_instance(world, material)
    mid.set_scalar_parameter_value('LaneAspect', 1.)
    target = u.RenderingLibrary.create_render_target2d(world, 256, 256, u.TextureRenderTargetFormat.RTF_RGBA8)
    def render(fill, strength):
        mid.set_scalar_parameter_value('FillAmount', fill)
        mid.set_scalar_parameter_value('ImpactWarningStrength', strength)
        u.RenderingLibrary.clear_render_target2d(world, target, u.LinearColor(.08,.1,.14,1))
        u.RenderingLibrary.draw_material_to_render_target(world, target, mid)
        return [(c.r,c.g,c.b,c.a) for c in u.RenderingLibrary.read_render_target(world,target,False)]
    differences = {}
    for fill in [0., .5, .79, .87, .93, .98, 1.]:
        baseline = render(fill, 0.)
        animated = render(fill, 1.)
        count = sum(a != b for a,b in zip(baseline,animated))
        differences[str(fill)] = count
        if fill <= .8 or fill == 1.:
            assert count == 0, (name, fill, 'warning active outside charge window', count)
        else:
            assert count > 50, (name, fill, 'warning not visible', count)
            assert baseline[128*256+128] == animated[128*256+128], 'Interior must remain unchanged'
            u.RenderingLibrary.export_render_target(world,target,str(out),name+'_'+str(fill)+'.png')
    mid.set_scalar_parameter_value('ImpactWarningStart', .95)
    assert render(.9,0.) == render(.9,1.), 'Start parameter must delay the accent'
    report[name] = differences
(out/'verification.json').write_text(json.dumps(report,indent=2))
u.log('IMPACT_WARNING_RENDER_PASS '+json.dumps(report))
