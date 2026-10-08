"""Repair the missing opacity shape behind the two rectangular swap sprites."""
from pathlib import Path
import json
import shutil
import unreal as u

path = '/Game/HeavensDivide/VFX/Swap/M_SwapInkSlash'
texture_path = '/Game/Assets/VFX/MagicProjectilesVol4/Textures/T_Slash01'
material = u.load_asset(path)
texture = u.load_asset(texture_path)
assert isinstance(material, u.Material) and isinstance(texture, u.Texture2D)
root = Path(u.Paths.project_dir()).resolve()
backup = root / 'Saved/PickupAndSwapFix/Backup'
backup.mkdir(parents=True, exist_ok=True)
source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
if not (backup / source.name).exists():
    shutil.copy2(source, backup / source.name)
shapes = [e for e in u.MaterialEditingLibrary.get_material_expressions(material)
          if isinstance(e, u.MaterialExpressionTextureSampleParameter2D)
          and str(e.get_editor_property('parameter_name')) == 'Shape']
assert len(shapes) == 1
old_texture = shapes[0].get_editor_property('texture')
shapes[0].set_editor_property('texture', texture)
assert material.get_editor_property('blend_mode') == u.BlendMode.BLEND_TRANSLUCENT
u.MaterialEditingLibrary.recompile_material(material)
assert u.EditorAssetLibrary.save_loaded_asset(material, False)
report = {'material': path, 'previous_texture': old_texture.get_path_name() if old_texture else None,
          'shape_texture': texture_path, 'mask_channel': 'Alpha'}
(backup.parent / 'SwapRepair.json').write_text(json.dumps(report, indent=2))
u.log('SWAP_INK_SLASH_REPAIR_PASS ' + json.dumps(report))
