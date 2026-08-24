import unreal

asset_path = "/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiHeavyBlade"
upgrade = unreal.load_asset(asset_path)
if not upgrade:
    raise RuntimeError(f"Could not load {asset_path}")

category_type = type(upgrade.get_editor_property("category"))
upgrade.set_editor_property("category", getattr(category_type, "CURSED"))
if not unreal.EditorAssetLibrary.save_loaded_asset(upgrade, only_if_is_dirty=False):
    raise RuntimeError("Failed to save HeavyBlade")

modifiers = upgrade.get_editor_property("stat_modifiers")
modifier_values = [modifier.get_editor_property("value_per_level") for modifier in modifiers]
unreal.log(
    "[BloodPactAsset] Id={} Category={} MaxLevel={} ModifierValues={}".format(
        upgrade.get_editor_property("upgrade_id"),
        upgrade.get_editor_property("category"),
        upgrade.get_editor_property("max_level"),
        modifier_values,
    )
)
