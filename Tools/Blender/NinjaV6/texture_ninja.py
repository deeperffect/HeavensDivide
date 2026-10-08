"""Rework NinjaV6's existing UV texture and create exportable surface maps in Blender.

The supplied mesh, UVs, silhouette and source files are kept intact.
Material masks combine atlas color with rasterized mesh-space positions so that
warm skin is not treated as gold and purple cloth is not treated as hair.
"""
import bpy
import json
import numpy as np
from pathlib import Path
from mathutils import Vector

ROOT = Path(r"C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide")
OUT = ROOT / "Art/Characters/Ninja/NinjaV6_Textured"
TEX = OUT / 'Textures'
TEX.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(OUT / 'NinjaV6_Inspection.blend'))
scene = bpy.context.scene
obj = bpy.data.objects['NinjaV6']
mesh = obj.data
source_mat = obj.data.materials[0]
source_image = next(n.image for n in source_mat.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image)
width, height = source_image.size
pixels = np.empty(width * height * 4, np.float32)
source_image.pixels.foreach_get(pixels)
original = pixels.reshape(height, width, 4)
rgb = original[..., :3].copy()
print('ATLAS_RANGE', np.percentile(rgb, [10, 50, 90, 99], axis=(0, 1)).tolist(), flush=True)

# Rasterize the original UV triangles into object-space coordinates.
mesh.calc_loop_triangles()
uv = np.empty(len(mesh.loops) * 2, np.float32)
mesh.uv_layers.active.data.foreach_get('uv', uv)
uv = uv.reshape(-1, 2)
verts = np.empty(len(mesh.vertices) * 3, np.float32)
mesh.vertices.foreach_get('co', verts)
verts = verts.reshape(-1, 3)
positions = np.zeros((height, width, 3), np.float32)
normals = np.zeros((height, width, 3), np.float32)
covered = np.zeros((height, width), bool)
for tri in mesh.loop_triangles:
    t = uv[list(tri.loops)] * (width, height) - .5
    lo = np.maximum(np.floor(t.min(axis=0)).astype(int), 0)
    hi = np.minimum(np.ceil(t.max(axis=0)).astype(int), (width-1, height-1))
    if np.any(hi < lo):
        continue
    xx, yy = np.meshgrid(np.arange(lo[0], hi[0]+1), np.arange(lo[1], hi[1]+1))
    a, b, c = t
    den = (b[1]-c[1])*(a[0]-c[0]) + (c[0]-b[0])*(a[1]-c[1])
    if abs(den) < 1e-8:
        continue
    w0 = ((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1])) / den
    w1 = ((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1])) / den
    w2 = 1-w0-w1
    inside = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
    yi, xi = yy[inside], xx[inside]
    p = verts[list(tri.vertices)]
    positions[yi, xi] = w0[inside, None]*p[0] + w1[inside, None]*p[1] + w2[inside, None]*p[2]
    normals[yi, xi] = tri.normal
    covered[yi, xi] = True
print('UV_COVERAGE', float(covered.mean()), flush=True)

# Extend spatial labels into the existing texture padding without changing UVs.
valid = covered.copy()
for _ in range(5):
    grown = valid.copy()
    for axis, shift in [(0, 1), (0, -1), (1, 1), (1, -1)]:
        neighbor = np.roll(valid, shift, axis)
        pick = ~grown & neighbor
        if axis == 0:
            pick[0 if shift == 1 else -1, :] = False
        else:
            pick[:, 0 if shift == 1 else -1] = False
        positions[pick] = np.roll(positions, shift, axis)[pick]
        normals[pick] = np.roll(normals, shift, axis)[pick]
        grown |= pick
    valid = grown

x, y, z = [positions[..., i] for i in range(3)]
r, g, b = [rgb[..., i] for i in range(3)]
mx = rgb.max(axis=-1)
mn = rgb.min(axis=-1)
chroma = mx-mn
sat = chroma / np.maximum(mx, 1e-6)
luma = rgb @ np.array([.2126, .7152, .0722], np.float32)

def smooth(a, b, value):
    t = np.clip((value-a)/(b-a), 0, 1)
    return t*t*(3-2*t)

# Soft color membership avoids hard seams at antialiased painted boundaries.
purple = smooth(.006, .050, b-g) * smooth(.005, .030, r-g)
purple *= 1-smooth(.01, .045, r-b)
warm = smooth(.02, .12, r-b) * smooth(.01, .06, g-b)
neutral = (1-purple)*(1-warm)

# Low-value purple above the collar and the long ponytail behind the neck.
hair_region = ((z > .805) | ((z > .635) & (y > .038))) & valid
hair = purple * hair_region * (1-smooth(.27, .46, mx))
purple_cloth = purple*(1-hair)

# Yellow-gold is separated from peach skin by hue and anatomy.
gold_hue = smooth(.80, 1.50, (g-b) / np.maximum(r-g, .001))
gold_region = ((z > .94) | (z < .29) | ((x < -.035) & (z > .655) & (z < .82)) |
               ((np.abs(x) > .13) & (z > .405) & (z < .655))) & valid
gold = warm * gold_hue * gold_region
skin = warm*(1-gold)

leather_region = ((z < .285) | ((np.abs(x) > .13) & (z < .65)) |
                  ((x < -.055) & (z > .69) & (z < .81))) & valid
leather = neutral * leather_region
cloth = np.clip(1-hair-gold-skin-leather, 0, 1)

color = rgb.copy()

def blend(target, mask):
    global color
    color = color*(1-mask[..., None]) + target*mask[..., None]

# Retain the painted flowers and folds while making the palette cohesive.
purple_target = rgb * np.array([.98, 1.05, .98], np.float32)
blend(purple_target, purple_cloth*.8)
hair_target = rgb * np.array([.62, .71, .66], np.float32)
blend(hair_target, hair*.95)
charcoal_target = luma[..., None] * np.array([.92, .89, 1.02], np.float32)
blend(charcoal_target, neutral*.70)
gold_target = rgb * np.array([1.02, .85, .65], np.float32)
blend(gold_target, gold*.9)

# The generated atlas has a pale seam at the top of the obi, absent in the reference.
waist = ((z > .645) & (z < .677) & (np.abs(x) < .09) & valid)
pale = waist * smooth(.40, .62, mx) * (1-smooth(.22, .46, sat))
waist_target = np.broadcast_to(np.array([.32, .16, .45], np.float32), color.shape)
blend(waist_target, pale)
skin *= 1-pale
cloth = np.maximum(cloth, pale)
color = np.clip(color, 0, 1)

# Protect existing facial landmarks from the cloth/hair color remapping.
# Their placement follows this mesh; projecting a differently proportioned
# illustration over them would create doubled eyes or lips.
face_mask = (smooth(.815, .831, z)*(1-smooth(.918, .935, z)) *
             (1-smooth(.036, .050, np.abs(x))) * smooth(.25, .65, -normals[..., 1]) *
             (1-smooth(-.055, -.030, y)) * (1-purple*.85) * valid)
blend(rgb, face_mask)
lip_mask = (face_mask * ((z > .825) & (z < .854) & (np.abs(x) < .024)) *
            smooth(.06, .14, r-g) * (1-smooth(.04, .08, g-b)))
blend(np.clip(rgb*np.array([1.08, .92, .95], np.float32), 0, 1), lip_mask*.75)

# Texture maps use the same UV layout and resolution as the source atlas.
roughness = .84*cloth + .62*hair + .64*skin + .43*gold + .66*leather
roughness = np.clip(roughness, .28, .86)
metallic = np.clip(gold*.70, 0, .70)
specular = .22*cloth + .24*hair + .30*skin + .5*gold + .26*leather

def save_color(name, array):
    image = source_image.copy()
    image.name = name
    if image.packed_file:
        image.unpack(method='REMOVE')
    data = np.ones((height, width, 4), np.float32)
    data[..., :3] = array
    image.pixels.foreach_set(data.reshape(-1))
    image.update()
    image.filepath_raw = str(TEX / (name + '.png'))
    image.file_format = 'PNG'
    image.save()
    return image

def save_data(name, array):
    image = bpy.data.images.new(name, width=width, height=height, alpha=False)
    image.colorspace_settings.name = 'Non-Color'
    data = np.ones((height, width, 4), np.float32)
    if array.ndim == 2:
        data[..., :3] = array[..., None]
    else:
        data[..., :3] = array
    image.pixels.foreach_set(data.reshape(-1))
    image.update()
    image.filepath_raw = str(TEX / (name + '.png'))
    image.file_format = 'PNG'
    image.save()
    return image

base = save_color('T_NinjaV6_BaseColor', color)
rough = save_data('T_NinjaV6_Roughness', roughness)
metal = save_data('T_NinjaV6_Metallic', metallic)
spec = save_data('T_NinjaV6_Specular', np.clip(specular, .20, .5))
regions = save_data('T_NinjaV6_SurfaceMasks', np.stack([skin, hair, leather], -1))
print('SURFACE_COVERAGE', json.dumps({key: float(a[covered].mean()) for key, a in
    [('gold', gold), ('skin', skin), ('hair', hair), ('cloth', cloth), ('leather', leather)]}), flush=True)

mat = bpy.data.materials.new('M_Bloodshift_Ninja_PBR')
mat.use_nodes = True
mat.diffuse_color = (.08, .025, .14, 1)
nodes, links = mat.node_tree.nodes, mat.node_tree.links
nodes.clear()
output = nodes.new('ShaderNodeOutputMaterial')
output.location = (650, 140)
shader = nodes.new('ShaderNodeBsdfPrincipled')
shader.name = 'Bloodshift Character Surface'
shader.label = 'Cloth / lacquer / skin / hair / gold'
shader.location = (320, 140)
shader.inputs['IOR'].default_value = 1.45
shader.inputs['Specular IOR Level'].default_value = .3
links.new(shader.outputs['BSDF'], output.inputs['Surface'])

def texnode(image, label, location):
    n = nodes.new('ShaderNodeTexImage')
    n.image, n.label, n.location = image, label, location
    n.interpolation = 'Linear'
    return n

base_node = texnode(base, 'Painted colors / original UVs', (-440, 520))
links.new(base_node.outputs['Color'], shader.inputs['Base Color'])
rough_node = texnode(rough, 'Matte fabric / soft skin / polished trim', (-440, 240))
links.new(rough_node.outputs['Color'], shader.inputs['Roughness'])
metal_node = texnode(metal, 'Gold trim only', (-440, -40))
links.new(metal_node.outputs['Color'], shader.inputs['Metallic'])
spec_node = texnode(spec, 'Controlled surface highlights', (-440, -320))
links.new(spec_node.outputs['Color'], shader.inputs['Specular IOR Level'])
obj.data.materials.clear()
obj.data.materials.append(mat)
obj.name = 'NinjaV6_ReferenceTextured'
obj['source_mesh'] = 'Desktop/NinjaV6/tripo_convert_c3ad1023-aa67-4cc0-af6f-2eec6c6f23d5.fbx'
obj['mesh_unchanged'] = True
obj['texture_notes'] = 'Original UVs; palette refinement and spatially classified PBR maps.'

# Neutral, matching front/back studio. This is presentation only, not baked illumination.
for light in bpy.data.lights:
    light.energy *= .15
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .25
scene.render.resolution_x = 1024
scene.render.resolution_y = 1536
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.cycles.samples = 48
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'None'
scene.camera.data.ortho_scale = 1.056

def camera_at(direction):
    center = Vector((0, 0, .486))
    scene.camera.location = center + Vector(direction).normalized()*3
    scene.camera.rotation_euler = (center-scene.camera.location).to_track_quat('-Z', 'Y').to_euler()

for view, direction in [('Front', (0, -1, 0)), ('Back', (0, 1, 0)), ('ThreeQuarter', (1, -2, .12))]:
    camera_at(direction)
    scene.render.filepath = str(OUT / f'NinjaV6_Textured_{view}.png')
    bpy.ops.render.render(write_still=True)

# Leave the editable project in a useful front material-preview view.
camera_at((0, -1, 0))
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.shading.type = 'MATERIAL'
            space.shading.use_scene_world = True
            space.shading.use_scene_lights = True
            space.shading.studiolight_rotate_z = .7
            space.shading.studiolight_intensity = .8
            space.overlay.show_overlays = False
            space.region_3d.view_distance = 1.65
            space.region_3d.view_location = Vector((0, 0, .52))
            space.region_3d.view_rotation = scene.camera.rotation_euler.to_quaternion()
            space.region_3d.view_perspective = 'ORTHO'

for image in (base, rough, metal, spec, regions):
    image.pack()
scene['reference_front'] = str(ROOT / 'Art/Characters/Ninja/Bloodshift_Ninja_Front_v2.png')
scene['reference_back'] = str(ROOT / 'Art/Characters/Ninja/Bloodshift_Ninja_Back_v1.png')
scene['workflow'] = 'Palette-refined original UV atlas with separate roughness, metallic and specular maps; original geometry preserved.'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'Bloodshift_NinjaV6_Textured.blend'))
print('TEXTURED_PROJECT_SAVED', flush=True)
