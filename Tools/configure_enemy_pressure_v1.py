import unreal


SPAWNER_BLUEPRINT = "/Game/HeavensDivide/Blueprints/EnemyCharacters/BP_EnemySpawner"
ACTIVE_LEVEL = "/Game/Maps/Lvl_B1_Lvl1"

ENEMY_CLASSES = {
    "Grunt": "/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGrunt.BP_EnemyGrunt_C",
    "Devil": "/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyDevilRanged.BP_EnemyDevilRanged_C",
    "Skeleton": "/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyFloatingSkeleton.BP_EnemyFloatingSkeleton_C",
    "Ogre": "/Game/HeavensDivide/Blueprints/EnemyCharacters/Elites/BP_EnemyOgre.BP_EnemyOgre_C",
    "Gorilla": "/Game/HeavensDivide/Blueprints/EnemyCharacters/Elites/BP_EnemyGorilla.BP_EnemyGorilla_C",
}

UNLOCK_SECONDS = {"Grunt": 0.0, "Devil": 30.0, "Skeleton": 120.0, "Ogre": 180.0, "Gorilla": 360.0}
HEALTH_PER_MINUTE = {"Grunt": 0.05, "Devil": 0.08, "Skeleton": 0.10, "Ogre": 0.12, "Gorilla": 0.0}
SPAWN_MODES = {
    "Grunt": unreal.EnemyPressureSpawnMode.MAINTAIN_POPULATION,
    "Devil": unreal.EnemyPressureSpawnMode.MAINTAIN_POPULATION,
    "Skeleton": unreal.EnemyPressureSpawnMode.TIMED_THREAT,
    "Ogre": unreal.EnemyPressureSpawnMode.TIMED_THREAT,
    "Gorilla": unreal.EnemyPressureSpawnMode.TIMED_THREAT,
}
RESPAWN_DELAYS = {"Grunt": (0.0, 0.0), "Devil": (4.0, 6.0), "Skeleton": (12.0, 18.0), "Ogre": (15.0, 22.0), "Gorilla": (45.0, 60.0)}

# name, start, end (0=open), global cap, normal/accelerated/emergency intervals,
# then desired/max/priority by enemy key.
PHASES = (
    ("Intro", 0.0, 60.0, 22, 0.65, 0.35, 0.20,
     {"Grunt": (15, 20, 1.0), "Devil": (1, 1, 1.5), "Skeleton": (0, 0, 2.0), "Ogre": (0, 0, 2.0), "Gorilla": (0, 0, 3.0)}),
    ("EarlyPressure", 60.0, 120.0, 30, 0.60, 0.32, 0.20,
     {"Grunt": (22, 28, 1.0), "Devil": (2, 2, 1.5), "Skeleton": (0, 0, 2.0), "Ogre": (0, 0, 2.0), "Gorilla": (0, 0, 3.0)}),
    ("Mobility", 120.0, 180.0, 40, 0.55, 0.30, 0.18,
     {"Grunt": (30, 38, 1.0), "Devil": (2, 2, 1.5), "Skeleton": (0, 1, 2.0), "Ogre": (0, 0, 2.0), "Gorilla": (0, 0, 3.0)}),
    ("HeavyIntroduction", 180.0, 240.0, 50, 0.55, 0.30, 0.18,
     {"Grunt": (38, 46, 1.0), "Devil": (2, 2, 1.5), "Skeleton": (0, 1, 2.0), "Ogre": (0, 1, 2.0), "Gorilla": (0, 0, 3.0)}),
    ("Horde", 240.0, 300.0, 62, 0.50, 0.28, 0.18,
     {"Grunt": (48, 56, 1.0), "Devil": (3, 3, 1.5), "Skeleton": (0, 1, 2.0), "Ogre": (0, 1, 2.0), "Gorilla": (0, 0, 3.0)}),
    ("Chaos", 300.0, 360.0, 72, 0.50, 0.28, 0.18,
     {"Grunt": (55, 65, 1.0), "Devil": (3, 3, 1.5), "Skeleton": (0, 2, 2.0), "Ogre": (0, 2, 2.0), "Gorilla": (0, 0, 3.0)}),
    ("GorillaIntroduction", 360.0, 420.0, 80, 0.45, 0.25, 0.15,
     {"Grunt": (60, 72, 1.0), "Devil": (3, 3, 1.5), "Skeleton": (0, 2, 2.0), "Ogre": (0, 2, 2.0), "Gorilla": (0, 1, 3.0)}),
    ("Escalation", 420.0, 480.0, 88, 0.45, 0.25, 0.15,
     {"Grunt": (66, 80, 1.0), "Devil": (4, 4, 1.5), "Skeleton": (0, 2, 2.0), "Ogre": (0, 2, 2.0), "Gorilla": (0, 1, 3.0)}),
    ("Endgame", 480.0, 0.0, 96, 0.45, 0.25, 0.15,
     {"Grunt": (72, 86, 1.0), "Devil": (4, 4, 1.5), "Skeleton": (0, 2, 2.0), "Ogre": (0, 3, 2.0), "Gorilla": (0, 1, 3.0)}),
)

PHASE_EVENT_CADENCE = (
    (False, 35.0, 45.0), (False, 35.0, 45.0),
    (True, 20.0, 30.0), (True, 30.0, 40.0),
    (True, 25.0, 35.0), (True, 25.0, 35.0),
    (True, 20.0, 30.0), (True, 20.0, 30.0), (True, 20.0, 30.0),
)


def load_class(path):
    value = unreal.load_class(None, path)
    if not value:
        raise RuntimeError("Could not load class: " + path)
    return value


classes = {name: load_class(path) for name, path in ENEMY_CLASSES.items()}
spawner_generated_class = load_class(SPAWNER_BLUEPRINT + ".BP_EnemySpawner_C")
spawner_cdo = unreal.get_default_object(spawner_generated_class)

spawn_entries = []
for name in ("Grunt", "Devil", "Skeleton", "Ogre", "Gorilla"):
    entry = unreal.EnemySpawnEntry()
    entry.set_editor_property("enemy_class", classes[name])
    entry.set_editor_property("spawn_weight", 1.0)
    entry.set_editor_property("minimum_run_time", UNLOCK_SECONDS[name])
    entry.set_editor_property("maximum_run_time", 0.0)
    entry.set_editor_property("spawn_cost", 1)
    entry.set_editor_property("max_alive_of_this_type", 0)
    entry.set_editor_property("enabled", True)
    entry.set_editor_property("health_scaling_per_minute", HEALTH_PER_MINUTE[name])
    entry.set_editor_property("pressure_spawn_mode", SPAWN_MODES[name])
    entry.set_editor_property("min_respawn_delay_after_death", RESPAWN_DELAYS[name][0])
    entry.set_editor_property("max_respawn_delay_after_death", RESPAWN_DELAYS[name][1])
    spawn_entries.append(entry)

pressure_phases = []
for phase_index, (name, start, end, global_cap, normal, accelerated, emergency, populations) in enumerate(PHASES):
    phase = unreal.EnemyPressurePhase()
    phase.set_editor_property("phase_name", name)
    phase.set_editor_property("start_time_seconds", start)
    phase.set_editor_property("end_time_seconds", end)
    phase.set_editor_property("global_max_alive", global_cap)
    phase.set_editor_property("normal_spawn_interval", normal)
    phase.set_editor_property("accelerated_spawn_interval", accelerated)
    phase.set_editor_property("emergency_spawn_interval", emergency)
    population_entries = []
    for enemy_name in ("Grunt", "Devil", "Skeleton", "Ogre", "Gorilla"):
        desired, maximum, priority = populations[enemy_name]
        population = unreal.EnemyPopulationPhaseEntry()
        population.set_editor_property("enemy_class", classes[enemy_name])
        population.set_editor_property("desired_population", desired)
        population.set_editor_property("max_population", maximum)
        population.set_editor_property("refill_priority", priority)
        population_entries.append(population)
    phase.set_editor_property("enemy_population_entries", population_entries)
    events_enabled, event_min, event_max = PHASE_EVENT_CADENCE[phase_index]
    phase.set_editor_property("events_enabled", events_enabled)
    phase.set_editor_property("event_interval_min", event_min)
    phase.set_editor_property("event_interval_max", event_max)
    pressure_phases.append(phase)

def event_member(enemy_name, count, class_overflow):
    member = unreal.EnemyPressureEventEnemyEntry()
    member.set_editor_property("enemy_class", classes[enemy_name])
    member.set_editor_property("count", count)
    member.set_editor_property("class_overflow_allowance", class_overflow)
    return member

def event_definition(name, minimum_time, weight, cooldown_min, cooldown_max, arc, distance_min, distance_max, delay, first_member_delay, overflow, members):
    event = unreal.EnemyPressureEventDefinition()
    event.set_editor_property("event_name", name)
    event.set_editor_property("enabled", True)
    event.set_editor_property("minimum_run_time", minimum_time)
    event.set_editor_property("maximum_run_time", 0.0)
    event.set_editor_property("weight", weight)
    event.set_editor_property("event_cooldown_min", cooldown_min)
    event.set_editor_property("event_cooldown_max", cooldown_max)
    event.set_editor_property("spawn_arc_degrees", arc)
    event.set_editor_property("spawn_distance_min", distance_min)
    event.set_editor_property("spawn_distance_max", distance_max)
    event.set_editor_property("delay_between_members", delay)
    event.set_editor_property("delay_after_first_member", first_member_delay)
    event.set_editor_property("allow_temporary_population_overflow", True)
    event.set_editor_property("event_population_overflow_allowance", overflow)
    event.set_editor_property("ignore_threat_death_cooldown", False)
    event.set_editor_property("enemy_entries", members)
    return event

pressure_events = [
    event_definition("GruntSurge", 120.0, 5.0, 25.0, 40.0, 40.0, 1500.0, 2200.0, 0.07, 0.0, 20, [event_member("Grunt", 18, 18)]),
    event_definition("SkeletonRush", 180.0, 3.0, 35.0, 50.0, 28.0, 1700.0, 2200.0, 0.40, 0.0, 2, [event_member("Skeleton", 2, 1)]),
    event_definition("HeavyPush", 240.0, 3.0, 40.0, 60.0, 38.0, 1600.0, 2300.0, 0.08, 0.25, 10, [event_member("Ogre", 1, 0), event_member("Grunt", 7, 8)]),
]

spawner_cdo.set_editor_property("enemy_spawn_entries", spawn_entries)
spawner_cdo.set_editor_property("pressure_phases", pressure_phases)
spawner_cdo.set_editor_property("absolute_hard_alive_cap", 100)
spawner_cdo.set_editor_property("normal_max_batch_size", 2)
spawner_cdo.set_editor_property("accelerated_max_batch_size", 3)
spawner_cdo.set_editor_property("emergency_max_batch_size", 4)
spawner_cdo.set_editor_property("timed_threat_spawn_slot_chance", 0.20)
spawner_cdo.set_editor_property("pressure_events", pressure_events)
spawner_cdo.set_editor_property("directional_bias_enabled", True)
spawner_cdo.set_editor_property("directional_bias_chance", 0.55)
spawner_cdo.set_editor_property("directional_bias_arc_degrees", 120.0)
spawner_cdo.set_editor_property("directional_bias_duration_min", 8.0)
spawner_cdo.set_editor_property("directional_bias_duration_max", 12.0)
spawner_cdo.set_editor_property("enable_spatial_pressure_recycling", True)
spawner_cdo.set_editor_property("spatial_pressure_grunt_class", classes["Grunt"])
spawner_cdo.set_editor_property("spatial_pressure_evaluation_interval", 2.0)
spawner_cdo.set_editor_property("minimum_player_speed_for_directional_recycling", 150.0)
spawner_cdo.set_editor_property("minimum_grunt_population_ratio_for_recycling", 0.85)
spawner_cdo.set_editor_property("stale_grunt_minimum_distance", 2800.0)
spawner_cdo.set_editor_property("stale_grunt_behind_dot_threshold", -0.50)
spawner_cdo.set_editor_property("max_grunts_recycled_per_evaluation", 4)
spawner_cdo.set_editor_property("minimum_seconds_alive_before_recyclable", 5.0)
spawner_cdo.set_editor_property("replacement_spawn_distance_min", 1200.0)
spawner_cdo.set_editor_property("replacement_spawn_distance_max", 2000.0)
spawner_cdo.set_editor_property("spatial_sector_count", 8)
spawner_cdo.set_editor_property("prefer_underrepresented_sectors", True)
spawner_cdo.set_editor_property("replacement_sector_randomness", 0.25)
spawner_cdo.set_editor_property("event_grunt_recycle_protection_seconds", 10.0)

blueprint = unreal.load_asset(SPAWNER_BLUEPRINT)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
if not unreal.EditorAssetLibrary.save_asset(SPAWNER_BLUEPRINT, only_if_is_dirty=False):
    raise RuntimeError("Failed to save " + SPAWNER_BLUEPRINT)

# New properties should inherit from the Blueprint CDO. Verify the active placed spawner
# resolves to the saved data; only save the map if an explicit stale override needs repair.
unreal.EditorLevelLibrary.load_level(ACTIVE_LEVEL)
placed_spawners = [
    actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
    if actor.get_class() == spawner_generated_class
]
if len(placed_spawners) != 1:
    raise RuntimeError("Expected exactly one active BP_EnemySpawner in Lvl_B1_Lvl1; found %d" % len(placed_spawners))

placed = placed_spawners[0]
placed_phases = placed.get_editor_property("pressure_phases")
placed_entries = placed.get_editor_property("enemy_spawn_entries")
map_changed = False
def phases_match(actual):
    if len(actual) != len(pressure_phases):
        return False
    for index, expected in enumerate(PHASES):
        name, start, end, cap, normal, accelerated, emergency, populations = expected
        phase = actual[index]
        if (str(phase.get_editor_property("phase_name")) != name
                or abs(phase.get_editor_property("start_time_seconds") - start) > 0.001
                or abs(phase.get_editor_property("end_time_seconds") - end) > 0.001
                or phase.get_editor_property("global_max_alive") != cap
                or phase.get_editor_property("events_enabled") != PHASE_EVENT_CADENCE[index][0]
                or abs(phase.get_editor_property("event_interval_min") - PHASE_EVENT_CADENCE[index][1]) > 0.001
                or abs(phase.get_editor_property("event_interval_max") - PHASE_EVENT_CADENCE[index][2]) > 0.001):
            return False
        entries = phase.get_editor_property("enemy_population_entries")
        if len(entries) != 5:
            return False
        for entry_index, enemy_name in enumerate(("Grunt", "Devil", "Skeleton", "Ogre", "Gorilla")):
            desired, maximum, priority = populations[enemy_name]
            population = entries[entry_index]
            if (population.get_editor_property("enemy_class") != classes[enemy_name]
                    or population.get_editor_property("desired_population") != desired
                    or population.get_editor_property("max_population") != maximum
                    or abs(population.get_editor_property("refill_priority") - priority) > 0.001):
                return False
    return True

if not phases_match(placed_phases):
    placed.set_editor_property("pressure_phases", pressure_phases)
    map_changed = True

def spawn_entries_match(actual):
    if len(actual) != len(spawn_entries):
        return False
    for index, expected_name in enumerate(("Grunt", "Devil", "Skeleton", "Ogre", "Gorilla")):
        entry = actual[index]
        if (entry.get_editor_property("enemy_class") != classes[expected_name]
                or abs(entry.get_editor_property("minimum_run_time") - UNLOCK_SECONDS[expected_name]) > 0.001
                or abs(entry.get_editor_property("health_scaling_per_minute") - HEALTH_PER_MINUTE[expected_name]) > 0.0001
                or entry.get_editor_property("pressure_spawn_mode") != SPAWN_MODES[expected_name]
                or abs(entry.get_editor_property("min_respawn_delay_after_death") - RESPAWN_DELAYS[expected_name][0]) > 0.001
                or abs(entry.get_editor_property("max_respawn_delay_after_death") - RESPAWN_DELAYS[expected_name][1]) > 0.001
                or entry.get_editor_property("max_alive_of_this_type") != 0):
            return False
    return True

if not spawn_entries_match(placed_entries):
    placed.set_editor_property("enemy_spawn_entries", spawn_entries)
    map_changed = True
if placed.get_editor_property("absolute_hard_alive_cap") != 100:
    placed.set_editor_property("absolute_hard_alive_cap", 100)
    map_changed = True
if abs(placed.get_editor_property("timed_threat_spawn_slot_chance") - 0.20) > 0.001:
    placed.set_editor_property("timed_threat_spawn_slot_chance", 0.20)
    map_changed = True
def events_match(actual):
    expected = (
        ("GruntSurge", 18, 40.0, 1500.0, 2200.0, 0.07, 0.0, 20),
        ("SkeletonRush", 2, 28.0, 1700.0, 2200.0, 0.40, 0.0, 2),
        ("HeavyPush", 8, 38.0, 1600.0, 2300.0, 0.08, 0.25, 10),
    )
    if len(actual) != len(expected):
        return False
    for event, values in zip(actual, expected):
        name, member_count, arc, distance_min, distance_max, delay, first_delay, overflow = values
        if (str(event.get_editor_property("event_name")) != name
                or sum(m.get_editor_property("count") for m in event.get_editor_property("enemy_entries")) != member_count
                or abs(event.get_editor_property("spawn_arc_degrees") - arc) > 0.001
                or abs(event.get_editor_property("spawn_distance_min") - distance_min) > 0.001
                or abs(event.get_editor_property("spawn_distance_max") - distance_max) > 0.001
                or abs(event.get_editor_property("delay_between_members") - delay) > 0.001
                or abs(event.get_editor_property("delay_after_first_member") - first_delay) > 0.001
                or event.get_editor_property("event_population_overflow_allowance") != overflow):
            return False
    return True

if not events_match(placed.get_editor_property("pressure_events")):
    placed.set_editor_property("pressure_events", pressure_events)
    map_changed = True
if (not placed.get_editor_property("directional_bias_enabled")
        or abs(placed.get_editor_property("directional_bias_chance") - 0.55) > 0.001
        or abs(placed.get_editor_property("directional_bias_arc_degrees") - 120.0) > 0.001
        or abs(placed.get_editor_property("directional_bias_duration_min") - 8.0) > 0.001
        or abs(placed.get_editor_property("directional_bias_duration_max") - 12.0) > 0.001):
    placed.set_editor_property("directional_bias_enabled", True)
    placed.set_editor_property("directional_bias_chance", 0.55)
    placed.set_editor_property("directional_bias_arc_degrees", 120.0)
    placed.set_editor_property("directional_bias_duration_min", 8.0)
    placed.set_editor_property("directional_bias_duration_max", 12.0)
    map_changed = True
spatial_settings = {
    "enable_spatial_pressure_recycling": True,
    "spatial_pressure_grunt_class": classes["Grunt"],
    "spatial_pressure_evaluation_interval": 2.0,
    "minimum_player_speed_for_directional_recycling": 150.0,
    "minimum_grunt_population_ratio_for_recycling": 0.85,
    "stale_grunt_minimum_distance": 2800.0,
    "stale_grunt_behind_dot_threshold": -0.50,
    "max_grunts_recycled_per_evaluation": 4,
    "minimum_seconds_alive_before_recyclable": 5.0,
    "replacement_spawn_distance_min": 1200.0,
    "replacement_spawn_distance_max": 2000.0,
    "spatial_sector_count": 8,
    "prefer_underrepresented_sectors": True,
    "replacement_sector_randomness": 0.25,
    "event_grunt_recycle_protection_seconds": 10.0,
}
for property_name, expected_value in spatial_settings.items():
    actual_value = placed.get_editor_property(property_name)
    mismatch = actual_value != expected_value if not isinstance(expected_value, float) else abs(actual_value - expected_value) > 0.001
    if mismatch:
        placed.set_editor_property(property_name, expected_value)
        map_changed = True
if map_changed and not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Failed to save repaired placed-spawner overrides in " + ACTIVE_LEVEL)

unreal.log("[EnemyPressureV1] Saved BP defaults: phases=%d definitions=%d hard_cap=%d" % (
    len(spawner_cdo.get_editor_property("pressure_phases")),
    len(spawner_cdo.get_editor_property("enemy_spawn_entries")),
    spawner_cdo.get_editor_property("absolute_hard_alive_cap")))
unreal.log("[EnemyPressureV1] Active instance: %s phases=%d definitions=%d map_changed=%s" % (
    placed.get_name(),
    len(placed.get_editor_property("pressure_phases")),
    len(placed.get_editor_property("enemy_spawn_entries")),
    map_changed))
for index, phase in enumerate(placed.get_editor_property("pressure_phases")):
    unreal.log("[EnemyPressureV1] Phase[%d] %s %.0f..%.0f cap=%d intervals=%.2f/%.2f/%.2f events=%s event_interval=%.0f..%.0f" % (
        index,
        phase.get_editor_property("phase_name"),
        phase.get_editor_property("start_time_seconds"),
        phase.get_editor_property("end_time_seconds"),
        phase.get_editor_property("global_max_alive"),
        phase.get_editor_property("normal_spawn_interval"),
        phase.get_editor_property("accelerated_spawn_interval"),
        phase.get_editor_property("emergency_spawn_interval"),
        phase.get_editor_property("events_enabled"),
        phase.get_editor_property("event_interval_min"),
        phase.get_editor_property("event_interval_max")))
    for population in phase.get_editor_property("enemy_population_entries"):
        unreal.log("[EnemyPressureV1]   %s desired=%d max=%d priority=%.2f" % (
            population.get_editor_property("enemy_class").get_name(),
            population.get_editor_property("desired_population"),
            population.get_editor_property("max_population"),
            population.get_editor_property("refill_priority")))
for entry in placed.get_editor_property("enemy_spawn_entries"):
    unreal.log("[EnemyPressureV1] Definition %s mode=%s unlock=%.0f cooldown=%.0f..%.0f health_per_min=%.2f cost=%d legacy_cap=%d" % (
        entry.get_editor_property("enemy_class").get_name(),
        entry.get_editor_property("pressure_spawn_mode"),
        entry.get_editor_property("minimum_run_time"),
        entry.get_editor_property("min_respawn_delay_after_death"),
        entry.get_editor_property("max_respawn_delay_after_death"),
        entry.get_editor_property("health_scaling_per_minute"),
        entry.get_editor_property("spawn_cost"),
        entry.get_editor_property("max_alive_of_this_type")))
for event in placed.get_editor_property("pressure_events"):
    unreal.log("[EnemyPressureV2] Event %s min=%.0f weight=%.1f cooldown=%.0f..%.0f arc=%.0f distance=%.0f..%.0f delay=%.2f first_delay=%.2f overflow=%d ignore_threat_cooldown=%s members=%s" % (
        event.get_editor_property("event_name"), event.get_editor_property("minimum_run_time"), event.get_editor_property("weight"),
        event.get_editor_property("event_cooldown_min"), event.get_editor_property("event_cooldown_max"), event.get_editor_property("spawn_arc_degrees"),
        event.get_editor_property("spawn_distance_min"), event.get_editor_property("spawn_distance_max"), event.get_editor_property("delay_between_members"),
        event.get_editor_property("delay_after_first_member"), event.get_editor_property("event_population_overflow_allowance"), event.get_editor_property("ignore_threat_death_cooldown"),
        [(m.get_editor_property("enemy_class").get_name(), m.get_editor_property("count"), m.get_editor_property("class_overflow_allowance")) for m in event.get_editor_property("enemy_entries")]))
unreal.log("[EnemyPressureV2] Bias enabled=%s chance=%.2f arc=%.0f duration=%.0f..%.0f" % (
    placed.get_editor_property("directional_bias_enabled"), placed.get_editor_property("directional_bias_chance"),
    placed.get_editor_property("directional_bias_arc_degrees"), placed.get_editor_property("directional_bias_duration_min"),
    placed.get_editor_property("directional_bias_duration_max")))
unreal.log("[EnemyPressureV2.2] Spatial enabled=%s class=%s interval=%.1f speed=%.0f ratio=%.2f stale_distance=%.0f behind_dot=%.2f max=%d age=%.0f replacement=%.0f..%.0f sectors=%d randomness=%.2f event_protection=%.0f" % (
    placed.get_editor_property("enable_spatial_pressure_recycling"),
    placed.get_editor_property("spatial_pressure_grunt_class").get_name(),
    placed.get_editor_property("spatial_pressure_evaluation_interval"),
    placed.get_editor_property("minimum_player_speed_for_directional_recycling"),
    placed.get_editor_property("minimum_grunt_population_ratio_for_recycling"),
    placed.get_editor_property("stale_grunt_minimum_distance"),
    placed.get_editor_property("stale_grunt_behind_dot_threshold"),
    placed.get_editor_property("max_grunts_recycled_per_evaluation"),
    placed.get_editor_property("minimum_seconds_alive_before_recyclable"),
    placed.get_editor_property("replacement_spawn_distance_min"),
    placed.get_editor_property("replacement_spawn_distance_max"),
    placed.get_editor_property("spatial_sector_count"),
    placed.get_editor_property("replacement_sector_randomness"),
    placed.get_editor_property("event_grunt_recycle_protection_seconds")))
