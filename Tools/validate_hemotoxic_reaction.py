import unreal

path = "/Game/HeavensDivide/Upgrades/Synergy/DA_Synergy_HemotoxicReaction"
asset = unreal.load_asset(path)
if not asset:
    raise RuntimeError("Hemotoxic Reaction asset missing")

expected = {
    "upgrade_id": "HemotoxicReaction",
    "category": unreal.UpgradeCategory.SYNERGY,
    "investment_owner": unreal.UpgradeInvestmentOwner.NONE,
    "role": unreal.UpgradeRole.SPECIAL,
    "build_family_id": "BleedPoison",
    "meta_unlock_id": "Synergy.HemotoxicReaction",
    "requires_meta_unlock": True,
    "unlocked_by_default": False,
    "max_level": 1,
    "prerequisite_upgrade_ids": ["BleedingEdge", "VenomousKunai"],
    "hemotoxic_reaction_multiplier": 1.5,
    "hemotoxic_reaction_radius": 900.0,
}
for prop, value in expected.items():
    actual = asset.get_editor_property(prop)
    if prop in ("upgrade_id", "build_family_id", "meta_unlock_id"):
        actual = str(actual)
    elif prop == "prerequisite_upgrade_ids":
        actual = [str(item) for item in actual]
    if actual != value:
        raise RuntimeError(f"{prop}: actual={actual}, expected={value}")

effects = list(asset.get_editor_property("special_effects"))
if effects != [unreal.UpgradeSpecialEffect.HEMOTOXIC_REACTION]:
    raise RuntimeError(f"Unexpected special effects: {effects}")

controller = unreal.load_asset("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController")
cdo = unreal.get_default_object(controller.generated_class())
component = cdo.get_editor_property("player_upgrade_component")
ids = [str(item.get_editor_property("upgrade_id")) for item in component.get_editor_property("upgrade_pool") if item]
if ids.count("HemotoxicReaction") != 1:
    raise RuntimeError(f"Expected one HemotoxicReaction pool entry, found {ids.count('HemotoxicReaction')}")

unreal.log(f"[HemotoxicReactionValidation] PASS Pool={len(ids)}")
