"""Render regression for the saved Grunt dissolve. Run with -AllowCommandletRendering -noraytracing."""
import unreal
from pathlib import Path

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGrunt')
actor = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector())
mesh = actor.get_editor_property('mesh')
material = mesh.get_material(0)
assert not unreal.MaterialEditingLibrary.recompile_material(material)
mid = mesh.create_dynamic_material_instance(0)
origin, extent = actor.get_actor_bounds(False)
width = max(extent.y, extent.z, 100.0) * 2.5
camera = actors.spawn_actor_from_class(unreal.SceneCapture2D,
    origin + unreal.Vector(-width * 2.0, 0, 0), unreal.Rotator())
cap = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
cap.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
cap.set_editor_property('ortho_width', width)
cap.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_BASE_COLOR)
cap.set_editor_property('capture_every_frame', False)
cap.set_editor_property('capture_on_movement', False)
rt = unreal.RenderingLibrary.create_render_target2d(actor, 256, 256, unreal.TextureRenderTargetFormat.RTF_RGBA8)
cap.set_editor_property('texture_target', rt)
cap.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
cap.show_only_actor_components(actor)
folder = Path(unreal.Paths.project_saved_dir(), 'Automation/DissolveRendering').resolve()
folder.mkdir(parents=True, exist_ok=True)
coverage = []
for amount in [0.0, 0.5, 1.0]:
    mid.set_scalar_parameter_value('DissolveAmount', amount)
    cap.capture_scene()
    pixels = unreal.RenderingLibrary.read_render_target(actor, rt, False)
    coverage.append(sum(1 for pixel in pixels if pixel.r or pixel.g or pixel.b))
    unreal.RenderingLibrary.export_render_target(actor, rt, str(folder), 'grunt_' + str(amount) + '.png')
assert coverage[0] > coverage[1] > coverage[2], coverage
assert coverage[2] == 0, coverage
(folder / 'result.txt').write_text('PASS: visible pixel counts at 0, 0.5, 1 = ' + str(coverage))
unreal.log('ENEMY_DISSOLVE_RENDER_PASS ' + str(coverage))
actors.destroy_actor(camera)
actors.destroy_actor(actor)

