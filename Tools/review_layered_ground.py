"""Capture and verify the layered grass arena; -GroundApplyFirst applies the layered assets first."""
from pathlib import Path
import unreal as u
root = Path(u.Paths.project_dir())
source = (root / 'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
changes = {
    'OUTPUT = ROOT / "Saved/GroundReferencePass"': 'OUTPUT = ROOT / "Saved/LayeredGround"',
    '"Tools/apply_reference_ground.py"': '"Tools/apply_layered_ground.py"',
    'if not LEVEL.load_level(MAP):': 'if not apply and not LEVEL.load_level(MAP):',
    'if map_hashes() != initial_hashes:': 'if not apply and map_hashes() != initial_hashes:',
    'STATE["map_hashes"] = initial_hashes': 'STATE["map_hashes"] = map_hashes()',
    'actor != STATE["floor"] and actor.get_components_by_class(u.MeshComponent)':
        'actor != STATE["floor"] and actor.get_actor_label() != "GroundRockStamps" and actor.get_components_by_class(u.MeshComponent)',
    '"GameplayMeadowLoop", "kind": "gameplay", "focus": [1500, 3000, start.z]':
        '"GameplayBoundary", "kind": "gameplay", "focus": [0, 4500, start.z]',
    '"GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 4000':
        '"GrassCloseup", "kind": "ground", "focus": [1500, -1550, ground_z], "width": 1800',
    'STATE["report"]["material"] = inspect_material()':
        'STATE["report"]["material"] = inspect_material()\n    STATE["report"]["scene"] = runpy.run_path(str(ROOT / "Tools/inspect_layered_ground.py"))["REPORT"]',
}
changes['/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground']='/Game/HeavensDivide/Materials/LayeredGround/MI_Lvl1_Ground'
changes['/Game/HeavensDivide/Materials/EnvironmentPolish/M_Lvl1_Ground']='/Game/HeavensDivide/Materials/LayeredGround/M_Lvl1_Ground'
for old, new in changes.items():
    assert source.count(old) == 1, 'Shared review harness changed: ' + old
    source = source.replace(old, new)
# Additional views and a real-size transient Samurai make scale review concrete.
source=source.replace('    STATE["report"] = {"passed": False,', '''    STATE["specs"] += [
        {"name":"GameplayMeadow","kind":"gameplay","focus":[1500,3000,start.z],"size":(1600,900)},
        {"name":"RockCloseup","kind":"ground","focus":[1800,850,ground_z],"width":2600,"size":(1600,1000)}]
    STATE["report"] = {"passed": False,''')
source=source.replace('    STATE["view"] = spec', '''    STATE["view"] = spec
    if spec['kind']=='gameplay':
        hero_class=u.EditorAssetLibrary.load_blueprint_class('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
        hero=spawn(hero_class,u.Vector(spec['focus'][0],spec['focus'][1],100))
        capsule=hero.get_component_by_class(u.CapsuleComponent)
        hero.set_actor_location(u.Vector(spec['focus'][0],spec['focus'][1],STATE['floor'].get_actor_location().z+capsule.get_scaled_capsule_half_height()),False,False)
        hero.set_actor_hidden_in_game(False)
        STATE['view']['scale_reference']={'asset':hero_class.get_path_name(),'capsule_half_height':capsule.get_scaled_capsule_half_height()}
''')
exec(compile(source, str(root / 'Tools/review_reference_ground.py'), 'exec'), globals())
