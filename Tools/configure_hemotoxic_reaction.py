import unreal

folder = "/Game/HeavensDivide/Upgrades/Synergy"
name = "DA_Synergy_HemotoxicReaction"
object_path = f"{folder}/{name}"
asset = unreal.load_asset(object_path)
if not asset:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.UpgradeDefinition)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.UpgradeDefinition, factory)

asset.set_editor_property("upgrade_id", "HemotoxicReaction")
asset.set_editor_property("display_name", "Hemotoxic Reaction")
asset.set_editor_property("description", "Swapping detonates enemies afflicted by both Bleed and Poison, instantly dealing 150% of their remaining status damage and consuming both effects.")
asset.set_editor_property("category", unreal.UpgradeCategory.SYNERGY)
asset.set_editor_property("investment_owner", unreal.UpgradeInvestmentOwner.NONE)
asset.set_editor_property("role", unreal.UpgradeRole.SPECIAL)
asset.set_editor_property("build_family_id", "BleedPoison")
asset.set_editor_property("meta_unlock_id", "Synergy.HemotoxicReaction")
asset.set_editor_property("requires_meta_unlock", True)
asset.set_editor_property("unlocked_by_default", False)
asset.set_editor_property("max_level", 1)
asset.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
asset.set_editor_property("prerequisite_upgrade_ids", ["BleedingEdge", "VenomousKunai"])
asset.set_editor_property("stat_modifiers", [])
asset.set_editor_property("special_effects", [unreal.UpgradeSpecialEffect.HEMOTOXIC_REACTION])
asset.set_editor_property("hemotoxic_reaction_multiplier", 1.5)
asset.set_editor_property("hemotoxic_reaction_radius", 900.0)
unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

controller_bp = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller_bp.generated_class())
component = cdo.get_editor_property("player_upgrade_component")
pool = [item for item in component.get_editor_property("upgrade_pool") if item]
if not any(str(item.get_editor_property("upgrade_id")) == "HemotoxicReaction" for item in pool):
    controller_bp.modify(True)
    cdo.modify(True)
    component.modify(True)
    pool.append(asset)
    component.set_editor_property("upgrade_pool", pool)
unreal.EditorAssetLibrary.save_loaded_asset(controller_bp, only_if_is_dirty=False)
unreal.log(f"[HemotoxicReaction] Configured Pool={len(pool)}")
