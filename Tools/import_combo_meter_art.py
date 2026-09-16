"""Import generated ink artwork and author its UI glow material. Run in Unreal Python."""
from pathlib import Path
import unreal

folder = '/Game/HeavensDivide/Blueprints/UI/Combo'
task = unreal.AssetImportTask()
task.filename = str(Path(unreal.Paths.project_dir()).resolve() / 'Art/ComboMeter/ComboInkSlash.png')
task.destination_path = folder
task.destination_name = 'T_ComboInkSlash'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert task.imported_object_paths, 'Combo slash import failed'
texture = unreal.load_asset(task.imported_object_paths[0])
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
unreal.EditorAssetLibrary.save_loaded_asset(texture)

path = folder + '/M_ComboInkGlow'
material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ComboInkGlow', folder, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
lib = unreal.MaterialEditingLibrary
# Reuse the connected graph: constructor-loaded assets can be rooted, so deleting
# their expressions is unsafe during a commandlet reimport.
existing = []
def visit(node):
    if node and node not in existing:
        existing.append(node)
        for child in lib.get_inputs_for_material_expression(material, node):
            visit(child)
for prop in [unreal.MaterialProperty.MP_EMISSIVE_COLOR, unreal.MaterialProperty.MP_OPACITY, unreal.MaterialProperty.MP_FRONT_MATERIAL]:
    visit(lib.get_material_property_input_node(material, prop))
def expression(kind, x, y):
    for node in existing:
        if isinstance(node, kind):
            existing.remove(node)
            return node
    return lib.create_material_expression(material, kind, x, y)
tex = expression(unreal.MaterialExpressionTextureObjectParameter, -600, 0)
tex.set_editor_property('parameter_name', 'InkTexture')
tex.set_editor_property('texture', texture)
uv = expression(unreal.MaterialExpressionTextureCoordinate, -600, 180)
glow = expression(unreal.MaterialExpressionScalarParameter, -600, 340)
glow.set_editor_property('parameter_name', 'GlowStrength')
glow.set_editor_property('default_value', 0.0)
custom = expression(unreal.MaterialExpressionCustom, -280, 0)
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs = []
for name in ['Ink', 'UV', 'Glow']:
    entry = unreal.CustomInput()
    entry.set_editor_property('input_name', name)
    inputs.append(entry)
custom.set_editor_property('inputs', inputs)
custom.set_editor_property('code', '''
float4 ink = Texture2DSample(Ink, InkSampler, UV);
float halo = 0;
if (Glow > 0.001) {
    float weight = 0;
    [unroll] for (int x=-3; x<=3; ++x) {
        [unroll] for (int y=-3; y<=3; ++y) {
            float w = exp(-float(x*x+y*y)/5.0);
            halo += Texture2DSample(Ink, InkSampler, UV+float2(x*0.006,y*0.025)).a*w;
            weight += w;
        }
    }
    halo /= weight;
}
float a = saturate(ink.a + halo*Glow*1.8);
float3 color = lerp(ink.rgb, float3(1.0,0.93,0.72), saturate(Glow*0.65));
return float4(color, a);
''')
assert lib.connect_material_expressions(tex, '', custom, 'Ink')
assert lib.connect_material_expressions(uv, '', custom, 'UV')
assert lib.connect_material_expressions(glow, '', custom, 'Glow')
rgb = expression(unreal.MaterialExpressionComponentMask, 20, 0)
rgb.set_editor_property('r', True)
rgb.set_editor_property('g', True)
rgb.set_editor_property('b', True)
rgb.set_editor_property('a', False)
alpha = expression(unreal.MaterialExpressionComponentMask, 20, 160)
alpha.set_editor_property('r', False)
alpha.set_editor_property('g', False)
alpha.set_editor_property('b', False)
alpha.set_editor_property('a', True)
assert lib.connect_material_expressions(custom, '', rgb, '')
assert lib.connect_material_expressions(custom, '', alpha, '')
assert lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
# This project enables Substrate. Its UI output must be connected explicitly;
# legacy emissive/opacity pins alone do not drive a Substrate material.
ui = expression(unreal.MaterialExpressionSubstrateUI, 240, 0)
assert lib.connect_material_expressions(rgb, '', ui, 'Color')
assert lib.connect_material_expressions(alpha, '', ui, 'Opacity')
assert lib.connect_material_property(ui, '', unreal.MaterialProperty.MP_FRONT_MATERIAL)
lib.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
unreal.log('COMBO_METER_ART_IMPORTED')

