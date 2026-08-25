import unreal


PIERCE_PATH = "/Game/HeavensDivide/Upgrades/Ninja/DA_Upgrade_NinjaProjectilePierce"
TRIAL_FOLDER = "/Game/HeavensDivide/Upgrades/NinjaTrial"
CONTROLLER_PATH = "/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController"


def make_or_load_upgrade(asset_name):
    asset_path = f"{TRIAL_FOLDER}/{asset_name}"
    existing = unreal.load_asset(asset_path)
    if existing:
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.UpgradeDefinition)
    created = asset_tools.create_asset(asset_name, TRIAL_FOLDER, unreal.UpgradeDefinition, factory)
    if not created:
        raise RuntimeError(f"Could not create {asset_path}")
    return created


def configure_modifier(upgrade, stat_name):
    modifier = unreal.UpgradeStatModifierDefinition()
    modifier.set_editor_property("target", unreal.UpgradeStatTarget.NINJA)
    modifier.set_editor_property("character_stat", getattr(unreal.CharacterStatType, stat_name))
    modifier.set_editor_property("operation", unreal.StatModifierOperation.ADD_FLAT)
    modifier.set_editor_property("value_per_level", 1.0)
    upgrade.set_editor_property("stat_modifiers", [modifier])


pierce = unreal.load_asset(PIERCE_PATH)
if not pierce:
    raise RuntimeError(f"Could not load {PIERCE_PATH}")

category_type = type(pierce.get_editor_property("category"))
ninja_trial_category = getattr(category_type, "NINJA_TRIAL")
pierce.set_editor_property("category", ninja_trial_category)

bounce = make_or_load_upgrade("DA_Upgrade_NinjaProjectileBounce")
bounce.set_editor_property("upgrade_id", "ProjectileBounce")
bounce.set_editor_property("display_name", "Projectile Bounce")
bounce.set_editor_property("description", "Kunai bounce to 1 nearby enemy after hitting a target.")
bounce.set_editor_property("category", ninja_trial_category)
bounce.set_editor_property("max_level", 1)
bounce.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
bounce.set_editor_property("prerequisite_upgrade_ids", [])
bounce.set_editor_property("special_effects", [])
configure_modifier(bounce, "PROJECTILE_BOUNCE_BONUS")

split = make_or_load_upgrade("DA_Upgrade_NinjaProjectileSplit")
split.set_editor_property("upgrade_id", "ProjectileSplit")
split.set_editor_property("display_name", "Projectile Split")
split.set_editor_property("description", "After hitting an enemy, Kunai split into 2 new Kunai.")
split.set_editor_property("category", ninja_trial_category)
split.set_editor_property("max_level", 1)
split.set_editor_property("rarity", unreal.UpgradeRarity.COMMON)
split.set_editor_property("prerequisite_upgrade_ids", [])
split.set_editor_property("special_effects", [])
configure_modifier(split, "PROJECTILE_SPLIT_BONUS")

for asset in (pierce, bounce, split):
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
for asset in (pierce, bounce, split):
    if asset not in upgrade_pool:
        upgrade_pool.append(asset)
upgrade_component.set_editor_property("upgrade_pool", upgrade_pool)
controller_bp.modify()
if not unreal.EditorAssetLibrary.save_loaded_asset(controller_bp, only_if_is_dirty=False):
    raise RuntimeError("Failed to save BP_SurvivorPlayerController")

unreal.log(
    "[NinjaTrialUpgradeSetup] PierceMaxLevel={} PoolSize={} Assets={}".format(
        pierce.get_editor_property("max_level"),
        len(upgrade_pool),
        [asset.get_path_name() for asset in (pierce, bounce, split)],
    )
)
