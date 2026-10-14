"""Render all ten saved presets through the gameplay camera, without saving.
-GroundApplyFirst imports/applies assets before review. -ArenaQuick captures
just the default preset, useful for tuning. Omit it for the full comparison.
"""
from pathlib import Path
import json
import unreal as u
root=Path(u.Paths.project_dir())
source=(root/'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
changes={
 'if not LEVEL.load_level(MAP):':'if not apply and not LEVEL.load_level(MAP):',
 'OUTPUT = ROOT / "Saved/GroundReferencePass"':'OUTPUT = ROOT / "Saved/ArenaVariations"',
 '"Tools/apply_reference_ground.py"':'"Tools/apply_arena_variations.py"',
 'MATERIAL = "/Game/HeavensDivide/Materials/EnvironmentPolish/MI_Lvl1_Ground"':'MATERIAL = "/Game/HeavensDivide/Materials/ArenaVariations/MI_01_VerdantOchre"',
 'PARENT = "/Game/HeavensDivide/Materials/EnvironmentPolish/M_Lvl1_Ground"':'PARENT = "/Game/HeavensDivide/Materials/ArenaVariations/M_ArenaSurface"',
 'for name in ("GrassTexture", "DirtTexture", "StoneTexture"):':'for name in ("GroundTexture",):',
 'if map_hashes() != initial_hashes:':'if not apply and map_hashes() != initial_hashes:',
 'STATE["map_hashes"] = initial_hashes':'STATE["map_hashes"] = map_hashes()',
 'actor != STATE["floor"] and actor.get_components_by_class(u.MeshComponent)':'actor != STATE["floor"] and actor.get_actor_label() != "TerracottaArenaRim" and actor.get_components_by_class(u.MeshComponent)',
}
for old,new in changes.items():
    assert source.count(old)==1,'Shared review changed: '+old
    source=source.replace(old,new)
start=source.index('    STATE["specs"] = [')
end=source.index('    STATE["report"] = ',start)
source=source[:start]+'''    STATE["saved_material"] = STATE["floor"].static_mesh_component.get_material(0).get_path_name().split(".")[0]
    globals()['MATERIAL']=STATE['saved_material']
    config=json.loads((ROOT/'Art/ArenaVariations/Variants.json').read_text())
    choices=config['variants'][:1] if '-ArenaQuick' in u.SystemLibrary.get_command_line() and STATE['folder']=='FirstReview' else config['variants']
    STATE["specs"] = []
    for v in choices:
        STATE["specs"] += [
            {"name":v['id']+'_Gameplay',"variant":v['id'],"kind":"gameplay","focus":vector(start),"size":(1920,1080)},
            {"name":v['id']+'_Overview',"variant":v['id'],"kind":"ground","focus":[0,0,ground_z],"width":12400,"size":(1400,1400)}]
    STATE["specs"] += [{"name":"Default_StoneRim","variant":config['default'],"kind":"gameplay","focus":[0,4500,start.z],"size":(1920,1080)}]
'''+source[end:]
source=source.replace('STATE["report"]["material"] = inspect_material()', 'STATE["report"]["material"] = inspect_material()\n    STATE["report"]["scene"] = runpy.run_path(str(ROOT/"Tools/inspect_arena_variations.py"))["REPORT"]')
# Per-view selection: camera, lighting and shader are identical for all variants.
needle='    STATE["view"] = spec'
source=source.replace(needle,needle+'''
    global MATERIAL
    runpy.run_path(str(ROOT/'Tools/select_arena_variation.py'),init_globals={'VARIANT_ID':spec['variant'],'SAVE':False})
    MATERIAL='/Game/HeavensDivide/Materials/ArenaVariations/MI_'+spec['variant']
    STATE['view']['surface']=inspect_material()
    if spec['kind']=='gameplay':
        hero_class=u.EditorAssetLibrary.load_blueprint_class('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
        hero=spawn(hero_class,u.Vector(spec['focus'][0],spec['focus'][1],100),u.Rotator(pitch=0,yaw=0,roll=0))
        capsule=hero.get_component_by_class(u.CapsuleComponent)
        hero.set_actor_location(u.Vector(spec['focus'][0],spec['focus'][1],STATE['floor'].get_actor_location().z+capsule.get_scaled_capsule_half_height()),False,False)
        hero.set_actor_hidden_in_game(False)
        STATE['view']['scale_reference']={'asset':hero_class.get_path_name(),'capsule_half_height':capsule.get_scaled_capsule_half_height()}
    u.AutomationLibrary.finish_loading_before_screenshot()
''')
# Restore selection before final material readback and cleanup; no map writes.
needle='def finish_batch():\n    cleanup_view()'
source=source.replace(needle,needle+'''
    global MATERIAL
    saved_id=STATE['saved_material'].rsplit('/MI_',1)[1]
    runpy.run_path(str(ROOT/'Tools/select_arena_variation.py'),init_globals={'VARIANT_ID':saved_id,'SAVE':False})
    MATERIAL=STATE['saved_material']
''')
exec(compile(source,str(root/'Tools/review_reference_ground.py'),'exec'),globals())
