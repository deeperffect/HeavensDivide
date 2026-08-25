import unreal


CONFIG = {
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiArea": ([0.15, 0.25, 0.40], "Increase Samurai attack area by {Percent}% ."),
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiDamageReduction": ([0.05, 0.08, 0.12], "Gain {Percent}% damage reduction."),
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiHPRegen": ([0.3, 0.5, 0.8], "Regenerate {Magnitude} HP per second."),
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiDeepCuts": ([0.20, 0.30, 0.45], "Bleed deals {Percent}% more damage."),
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaDodgeChance": ([0.05, 0.08, 0.12], "Gain {Percent}% dodge chance."),
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaHPonKill": ([0.15, 0.25, 0.40], "Restore {Magnitude} HP on kill."),
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaPotentVenom": ([0.20, 0.30, 0.45], "Poison deals {Percent}% more damage."),
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalAttackSpeed": ([0.05, 0.08, 0.12], "Increase attack speed by {Percent}%."),
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalDamage": ([0.10, 0.15, 0.25], "Increase damage by {Percent}%."),
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalMaxHealth": ([0.05, 0.08, 0.12], "Increase maximum health by {Percent}%."),
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalMoveSpeed": ([0.05, 0.08, 0.12], "Increase movement speed by {Percent}%."),
    "/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalPickupRadius": ([0.15, 0.25, 0.40], "Increase pickup radius by {Percent}%."),
}

rarities = [unreal.UpgradeRarity.COMMON, unreal.UpgradeRarity.RARE, unreal.UpgradeRarity.EPIC]
for path, (values, description) in CONFIG.items():
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing rarity asset: {path}")
    entries = []
    for rarity, magnitude in zip(rarities, values):
        entry = unreal.UpgradeRarityMagnitude()
        entry.set_editor_property("rarity", rarity)
        entry.set_editor_property("magnitude", magnitude)
        entries.append(entry)
    asset.set_editor_property("uses_rolled_rarity", True)
    asset.set_editor_property("rarity_magnitudes", entries)
    asset.set_editor_property("rolled_description_format", description.replace("% .", "%."))
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

# Explicitly suppress rarity scaling on every definition not listed above.
configured_paths = set(CONFIG.keys())
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for data in registry.get_assets_by_path("/Game/HeavensDivide/Upgrades", recursive=True):
    path = str(data.package_name)
    asset = unreal.load_asset(path)
    if asset and isinstance(asset, unreal.UpgradeDefinition) and path not in configured_paths:
        asset.set_editor_property("uses_rolled_rarity", False)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

unreal.log(f"[UpgradeRarityConfig] Configured={len(CONFIG)}")
