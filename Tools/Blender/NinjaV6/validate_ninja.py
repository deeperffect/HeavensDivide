import bpy
import json
import numpy as np
from pathlib import Path

OUT = Path(r"C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide\Art\Characters\Ninja\NinjaV6_Textured")
bpy.ops.wm.open_mainfile(filepath=str(OUT/'Bloodshift_NinjaV6_Textured.blend'))
report = json.loads((OUT/'Validation.json').read_text())
mat = bpy.data.objects['NinjaV6_ReferenceTextured'].data.materials[0]
checks = []
for node in mat.node_tree.nodes:
    if node.type != 'TEX_IMAGE' or not node.image:
        continue
    image = node.image
    assert image.packed_file, image.name
    disk = bpy.data.images.load(str(OUT/'Textures'/(image.name+'.png')), check_existing=False)
    disk.colorspace_settings.name = image.colorspace_settings.name
    a = np.empty(len(image.pixels), np.float32)
    b = np.empty(len(disk.pixels), np.float32)
    image.pixels.foreach_get(a)
    disk.pixels.foreach_get(b)
    delta = float(np.max(np.abs(a-b)))
    assert delta < 1/255 + 1e-6, (image.name, delta)
    checks.append({'image': image.name, 'packed': True, 'max_difference_from_png': delta})
report['final_reopen_checks'] = checks
bpy.context.scene.render.filepath = str(OUT/'NinjaV6_Textured_Front.png')
bpy.ops.render.render(write_still=True)

for suffix in ('glb', 'fbx'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if suffix == 'glb':
        bpy.ops.import_scene.gltf(filepath=str(OUT/f'Bloodshift_NinjaV6_Textured.{suffix}'))
    else:
        bpy.ops.import_scene.fbx(filepath=str(OUT/f'Bloodshift_NinjaV6_Textured.{suffix}'))
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    assert len(meshes) == 1
    material = meshes[0].data.materials[0]
    images = [n.image for n in material.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image]
    assert len(images) >= 3, (suffix, [i.name for i in images])
    assert all(i.size[0] == 2048 and i.size[1] == 2048 for i in images)
    report[f'{suffix}_roundtrip'] = {
        'mesh_count': len(meshes), 'polygons': len(meshes[0].data.polygons),
        'material': material.name, 'texture_images': [i.name for i in images],
        'all_images_loaded': True}
(OUT/'Validation.json').write_text(json.dumps(report, indent=2))
print('FINAL_VALIDATION', json.dumps(report), flush=True)
