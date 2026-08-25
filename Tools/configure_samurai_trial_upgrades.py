import unreal


TRIAL_FOLDER = "/Game/HeavensDivide/Upgrades/SamuraiTrial"
CONTROLLER_PATH = "/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController"
EXCLUSIVITY_GROUP = "SamuraiBladeTechnique"


def make_or_load_upgrade(asset_name):
    asset_path = f"{TRIAL_FOLDER}/{asset_name}"
    existing = unreal.load_asset(asset_path)
    if existing:
        return existing
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.UpgradeDefinition)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, TRIAL_FOLDER, unreal.UpgradeDefinition, factory
    )
    if not created:
        raise RuntimeError(f"Could not create {asset_path}")
    return created


def configure(asset, upgrade_id, display_name, description, special_effect):
    asset.set_editor_property("upgrade_id", upgrade_id)
    asset.set_editor_property("display_name", display_name)
    asset.set_editor_property("description", description)
    asset.set_editor_property("category", unreal.UpgradeCategory.SAMURAI_TRIAL)
    asset.set_editor_property("max_level", 1)
    asset.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
    asset.set_editor_property("prerequisite_upgrade_ids", [])
    asset.set_editor_property("stat_modifiers", [])
    asset.set_editor_property("special_effects", [special_effect])
    asset.set_editor_property("exclusivity_group", EXCLUSIVITY_GROUP)


cleaver = make_or_load_upgrade("DA_Upgrade_SamuraiTechniqueCleaver")
configure(
    cleaver,
    "SamuraiTechnique.Cleaver",
    "Cleaver",
    "Excess Primary damage transfers to another nearby enemy and continues chaining while the excess damage keeps killing.",
    unreal.UpgradeSpecialEffect.SAMURAI_CLEAVER,
)

duelist = make_or_load_upgrade("DA_Upgrade_SamuraiTechniqueDuelist")
configure(
    duelist,
    "SamuraiTechnique.Duelist",
    "Duelist",
    "Repeated Primary hits against the same enemy deal progressively increased damage.",
    unreal.UpgradeSpecialEffect.SAMURAI_DUELIST,
)

deathblow = make_or_load_upgrade("DA_Upgrade_SamuraiTechniqueDeathblow")
configure(
    deathblow,
    "SamuraiTechnique.Deathblow",
    "Deathblow",
    "Killing the Primary Target releases a damaging slash shockwave around the victim.",
    unreal.UpgradeSpecialEffect.SAMURAI_DEATHBLOW,
)

assets = (cleaver, duelist, deathblow)
for asset in assets:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {asset.get_path_name()}")

controller_bp = unreal.load_asset(CONTROLLER_PATH)
if not controller_bp:
    raise RuntimeError(f"Could not load {CONTROLLER_PATH}")
controller_cdo = unreal.get_default_object(controller_bp.generated_class())
upgrade_component = controller_cdo.get_editor_property("player_upgrade_component")
if not upgrade_component:
    raise RuntimeError("BP_SurvivorPlayerController has no PlayerUpgradeComponent")

upgrade_pool = list(upgrade_component.get_editor_property("upgrade_pool"))
for asset in assets:
    if asset not in upgrade_pool:
        upgrade_pool.append(asset)
upgrade_component.set_editor_property("upgrade_pool", upgrade_pool)
controller_bp.modify()
if not unreal.EditorAssetLibrary.save_loaded_asset(controller_bp, only_if_is_dirty=False):
    raise RuntimeError("Failed to save BP_SurvivorPlayerController")

unreal.log(
    "[SamuraiTrialUpgradeSetup] PoolSize={} Assets={} ExclusivityGroup={}".format(
        len(upgrade_pool),
        [asset.get_path_name() for asset in assets],
        EXCLUSIVITY_GROUP,
    )
)
