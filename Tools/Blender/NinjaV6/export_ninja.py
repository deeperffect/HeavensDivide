import bpy
import json
import struct
import hashlib
import numpy as np
from pathlib import Path

ROOT = Path(r"C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide")
OUT = ROOT / 'Art/Characters/Ninja/NinjaV6_Textured'
TEX = OUT / 'Textures'
BLEND = OUT / 'Bloodshift_NinjaV6_Textured.blend'
bpy.ops.wm.open_mainfile(filepath=str(BLEND))
scene = bpy.context.scene
obj = bpy.data.objects['NinjaV6_ReferenceTextured']
mat = obj.data.materials[0]
report = {'blender_version': bpy.app.version_string, 'texture_checks': [], 'geometry_checks': {}}

# Check the packed data actually agrees with the saved, portable PNGs.
for node in mat.node_tree.nodes:
    if node.type != 'TEX_IMAGE' or not node.image:
        continue
    image = node.image
    path = TEX / (image.name + '.png')
    assert path.exists(), path
    disk = bpy.data.images.load(str(path), check_existing=False)
    disk.colorspace_settings.name = image.colorspace_settings.name
    a = np.empty(len(image.pixels), np.float32)
    b = np.empty(len(disk.pixels), np.float32)
    image.pixels.foreach_get(a)
    disk.pixels.foreach_get(b)
    difference = float(np.max(np.abs(a-b)))
    report['texture_checks'].append({'name': image.name, 'size': list(image.size),
        'colorspace': image.colorspace_settings.name, 'pre_repack_disk_max_difference': difference,
        'repacked_from_verified_png': True})
    # Always use the saved PNG as canonical, then pack that exact file.
    original_name = image.name
    image.name = original_name + '_WorkingBuffer'
    disk.name = original_name
    disk.pack()
    node.image = disk

def data_array(data, attribute, width):
    values = np.empty(len(data)*width, np.float32)
    data.foreach_get(attribute, values)
    return values

with bpy.data.libraries.load(str(OUT/'NinjaV6_Inspection.blend'), link=False) as (source, loaded):
    loaded.meshes = source.meshes
original_mesh = loaded.meshes[0]
report['geometry_checks'] = {
    'vertices': len(obj.data.vertices), 'polygons': len(obj.data.polygons),
    'vertices_unchanged': bool(np.array_equal(data_array(original_mesh.vertices, 'co', 3), data_array(obj.data.vertices, 'co', 3))),
    'uv_coordinates_unchanged': bool(np.array_equal(data_array(original_mesh.uv_layers.active.data, 'uv', 2), data_array(obj.data.uv_layers.active.data, 'uv', 2))),
    'armature_present': any(o.type == 'ARMATURE' for o in scene.objects),
}
assert report['geometry_checks']['vertices_unchanged']
assert report['geometry_checks']['uv_coordinates_unchanged']
bpy.data.meshes.remove(original_mesh)

def map_pixels(name):
    image = bpy.data.images.load(str(TEX/(name+'.png')), check_existing=False)
    image.colorspace_settings.name = 'Non-Color'
    array = np.empty(len(image.pixels), np.float32)
    image.pixels.foreach_get(array)
    return image, array.reshape(image.size[1], image.size[0], 4)

rough, rough_pixels = map_pixels('T_NinjaV6_Roughness')
metal, metal_pixels = map_pixels('T_NinjaV6_Metallic')
width, height = rough.size
packed = np.ones((height, width, 4), np.float32)
packed[..., 1] = rough_pixels[..., 0]
packed[..., 2] = metal_pixels[..., 0]
for node in list(mat.node_tree.nodes):
    if (node.type == 'TEX_IMAGE' and node.image and node.image.name == 'T_NinjaV6_ORM') or node.type == 'SEPARATE_COLOR':
        mat.node_tree.nodes.remove(node)
existing_orm = bpy.data.images.get('T_NinjaV6_ORM')
if existing_orm:
    existing_orm.name = 'T_NinjaV6_ORM_Previous'
orm = bpy.data.images.new('T_NinjaV6_ORM', width=width, height=height, alpha=False)
orm.colorspace_settings.name = 'Non-Color'
orm.pixels.foreach_set(packed.reshape(-1))
orm.update()
orm.file_format = 'PNG'
orm.filepath_raw = str(TEX/'T_NinjaV6_ORM.png')
orm.save()
orm.pack()
orm_node = mat.node_tree.nodes.new('ShaderNodeTexImage')
orm_node.image = orm
orm_node.location = (-440, 200)
orm_node.label = 'R: neutral AO / G: Roughness / B: Metallic'
separate = mat.node_tree.nodes.new('ShaderNodeSeparateColor')
separate.mode = 'RGB'
separate.location = (-80, 140)
shader = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
mat.node_tree.links.new(orm_node.outputs['Color'], separate.inputs['Color'])
mat.node_tree.links.new(separate.outputs['Green'], shader.inputs['Roughness'])
mat.node_tree.links.new(separate.outputs['Blue'], shader.inputs['Metallic'])
for node in list(mat.node_tree.nodes):
    if node.type == 'TEX_IMAGE' and node.image and node.image.name.startswith(('T_NinjaV6_Roughness', 'T_NinjaV6_Metallic')):
        mat.node_tree.nodes.remove(node)

bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.gltf(filepath=str(OUT/'Bloodshift_NinjaV6_Textured.glb'),
    export_format='GLB', use_selection=True, export_materials='EXPORT', export_extras=True)
# FBX's material exporter expects direct scalar image connections.
fbx_nodes = []
for image, socket in [(rough, 'Roughness'), (metal, 'Metallic')]:
    node = mat.node_tree.nodes.new('ShaderNodeTexImage')
    node.image = image
    mat.node_tree.links.new(node.outputs['Color'], shader.inputs[socket])
    fbx_nodes.append(node)
bpy.ops.export_scene.fbx(filepath=str(OUT/'Bloodshift_NinjaV6_Textured.fbx'),
    use_selection=True, object_types={'MESH'}, add_leaf_bones=False, bake_anim=False,
    path_mode='COPY', embed_textures=True, axis_forward='-Z', axis_up='Y')
for node in fbx_nodes:
    mat.node_tree.nodes.remove(node)
mat.node_tree.links.new(separate.outputs['Green'], shader.inputs['Roughness'])
mat.node_tree.links.new(separate.outputs['Blue'], shader.inputs['Metallic'])

glb = (OUT/'Bloodshift_NinjaV6_Textured.glb').read_bytes()
magic, version, size = struct.unpack_from('<4sII', glb)
assert magic == b'glTF' and version == 2 and size == len(glb)
json_size, json_type = struct.unpack_from('<I4s', glb, 12)
manifest = json.loads(glb[20:20+json_size].decode('utf8'))
report['glb'] = {'meshes': len(manifest.get('meshes', [])),
    'materials': len(manifest.get('materials', [])), 'images': len(manifest.get('images', [])),
    'embedded_images': all('bufferView' in i for i in manifest.get('images', [])),
    'has_base_color': 'baseColorTexture' in manifest['materials'][0]['pbrMetallicRoughness'],
    'has_roughness_metallic': 'metallicRoughnessTexture' in manifest['materials'][0]['pbrMetallicRoughness']}
assert report['glb']['embedded_images']
assert report['glb']['has_base_color'] and report['glb']['has_roughness_metallic']
for image in bpy.data.images:
    if image.name.startswith('T_NinjaV6_') and image.filepath:
        image.filepath = bpy.path.relpath(image.filepath, start=str(OUT))

scene['workflow'] = 'Refined original UV atlas; protected facial landmarks; matte fabric, hair, skin, leather and warm metal response.'
bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
report['outputs'] = {name: {'bytes': (OUT/name).stat().st_size,
    'sha256': hashlib.sha256((OUT/name).read_bytes()).hexdigest()} for name in
    ['Bloodshift_NinjaV6_Textured.blend', 'Bloodshift_NinjaV6_Textured.glb', 'Bloodshift_NinjaV6_Textured.fbx']}
(OUT/'Validation.json').write_text(json.dumps(report, indent=2))
print('EXPORT_VALIDATION', json.dumps(report), flush=True)
