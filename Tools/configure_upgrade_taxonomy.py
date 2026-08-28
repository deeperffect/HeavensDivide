import unreal


ROOT = "/Game/HeavensDivide/Upgrades"

STARTERS = {
    "DA_Upgrade_SamuraiApplyMarks",
}

EVOLUTIONS = {
    "DA_Upgrade_NinjaBladeCascade",
    "DA_Upgrade_NinjaExecutionersKunai",
    "DA_Upgrade_NinjaSpreadMarks",
}

MECHANICS = {
    "DA_Upgrade_SamuraiDoubleCut",
    "DA_Upgrade_NinjaFanOfBlades",
}

MARK_FAMILY = {
    "DA_Upgrade_SamuraiApplyMarks",
    "DA_Upgrade_NinjaExecutionersKunai",
    "DA_Upgrade_NinjaSpreadMarks",
}


asset_paths = unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)
classified = []
for asset_path in asset_paths:
    asset = unreal.load_asset(asset_path)
    if not isinstance(asset, unreal.UpgradeDefinition):
        continue

    asset_name = asset.get_name()
    category = asset.get_editor_property("category")
    if category == unreal.UpgradeCategory.SAMURAI:
        owner = unreal.UpgradeInvestmentOwner.SAMURAI
    elif category == unreal.UpgradeCategory.NINJA:
        owner = unreal.UpgradeInvestmentOwner.NINJA
    else:
        owner = unreal.UpgradeInvestmentOwner.NONE

    if category in (
        unreal.UpgradeCategory.SAMURAI_TRIAL,
        unreal.UpgradeCategory.NINJA_TRIAL,
        unreal.UpgradeCategory.SYNERGY,
        unreal.UpgradeCategory.CURSED,
    ):
        role = unreal.UpgradeRole.SPECIAL
    elif asset_name in STARTERS:
        role = unreal.UpgradeRole.STARTER
    elif asset_name in EVOLUTIONS:
        role = unreal.UpgradeRole.EVOLUTION
    elif asset_name in MECHANICS:
        role = unreal.UpgradeRole.MECHANIC
    else:
        role = unreal.UpgradeRole.STAT

    family = "Mark" if asset_name in MARK_FAMILY else "None"
    asset.set_editor_property("investment_owner", owner)
    asset.set_editor_property("role", role)
    asset.set_editor_property("build_family_id", family)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {asset_path}")

    classified.append(
        "{}|{}|{}|{}|{}".format(
            asset_name,
            category,
            owner,
            role,
            family,
        )
    )

unreal.log("[UpgradeTaxonomy] Count={}".format(len(classified)))
for row in sorted(classified):
    unreal.log("[UpgradeTaxonomy] {}".format(row))
