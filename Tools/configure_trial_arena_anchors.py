import unreal

LEVEL = "/Game/Maps/Lvl_B1_Lvl1"
ANCHORS = (
    ("TrialArenaAnchor_01", unreal.Vector(0.0, 50000.0, 0.0)),
    ("TrialArenaAnchor_02", unreal.Vector(0.0, 75000.0, 0.0)),
    ("TrialArenaAnchor_03", unreal.Vector(50000.0, 0.0, 0.0)),
)

unreal.EditorLevelLibrary.load_level(LEVEL)
anchor_class = unreal.load_class(None, "/Script/HeavensDivide.TrialArenaAnchor")
if not anchor_class:
    raise RuntimeError("TrialArenaAnchor native class is unavailable; compile the project first.")

existing = {
    actor.get_actor_label(): actor
    for actor in unreal.EditorLevelLibrary.get_all_level_actors()
    if actor.get_class() == anchor_class
}

for label, location in ANCHORS:
    actor = existing.get(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(anchor_class, location)
        actor.set_actor_label(label)
    else:
        actor.set_actor_location(location, False, False)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Failed to save trial arena anchors into Lvl_B1_Lvl1.")

unreal.log("[TrialRuntime] Configured 3 non-overlapping TrialArenaAnchor actors in Lvl_B1_Lvl1")
