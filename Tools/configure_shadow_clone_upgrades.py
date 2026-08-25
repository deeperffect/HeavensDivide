import unreal


FOLDER = "/Game/HeavensDivide/Upgrades/Ninja"


def make_or_load(name):
    path = f"{FOLDER}/{name}"
    asset = unreal.load_asset(path)
    if asset:
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.UpgradeDefinition)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.UpgradeDefinition, factory)


def configure_base(asset, upgrade_id, display_name, description, role, max_level):
    asset.set_editor_property("upgrade_id", upgrade_id)
    asset.set_editor_property("display_name", display_name)
    asset.set_editor_property("description", description)
    asset.set_editor_property("category", unreal.UpgradeCategory.NINJA)
    asset.set_editor_property("investment_owner", unreal.UpgradeInvestmentOwner.NINJA)
    asset.set_editor_property("role", role)
    asset.set_editor_property("build_family_id", "ShadowClone")
    asset.set_editor_property("max_level", max_level)
    asset.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
    asset.set_editor_property("stat_modifiers", [])


shadow_step = make_or_load("DA_Upgrade_NinjaShadowStep")
configure_base(shadow_step, "ShadowStep", "Shadow Step", "Dashing as Ninja leaves behind a Shadow Clone that performs 1 Ninja attack.", unreal.UpgradeRole.STARTER, 1)
shadow_step.set_editor_property("uses_rolled_rarity", False)
shadow_step.set_editor_property("prerequisite_upgrade_ids", [])
shadow_step.set_editor_property("prerequisite_requirements", [])
shadow_step.set_editor_property("special_effects", [unreal.UpgradeSpecialEffect.SHADOW_STEP])
unreal.EditorAssetLibrary.save_loaded_asset(shadow_step, only_if_is_dirty=False)

multiple = make_or_load("DA_Upgrade_NinjaMultipleStrikes")
configure_base(multiple, "MultipleStrikes", "Multiple Strikes", "Shadow Clones perform additional attacks before disappearing.", unreal.UpgradeRole.SUPPORT, 3)
multiple.set_editor_property("uses_rolled_rarity", True)
multiple.set_editor_property("prerequisite_upgrade_ids", ["ShadowStep"])
multiple.set_editor_property("prerequisite_requirements", [])
multiple.set_editor_property("special_effects", [])
multiple.set_editor_property("rolled_description_format", "Shadow Clones perform +{Magnitude} additional attacks.")
entries = []
for rarity, amount, description in (
    (unreal.UpgradeRarity.COMMON, 1.0, "Shadow Clones perform +1 additional attack."),
    (unreal.UpgradeRarity.RARE, 2.0, "Shadow Clones perform +2 additional attacks."),
    (unreal.UpgradeRarity.EPIC, 3.0, "Shadow Clones perform +3 additional attacks."),
):
    entry = unreal.UpgradeRarityMagnitude()
    entry.set_editor_property("rarity", rarity)
    entry.set_editor_property("magnitude", amount)
    entry.set_editor_property("description_override", description)
    entries.append(entry)
multiple.set_editor_property("rarity_magnitudes", entries)
unreal.EditorAssetLibrary.save_loaded_asset(multiple, only_if_is_dirty=False)

frenzy = make_or_load("DA_Upgrade_NinjaAfterimageFrenzy")
configure_base(frenzy, "AfterimageFrenzy", "Afterimage Frenzy", "Shadow Clones attack 200% faster.", unreal.UpgradeRole.MECHANIC, 1)
frenzy.set_editor_property("uses_rolled_rarity", False)
frenzy.set_editor_property("prerequisite_upgrade_ids", ["ShadowStep"])
requirement = unreal.UpgradePrerequisiteRequirement()
requirement.set_editor_property("upgrade_id", "MultipleStrikes")
requirement.set_editor_property("minimum_level", 1)
frenzy.set_editor_property("prerequisite_requirements", [requirement])
frenzy.set_editor_property("special_effects", [unreal.UpgradeSpecialEffect.AFTERIMAGE_FRENZY])
frenzy.set_editor_property("afterimage_frenzy_attack_speed_bonus", 2.0)
unreal.EditorAssetLibrary.save_loaded_asset(frenzy, only_if_is_dirty=False)

controller = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller.generated_class())
component = cdo.get_editor_property("player_upgrade_component")
pool = [item for item in component.get_editor_property("upgrade_pool") if item]
ids = [str(item.get_editor_property("upgrade_id")) for item in pool]
for asset in (shadow_step, multiple, frenzy):
    if str(asset.get_editor_property("upgrade_id")) not in ids:
        pool.append(asset)
component.set_editor_property("upgrade_pool", pool)
unreal.EditorAssetLibrary.save_loaded_asset(controller, only_if_is_dirty=False)
unreal.log(f"[ShadowCloneConfig] PASS Pool={len(pool)}")
