"""Import the generated Ascension panel as a cooked UI texture."""
from pathlib import Path
import unreal

task = unreal.AssetImportTask()
task.filename = str(Path(unreal.Paths.project_dir()).resolve() / 'Art/SkillTree/AscensionPanel.png')
task.destination_path = '/Game/HeavensDivide/Blueprints/UI/SkillTree'
task.destination_name = 'AscensionPanel'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert task.imported_object_paths, 'Panel import failed'
texture = unreal.load_asset(task.imported_object_paths[0])
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('ASCENSION_PANEL_IMPORTED')
