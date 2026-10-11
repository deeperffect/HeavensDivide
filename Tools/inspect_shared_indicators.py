import unreal as u
from pathlib import Path
import json

out = Path(u.Paths.project_saved_dir())/'UnifiedEnemyIndicators'
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
report = {}
for name in ['M_AttackIndicatorCircle', 'M_AttackIndicatorRectangle']:
    m = u.load_asset('/Game/HeavensDivide/Materials/'+name)
    t = u.AssetExportTask(); t.object=m; t.exporter=u.ObjectExporterT3D()
    t.filename=str(out/(name+'.copy')); t.automated=True; t.prompt=False; t.replace_identical=True
    assert u.Exporter.run_asset_export_task(t)
    mid = u.MaterialLibrary.create_dynamic_material_instance(world, m)
    mid.set_scalar_parameter_value('FillAmount', .55)
    mid.set_scalar_parameter_value('Fade', 1.)
    mid.set_scalar_parameter_value('LaneAspect', 1.)
    target = u.RenderingLibrary.create_render_target2d(world,256,256,u.TextureRenderTargetFormat.RTF_RGBA8)
    u.RenderingLibrary.clear_render_target2d(world,target,u.LinearColor(.1,.1,.1,1))
    u.RenderingLibrary.draw_material_to_render_target(world,target,mid)
    pixels=u.RenderingLibrary.read_render_target(world,target,False)
    report[name]={'red_pixels':sum(p.r>p.g*1.3 and p.r>80 for p in pixels),
                  'center':str(pixels[128*256+128]),
                  'fade':mid.get_scalar_parameter_value('Fade')}
(out/'DirectRender.json').write_text(json.dumps(report,indent=2))
u.log('SHARED_INDICATOR_INSPECTION '+json.dumps(report))
