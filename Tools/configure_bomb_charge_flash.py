import unreal
path='/Game/HeavensDivide/Materials/M_BombChargeFlash'
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not m:
 m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_BombChargeFlash','/Game/HeavensDivide/Materials',unreal.Material,unreal.MaterialFactoryNew())
 m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 color=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionVectorParameter,-250,0)
 color.set_editor_property('parameter_name','FlashColor')
 color.set_editor_property('default_value',unreal.LinearColor(3,0,0,1))
 unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 data=m.get_editor_property('editor_only_data')
 unreal.SystemLibrary.execute_console_command(None,'setnopec '+data.get_path_name()+' EmissiveColor (UseConstant=False)')
 unreal.MaterialEditingLibrary.recompile_material(m)
 assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
bp=unreal.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinBomb')
cdo=unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('bomb_flash_material',m)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
unreal.log('BOMB_FLASH_MATERIAL_SETUP_OK')
