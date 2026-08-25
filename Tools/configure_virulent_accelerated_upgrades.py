import unreal


def make_or_load(folder, name):
    path = f"/Game/HeavensDivide/Upgrades/{folder}"
    object_path = f"{path}/{name}"
    asset = unreal.load_asset(object_path)
    if asset:
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.UpgradeDefinition)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.UpgradeDefinition, factory)


virulent = make_or_load("Ninja", "DA_Upgrade_NinjaVirulentStrain")
virulent.set_editor_property("upgrade_id", "VirulentStrain")
virulent.set_editor_property("display_name", "Virulent Strain")
virulent.set_editor_property("description", "At 5+ Poison stacks, each Poison tick also damages nearby enemies.")
virulent.set_editor_property("category", unreal.UpgradeCategory.NINJA)
virulent.set_editor_property("investment_owner", unreal.UpgradeInvestmentOwner.NINJA)
virulent.set_editor_property("role", unreal.UpgradeRole.EVOLUTION)
virulent.set_editor_property("build_family_id", "Poison")
virulent.set_editor_property("max_level", 1)
virulent.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
virulent.set_editor_property("prerequisite_upgrade_ids", ["VenomousKunai"])
requirement = unreal.UpgradePrerequisiteRequirement()
requirement.set_editor_property("upgrade_id", "PotentVenom")
requirement.set_editor_property("minimum_level", 2)
virulent.set_editor_property("prerequisite_requirements", [requirement])
virulent.set_editor_property("special_effects", [unreal.UpgradeSpecialEffect.VIRULENT_STRAIN])
virulent.set_editor_property("stat_modifiers", [])
virulent.set_editor_property("virulent_strain_threshold", 5)
virulent.set_editor_property("virulent_strain_radius", 350.0)
virulent.set_editor_property("virulent_strain_damage_multiplier", 0.5)
unreal.EditorAssetLibrary.save_loaded_asset(virulent, only_if_is_dirty=False)

accelerated = make_or_load("Synergy", "DA_Synergy_AcceleratedVenom")
accelerated.set_editor_property("upgrade_id", "AcceleratedVenom")
accelerated.set_editor_property("display_name", "Accelerated Venom")
accelerated.set_editor_property("description", "Poison ticks twice as fast on Bleeding enemies.")
accelerated.set_editor_property("category", unreal.UpgradeCategory.SYNERGY)
accelerated.set_editor_property("investment_owner", unreal.UpgradeInvestmentOwner.NONE)
accelerated.set_editor_property("role", unreal.UpgradeRole.SPECIAL)
accelerated.set_editor_property("build_family_id", "BleedPoison")
accelerated.set_editor_property("meta_unlock_id", "Synergy.AcceleratedVenom")
accelerated.set_editor_property("requires_meta_unlock", True)
accelerated.set_editor_property("unlocked_by_default", False)
accelerated.set_editor_property("max_level", 1)
accelerated.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
accelerated.set_editor_property("prerequisite_upgrade_ids", ["BleedingEdge", "VenomousKunai"])
accelerated.set_editor_property("prerequisite_requirements", [])
accelerated.set_editor_property("special_effects", [unreal.UpgradeSpecialEffect.ACCELERATED_VENOM])
accelerated.set_editor_property("stat_modifiers", [])
accelerated.set_editor_property("accelerated_venom_tick_rate_multiplier", 2.0)
unreal.EditorAssetLibrary.save_loaded_asset(accelerated, only_if_is_dirty=False)

controller = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller.generated_class())
component = cdo.get_editor_property("player_upgrade_component")
pool = [item for item in component.get_editor_property("upgrade_pool") if item]
known = {str(item.get_editor_property("upgrade_id")) for item in pool}
controller.modify(True)
cdo.modify(True)
component.modify(True)
for asset in (virulent, accelerated):
    if str(asset.get_editor_property("upgrade_id")) not in known:
        pool.append(asset)
component.set_editor_property("upgrade_pool", pool)
unreal.EditorAssetLibrary.save_loaded_asset(controller, only_if_is_dirty=False)
unreal.log(f"[StatusBuildUpgrades] Configured Pool={len(pool)}")
