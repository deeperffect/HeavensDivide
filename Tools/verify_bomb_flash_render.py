"""Render saved Bomb flash on/off; run with -AllowCommandletRendering -noraytracing.

Uses the conventional mesh path for immediate commandlet scene captures. Runtime
retains Nanite and uses the same opaque material; Nanite itself needs a PIE check.
"""
import unreal
from pathlib import Path
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp=unreal.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinBomb')
a=actors.spawn_actor_from_class(bp.generated_class(),unreal.Vector())
bomb=next(m for m in a.get_components_by_class(unreal.StaticMeshComponent) if m.get_name()=='Bomb')
unreal.log('BOMB_RENDER_FLAGS '+str(bomb.get_editor_property('visible'))+' hidden='+str(bomb.get_editor_property('hidden_in_game')))
bomb.set_force_disable_nanite(True)
bomb.set_visibility(True)
bomb.set_hidden_in_game(False)
mat=unreal.get_default_object(bp.generated_class()).get_editor_property('bomb_flash_material')
unreal.MaterialEditingLibrary.recompile_material(mat)
mid=unreal.MaterialLibrary.create_dynamic_material_instance(a,mat)
mid.set_vector_parameter_value('FlashColor',unreal.LinearColor(3,0,0,1))
originals=[bomb.get_material(i) for i in range(bomb.get_num_materials())]
origin, extent, radius=unreal.SystemLibrary.get_component_bounds(bomb)
unreal.log('BOMB_BOUNDS '+str(origin)+' '+str(extent))
width=max(extent.x,extent.y,extent.z,1.0)*3
camera=actors.spawn_actor_from_class(unreal.SceneCapture2D,origin+unreal.Vector(-width*2,0,0),unreal.Rotator())
cap=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
cap.set_editor_property('projection_type',unreal.CameraProjectionMode.ORTHOGRAPHIC)
cap.set_editor_property('ortho_width',width)
cap.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)
cap.set_editor_property('capture_every_frame',False)
cap.set_editor_property('capture_on_movement',False)
rt=unreal.RenderingLibrary.create_render_target2d(a,128,128,unreal.TextureRenderTargetFormat.RTF_RGBA8)
cap.set_editor_property('texture_target',rt)
cap.set_editor_property('primitive_render_mode',unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
cap.show_only_component(bomb)
folder=Path(unreal.Paths.project_saved_dir(),'Automation/BombFlashRendering').resolve()
folder.mkdir(parents=True,exist_ok=True)
counts=[]
for name,on in [('flash_on',True),('flash_off',False)]:
 for slot,original in enumerate(originals):
  bomb.set_material(slot,mid if on else original)
 cap.capture_scene()
 pixels=unreal.RenderingLibrary.read_render_target(a,rt,False)
 counts.append(sum(1 for p in pixels if p.r>40 and p.r>p.g*2 and p.r>p.b*2))
 unreal.RenderingLibrary.export_render_target(a,rt,str(folder),name+'.png')
(folder/'result.txt').write_text(str(counts))
assert counts[0]>100 and counts[0]>counts[1]+100,counts
unreal.log('BOMB_FLASH_RENDER_PASS '+str(counts))
actors.destroy_actor(camera)
actors.destroy_actor(a)




