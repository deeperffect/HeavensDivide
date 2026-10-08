import bpy
import json
from pathlib import Path
from mathutils import Vector

OUT = Path(r"C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide\Art\Characters\Ninja\NinjaV6_Textured")
bpy.ops.wm.open_mainfile(filepath=str(OUT / 'NinjaV6_Inspection.blend'))
scene = bpy.context.scene
obj = bpy.data.objects['NinjaV6']
material = obj.data.materials[0]
principled = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
image_node = next(n for n in material.node_tree.nodes if n.type == 'TEX_IMAGE')
output = next(n for n in material.node_tree.nodes if n.type == 'OUTPUT_MATERIAL')
print('MATERIAL_DETAILS', json.dumps({
    'image_colorspace': image_node.image.colorspace_settings.name,
    'inputs': {i.name: str(i.default_value) for i in principled.inputs if hasattr(i, 'default_value')},
    'links': [(l.from_node.name, l.from_socket.name, l.to_node.name, l.to_socket.name) for l in material.node_tree.links],
    'flat_faces': sum(not p.use_smooth for p in obj.data.polygons),
}))
scene.camera.location = Vector((0, -3, .486))
scene.camera.rotation_euler = (Vector((0, 0, .486)) - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
scene.camera.data.ortho_scale = 1.056
scene.render.resolution_x = 1024
scene.render.resolution_y = 1536
scene.cycles.samples = 16
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'None'
emission = material.node_tree.nodes.new('ShaderNodeEmission')
material.node_tree.links.new(image_node.outputs['Color'], emission.inputs['Color'])
material.node_tree.links.new(emission.outputs[0], output.inputs['Surface'])
scene.render.filepath = str(OUT / 'Original_Albedo_Front.png')
bpy.ops.render.render(write_still=True)
material.node_tree.links.new(principled.outputs[0], output.inputs['Surface'])
for light in bpy.data.lights:
    light.energy *= .15
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .25
scene.render.filepath = str(OUT / 'Original_Neutral_Front.png')
bpy.ops.render.render(write_still=True)
