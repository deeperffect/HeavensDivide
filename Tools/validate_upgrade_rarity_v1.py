import unreal


EXPECTED = {
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiArea": [0.15, 0.25, 0.40],
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiDamageReduction": [0.05, 0.08, 0.12],
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiHPRegen": [0.3, 0.5, 0.8],
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiDeepCuts": [0.20, 0.30, 0.45],
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaDodgeChance": [0.05, 0.08, 0.12],
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaHPonKill": [0.15, 0.25, 0.40],
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaPotentVenom": [0.20, 0.30, 0.45],
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalAttackSpeed": [0.05, 0.08, 0.12],
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalDamage": [0.10, 0.15, 0.25],
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalMaxHealth": [0.05, 0.08, 0.12],
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalMoveSpeed": [0.05, 0.08, 0.12],
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalPickupRadius": [0.15, 0.25, 0.40],
}

for path, values in EXPECTED.items():
    asset = unreal.load_asset(path)
    if not asset or not asset.get_editor_property("uses_rolled_rarity"):
        raise RuntimeError(f"Missing or non-scalable rarity asset: {path}")
    entries = asset.get_editor_property("rarity_magnitudes")
    actual = [round(item.get_editor_property("magnitude"), 4) for item in entries]
    if actual != values:
        raise RuntimeError(f"Rarity values mismatch for {path}: {actual} != {values}")
    if len(entries) != 3:
        raise RuntimeError(f"Expected exactly three rarity values for {path}")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
for data in registry.get_assets_by_path("/Game/HeavensDivide/Upgrades", recursive=True):
    path = str(data.package_name)
    asset = unreal.load_asset(path)
    if asset and isinstance(asset, unreal.UpgradeDefinition) and path not in EXPECTED:
        if asset.get_editor_property("uses_rolled_rarity"):
            raise RuntimeError(f"Fixed mechanic incorrectly enables rolled rarity: {path}")

unreal.log(f"[UpgradeRarityValidation] PASS Scalable={len(EXPECTED)}")
