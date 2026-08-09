import os
import unreal


ASSETS = [
    "/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemyBase",
    "/Game/HeavensDivide/Blueprints/EnemyCharacters/ABP_EnemyBase",
    "/Game/Assets/EnemyCharacters/Basic/Enemy_BasicAxe",
    "/Game/Assets/EnemyCharacters/Basic/Enemy_BasicAxe_Anim",
    "/Game/Assets/EnemyCharacters/Basic/Enemy_BasicAxe_Skeleton",
]


OUT_PATH = r"C:\Users\deepe\Documents\Unreal Projects\HeavensDivide\Saved\enemy_anim_inspect.txt"


def log(msg):
    unreal.log("INSPECT_ENEMY_ANIM: " + msg)
    with open(OUT_PATH, "a", encoding="utf-8") as f:
        f.write(msg + "\n")


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    log(f"{path}: {asset.get_class().get_name() if asset else 'MISSING'}")
    return asset


def describe_bp_enemy(asset):
    generated_class = asset.generated_class() if hasattr(asset, "generated_class") else None
    log(f"BP generated class: {generated_class.get_name() if generated_class else 'None'}")
    cdo = unreal.get_default_object(generated_class) if generated_class else None
    if not cdo:
        return

    mesh = cdo.get_editor_property("mesh") if cdo.has_editor_property("mesh") else None
    log(f"BP mesh component: {mesh.get_name() if mesh else 'None'}")
    if not mesh:
        return

    skeletal_mesh = mesh.get_editor_property("skeletal_mesh")
    anim_class = mesh.get_editor_property("anim_class")
    animation_mode = mesh.get_editor_property("animation_mode")
    visible = mesh.get_editor_property("visible")
    hidden_in_game = mesh.get_editor_property("hidden_in_game")

    log(f"BP skeletal mesh: {skeletal_mesh.get_path_name() if skeletal_mesh else 'None'}")
    log(f"BP anim class: {anim_class.get_path_name() if anim_class else 'None'}")
    log(f"BP animation mode: {animation_mode}")
    log(f"BP mesh visible: {visible}")
    log(f"BP mesh hidden in game: {hidden_in_game}")


def describe_anim_bp(asset):
    generated_class = asset.generated_class() if hasattr(asset, "generated_class") else None
    target_skeleton = asset.get_editor_property("target_skeleton") if asset.has_editor_property("target_skeleton") else None
    log(f"ABP generated class: {generated_class.get_path_name() if generated_class else 'None'}")
    log(f"ABP target skeleton: {target_skeleton.get_path_name() if target_skeleton else 'None'}")


if os.path.exists(OUT_PATH):
    os.remove(OUT_PATH)

for path in ASSETS:
    asset = load(path)
    if not asset:
        continue
    if path.endswith("BP_EnemyBase"):
        describe_bp_enemy(asset)
    if path.endswith("ABP_EnemyBase"):
        describe_anim_bp(asset)
