"""Apply the shared page texture to the authored main menu defaults."""
import unreal

bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu')
texture = unreal.load_asset('/Game/HeavensDivide/Blueprints/UI/SkillTree/AscensionPanel')
assert bp and texture
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('collection_panel_texture', texture)
# The shared image fills each page's bounds; no legacy brush-size compensation.
cdo.set_editor_property('collection_panel_image_scale', unreal.Vector2D(1, 1))
cdo.set_editor_property('collection_panel_image_offset', unreal.Vector2D(0, 0))
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log('SHARED_MENU_BACKGROUND_CONFIGURED')
