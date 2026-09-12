"""Render saved ability materials directly to a target; requires commandlet rendering."""
import unreal
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mat=unreal.load_asset('/Game/HeavensDivide/Materials/M_AbilityRing')
unreal.MaterialEditingLibrary.recompile_material(mat)
mid=unreal.MaterialLibrary.create_dynamic_material_instance(world,mat)
mid.set_vector_parameter_value('Tint',unreal.LinearColor(3,1,0.1,1))
rt=unreal.RenderingLibrary.create_render_target2d(world,256,256,unreal.TextureRenderTargetFormat.RTF_RGBA8)
folder=Path(unreal.Paths.project_saved_dir(),'Automation/AbilityVisuals').resolve();folder.mkdir(parents=True,exist_ok=True)
counts=[]
for value in [1.0,0.0]:
 mid.set_scalar_parameter_value('Intensity',value)
 unreal.RenderingLibrary.clear_render_target2d(world,rt,unreal.LinearColor(0,0,0,0))
 unreal.RenderingLibrary.draw_material_to_render_target(world,rt,mid)
 pixels=unreal.RenderingLibrary.read_render_target(world,rt,False)
 counts.append(sum(1 for p in pixels if p.r>50))
 unreal.RenderingLibrary.export_render_target(world,rt,str(folder),'ring_'+str(value)+'.png')
assert 300<counts[0]<15000 and counts[1]==0,counts
(folder/'result.txt').write_text('PASS: lit ring pixels '+str(counts))
unreal.log('ABILITY_VISUALS_PASS '+str(counts))
