"""Stage and preview the supplied Grave Cantor without altering Desktop originals."""
import bpy, json, shutil
from pathlib import Path
from mathutils import Vector, Matrix

ROOT = Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
SOURCE = Path(r'C:\Users\deepe\OneDrive\Desktop\Grave Cantor')
OUT = ROOT / 'Saved/GraveCantorReplacement'
ART = ROOT / 'Art/Enemies/GraveCantor'
OUT.mkdir(parents=True, exist_ok=True)
ART.mkdir(parents=True, exist_ok=True)
for filename in ['GraveCantor.fbx', 'GraveCantor_base.jpg']:
    shutil.copy2(SOURCE / filename, ART / filename)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(ART / 'GraveCantor.fbx'))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
points = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
low = Vector([min(p[i] for p in points) for i in range(3)])
high = Vector([max(p[i] for p in points) for i in range(3)])
report = {'original_bounds': [list(low), list(high)], 'objects': []}
for obj in bpy.context.scene.objects:
    row = {'name': obj.name, 'type': obj.type, 'matrix': [list(r) for r in obj.matrix_world]}
    if obj.type == 'MESH':
        obj.data.calc_loop_triangles()
        row.update(vertices=len(obj.data.vertices), triangles=len(obj.data.loop_triangles),
                   uv_layers=[x.name for x in obj.data.uv_layers], groups=[g.name for g in obj.vertex_groups])
    if obj.type == 'ARMATURE': row['bones'] = [b.name for b in obj.data.bones]
    report['objects'].append(row)

# All replacement meshes use a one-metre reference height, with feet at zero.
height = high.z-low.z
offset = Vector(((low.x+high.x)/2, (low.y+high.y)/2, low.z))
for obj in meshes:
    world = obj.matrix_world.copy()
    for v in obj.data.vertices: v.co = (world @ v.co-offset)/height
    obj.matrix_world = Matrix.Identity(4)
image = bpy.data.images.load(str(ART / 'GraveCantor_base.jpg'), check_existing=True)
report['texture_size'] = list(image.size)
material = bpy.data.materials.new('GraveCantor_Base')
material.use_nodes = True
bsdf = material.node_tree.nodes.get('Principled BSDF')
bsdf.inputs['Roughness'].default_value = .78
tex = material.node_tree.nodes.new('ShaderNodeTexImage'); tex.image = image
material.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
for obj in meshes: obj.data.materials.clear(); obj.data.materials.append(material)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'; scene.cycles.samples = 12; scene.cycles.use_denoising = True
scene.render.resolution_x = 640; scene.render.resolution_y = 768; scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'AgX'
scene.world = bpy.data.worlds.new('Studio'); scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value = (.09,.11,.14,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .65
center = Vector((0,0,.5))
def aim(obj): obj.rotation_euler = (center-obj.location).to_track_quat('-Z','Y').to_euler()
for name, position, energy in [('Key',(-1,-1.5,1.5),35),('Fill',(1,-1,.9),20),('Rim',(0,1.5,1.5),40)]:
    light = bpy.data.lights.new(name,'AREA'); light.energy = energy; light.size = 1
    obj = bpy.data.objects.new(name,light); scene.collection.objects.link(obj); obj.location = position; aim(obj)
camera = bpy.data.objects.new('PreviewCamera',bpy.data.cameras.new('PreviewCamera'))
scene.collection.objects.link(camera); camera.data.type = 'ORTHO'; camera.data.ortho_scale = 1.15
scene.camera = camera
for view, position in [('Front',(0,-3,.5)),('Side',(3,0,.5)),('Back',(0,3,.5))]:
    camera.location = position; aim(camera)
    scene.render.filepath = str(OUT / ('GraveCantor_'+view+'.png'))
    bpy.ops.render.render(write_still=True)
image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'GraveCantor_Source.blend'))
(OUT / 'SourceInspection.json').write_text(json.dumps(report,indent=2))
print('GRAVE_CANTOR_SOURCE_INSPECTED',json.dumps(report),flush=True)
