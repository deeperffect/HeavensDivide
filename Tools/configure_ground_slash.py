"""Integrate the vendor GroundSlashV2 via the Blade Wave upgrade presentation."""
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
upgrade_path = '/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiBladeWave'
upgrade = u.load_asset(upgrade_path)
assert upgrade
backup = root / 'Saved/Backups/GroundSlashIntegration'
backup.mkdir(parents=True, exist_ok=True)
source = root / 'Content' / (upgrade_path.removeprefix('/Game/') + '.uasset')
if not (backup / source.name).exists():
    shutil.copy2(source, backup / source.name)
source_path = '/Game/Assets/VFX/GroundSlash/Particles/NiagaraSystems/NS_GroundSlashV2'
target = '/Game/HeavensDivide/VFX/NS_GroundSlash_BladeWave'
fx = u.load_asset(target) if u.EditorAssetLibrary.does_asset_exist(target) else u.EditorAssetLibrary.duplicate_asset(source_path, target)
assert fx
original = u.load_asset('/Game/Assets/VFX/GroundSlash/Particles/NiagaraSystems/NS_GroundSlashV1')
assert u.SwapVFXSetupLibrary.complete_ground_slash_effects(fx, original)
bound = u.SwapVFXSetupLibrary.bind_ground_slash_debris(fx)
assert bound == 3, f'Expected all three debris mesh renderers; got {bound}'
assert u.SwapVFXSetupLibrary.bind_ground_slash_trail_orientation(fx) == 1
assert u.EditorAssetLibrary.save_loaded_asset(fx, False)

mat_path = '/Game/HeavensDivide/Materials/M_BladeWaveDebris'
mat = u.load_asset(mat_path) if u.EditorAssetLibrary.does_asset_exist(mat_path) else None
if not mat:
    mat = u.AssetToolsHelpers.get_asset_tools().create_asset('M_BladeWaveDebris', '/Game/HeavensDivide/Materials', u.Material, u.MaterialFactoryNew())
    lib = u.MaterialEditingLibrary
    color = lib.create_material_expression(mat, u.MaterialExpressionVectorParameter, -300, 0)
    color.set_editor_property('parameter_name', 'StoneColor')
    color.set_editor_property('default_value', u.LinearColor(.12,.10,.065,1))
    lib.connect_material_property(color, '', u.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(mat, u.MaterialExpressionConstant, -300, 150)
    rough.set_editor_property('r', .95)
    lib.connect_material_property(rough, '', u.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('used_with_niagara_mesh_particles', True)
    lib.recompile_material(mat)
    assert u.EditorAssetLibrary.save_loaded_asset(mat, False)
settings = upgrade.get_editor_property('presentation')
assert settings.get_editor_property('pulse_system') in [u.load_asset(source_path), fx], 'Unexpected user-assigned effect; preserve it'
settings.set_editor_property('pulse_system', fx)
settings.set_editor_property('ground_slash_motion', True)
settings.set_editor_property('slowdown_delay', .4)
settings.set_editor_property('slowdown_rate', 3.5)
settings.set_editor_property('ground_trace_distance', 150)
settings.set_editor_property('ground_offset', 10)
settings.set_editor_property('max_travel_time', 5)
settings.set_editor_property('debris_lifetime', 5)
settings.set_editor_property('debris_material', mat)
upgrade.set_editor_property('presentation', settings)
assert u.EditorAssetLibrary.save_loaded_asset(upgrade, False)
u.log('GROUND_SLASH_INTEGRATION_PASS: three debris bindings, vendor delay/rate/ground offset, upgrade assignment saved')
