"""Read-only check that Level 1 automatically loads its polished lighting.

Run through Unreal Editor Python. Writes Saved/EnvironmentPolish/LightingConnection.json.
This checks current lighting without comparing old gameplay actor transforms.
"""
import hashlib
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/EnvironmentPolish'
OUT.mkdir(parents=True, exist_ok=True)
FILES = [ROOT / 'Content/Maps/Lvl_B1_Lvl1.umap',
         ROOT / 'Content/Maps/Lvl_B1_Lvl1_Lighting.umap']
before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in FILES}
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert level.load_level('/Game/Maps/Lvl_B1_Lvl1')
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
stream = u.GameplayStatics.get_streaming_level(world, '/Game/Maps/Lvl_B1_Lvl1_Lighting')
assert isinstance(stream, u.LevelStreamingAlwaysLoaded), 'Lighting must be an Always Loaded sublevel'
assert stream.get_loaded_level() and stream.is_level_visible(), 'Lighting sublevel is not loaded/visible'
all_actors = actors.get_all_level_actors()
sun = next(a for a in all_actors if isinstance(a, u.DirectionalLight))
assert sun.get_path_name().startswith('/Game/Maps/Lvl_B1_Lvl1_Lighting.')
sc = sun.get_component_by_class(u.DirectionalLightComponent)
assert abs(sc.get_editor_property('intensity') - 7.5) < .01
assert abs(sun.get_actor_rotation().pitch + 43) < .01
fog = next(a for a in all_actors if isinstance(a, u.ExponentialHeightFog))
fc = fog.get_component_by_class(u.ExponentialHeightFogComponent)
assert fc.get_editor_property('fog_max_opacity') < .13
grades = [a for a in all_actors if isinstance(a, u.PostProcessVolume)
          and a.get_editor_property('enabled') and a.get_editor_property('unbound')]
assert len(grades) == 1 and grades[0].get_actor_label() == 'Bloodshift Combat Grade'
grade = grades[0]
settings = grade.get_editor_property('settings')
assert grade.get_editor_property('priority') == 50 and grade.get_editor_property('blend_weight') == 1
assert settings.get_editor_property('auto_exposure_min_brightness') == settings.get_editor_property('auto_exposure_max_brightness') == 1
assert settings.get_editor_property('motion_blur_amount') == 0
after = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in FILES}
assert before == after, 'Read-only lighting inspection changed a map'
report = dict(passed=True, main_map='/Game/Maps/Lvl_B1_Lvl1',
              lighting_map='/Game/Maps/Lvl_B1_Lvl1_Lighting',
              streaming_class=stream.get_class().get_name(), loaded=True, visible=True,
              sun=sun.get_path_name(), sun_intensity=sc.get_editor_property('intensity'),
              sun_rotation=str(sun.get_actor_rotation()),
              fog_max_opacity=fc.get_editor_property('fog_max_opacity'),
              grade=grade.get_path_name(), grade_priority=50, fixed_exposure=True,
              bloom=settings.get_editor_property('bloom_intensity'),
              vignette=settings.get_editor_property('vignette_intensity'),
              maps_unchanged=True, map_hashes=after)
(OUT / 'LightingConnection.json').write_text(json.dumps(report, indent=2))
u.log('LEVEL1_LIGHTING_CONNECTION_PASSED ' + json.dumps(report))
