"""Render real native spell geometry in the saved arena without saving previews.

Editor-world Cursed Ground uses the documented headless camera fallback; gameplay
viewport sizing is verified separately by BossSpellPressure automation.
"""
import hashlib
import json
import struct
import time
import traceback
import uuid
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/BossSpellPressure/Preview'; OUT.mkdir(parents=True,exist_ok=True)
MAP=ROOT/'Content/Maps/Lvl_B1_Arena.umap'
LEVEL=u.get_editor_subsystem(u.LevelEditorSubsystem)
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
STATE=dict(stage='begin',index=0,busy=False,closed=False,handle=None,started=time.monotonic(),temporary=[],views=[])
SPELLS=[('Lances',1),('CursedGround',3),('Eclipse',6),('Summons',4)]

def finish(error=None):
    if STATE['closed']:return
    STATE['closed']=True
    if STATE.get('boss'):STATE['boss'].stop_boss_combat()
    LEVEL.eject_pilot_level_actor()
    for actor in reversed(STATE['temporary']):ACTORS.destroy_actor(actor)
    if STATE['handle']:u.unregister_slate_post_tick_callback(STATE['handle'])
    if error:
        (OUT/'Error.txt').write_text(error);u.log_error(error)
    else:
        assert hashlib.sha256(MAP.read_bytes()).hexdigest()==STATE['map_hash']
        (OUT/'Validation.json').write_text(json.dumps(dict(passed=True,map_unchanged=True,views=STATE['views']),indent=2))
        u.log('BOSS_SPELL_PREVIEW_PASSED')
    u.EditorPythonScripting.set_keep_python_script_alive(False)
    u.SystemLibrary.quit_editor()

def begin():
    STATE['map_hash']=hashlib.sha256(MAP.read_bytes()).hexdigest()
    assert LEVEL.load_level('/Game/Maps/Lvl_B1_Arena')
    boss=next(a for a in ACTORS.get_all_level_actors() if isinstance(a,u.CinderOracleBoss))
    capsule=boss.get_component_by_class(u.CapsuleComponent)
    base=boss.get_actor_location()-u.Vector(0,0,capsule.get_scaled_capsule_half_height())
    player_class=u.EditorAssetLibrary.load_blueprint_class('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai')
    player=ACTORS.spawn_actor_from_class(player_class,base+u.Vector(900,0,96),transient=True)
    STATE['temporary'].append(player)
    focus=base+u.Vector(900,0,0)
    location=focus+u.Vector(-900,0,1400)
    camera=ACTORS.spawn_actor_from_class(u.CameraActor,location,u.MathLibrary.find_look_at_rotation(location,focus),transient=True)
    STATE['temporary'].append(camera)
    cc=camera.get_component_by_class(u.CameraComponent)
    cc.set_editor_property('field_of_view',70.)
    cc.set_editor_property('constrain_aspect_ratio',False)
    cc.set_editor_property('post_process_blend_weight',0.)
    STATE.update(boss=boss,player=player,camera=camera)
    boss.set_target(player)
    LEVEL.editor_set_game_view(True);LEVEL.pilot_level_actor(camera)
    u.AutomationLibrary.finish_loading_before_screenshot()
    STATE['stage']='spell'

def spell():
    boss=STATE['boss'];boss.stop_boss_combat();boss.start_boss_combat()
    name,ability=SPELLS[STATE['index']];boss.cast_ability(ability)
    hazards=[a for a in ACTORS.get_all_level_actors() if isinstance(a,u.EncounterHazard)]
    assert hazards,(name,'Native cast did not create hazards')
    for hazard in hazards:
        surface=hazard.get_editor_property('ground_surface');surface.set_visibility(True)
        surface.get_material(0).set_scalar_parameter_value('FillAmount',.7)
    summons=[a for a in ACTORS.get_all_level_actors() if isinstance(a,u.EnemyBase) and a.get_owner()==boss]
    STATE['views'].append(dict(spell=name,hazards=len(hazards),summons=len(summons)))
    STATE.update(stage='warm',at=time.monotonic())

def capture():
    name,_=SPELLS[STATE['index']]
    pending=OUT/('_'+name+'_'+uuid.uuid4().hex+'.png')
    task=u.AutomationLibrary.take_high_res_screenshot(1600,900,str(pending),camera=STATE['camera'])
    assert task and task.is_valid_task()
    STATE.update(stage='capture',at=time.monotonic(),pending=pending,task=task)

def tick(delta):
    if STATE['busy'] or STATE['closed']:return
    STATE['busy']=True
    try:
        if time.monotonic()-STATE['started']>480:raise TimeoutError('Boss spell preview timed out')
        if STATE['stage']=='begin':begin()
        elif STATE['stage']=='spell':spell()
        elif STATE['stage']=='warm':
            LEVEL.editor_invalidate_viewports()
            if time.monotonic()-STATE['at']>6:capture()
        elif STATE['stage']=='capture':
            if time.monotonic()-STATE['at']>90:raise TimeoutError('Screenshot did not finish')
            if STATE['task'].is_task_done() and STATE['pending'].exists():
                raw=STATE['pending'].read_bytes();assert raw[:8]==b'\x89PNG\r\n\x1a\n'
                assert struct.unpack('>II',raw[16:24])==(1600,900)
                name,_=SPELLS[STATE['index']];STATE['pending'].replace(OUT/(name+'.png'))
                STATE['index']+=1
                if STATE['index']==len(SPELLS):finish()
                else:STATE['stage']='spell'
    except Exception:finish(traceback.format_exc())
    finally:STATE['busy']=False

u.EditorPythonScripting.set_keep_python_script_alive(True)
STATE['handle']=u.register_slate_post_tick_callback(tick)
