"""Create the swap ghost material; preserve its graph and repair mesh usage on reruns."""
import unreal
path='/Game/HeavensDivide/Materials/M_SwapGhost'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SwapGhost','/Game/HeavensDivide/Materials',unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided',True)
    tint=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionVectorParameter,-300,0)
    tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(1,.1,.05,1))
    opacity=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionScalarParameter,-300,200)
    opacity.set_editor_property('parameter_name','Opacity');opacity.set_editor_property('default_value',.65)
    unreal.MaterialEditingLibrary.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
else:
    m=unreal.load_asset(path)

# Cloth sections require their own shader permutation even on pose snapshots.
# Without it, Unreal substitutes an opaque default material for just those sections.
for usage in [unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, unreal.MaterialUsage.MATUSAGE_CLOTHING]:
    unreal.MaterialEditingLibrary.set_base_material_usage(m,usage,True)
unreal.MaterialEditingLibrary.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
assert m.get_editor_property('used_with_skeletal_mesh')
assert m.get_editor_property('used_with_clothing')
unreal.log('SWAP_MATERIAL_OK')
