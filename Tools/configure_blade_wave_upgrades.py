import unreal

FOLDER = "/Game/HeavensDivide/Upgrades/Samurai"


def make(name):
    path = f"{FOLDER}/{name}"
    asset = unreal.load_asset(path)
    if asset:
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.UpgradeDefinition)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.UpgradeDefinition, factory)


def base(asset, upgrade_id, name, description, role, max_level):
    asset.set_editor_property("upgrade_id", upgrade_id)
    asset.set_editor_property("display_name", name)
    asset.set_editor_property("description", description)
    asset.set_editor_property("category", unreal.UpgradeCategory.SAMURAI)
    asset.set_editor_property("investment_owner", unreal.UpgradeInvestmentOwner.SAMURAI)
    asset.set_editor_property("role", role)
    asset.set_editor_property("build_family_id", "BladeWave")
    asset.set_editor_property("max_level", max_level)
    asset.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
    asset.set_editor_property("stat_modifiers", [])
    asset.set_editor_property("special_effects", [])


blade = make("DA_Upgrade_SamuraiBladeWave")
base(blade, "BladeWave", "Blade Wave", "Samurai attacks release a crescent blade wave that travels forward and damages enemies.", unreal.UpgradeRole.STARTER, 1)
blade.set_editor_property("uses_rolled_rarity", False)
blade.set_editor_property("prerequisite_upgrade_ids", [])
blade.set_editor_property("prerequisite_requirements", [])
unreal.EditorAssetLibrary.save_loaded_asset(blade, only_if_is_dirty=False)

wide = make("DA_Upgrade_SamuraiWideArc")
base(wide, "WideArc", "Wide Arc", "Blade Waves become wider and deal more damage.", unreal.UpgradeRole.SUPPORT, 5)
wide.set_editor_property("uses_rolled_rarity", True)
wide.set_editor_property("prerequisite_upgrade_ids", ["BladeWave"])
wide.set_editor_property("prerequisite_requirements", [])
wide.set_editor_property("rolled_description_format", "Blade Waves deal {Percent}% more damage and are {Percent}% wider.")
entries = []
for rarity, value in ((unreal.UpgradeRarity.COMMON, .10), (unreal.UpgradeRarity.RARE, .18), (unreal.UpgradeRarity.EPIC, .30)):
    entry = unreal.UpgradeRarityMagnitude()
    entry.set_editor_property("rarity", rarity)
    entry.set_editor_property("magnitude", value)
    entries.append(entry)
wide.set_editor_property("rarity_magnitudes", entries)
unreal.EditorAssetLibrary.save_loaded_asset(wide, only_if_is_dirty=False)

returning = make("DA_Upgrade_SamuraiReturningBlade")
base(returning, "ReturningBlade", "Returning Blade", "Blade Waves return to Samurai after reaching maximum range, damaging enemies again.", unreal.UpgradeRole.MECHANIC, 1)
returning.set_editor_property("uses_rolled_rarity", False)
returning.set_editor_property("prerequisite_upgrade_ids", ["BladeWave"])
returning.set_editor_property("prerequisite_requirements", [])
unreal.EditorAssetLibrary.save_loaded_asset(returning, only_if_is_dirty=False)

crossing = make("DA_Upgrade_SamuraiCrossingBlades")
base(crossing, "CrossingBlades", "Crossing Blades", "Every third Samurai attack releases 3 Blade Waves in a fan instead of 1.", unreal.UpgradeRole.EVOLUTION, 1)
crossing.set_editor_property("uses_rolled_rarity", False)
crossing.set_editor_property("prerequisite_upgrade_ids", ["BladeWave"])
req = unreal.UpgradePrerequisiteRequirement()
req.set_editor_property("upgrade_id", "WideArc")
req.set_editor_property("minimum_level", 2)
crossing.set_editor_property("prerequisite_requirements", [req])
unreal.EditorAssetLibrary.save_loaded_asset(crossing, only_if_is_dirty=False)

controller = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller.generated_class())
component = cdo.get_editor_property("player_upgrade_component")
pool = [item for item in component.get_editor_property("upgrade_pool") if item]
ids = [str(item.get_editor_property("upgrade_id")) for item in pool]
for asset in (blade, wide, returning, crossing):
    if str(asset.get_editor_property("upgrade_id")) not in ids:
        pool.append(asset)
component.set_editor_property("upgrade_pool", pool)
unreal.EditorAssetLibrary.save_loaded_asset(controller, only_if_is_dirty=False)
unreal.log(f"[BladeWaveConfig] PASS Pool={len(pool)}")
