import unreal

paths = {
    "BladeWave": "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiBladeWave",
    "WideArc": "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiWideArc",
    "ReturningBlade": "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiReturningBlade",
    "CrossingBlades": "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiCrossingBlades",
}
assets = {key: unreal.load_asset(path) for key, path in paths.items()}
if not all(assets.values()):
    raise RuntimeError(f"Missing Blade Wave assets: {assets}")

expected = {
    "BladeWave": (unreal.UpgradeRole.STARTER, 1, False, [], []),
    "WideArc": (unreal.UpgradeRole.SUPPORT, 5, True, ["BladeWave"], []),
    "ReturningBlade": (unreal.UpgradeRole.MECHANIC, 1, False, ["BladeWave"], []),
    "CrossingBlades": (unreal.UpgradeRole.EVOLUTION, 1, False, ["BladeWave"], [("WideArc", 2)]),
}
for upgrade_id, asset in assets.items():
    requirements = [(str(r.get_editor_property("upgrade_id")), r.get_editor_property("minimum_level")) for r in asset.get_editor_property("prerequisite_requirements")]
    actual = (asset.get_editor_property("role"), asset.get_editor_property("max_level"), asset.get_editor_property("uses_rolled_rarity"), [str(v) for v in asset.get_editor_property("prerequisite_upgrade_ids")], requirements)
    if actual != expected[upgrade_id]:
        raise RuntimeError(f"Metadata mismatch {upgrade_id}: {actual} != {expected[upgrade_id]}")
    if str(asset.get_editor_property("upgrade_id")) != upgrade_id or asset.get_editor_property("category") != unreal.UpgradeCategory.SAMURAI or asset.get_editor_property("investment_owner") != unreal.UpgradeInvestmentOwner.SAMURAI or str(asset.get_editor_property("build_family_id")) != "BladeWave":
        raise RuntimeError(f"Taxonomy mismatch {upgrade_id}")

wide_values = [round(v.get_editor_property("magnitude"), 4) for v in assets["WideArc"].get_editor_property("rarity_magnitudes")]
if wide_values != [0.10, 0.18, 0.30]:
    raise RuntimeError(f"Wide Arc rarity mismatch: {wide_values}")

controller = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
component = unreal.get_default_object(controller.generated_class()).get_editor_property("player_upgrade_component")
pool_ids = [str(item.get_editor_property("upgrade_id")) for item in component.get_editor_property("upgrade_pool") if item]
for upgrade_id in paths:
    if pool_ids.count(upgrade_id) != 1:
        raise RuntimeError(f"Expected one pool entry for {upgrade_id}; found {pool_ids.count(upgrade_id)}")
unreal.log(f"[BladeWaveValidation] PASS Pool={len(pool_ids)}")
