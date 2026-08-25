import unreal

EXPECTED = {
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiBleedingEdge": ("BleedingEdge", unreal.UpgradeCategory.SAMURAI, unreal.UpgradeInvestmentOwner.SAMURAI, unreal.UpgradeRole.STARTER, "Bleed", 1, []),
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiDeepCuts": ("DeepCuts", unreal.UpgradeCategory.SAMURAI, unreal.UpgradeInvestmentOwner.SAMURAI, unreal.UpgradeRole.SUPPORT, "Bleed", 5, ["BleedingEdge"]),
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaVenomousKunai": ("VenomousKunai", unreal.UpgradeCategory.NINJA, unreal.UpgradeInvestmentOwner.NINJA, unreal.UpgradeRole.STARTER, "Poison", 1, []),
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaPotentVenom": ("PotentVenom", unreal.UpgradeCategory.NINJA, unreal.UpgradeInvestmentOwner.NINJA, unreal.UpgradeRole.SUPPORT, "Poison", 5, ["VenomousKunai"]),
}

REMOVED = {
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiOpenWounds": "OpenWounds",
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaConcentratedToxin": "ConcentratedToxin",
}

assets = []
for path, expected in EXPECTED.items():
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing {path}")
    actual = (
        str(asset.get_editor_property("upgrade_id")),
        asset.get_editor_property("category"),
        asset.get_editor_property("investment_owner"),
        asset.get_editor_property("role"),
        str(asset.get_editor_property("build_family_id")),
        asset.get_editor_property("max_level"),
        [str(value) for value in asset.get_editor_property("prerequisite_upgrade_ids")],
    )
    if actual != expected:
        raise RuntimeError(f"Metadata mismatch {path}: actual={actual}, expected={expected}")
    assets.append(asset)

controller_bp = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller_bp.generated_class())
component = cdo.get_editor_property("player_upgrade_component")
pool_ids = [str(item.get_editor_property("upgrade_id")) for item in component.get_editor_property("upgrade_pool") if item]
for asset in assets:
    upgrade_id = str(asset.get_editor_property("upgrade_id"))
    if pool_ids.count(upgrade_id) != 1:
        raise RuntimeError(f"Expected exactly one pool entry for {upgrade_id}; found {pool_ids.count(upgrade_id)}")

for path, upgrade_id in REMOVED.items():
    if upgrade_id in pool_ids:
        raise RuntimeError(f"Obsolete upgrade remains in pool: {upgrade_id}")
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_warning(f"[StatusUpgradeValidation] Inactive orphan retained due to serialized Blueprint reference: {path}")

unreal.log(f"[StatusUpgradeValidation] PASS Assets={len(assets)} Pool={len(pool_ids)}")
