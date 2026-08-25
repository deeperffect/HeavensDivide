import unreal

paths = [
    "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiOpenWounds",
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaConcentratedToxin",
]

for path in paths:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(path, load_assets_to_confirm=True)
        unreal.log(f"[StatusUpgradeCleanup] Referencers={path} -> {referencers}")
        if not unreal.EditorAssetLibrary.delete_asset(path):
            raise RuntimeError(f"Failed to delete obsolete status upgrade: {path}")
        unreal.log(f"[StatusUpgradeCleanup] Deleted={path}")
    else:
        unreal.log(f"[StatusUpgradeCleanup] AlreadyAbsent={path}")
