import unreal


def names(values):
    return [str(value) for value in values]


virulent = unreal.load_asset("/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaVirulentStrain")
accelerated = unreal.load_asset("/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_AcceleratedVenom")
if not virulent or not accelerated:
    raise RuntimeError("Virulent Strain or Accelerated Venom asset is missing")

virulent_requirements = virulent.get_editor_property("prerequisite_requirements")
virulent_actual = (
    str(virulent.get_editor_property("upgrade_id")),
    virulent.get_editor_property("category"),
    virulent.get_editor_property("investment_owner"),
    virulent.get_editor_property("role"),
    str(virulent.get_editor_property("build_family_id")),
    virulent.get_editor_property("max_level"),
    names(virulent.get_editor_property("prerequisite_upgrade_ids")),
    [(str(item.get_editor_property("upgrade_id")), item.get_editor_property("minimum_level")) for item in virulent_requirements],
    virulent.get_editor_property("special_effects"),
    virulent.get_editor_property("virulent_strain_threshold"),
    virulent.get_editor_property("virulent_strain_radius"),
    virulent.get_editor_property("virulent_strain_damage_multiplier"),
)
virulent_expected = (
    "VirulentStrain", unreal.UpgradeCategory.NINJA, unreal.UpgradeInvestmentOwner.NINJA,
    unreal.UpgradeRole.EVOLUTION, "Poison", 1, ["VenomousKunai"], [("PotentVenom", 2)],
    [unreal.UpgradeSpecialEffect.VIRULENT_STRAIN], 5, 350.0, 0.5,
)
if virulent_actual != virulent_expected:
    raise RuntimeError(f"Virulent Strain mismatch: actual={virulent_actual}, expected={virulent_expected}")

accelerated_actual = (
    str(accelerated.get_editor_property("upgrade_id")),
    accelerated.get_editor_property("category"),
    accelerated.get_editor_property("investment_owner"),
    accelerated.get_editor_property("role"),
    str(accelerated.get_editor_property("build_family_id")),
    str(accelerated.get_editor_property("meta_unlock_id")),
    accelerated.get_editor_property("requires_meta_unlock"),
    accelerated.get_editor_property("unlocked_by_default"),
    accelerated.get_editor_property("max_level"),
    names(accelerated.get_editor_property("prerequisite_upgrade_ids")),
    accelerated.get_editor_property("special_effects"),
    accelerated.get_editor_property("accelerated_venom_tick_rate_multiplier"),
)
accelerated_expected = (
    "AcceleratedVenom", unreal.UpgradeCategory.SYNERGY, unreal.UpgradeInvestmentOwner.NONE,
    unreal.UpgradeRole.SPECIAL, "BleedPoison", "Synergy.AcceleratedVenom", True, False, 1,
    ["BleedingEdge", "VenomousKunai"], [unreal.UpgradeSpecialEffect.ACCELERATED_VENOM], 2.0,
)
if accelerated_actual != accelerated_expected:
    raise RuntimeError(f"Accelerated Venom mismatch: actual={accelerated_actual}, expected={accelerated_expected}")

controller = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller.generated_class())
component = cdo.get_editor_property("player_upgrade_component")
pool_ids = [str(item.get_editor_property("upgrade_id")) for item in component.get_editor_property("upgrade_pool") if item]
for upgrade_id in ("VirulentStrain", "AcceleratedVenom"):
    if pool_ids.count(upgrade_id) != 1:
        raise RuntimeError(f"Expected exactly one pool entry for {upgrade_id}; found {pool_ids.count(upgrade_id)}")

unreal.log(f"[VirulentAcceleratedValidation] PASS Pool={len(pool_ids)}")
