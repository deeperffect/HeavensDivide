"""Import painted ground textures and create mesh/landscape blend materials.

Creates new assets only. Existing assets and levels are never overwritten.
Run through UnrealEditor-Cmd -run=pythonscript with commandlet rendering enabled.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
DEST = '/Game/HeavensDivide/Materials/StylizedGround'
lib = u.MaterialEditingLibrary
assets = u.AssetToolsHelpers.get_asset_tools()
textures = {}
for layer in ['Grass', 'Dirt', 'Stone']:
    name = 'T_StylizedGround_' + layer
    texture = u.load_asset(DEST + '/' + name) if u.EditorAssetLibrary.does_asset_exist(DEST + '/' + name) else None
    if not texture:
        task = u.AssetImportTask()
        task.filename = str(ROOT / 'Art/StylizedGround' / (layer + '_BaseColor.png'))
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.save = False
        assets.import_asset_tasks([task])
        texture = u.load_asset(DEST + '/' + name)
        assert texture
        texture.set_editor_property('srgb', True)
        texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD)
        texture.set_editor_property('address_x', u.TextureAddress.TA_MIRROR)
        texture.set_editor_property('address_y', u.TextureAddress.TA_MIRROR)
        # Generated source is non-power-of-two: stretch in the build pipeline
        # so mipmaps work without adding a padded border to the tile.
        texture.set_editor_property('power_of_two_mode', u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
        texture.set_editor_property('max_texture_size', 2048)
        assert u.EditorAssetLibrary.save_loaded_asset(texture, False)
    textures[layer] = texture

CODE = r'''
struct GroundNoise
{
    float hash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
    float noise(float2 p)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(hash(i), hash(i + float2(1,0)), f.x),
                    lerp(hash(i + float2(0,1)), hash(i + 1), f.x), f.y);
    }
};
GroundNoise n;
float2 p = Position.xy;
float2 macro = p / max(PatchSize, 1.0);
float grain = n.noise(macro * 9.0 + 19.0) - .5;
float g = n.noise(macro) * .72 + n.noise(macro * 2.7 + 3.4) * .28;
float s = n.noise(macro * 1.31 + 73.0);
float edge = max(BlendSoftness, .015);
float grassMask = smoothstep(1.0 - GrassCoverage - edge, 1.0 - GrassCoverage + edge, g + grain * .09);
float stoneMask = smoothstep(1.0 - StoneCoverage - edge, 1.0 - StoneCoverage + edge, s + grain * .05);
grassMask *= step(.001, GrassCoverage);
stoneMask *= step(.001, StoneCoverage);
grassMask = lerp(grassMask, 1.0, step(.999, GrassCoverage));
stoneMask = lerp(stoneMask, 1.0, step(.999, StoneCoverage));
float3 automaticWeights = float3(grassMask * (1-stoneMask), (1-grassMask) * (1-stoneMask), stoneMask);
float3 weights = lerp(automaticWeights, PaintWeights / max(dot(PaintWeights, 1.0), .001), saturate(UsePaint));
float3 grass = Texture2DSample(GrassTexture, GrassTextureSampler, p / max(GrassTileSize, 1.0)).rgb * GrassTint.rgb;
float3 dirt = Texture2DSample(DirtTexture, DirtTextureSampler, p / max(DirtTileSize, 1.0)).rgb * DirtTint.rgb;
float3 stone = Texture2DSample(StoneTexture, StoneTextureSampler, p / max(StoneTileSize, 1.0)).rgb * StoneTint.rgb;
float3 rgb = grass * weights.x + dirt * weights.y + stone * weights.z;
float variation = 1.0 + (n.noise(macro * .47 + 12.0) - .5) * MacroVariation;
float grey = dot(rgb, float3(.2126, .7152, .0722));
return max(0.0, lerp(grey.xxx, rgb, Saturation) * Brightness * variation);
'''

def build(name, landscape=False, preview=False):
    path = DEST + '/' + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        return u.load_asset(path)
    m = assets.create_asset(name, DEST, u.Material, u.MaterialFactoryNew())
    assert m
    if preview:
        m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    def node(cls, x, y):
        return lib.create_material_expression(m, cls, x, y)
    inputs = {}
    def scalar(name, value, group, y):
        e = node(u.MaterialExpressionScalarParameter, -900, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', value)
        e.set_editor_property('group', group)
        inputs[name] = e
        return e
    for i, layer in enumerate(['Grass', 'Dirt', 'Stone']):
        e = node(u.MaterialExpressionTextureObjectParameter, -1400, i * 250)
        e.set_editor_property('parameter_name', layer + 'Texture')
        e.set_editor_property('texture', textures[layer])
        e.set_editor_property('group', 'Textures')
        inputs[layer + 'Texture'] = e
        tint = node(u.MaterialExpressionVectorParameter, -1400, i * 250 + 120)
        tint.set_editor_property('parameter_name', layer + 'Tint')
        tint.set_editor_property('default_value', u.LinearColor(1, 1, 1, 1))
        tint.set_editor_property('group', 'Color')
        inputs[layer + 'Tint'] = tint
    settings = [('GrassTileSize', 1000, 'Scale (cm)'), ('DirtTileSize', 1000, 'Scale (cm)'),
                ('StoneTileSize', 1400, 'Scale (cm)'), ('PatchSize', 1500, 'Blend'),
                ('GrassCoverage', .56, 'Blend'), ('StoneCoverage', .43, 'Blend'),
                ('BlendSoftness', .075, 'Blend'), ('MacroVariation', .16, 'Color'),
                ('Brightness', 1.0, 'Color'), ('Saturation', .9, 'Color'),
                ('UsePaint', 1.0 if landscape else 0.0, 'Painting')]
    for i, (key, value, group) in enumerate(settings):
        scalar(key, value, group, i * 130)
    if preview:
        uv = node(u.MaterialExpressionTextureCoordinate, -1400, 900)
        span = node(u.MaterialExpressionMultiply, -1200, 900)
        span.set_editor_property('const_b', 4500)
        lib.connect_material_expressions(uv, '', span, 'A')
        # World position input only needs XY.
        inputs['Position'] = span
    else:
        inputs['Position'] = node(u.MaterialExpressionWorldPosition, -1400, 900)
    if landscape:
        paint = node(u.MaterialExpressionCustom, -600, 1600)
        paint.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
        pins = []
        for index, key in enumerate(['Grass', 'Dirt', 'Stone']):
            e = node(u.MaterialExpressionLandscapeLayerSample, -1400, 1400 + index * 150)
            e.set_editor_property('parameter_name', key)
            e.set_editor_property('preview_weight', 1.0 if key == 'Grass' else 0.0)
            pin = u.CustomInput(); pin.set_editor_property('input_name', key); pins.append(pin)
        paint.set_editor_property('inputs', pins)
        samples = [e for e in lib.get_material_expressions(m) if isinstance(e, u.MaterialExpressionLandscapeLayerSample)]
        for e in samples:
            lib.connect_material_expressions(e, '', paint, str(e.get_editor_property('parameter_name')))
        paint.set_editor_property('code', 'float3 w = float3(Grass,Dirt,Stone); return dot(w,1.0) > .001 ? w : float3(1,0,0);')
    else:
        vertex = node(u.MaterialExpressionVertexColor, -1400, 1600)
        paint = node(u.MaterialExpressionCustom, -600, 1600)
        paint.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
        pin = u.CustomInput(); pin.set_editor_property('input_name', 'Color')
        paint.set_editor_property('inputs', [pin])
        lib.connect_material_expressions(vertex, '', paint, 'Color')
        paint.set_editor_property('code', 'float d=saturate(Color.r), s=saturate(Color.g); return float3((1-d)*(1-s),d*(1-s),s);')
    inputs['PaintWeights'] = paint
    custom = node(u.MaterialExpressionCustom, 0, 0)
    custom.set_editor_property('description', 'Painted grass / earth / flagstone blend')
    custom.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins = []
    for key in inputs:
        pin = u.CustomInput(); pin.set_editor_property('input_name', key); pins.append(pin)
    custom.set_editor_property('inputs', pins)
    custom.set_editor_property('code', CODE if not preview else CODE.replace(
        'return max(0.0, lerp(grey.xxx, rgb, Saturation) * Brightness * variation);',
        'return pow(max(0.0, lerp(grey.xxx, rgb, Saturation) * Brightness * variation), 1.0/2.2);'))
    for key, value in inputs.items():
        assert lib.connect_material_expressions(value, '', custom, key), key
    assert lib.connect_material_property(custom, '', u.MaterialProperty.MP_EMISSIVE_COLOR if preview else u.MaterialProperty.MP_BASE_COLOR)
    if not preview:
        roughness = scalar('Roughness', .93, 'Surface', 1700)
        specular = scalar('Specular', .12, 'Surface', 1850)
        lib.connect_material_property(roughness, '', u.MaterialProperty.MP_ROUGHNESS)
        lib.connect_material_property(specular, '', u.MaterialProperty.MP_SPECULAR)
    lib.recompile_material(m)
    assert u.EditorAssetLibrary.save_loaded_asset(m, False)
    return m

mesh = build('M_StylizedGround')
landscape = build('M_StylizedGround_Landscape', landscape=True)
instances = {}
for name, parent in [('MI_StylizedGround', mesh), ('MI_StylizedGround_Landscape', landscape)]:
    if u.EditorAssetLibrary.does_asset_exist(DEST + '/' + name):
        instance = u.load_asset(DEST + '/' + name)
    else:
        instance = assets.create_asset(name, DEST, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(instance, parent)
        lib.update_material_instance(instance)
        assert u.EditorAssetLibrary.save_loaded_asset(instance, False)
    instances[name] = instance.get_path_name()

# This diagnostic material renders the same shader with a 45m XY footprint.
preview = build('M_StylizedGround_Preview', preview=True)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
rt = u.RenderingLibrary.create_render_target2d(world, 1024, 1024, u.TextureRenderTargetFormat.RTF_RGBA8)
u.RenderingLibrary.draw_material_to_render_target(world, rt, preview)
pixels = u.RenderingLibrary.read_render_target(world, rt, False)
assert any(p.g > p.r * 1.1 for p in pixels), 'Expected green grass in compiled blend'
assert any(p.r > p.g * 1.05 for p in pixels), 'Expected warm dirt in compiled blend'
out = ROOT / 'Saved/StylizedGround'
out.mkdir(parents=True, exist_ok=True)
u.RenderingLibrary.export_render_target(world, rt, str(out), 'blend_preview.png')
(out / 'setup.json').write_text(json.dumps({'instances': instances, 'preview': str(out / 'blend_preview.png')}, indent=2))
u.log('STYLIZED_GROUND_READY ' + json.dumps(instances))
