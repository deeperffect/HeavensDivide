import bpy
import json
from pathlib import Path
from mathutils import Vector

ROOT = Path(r"C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide")
SOURCE = Path(r"C:\Users\deepe\OneDrive\Desktop\NinjaV6\tripo_convert_c3ad1023-aa67-4cc0-af6f-2eec6c6f23d5.fbx")
OUT = ROOT / "Art/Characters/Ninja/NinjaV6_Textured"
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(SOURCE))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
points = [o.matrix_world @ Vector(p) for o in meshes for p in o.bound_box]
minimum = Vector([min(p[i] for p in points) for i in range(3)])
maximum = Vector([max(p[i] for p in points) for i in range(3)])
report = {
    "blender": bpy.app.version_string,
    "bounds": {"min": list(minimum), "max": list(maximum)},
    "objects": [], "materials": [], "images": []
}
for o in bpy.context.scene.objects:
    report["objects"].append({
        "name": o.name, "type": o.type, "dimensions": list(o.dimensions),
        "location": list(o.location), "rotation": list(o.rotation_euler),
        "scale": list(o.scale), "parent": o.parent.name if o.parent else None,
        "vertices": len(o.data.vertices) if o.type == 'MESH' else None,
        "polygons": len(o.data.polygons) if o.type == 'MESH' else None,
        "uv_layers": [l.name for l in o.data.uv_layers] if o.type == 'MESH' else [],
        "material_slots": [s.material.name if s.material else None for s in o.material_slots],
    })
for m in bpy.data.materials:
    report["materials"].append({"name": m.name, "nodes": [
        {"name": n.name, "type": n.type, "image": n.image.name if n.type == 'TEX_IMAGE' and n.image else None}
        for n in m.node_tree.nodes] if m.node_tree else []})
for i in bpy.data.images:
    report["images"].append({"name": i.name, "path": i.filepath, "size": list(i.size)})
(OUT / "inspection.json").write_text(json.dumps(report, indent=2))
print("NINJA_INSPECTION", json.dumps(report))

# Keep the source mesh intact; stage an independent preview at its existing scale.
height = maximum.z - minimum.z
center = (minimum + maximum) * .5
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 16
scene.cycles.use_denoising = True
scene.render.resolution_x = 768
scene.render.resolution_y = 1024
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'Medium High Contrast' if 'Medium High Contrast' in [e.name for e in scene.view_settings.bl_rna.properties['look'].enum_items] else 'None'
scene.world = bpy.data.worlds.new('Neutral Studio')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value = (.18, .18, .18, 1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .65

def aim(o, target):
    o.rotation_euler = (target - o.location).to_track_quat('-Z', 'Y').to_euler()

for name, offset, energy, size in [
    ('Key', (-1.3, -1.5, 1.2), 260, 1.1),
    ('Fill', (1.2, -1.2, .5), 180, 1.0),
    ('Rear', (0, 1.5, .9), 300, 1.2),
]:
    light = bpy.data.lights.new(name, 'AREA')
    light.energy = energy * height * height
    light.shape = 'DISK'
    light.size = size * height
    obj = bpy.data.objects.new(name, light)
    scene.collection.objects.link(obj)
    obj.location = center + Vector(offset) * height
    aim(obj, center)

camera_data = bpy.data.cameras.new('Reference Camera')
camera = bpy.data.objects.new('Reference Camera', camera_data)
scene.collection.objects.link(camera)
camera_data.type = 'ORTHO'
camera_data.ortho_scale = height * 1.13
camera_data.clip_start = height * .001
camera_data.clip_end = height * 100
scene.camera = camera
for view, direction in [('front', (0, -1, 0)), ('back', (0, 1, 0)), ('side', (1, 0, 0))]:
    camera.location = center + Vector(direction) * height * 3
    aim(camera, center)
    scene.render.filepath = str(OUT / f'Before_{view}.png')
    bpy.ops.render.render(write_still=True)

for image in bpy.data.images:
    if image.source == 'FILE':
        image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'NinjaV6_Inspection.blend'))
