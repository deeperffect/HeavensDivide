import unreal


expected = {
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaShadowStep": (
        "ShadowStep", unreal.UpgradeRole.STARTER, 1, False, [], [], [unreal.UpgradeSpecialEffect.SHADOW_STEP]
    ),
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaMultipleStrikes": (
        "MultipleStrikes", unreal.UpgradeRole.SUPPORT, 3, True, ["ShadowStep"], [], []
    ),
    "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaAfterimageFrenzy": (
        "AfterimageFrenzy", unreal.UpgradeRole.MECHANIC, 1, False, ["ShadowStep"], [("MultipleStrikes", 1)], [unreal.UpgradeSpecialEffect.AFTERIMAGE_FRENZY]
    ),
}

assets = []
for path, wanted in expected.items():
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing {path}")
    requirements = [(str(r.get_editor_property("upgrade_id")), r.get_editor_property("minimum_level")) for r in asset.get_editor_property("prerequisite_requirements")]
    actual = (
        str(asset.get_editor_property("upgrade_id")), asset.get_editor_property("role"),
        asset.get_editor_property("max_level"), asset.get_editor_property("uses_rolled_rarity"),
        [str(v) for v in asset.get_editor_property("prerequisite_upgrade_ids")], requirements,
        list(asset.get_editor_property("special_effects")),
    )
    if actual != wanted:
        raise RuntimeError(f"Metadata mismatch {path}: {actual} != {wanted}")
    if asset.get_editor_property("category") != unreal.UpgradeCategory.NINJA or asset.get_editor_property("investment_owner") != unreal.UpgradeInvestmentOwner.NINJA or str(asset.get_editor_property("build_family_id")) != "ShadowClone":
        raise RuntimeError(f"Taxonomy mismatch {path}")
    assets.append(asset)

multiple = assets[1]
entries = multiple.get_editor_property("rarity_magnitudes")
values = [entry.get_editor_property("magnitude") for entry in entries]
descriptions = [str(entry.get_editor_property("description_override")) for entry in entries]
if values != [1.0, 2.0, 3.0] or descriptions != [
    "Shadow Clones perform +1 additional attack.",
    "Shadow Clones perform +2 additional attacks.",
    "Shadow Clones perform +3 additional attacks.",
]:
    raise RuntimeError(f"Multiple Strikes rarity mismatch: values={values} descriptions={descriptions}")

if abs(assets[2].get_editor_property("afterimage_frenzy_attack_speed_bonus") - 2.0) > 0.0001:
    raise RuntimeError("Afterimage Frenzy speed bonus mismatch")

controller = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller.generated_class())
pool = cdo.get_editor_property("player_upgrade_component").get_editor_property("upgrade_pool")
pool_ids = [str(item.get_editor_property("upgrade_id")) for item in pool if item]
for upgrade_id in ("ShadowStep", "MultipleStrikes", "AfterimageFrenzy"):
    if pool_ids.count(upgrade_id) != 1:
        raise RuntimeError(f"Expected one pool entry for {upgrade_id}, found {pool_ids.count(upgrade_id)}")

unreal.log(f"[ShadowCloneValidation] PASS Pool={len(pool_ids)}")
