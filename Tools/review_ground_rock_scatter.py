"""Validate and render Level1's persistent, instanced ground rock stamps.

Launch with -ExecutePythonScript=<this file> -RenderOffscreen
(not -run=pythonscript or -NullRHI). -RockScatterReviewFolder=Before selects the output
folder; default is After. -RockScatterApplyFirst runs apply_ground_rock_scatter.py first.
Only folders beginning with Before allow the scatter actor/ground switch to be
absent. All other folders require the complete saved scatter setup. Capture is
read-only; an explicitly requested apply may save the primary Level1 map.
Texture streaming remains enabled by default. Optional -RockScatterForceMips holds
ground textures at full residency for an art-only review; omit it for gameplay QA.

For a same-process before/after review, create
Saved/GroundRockScatter/KeepReviewOpen.txt before launch, capture Before, then
write ReviewCommand.json beside it with one of:
  {"action": "apply", "folder": "After"}
  {"action": "capture", "folder": "Review2"}
  {"action": "quit"}
Wait for ReviewReady.json with ready=true and the requested folder after each
command. Commands are consumed only between complete capture batches. Removing
KeepReviewOpen.txt also closes an idle session. A failed batch writes Error.json
and quits; a batch has a 10-minute deadline and an idle session a 30-minute one.

The harness never saves assets or maps. Only the explicitly requested apply
script may save assets/maps. Review actors are transient and mesh hiding is
restored. SurfaceLowAngle and GameplayTelegraphProbe use the real attack circle
material as a static readability probe, not a gameplay simulation.
"""

from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import re
import runpy
import struct
import time
import traceback
import uuid
import unreal as u


ROOT = Path(u.Paths.project_dir()).resolve()
OUTPUT = ROOT / "Saved/GroundRockScatter"
OUTPUT.mkdir(parents=True, exist_ok=True)
KEEP_OPEN = OUTPUT / "KeepReviewOpen.txt"
COMMAND = OUTPUT / "ReviewCommand.json"
READY = OUTPUT / "ReviewReady.json"
MAP = "/Game/Maps/Lvl_B1_Lvl1"
MATERIAL = "/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground"
PARENT = "/Game/HeavensDivide/Materials/EnvironmentPolish/M_Lvl1_Ground"
RIG = "/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_PlayerCameraRig"
SCATTER_LABEL = "GroundRockStamps"
SCATTER_BOUND = 4939.61
SCATTER_Z = 1.3
SCATTER_PARENT = "/Game/HeavensDivide/Materials/ReferenceGround/M_RockStamp"
SCATTER_TEXTURE = "/Game/HeavensDivide/Materials/ReferenceGround/T_RockStamps_Atlas"
SCATTER_MATERIALS = {
    "/Game/HeavensDivide/Materials/ReferenceGround/MI_RockStamp_%d" % index
    for index in range(4)}
TELEGRAPH_MATERIAL = "/Game/HeavensDivide/Materials/M_AttackIndicatorCircle"
TELEGRAPH_BOSS = "/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/SamuraiBoss/BP_SamuraiBoss"
MAP_FILES = [ROOT / "Content/Maps/Lvl_B1_Lvl1.umap",
             ROOT / "Content/Maps/Lvl_B1_Lvl1_Lighting.umap"]
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
LIB = u.MaterialEditingLibrary
SESSION = uuid.uuid4().hex
STATE = {"stage": "begin", "batch": 0, "temporary": [], "hidden": [],
         "busy": False, "closed": False, "applied": False, "views": [],
         "batch_at": time.monotonic(), "handle": None}


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def write_json(path, value):
    # Replace atomically so an external reviewer never reads half a status file.
    pending = path.with_suffix(path.suffix + ".tmp")
    pending.write_text(json.dumps(value, indent=2), encoding="utf-8")
    # OneDrive/Windows scanners can briefly lock the previous report during rename.
    for attempt in range(8):
        try:
            pending.replace(path)
            break
        except PermissionError:
            if attempt == 7:
                raise
            time.sleep(.1)


def folder_name(value):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", value):
        raise ValueError("Ground review folder must be a simple name such as Before or After2")
    return value


def package_name(asset):
    return asset.get_path_name().split(".")[0] if asset else None


def vector(value):
    return [value.x, value.y, value.z]


def rotation(value):
    return [value.pitch, value.yaw, value.roll]


def map_hashes():
    return {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in MAP_FILES}


def status(ready=False, **extra):
    write_json(READY, {"session": SESSION, "batch": STATE["batch"],
                      "folder": STATE.get("folder"), "ready": ready,
                      "stage": STATE["stage"], "updated_utc": utc_now(), **extra})


def cleanup_view():
    LEVEL.eject_pilot_level_actor()
    for actor, previous in STATE["hidden"]:
        if u.SystemLibrary.is_valid(actor):
            actor.set_is_temporarily_hidden_in_editor(previous)
    STATE["hidden"] = []
    for actor in reversed(STATE["temporary"]):
        if u.SystemLibrary.is_valid(actor):
            ACTORS.destroy_actor(actor)
    STATE["temporary"] = []
    STATE["camera"] = None
    STATE["rig"] = None


def close_editor():
    if STATE["closed"]:
        return
    STATE["closed"] = True
    cleanup_view()
    if STATE["handle"] is not None:
        u.unregister_slate_post_tick_callback(STATE["handle"])
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()


def fail():
    error = traceback.format_exc()
    report = {"passed": False, "session": SESSION, "batch": STATE["batch"],
              "folder": STATE.get("folder"), "stage": STATE["stage"],
              "time_utc": utc_now(), "error": error,
              "partial_validation": STATE.get("report", {})}
    write_json(OUTPUT / "Error.json", report)
    if STATE.get("out"):
        write_json(STATE["out"] / "Error.json", report)
    status(False, passed=False, error=error)
    u.log_error("GROUND_ROCK_SCATTER_REVIEW_FAILED " + error)
    try:
        close_editor()
    except Exception:
        # Even an unsuccessful cleanup must not leave an unattended editor alive.
        u.log_error(traceback.format_exc())
        if STATE["handle"] is not None:
            u.unregister_slate_post_tick_callback(STATE["handle"])
        u.SystemLibrary.quit_editor()


def spawn(actor_class, location, actor_rotation=None):
    actor = ACTORS.spawn_actor_from_class(actor_class, location,
                                          actor_rotation or u.Rotator(), transient=True)
    if not actor:
        raise RuntimeError("Could not create temporary review actor: " + str(actor_class))
    actor.set_actor_label("TEMP Ground Rock Scatter Review")
    STATE["temporary"].append(actor)
    return actor


def camera_settings(component):
    return {"position": vector(component.get_world_location()),
            "rotation": rotation(component.get_world_rotation()),
            "projection_mode": str(component.get_editor_property("projection_mode")),
            "field_of_view": component.get_editor_property("field_of_view"),
            "ortho_width": component.get_editor_property("ortho_width"),
            "aspect_ratio": component.get_editor_property("aspect_ratio"),
            "constrain_aspect_ratio": component.get_editor_property("constrain_aspect_ratio"),
            "post_process_blend_weight": component.get_editor_property("post_process_blend_weight")}


def inspect_material():
    floor = STATE["floor"]
    component = floor.get_component_by_class(u.StaticMeshComponent)
    material = component.get_material(0)
    if package_name(material) != MATERIAL:
        raise AssertionError("Level1 floor has unexpected material: " + str(package_name(material)))
    parent = material.get_base_material()
    if package_name(parent) != PARENT:
        raise AssertionError("Level1 ground has unexpected parent: " + str(package_name(parent)))
    if parent.get_editor_property("shading_model") != u.MaterialShadingModel.MSM_DEFAULT_LIT:
        raise AssertionError("Gameplay floor must use the lit ground material")
    scalars = {str(name): LIB.get_material_instance_scalar_parameter_value(material, name)
               for name in LIB.get_scalar_parameter_names(material)}
    vectors = {}
    for name in LIB.get_vector_parameter_names(material):
        color = LIB.get_material_instance_vector_parameter_value(material, name)
        vectors[str(name)] = [color.r, color.g, color.b, color.a]
    textures = {}
    for name in LIB.get_texture_parameter_names(material):
        texture = LIB.get_material_instance_texture_parameter_value(material, name)
        entry = {"asset": package_name(texture)}
        if isinstance(texture, u.Texture2D):
            entry.update(size=[texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
                         srgb=texture.get_editor_property("srgb"),
                         compression=str(texture.get_editor_property("compression_settings")),
                         address_x=str(texture.get_editor_property("address_x")),
                         address_y=str(texture.get_editor_property("address_y")))
            if STATE.get("force_ground_mips", False):
                texture.set_force_mip_levels_to_be_resident(120.0)
        textures[str(name)] = entry
    for name in ("GrassTexture", "DirtTexture", "StoneTexture"):
        if not textures.get(name, {}).get("asset"):
            raise AssertionError("Required ground texture is missing: " + name)
    statistics = LIB.get_statistics(material)
    stats = {name: statistics.get_editor_property(name) for name in
             ("num_pixel_shader_instructions", "num_samplers", "num_pixel_texture_samples")}
    return {"floor": floor.get_path_name(), "floor_location": vector(floor.get_actor_location()),
            "material": material.get_path_name(), "parent": parent.get_path_name(),
            "scalar_parameters": scalars, "vector_parameters": vectors,
            "texture_parameters": textures, "shader_statistics": stats,
            "shader_statistics_available": stats["num_pixel_shader_instructions"] > 0,
            "shader_statistics_note": "Zero statistics indicate no compiled statistics were available; they are not a zero-cost measurement."}


def inspect_scatter():
    """Validate saved geometry, full slab footprints, and non-gameplay settings."""
    matches = [actor for actor in ACTORS.get_all_level_actors()
               if actor.get_actor_label() == SCATTER_LABEL]
    result = {"required": STATE["require_scatter"], "actor_count": len(matches),
              "arena_half_extent_cm": SCATTER_BOUND, "expected_world_z_cm": SCATTER_Z,
              "expected_materials": sorted(SCATTER_MATERIALS), "components": [],
              "instances": 0, "errors": []}
    STATE["report"]["scatter"] = result
    STATE["scatter"] = matches[0] if len(matches) == 1 else None
    if not matches and not STATE["require_scatter"]:
        result["baseline_without_scatter"] = True
        return result
    if len(matches) != 1:
        raise AssertionError("Expected exactly one GroundRockStamps actor; found %d" % len(matches))
    actor = matches[0]
    result["actor"] = actor.get_path_name()
    result["tags"] = [str(tag) for tag in actor.get_editor_property("tags")]
    if SCATTER_LABEL not in result["tags"]:
        result["errors"].append("Scatter actor lacks GroundRockStamps tag")
    components = actor.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent)
    if len(components) != 4 or len(actor.get_components_by_class(u.MeshComponent)) != 4:
        result["errors"].append("Expected exactly four HISM mesh components; found %d" % len(components))
    assigned = []
    nearest = None
    for component in components:
        mesh = component.get_editor_property("static_mesh")
        material = component.get_material(0)
        count = component.get_instance_count()
        entry = {"component": component.get_path_name(), "mesh": package_name(mesh),
                 "material": package_name(material), "instances": count,
                 "collision": str(component.get_collision_enabled()),
                 "affects_navigation": component.get_editor_property("can_ever_affect_navigation"),
                 "casts_shadow": component.get_editor_property("cast_shadow"),
                 "streaming_distance_multiplier": component.get_editor_property("streaming_distance_multiplier"),
                 "required_streaming_multiplier": 0.0,
                 "receives_decals": component.get_editor_property("receives_decals"),
                 "world_centers_min": None, "world_centers_max": None,
                 "max_footprint_xy_cm": 0.0, "max_z_error_cm": 0.0}
        result["components"].append(entry)
        result["instances"] += count
        assigned.append(package_name(material))
        if component.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION:
            result["errors"].append(entry["component"] + " has collision")
        if entry["affects_navigation"] or entry["casts_shadow"]:
            result["errors"].append(entry["component"] + " affects navigation or casts shadows")
        if package_name(mesh) != "/Engine/BasicShapes/Plane" or count <= 0:
            result["errors"].append(entry["component"] + " has unexpected mesh/no instances")
            continue
        if material:
            parent = material.get_base_material()
            entry["parent"] = package_name(parent)
            entry["blend_mode"] = str(parent.get_editor_property("blend_mode"))
            entry["shading_model"] = str(parent.get_editor_property("shading_model"))
            entry["opacity_mask_clip_value"] = parent.get_editor_property("opacity_mask_clip_value")
            entry["instanced_static_mesh_usage"] = LIB.has_material_usage(
                parent, u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
            if not entry["instanced_static_mesh_usage"]:
                result["errors"].append(entry["component"] + " parent lacks InstancedStaticMeshes usage")
            statistics = LIB.get_statistics(material)
            entry["shader_statistics"] = {name: statistics.get_editor_property(name) for name in
                ("num_pixel_shader_instructions", "num_samplers", "num_pixel_texture_samples")}
            entry["shader_statistics_available"] = entry["shader_statistics"]["num_pixel_shader_instructions"] > 0
            entry["texture_parameters"] = {
                str(name): package_name(LIB.get_material_instance_texture_parameter_value(material, name))
                for name in LIB.get_texture_parameter_names(material)}
            if entry["parent"] != SCATTER_PARENT:
                result["errors"].append(entry["component"] + " has unexpected material parent")
            if (parent.get_editor_property("blend_mode") != u.BlendMode.BLEND_MASKED or
                    parent.get_editor_property("shading_model") != u.MaterialShadingModel.MSM_DEFAULT_LIT or
                    parent.get_editor_property("two_sided")):
                result["errors"].append(entry["component"] + " needs single-sided masked Default Lit material")
            if abs(entry["opacity_mask_clip_value"] - .5) > .0001:
                result["errors"].append(entry["component"] + " opacity mask clip must be 0.5")
            if SCATTER_TEXTURE not in entry["texture_parameters"].values():
                result["errors"].append(entry["component"] + " does not reference the rock atlas")
        bounds = mesh.get_bounds()
        local_corners = [u.Vector(bounds.origin.x + sx * bounds.box_extent.x,
                                  bounds.origin.y + sy * bounds.box_extent.y,
                                  bounds.origin.z)
                         for sx in (-1, 1) for sy in (-1, 1)]
        for index in range(count):
            transform = component.get_instance_transform(index, world_space=True)
            if isinstance(transform, (list, tuple)):
                if len(transform) == 2 and isinstance(transform[0], bool):
                    if not transform[0]:
                        raise AssertionError("Instance transform unavailable")
                    transform = transform[1]
                elif len(transform) == 1:
                    transform = transform[0]
            if not isinstance(transform, u.Transform):
                raise AssertionError("Instance transform unavailable: " + str(transform))
            center = vector(transform.translation)
            if entry["world_centers_min"] is None:
                entry["world_centers_min"] = list(center)
                entry["world_centers_max"] = list(center)
            else:
                entry["world_centers_min"] = [min(a, b) for a, b in zip(entry["world_centers_min"], center)]
                entry["world_centers_max"] = [max(a, b) for a, b in zip(entry["world_centers_max"], center)]
            points = [u.MathLibrary.transform_location(transform, corner) for corner in local_corners]
            largest_scale = max(sum((a - b) ** 2 for a, b in zip(vector(points[0]), vector(points[j]))) ** .5
                                for j in (1, 2)) / 100.0
            entry['required_streaming_multiplier'] = max(entry['required_streaming_multiplier'], largest_scale)
            extent = max(max(abs(point.x), abs(point.y)) for point in points)
            z_error = max(abs(point.z - SCATTER_Z) for point in points)
            entry["max_footprint_xy_cm"] = max(entry["max_footprint_xy_cm"], extent)
            entry["max_z_error_cm"] = max(entry["max_z_error_cm"], z_error)
            if extent > SCATTER_BOUND + .1 or z_error > .05:
                result["errors"].append("%s instance %d footprint/z outside limits: %.3f/%.3f"
                                        % (component.get_name(), index, extent, z_error))
            distance = (center[0] - 1500) ** 2 + (center[1] - 1200) ** 2
            if nearest is None or distance < nearest[0]:
                nearest = (distance, center)
        if entry['streaming_distance_multiplier'] + .001 < entry['required_streaming_multiplier']:
            result['errors'].append(entry['component'] + ' streaming density does not account for instance scale')
    if set(assigned) != SCATTER_MATERIALS or len(assigned) != 4:
        result["errors"].append("Scatter materials differ from the four expected variants: " + str(assigned))
    switch = STATE["report"]["material"]["scalar_parameters"].get("PlacedRockStamps")
    result["placed_rock_stamps"] = switch
    if switch is None or abs(switch - 1.0) > .0001:
        result["errors"].append("Base ground PlacedRockStamps scalar must be 1")
    if nearest:
        result["probe_focus"] = nearest[1]
    if result["errors"]:
        raise AssertionError("Scatter validation failed: " + "; ".join(result["errors"][:12]))
    result["verified"] = True
    return result


def begin_batch(folder, apply=False):
    cleanup_view()
    STATE.update(folder=folder_name(folder), batch=STATE["batch"] + 1,
                 batch_at=time.monotonic(), stage="loading", index=0, views=[])
    STATE["out"] = OUTPUT / STATE["folder"]
    STATE["out"].mkdir(parents=True, exist_ok=True)
    status(False)
    initial_hashes = map_hashes()
    if apply:
        script = ROOT / "Tools/apply_ground_rock_scatter.py"
        if not script.is_file():
            raise FileNotFoundError(script)
        runpy.run_path(str(script), run_name="__main__")
        STATE["applied"] = True
    after_apply_hashes = map_hashes()
    lighting_key = str(MAP_FILES[1].relative_to(ROOT))
    if after_apply_hashes[lighting_key] != initial_hashes[lighting_key]:
        raise AssertionError("The scatter apply unexpectedly saved the lighting map")
    if not apply and after_apply_hashes != initial_hashes:
        raise AssertionError("A map changed without an apply request")
    if not LEVEL.load_level(MAP):
        raise RuntimeError("Could not open " + MAP)
    all_actors = ACTORS.get_all_level_actors()
    floors = [actor for actor in all_actors if actor.get_actor_label() == "Plane"
              and isinstance(actor, u.StaticMeshActor)]
    starts = [actor for actor in all_actors if isinstance(actor, u.PlayerStart)]
    if len(floors) != 1 or len(starts) != 1:
        raise AssertionError("Expected one floor and PlayerStart; found %d/%d" % (len(floors), len(starts)))
    STATE["floor"] = floors[0]
    STATE["map_hashes"] = after_apply_hashes
    STATE["require_scatter"] = apply or STATE["applied"] or not STATE["folder"].lower().startswith("before")
    start = starts[0].get_actor_location()
    ground_z = floors[0].get_actor_location().z
    STATE["specs"] = [
        {"name": "GameplayStart", "kind": "gameplay", "focus": vector(start), "size": (1600, 900)},
        {"name": "GameplayMeadowLoop", "kind": "gameplay", "focus": [1500, 3000, start.z], "size": (1600, 900)},
        {"name": "GroundOverview", "kind": "ground", "focus": [0, 0, ground_z], "width": 15200, "size": (1600, 1600)},
        {"name": "GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 4000, "size": (1600, 1000)}]
    STATE["report"] = {"passed": False, "session": SESSION, "batch": STATE["batch"],
                       "folder": STATE["folder"], "map": MAP, "started_utc": utc_now(),
                       "errors": [],
                       "apply_requested": apply, "any_apply_in_session": STATE["applied"],
                       "map_hashes_before_apply": initial_hashes,
                       "map_hashes_after_apply": after_apply_hashes,
                       "force_ground_mips": STATE.get("force_ground_mips", False),
                       "texture_streaming": u.SystemLibrary.get_console_variable_int_value("r.TextureStreaming"),
                       "player_start": vector(start), "gameplay_rig": RIG,
                       "ground_only_note": "Other mesh actors are temporarily hidden. GroundRockStamps, the actual floor, saved lighting and post-processing remain active."}
    STATE["report"]["material"] = inspect_material()
    scatter = inspect_scatter()
    # Use identical framing before/after whenever both are captured in one session.
    if "probe_focus" not in STATE:
        STATE["probe_focus"] = scatter.get("probe_focus", [1500, 1200, SCATTER_Z])
    probe_focus = list(STATE["probe_focus"])
    STATE["specs"].extend([
        {"name": "SurfaceLowAngle", "kind": "low_angle", "focus": probe_focus,
         "telegraph": True, "size": (1600, 900)},
        {"name": "GameplayTelegraphProbe", "kind": "gameplay", "focus": probe_focus[:2] + [start.z],
         "telegraph": True, "size": (1600, 900)}])
    # This official helper waits for shader compilation and render resources.
    u.AutomationLibrary.finish_loading_before_screenshot()
    # UE5.8's "True" call only removes a previous subsystem OFF override and
    # ensures if none was pushed. Preserve the user's realtime setting; request
    # warmup draws explicitly below instead of using that unpaired pop operation.
    LEVEL.editor_invalidate_viewports()
    LEVEL.editor_set_game_view(True)
    STATE.update(stage="view", at=time.monotonic())
    status(False)


def prepare_view():
    cleanup_view()
    spec = STATE["specs"][STATE["index"]]
    STATE["view"] = spec
    if spec["kind"] == "gameplay":
        rig_class = u.EditorAssetLibrary.load_blueprint_class(RIG)
        if not rig_class:
            raise RuntimeError("Could not load the gameplay camera rig")
        STATE["rig"] = spawn(rig_class, u.Vector(*spec["focus"]))
        # SpringArm ticks in the editor; defer copying until construction/lag settle.
        STATE.update(stage="rig", at=time.monotonic())
        return
    for actor in ACTORS.get_all_level_actors():
        if actor not in (STATE["floor"], STATE.get("scatter")) and actor.get_components_by_class(u.MeshComponent):
            STATE["hidden"].append((actor, actor.is_temporarily_hidden_in_editor()))
            actor.set_is_temporarily_hidden_in_editor(True)
    focus = spec["focus"]
    if spec["kind"] == "low_angle":
        location = u.Vector(focus[0] + 1300, focus[1] - 900, focus[2] + 480)
        orientation = u.MathLibrary.find_look_at_rotation(location, u.Vector(*focus))
        camera = spawn(u.CameraActor, location, orientation)
        component = camera.get_component_by_class(u.CameraComponent)
        component.set_editor_property("field_of_view", 60.0)
        component.set_editor_property("constrain_aspect_ratio", False)
        component.set_editor_property("post_process_blend_weight", 0.0)
        add_telegraph_probe()
        pilot(camera)
        return
    camera = spawn(u.CameraActor, u.Vector(focus[0], focus[1], focus[2] + 5000),
                   u.Rotator(pitch=-90, yaw=0, roll=0))
    component = camera.get_component_by_class(u.CameraComponent)
    component.set_editor_property("projection_mode", u.CameraProjectionMode.ORTHOGRAPHIC)
    component.set_editor_property("ortho_width", spec["width"])
    component.set_editor_property("constrain_aspect_ratio", False)
    component.set_editor_property("post_process_blend_weight", 0.0)
    pilot(camera)


def copy_gameplay_camera():
    source = STATE["rig"].get_component_by_class(u.CameraComponent)
    if not source:
        raise AssertionError("Gameplay rig has no CameraComponent")
    camera = spawn(u.CameraActor, source.get_world_location(), source.get_world_rotation())
    target = camera.get_component_by_class(u.CameraComponent)
    for key in ("projection_mode", "field_of_view", "ortho_width", "aspect_ratio",
                "constrain_aspect_ratio", "post_process_settings", "post_process_blend_weight",
                "auto_calculate_ortho_planes", "ortho_near_clip_plane", "ortho_far_clip_plane"):
        target.set_editor_property(key, source.get_editor_property(key))
    STATE["view"]["gameplay_source_camera"] = camera_settings(source)
    if STATE["view"].get("telegraph"):
        add_telegraph_probe()
    pilot(camera)


def add_telegraph_probe():
    """Static runtime-material probe: no combat/player actors or saved assets."""
    focus = STATE["view"]["focus"]
    floor_z = STATE["floor"].get_actor_location().z
    actor = spawn(u.StaticMeshActor, u.Vector(focus[0], focus[1], floor_z + 4.0))
    component = actor.get_component_by_class(u.StaticMeshComponent)
    mesh = u.load_asset("/Engine/BasicShapes/Plane")
    boss_blueprint = u.load_asset(TELEGRAPH_BOSS)
    if not boss_blueprint or not boss_blueprint.generated_class():
        raise AssertionError("Saved boss Blueprint unavailable for authored telegraph parameters")
    boss_defaults = u.get_default_object(boss_blueprint.generated_class())
    authored = boss_defaults.get_editor_property("circle_telegraph_material")
    # Match ABossGroundTelegraph::InitializePersistentCircle exactly: use the
    # supplied surface material (including MI overrides), otherwise the surface fallback.
    material = authored if (authored and authored.get_base_material().get_editor_property(
        "material_domain") == u.MaterialDomain.MD_SURFACE) else u.load_asset(TELEGRAPH_MATERIAL)
    if not mesh or not material:
        raise AssertionError("Attack-circle probe mesh/material is missing")
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    component.set_cast_shadow(False)
    component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_scale3d(u.Vector(7.5, 7.5, 1.0))
    dynamic = component.create_dynamic_material_instance(0)
    if not dynamic:
        raise AssertionError("Could not create the attack-circle probe material")
    # These three values match InitializePersistentRectangle + SetTelegraphFillAmount.
    # Other values (including FillColor and BorderColor) must remain authored; the
    # game's black unfilled interior and pink/red charge are intentional.
    charge_parameters = {"FillAmount": .65, "Fill": .65, "LaneAspect": 1.0}
    for name, value in charge_parameters.items():
        dynamic.set_scalar_parameter_value(name, value)
    scalars = {str(name): dynamic.get_scalar_parameter_value(name)
               for name in LIB.get_scalar_parameter_names(material)}
    vectors = {}
    for name in LIB.get_vector_parameter_names(material):
        value = dynamic.get_vector_parameter_value(name)
        vectors[str(name)] = [value.r, value.g, value.b, value.a]
    if abs(scalars.get("FillAmount", -1) - .65) > .0001:
        raise AssertionError("The attack-circle probe does not expose the runtime FillAmount parameter")
    STATE["view"]["telegraph_probe"] = {
        "material": package_name(material), "configured_boss_material": package_name(authored),
        "boss_blueprint": TELEGRAPH_BOSS, "radius_cm": 375, "world_z_cm": floor_z + 4,
        "applied_runtime_parameters": charge_parameters,
        "resolved_scalar_parameters": scalars, "inherited_vector_parameters": vectors,
        "fill": .65, "source": "BossGroundTelegraph.cpp surface plane; FinalBossBase.cpp +4cm placement",
        "note": "Static runtime-material/height probe only. Authored colors and black unfilled interior are retained; this is not gameplay or collision verification."}


def pilot(camera):
    STATE["camera"] = camera
    STATE["view"]["captured_camera"] = camera_settings(camera.get_component_by_class(u.CameraComponent))
    LEVEL.pilot_level_actor(camera)
    STATE.update(stage="warm", at=time.monotonic())


def validate_viewport_camera():
    """Verify the renderer's viewport, not just the intended CameraActor pose."""
    key = LEVEL.get_active_viewport_config_key()
    if LEVEL.get_pilot_level_actor(key) != STATE["camera"]:
        raise AssertionError("Active viewport is not piloting the review camera")
    if not LEVEL.get_exact_camera_view(key):
        raise AssertionError("Viewport exact-camera mode is disabled; projection/FOV could differ")
    info = LEVEL.get_level_viewport_camera_info(key)
    # UE Python versions expose bool+out functions as output tuples or bool-prefixed tuples.
    if isinstance(info, (tuple, list)) and len(info) == 3 and isinstance(info[0], bool):
        if not info[0]:
            raise AssertionError("Could not read the active viewport camera")
        info = info[1:]
    if not isinstance(info, (tuple, list)) or len(info) != 2:
        raise AssertionError("Viewport camera readback unavailable: " + str(info))
    position, orientation = info
    fov = LEVEL.get_level_viewport_fov(key)
    if isinstance(fov, (tuple, list)):
        if len(fov) == 2 and isinstance(fov[0], bool):
            if not fov[0]:
                raise AssertionError("Could not read the viewport FOV")
            fov = fov[1]
        elif len(fov) == 1:
            fov = fov[0]
    if not isinstance(fov, (int, float)) or isinstance(fov, bool):
        raise AssertionError("Viewport FOV readback unavailable: " + str(fov))
    expected = STATE["view"]["captured_camera"]
    distance = sum((a - b) ** 2 for a, b in zip(vector(position), expected["position"])) ** .5
    expected_rotation = u.Rotator(pitch=expected["rotation"][0], yaw=expected["rotation"][1],
                                  roll=expected["rotation"][2])
    # Compare basis directions so equivalent Euler rotations at a top-down gimbal pose pass.
    rotation_error = max(sum((a - b) ** 2 for a, b in zip(vector(getter(orientation)),
                         vector(getter(expected_rotation)))) ** .5
                         for getter in (u.MathLibrary.get_forward_vector, u.MathLibrary.get_up_vector))
    readback = {"verified": False, "viewport": str(key), "position": vector(position),
                "rotation": rotation(orientation), "field_of_view": fov,
                "position_error_cm": distance, "rotation_basis_error": rotation_error,
                "exact_camera_view": True, "pilot": STATE["camera"].get_path_name()}
    STATE["view"]["viewport_camera"] = readback
    if distance > .1 or rotation_error > .001 or abs(fov - expected["field_of_view"]) > .001:
        raise AssertionError("Rendered viewport differs from the requested camera: " + json.dumps(readback))
    readback["verified"] = True


def capture():
    validate_viewport_camera()
    spec = STATE["view"]
    if STATE["require_scatter"]:
        u.log('GROUND_ROCK_ATLAS_STREAMING ' + spec['name'])
        u.SystemLibrary.execute_console_command(
            u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),
            'InvestigateTexture T_RockStamps_Atlas')
    path = STATE["out"] / (spec["name"] + ".png")
    # A unique temporary filename prevents an old screenshot from satisfying completion.
    pending = STATE["out"] / ("_" + spec["name"] + "_" + SESSION + "_%d.png" % STATE["batch"])
    width, height = spec["size"]
    task = u.AutomationLibrary.take_high_res_screenshot(width, height, str(pending), camera=STATE["camera"])
    if not task or not task.is_valid_task():
        raise RuntimeError("Editor rejected screenshot task " + spec["name"])
    STATE.update(task=task, pending=pending, final=path, stage="capture", at=time.monotonic())


def finish_capture():
    pending = STATE["pending"]
    raw = pending.read_bytes()
    if raw[:8] != b"\x89PNG\r\n\x1a\n" or len(raw) < 1024:
        raise AssertionError("Capture did not produce a valid PNG: " + str(pending))
    dimensions = struct.unpack(">II", raw[16:24])
    if dimensions != STATE["view"]["size"]:
        raise AssertionError("Screenshot dimensions do not match request: " + str(dimensions))
    pending.replace(STATE["final"])
    view = dict(STATE["view"])
    view.update(file=str(STATE["final"]), captured_utc=utc_now(), bytes=len(raw),
                sha256=hashlib.sha256(raw).hexdigest())
    STATE["views"].append(view)
    STATE["report"]["views"] = STATE["views"]
    write_json(STATE["out"] / "Validation.json", STATE["report"])
    cleanup_view()
    STATE.update(index=STATE["index"] + 1, stage="view")


def finish_batch():
    cleanup_view()
    if map_hashes() != STATE["map_hashes"]:
        raise AssertionError("A gameplay map changed on disk during review")
    # Read statistics after real scene rendering, rather than assuming NullRHI zeros mean cheap shaders.
    STATE["report"].update(completed_utc=utc_now(), map_hashes=map_hashes(),
                           maps_unchanged=True, material=inspect_material(), views=STATE["views"])
    STATE["report"]["scatter"] = inspect_scatter()
    STATE["report"]["passed"] = True
    write_json(STATE["out"] / "Validation.json", STATE["report"])
    write_json(OUTPUT / "LatestValidation.json", STATE["report"])
    STATE.update(stage="idle", idle_at=time.monotonic())
    status(True, passed=True, captures=[view["file"] for view in STATE["views"]])
    u.log("GROUND_ROCK_SCATTER_REVIEW_PASS " + str(STATE["out"]))
    if not KEEP_OPEN.exists():
        close_editor()


def check_command():
    if not KEEP_OPEN.exists():
        close_editor()
        return
    if time.monotonic() - STATE["idle_at"] > 1800:
        raise TimeoutError("Ground review session exceeded its 30-minute idle limit")
    if not COMMAND.exists():
        return
    command = json.loads(COMMAND.read_text(encoding="utf-8-sig"))
    action = command.get("action")
    if action not in ("apply", "capture", "quit"):
        raise ValueError("Unknown ground review action: " + str(action))
    COMMAND.unlink()
    write_json(OUTPUT / "LastCommand.json", {"session": SESSION, "received_utc": utc_now(), **command})
    if action == "quit":
        STATE["stage"] = "closed"
        status(False, passed=True)
        close_editor()
        return
    begin_batch(command.get("folder", "After" if action == "apply" else STATE["folder"]), action == "apply")


def tick(delta):
    if STATE["busy"] or STATE["closed"]:
        return
    STATE["busy"] = True
    try:
        stage = STATE["stage"]
        if stage in ("rig", "warm"):
            LEVEL.editor_invalidate_viewports()
        if stage != "idle" and time.monotonic() - STATE["batch_at"] > 600:
            raise TimeoutError("Ground review capture batch exceeded ten minutes")
        if stage == "begin":
            command_line = u.SystemLibrary.get_command_line()
            STATE["force_ground_mips"] = bool(re.search(r'(?:^|\s)-RockScatterForceMips(?:\s|$)', command_line))
            match = re.search(r'(?:^|\s)-RockScatterReviewFolder=(?:"([^"]+)"|([^\s]+))', command_line)
            begin_batch((match.group(1) or match.group(2)) if match else "After",
                        bool(re.search(r'(?:^|\s)-RockScatterApplyFirst(?:\s|$)', command_line)))
        elif stage == "view":
            if STATE["index"] >= len(STATE["specs"]):
                finish_batch()
            else:
                prepare_view()
        elif stage == "rig" and time.monotonic() - STATE["at"] > 2:
            copy_gameplay_camera()
        elif stage == "warm" and time.monotonic() - STATE["at"] > 8:
            capture()
        elif stage == "capture":
            if time.monotonic() - STATE["at"] > 90:
                raise TimeoutError("Screenshot did not complete: " + STATE["view"]["name"])
            if STATE["task"].is_task_done() and STATE["pending"].exists():
                finish_capture()
        elif stage == "idle":
            check_command()
    except Exception:
        fail()
    finally:
        STATE["busy"] = False


u.EditorPythonScripting.set_keep_python_script_alive(True)
status(False)
STATE["handle"] = u.register_slate_post_tick_callback(tick)
