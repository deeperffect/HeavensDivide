import unreal

ROOT = "/Game/HeavensDivide/Upgrades"

UPGRADES = [
    ("Samurai", "DA_Upgrade_SamuraiBleedingEdge", "BleedingEdge", "Bleeding Edge", "Samurai Primary attacks apply Bleed.", unreal.UpgradeRole.STARTER, "Bleed", 1, []),
    ("Samurai", "DA_Upgrade_SamuraiDeepCuts", "DeepCuts", "Deep Cuts", "Bleed deals +25% damage per level.", unreal.UpgradeRole.SUPPORT, "Bleed", 5, ["BleedingEdge"]),
    ("Ninja", "DA_Upgrade_NinjaVenomousKunai", "VenomousKunai", "Venomous Kunai", "Ninja kunai apply Poison.", unreal.UpgradeRole.STARTER, "Poison", 1, []),
    ("Ninja", "DA_Upgrade_NinjaPotentVenom", "PotentVenom", "Potent Venom", "Poison deals +25% damage per level.", unreal.UpgradeRole.SUPPORT, "Poison", 5, ["VenomousKunai"]),
]

REMOVED_UPGRADE_IDS = {"OpenWounds", "ConcentratedToxin"}
REMOVED_ASSET_PATHS = [
    f"{ROOT}/Samurai/DA_Upgrade_SamuraiOpenWounds",
    f"{ROOT}/Ninja/DA_Upgrade_NinjaConcentratedToxin",
]

factory = unreal.DataAssetFactory()
factory.set_editor_property("data_asset_class", unreal.UpgradeDefinition)
created = []
for folder, asset_name, upgrade_id, display_name, description, role, family, max_level, prerequisites in UPGRADES:
    package_path = f"{ROOT}/{folder}"
    object_path = f"{package_path}/{asset_name}"
    asset = unreal.load_asset(object_path)
    if not asset:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, package_path, unreal.UpgradeDefinition, factory)
    category = unreal.UpgradeCategory.SAMURAI if folder == "Samurai" else unreal.UpgradeCategory.NINJA
    owner = unreal.UpgradeInvestmentOwner.SAMURAI if folder == "Samurai" else unreal.UpgradeInvestmentOwner.NINJA
    asset.set_editor_property("upgrade_id", upgrade_id)
    asset.set_editor_property("display_name", display_name)
    asset.set_editor_property("description", description)
    asset.set_editor_property("category", category)
    asset.set_editor_property("investment_owner", owner)
    asset.set_editor_property("role", role)
    asset.set_editor_property("build_family_id", family)
    asset.set_editor_property("max_level", max_level)
    asset.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
    asset.set_editor_property("prerequisite_upgrade_ids", prerequisites)
    asset.set_editor_property("stat_modifiers", [])
    asset.set_editor_property("special_effects", [])
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    created.append(asset)

controller_bp = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
if not controller_bp:
    raise RuntimeError("BP_SurvivorPlayerController not found")
generated_class = controller_bp.generated_class()
cdo = unreal.get_default_object(generated_class)
upgrade_component = cdo.get_editor_property("player_upgrade_component")
pool = [item for item in upgrade_component.get_editor_property("upgrade_pool")
        if item and str(item.get_editor_property("upgrade_id")) not in REMOVED_UPGRADE_IDS]
known_ids = {str(item.get_editor_property("upgrade_id")) for item in pool if item}
for asset in created:
    if str(asset.get_editor_property("upgrade_id")) not in known_ids:
        pool.append(asset)
controller_bp.modify(True)
cdo.modify(True)
upgrade_component.modify(True)
upgrade_component.set_editor_property("upgrade_pool", pool)
unreal.EditorAssetLibrary.save_loaded_asset(controller_bp, only_if_is_dirty=False)
unreal.log(f"[StatusUpgrades] Configured={len(created)} Pool={len(pool)}")
