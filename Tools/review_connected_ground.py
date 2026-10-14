"""Capture saved connected ground, plus a temporary 4x-area world-scale proof."""
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir())
source=(root/'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
changes={
    'OUTPUT = ROOT / "Saved/GroundReferencePass"':'OUTPUT = ROOT / "Saved/ConnectedGround"',
    '"Tools/apply_reference_ground.py"':'"Tools/apply_connected_ground.py"',
    'if not LEVEL.load_level(MAP):':'if not apply and not LEVEL.load_level(MAP):',
    'if map_hashes() != initial_hashes:':'if not apply and map_hashes() != initial_hashes:',
    'STATE["map_hashes"] = initial_hashes':'STATE["map_hashes"] = map_hashes()',
    '/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground':'/Game/HeavensDivide/Materials/ConnectedGround/MI_ConnectedGround',
    '/Game/HeavensDivide/Materials/EnvironmentPolish/M_Lvl1_Ground':'/Game/HeavensDivide/Materials/ConnectedGround/M_ConnectedGround',
    'for name in ("GrassTexture", "DirtTexture", "StoneTexture"):':'for name in ("GroundTexture",):',
    '"GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 4000':'"GroundArtCloseup", "kind": "ground", "focus": [2200, 800, ground_z], "width": 1800',
    'STATE["report"]["material"] = inspect_material()':'STATE["report"]["material"] = inspect_material()\n    STATE["report"]["scene"] = runpy.run_path(str(ROOT/"Tools/inspect_connected_ground.py"))["REPORT"]',
}
for old,new in changes.items():
    assert source.count(old)==1,'Shared review harness changed: '+old
    source=source.replace(old,new)
source=source.replace('def cleanup_view():','''def cleanup_view():
    if STATE.get('review_floor_scale') is not None:
        STATE['floor'].set_actor_scale3d(STATE.pop('review_floor_scale'))''')
source=source.replace('    STATE["report"] = {"passed": False,','''    STATE["specs"] += [
        {'name':'GameplayBoundary','kind':'gameplay','focus':[0,4500,start.z],'size':(1600,900)},
        {'name':'ExpandedFloorCloseup','kind':'ground','focus':[2200,800,ground_z],'width':1800,'size':(1600,1000),'expand_floor':True}]
    STATE["report"] = {"passed": False,''')
source=source.replace('    STATE["view"] = spec','''    STATE["view"] = spec
    if spec.get('expand_floor'):
        original=STATE['floor'].get_actor_scale3d()
        STATE['review_floor_scale']=original
        STATE['floor'].set_actor_scale3d(u.Vector(original.x*2,original.y*2,original.z))
        spec['temporary_area_multiplier']=4
    if spec['kind']=='gameplay':
        hero_class=u.EditorAssetLibrary.load_blueprint_class('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
        hero=spawn(hero_class,u.Vector(spec['focus'][0],spec['focus'][1],100))
        capsule=hero.get_component_by_class(u.CapsuleComponent)
        hero.set_actor_location(u.Vector(spec['focus'][0],spec['focus'][1],STATE['floor'].get_actor_location().z+capsule.get_scaled_capsule_half_height()),False,False)
        hero.set_actor_hidden_in_game(False)
        spec['scale_reference']={'asset':hero_class.get_path_name(),'capsule_half_height':capsule.get_scaled_capsule_half_height()}
''')
# A tune command only edits this pass's material instance, never unrelated assets.
source=source.replace('    command = json.loads(COMMAND.read_text(encoding="utf-8-sig"))','''    try:
        command = json.loads(COMMAND.read_text(encoding="utf-8-sig"))
    except PermissionError:
        return  # A Windows writer may still hold the newly published command.
''')
source=source.replace('    if action not in ("apply", "capture", "quit"):','''    if action=='tune':
        COMMAND.unlink()
        runpy.run_path(str(ROOT/'Tools/tune_connected_ground.py'))
        begin_batch(command.get('folder','Tuned'),False)
        return
    if action not in ("apply", "capture", "quit"):''')
source=source.replace('            command_line = u.SystemLibrary.get_command_line()', '''            command_line = u.SystemLibrary.get_command_line()
            if '-GroundTuneFirst' in command_line:
                runpy.run_path(str(ROOT/'Tools/tune_connected_ground.py'))''')
exec(compile(source,str(root/'Tools/review_reference_ground.py'),'exec'),globals())
