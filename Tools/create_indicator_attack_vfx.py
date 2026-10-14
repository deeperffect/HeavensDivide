"""Copy attack bursts into the project; never change the vendor VFX."""
import json
from pathlib import Path
import unreal as u

sources = {
    'NS_ChargedImpact': '/Game/Assets/VFX/HitsImpactsV2/Particles/NiagaraSystems/NS_Impact_26',
    'NS_ChargedSlash': '/Game/Assets/VFX/HitsImpactsV2/Particles/NiagaraSystems/NS_Impact_SamuraiAttack',
    'NS_ChargedFire': '/Game/Assets/VFX/MagicProjectilesVol1/MagicProjectilesVol1/Particles/Hits/NS_Hit_Fireball01_Orange',
    'NS_ChargedFrost': '/Game/Assets/VFX/MagicProjectilesVol1/MagicProjectilesVol1/Particles/Hits/NS_Hit_Spell03_ICE',
    'NS_ChargedPoison': '/Game/Assets/VFX/MagicProjectilesVol1/MagicProjectilesVol1/Particles/Hits/NS_Hit_Spell03_NATURE',
    'NS_ChargedFlash': '/Game/Assets/VFX/MagicProjectilesVol1/MagicProjectilesVol1/Particles/Muzzles/NS_Muzzle_Fireball01_Orange',
}
results = []
for name, source in sources.items():
    original = u.load_asset(source)
    assert isinstance(original, u.NiagaraSystem), source
    destination = '/Game/HeavensDivide/VFX/ChargedAttacks/' + name
    effect = u.load_asset(destination) if u.EditorAssetLibrary.does_asset_exist(destination) else u.EditorAssetLibrary.duplicate_asset(source, destination)
    assert isinstance(effect, u.NiagaraSystem), destination
    u.SwapVFXSetupLibrary.prepare_portal_local_space(effect)
    # Particle sprites must scale too, not only their emission positions.
    assert u.SwapVFXSetupLibrary.prepare_pickup_burst_scale(effect), destination
    effect.set_editor_property('determinism', True)
    effect.set_editor_property('random_seed', 41723)
    assert u.EditorAssetLibrary.save_loaded_asset(effect, False)
    results.append({'asset': destination, 'source': source})
path = Path(u.Paths.project_dir()) / 'Saved/IndicatorAttackVFX/Assets.json'
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(results, indent=2))
u.log('INDICATOR_ATTACK_VFX_ASSETS_OK')
