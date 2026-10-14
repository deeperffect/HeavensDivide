"""Add helper smoke and combo impact assets without resetting existing combo tuning."""
import json
import shutil
import hashlib
import stat
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir())
out = root / 'Saved/JointComboFlow'
backup = out / 'AssetBackups'
backup.mkdir(parents=True, exist_ok=True)

def copy_fx(name, source, prepare=False):
    path = '/Game/HeavensDivide/VFX/Combos/Joint/' + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        return u.load_asset(path)
    fx = u.EditorAssetLibrary.duplicate_asset(source, path)
    assert isinstance(fx, u.NiagaraSystem), source
    if prepare:
        u.SwapVFXSetupLibrary.prepare_portal_local_space(fx)
        assert u.SwapVFXSetupLibrary.prepare_pickup_burst_scale(fx)
        assert u.SwapVFXSetupLibrary.configure_joint_combo_burst(fx)
    assert u.EditorAssetLibrary.save_loaded_asset(fx, False)
    return fx

smoke = copy_fx('NS_JointHelperSmokeBurst', '/Game/Assets/VFX/StylizedSmokeV1/Particles/NiagaraSystems/NS_StylizedSmoke_Impact_v01', True)
# This vendor burst is large and bright by default. Keep the join effect around
# one character, with a muted violet/grey gradient rather than a white flash.
smoke_tuned = out / 'SmokeTuned.marker'
if not smoke_tuned.exists():
    assert u.SwapVFXSetupLibrary.tint_smoke_system(smoke, u.LinearColor(.18, .15, .25, 1))
    assert u.EditorAssetLibrary.save_loaded_asset(smoke, False)
impact = copy_fx('NS_JointHit', '/Game/Assets/VFX/HitsImpactsV2/Particles/NiagaraSystems/NS_Impact_SamuraiAttack', True)
rows = []
for identity in ('BloodMoon', 'HeavensDivide', 'Moonwell'):
    disk = root / 'Content/HeavensDivide/Abilities/Combos' / ('DA_Combo_' + identity + '.uasset')
    if not (backup / disk.name).exists():
        shutil.copy2(disk, backup / disk.name)
    asset = u.load_asset('/Game/HeavensDivide/Abilities/Combos/DA_Combo_' + identity)
    assert asset
    # Fill missing assignments only. Rerunning this helper preserves artist edits.
    for field, fx in [('helper_arrival_vfx', smoke), ('helper_departure_vfx', smoke), ('hit_vfx', impact)]:
        current = asset.get_editor_property(field)
        if field.startswith('helper_') and u.EditorAssetLibrary.does_asset_exist('/Game/HeavensDivide/VFX/Swap/NS_SamuraiSmoke'):
            continue  # Character smoke profiles now supply the default helper burst.
        if not current or current.get_path_name() == '/Game/HeavensDivide/VFX/Combos/Joint/NS_JointHelperSmoke.NS_JointHelperSmoke':
            asset.set_editor_property(field, fx)
            if field == 'helper_arrival_vfx':
                asset.set_editor_property('helper_smoke_scale', .1)
                asset.set_editor_property('helper_smoke_seconds', .65)
    if not smoke_tuned.exists() and asset.get_editor_property('helper_arrival_vfx') == smoke and abs(asset.get_editor_property('helper_smoke_scale')-.45)<.001:
        asset.set_editor_property('helper_smoke_scale', .1)
    assert u.EditorAssetLibrary.save_loaded_asset(asset, False)
    rows.append(dict(id=identity, arrival=str(asset.get_editor_property('helper_arrival_vfx')),
        departure=str(asset.get_editor_property('helper_departure_vfx')), impact=asset.get_editor_property('hit_vfx').get_path_name(),
        release_start_speed=asset.get_editor_property('release_start_speed'), release_ramp_seconds=asset.get_editor_property('release_ramp_seconds'),
        helper_arrival_fraction=asset.get_editor_property('helper_arrival_fraction')))
# Remove only this task's superseded smoke copy, once no assets reference it.
old_smoke = '/Game/HeavensDivide/VFX/Combos/Joint/NS_JointHelperSmoke'
if u.EditorAssetLibrary.does_asset_exist(old_smoke):
    refs = list(u.EditorAssetLibrary.find_package_referencers_for_asset(old_smoke, True))
    assert not refs, refs
    disk = root / 'Content/HeavensDivide/VFX/Combos/Joint/NS_JointHelperSmoke.uasset'
    if disk.exists(): shutil.copy2(disk, backup / disk.name)
    assert u.EditorAssetLibrary.delete_asset(old_smoke)
    # Source-control-disabled commandlets may leave a verified residual file.
    if disk.exists():
        assert disk.resolve().is_relative_to((root / 'Content/HeavensDivide/VFX/Combos/Joint').resolve())
        assert hashlib.sha256(disk.read_bytes()).digest() == hashlib.sha256((backup / disk.name).read_bytes()).digest()
        disk.chmod(disk.stat().st_mode | stat.S_IWRITE)
        disk.unlink()
smoke_tuned.write_text('Muted smoke gradient; character-sized burst.\n')
(out / 'Assets.json').write_text(json.dumps(rows, indent=2))
print('Combo flow: smoke and hit effects saved for all three definitions.')
