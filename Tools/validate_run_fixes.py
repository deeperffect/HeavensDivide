"""Audio-enabled PIE validation of all four saved maps. Never saves map changes."""
from pathlib import Path
import json
import time
import traceback
import unreal as u

root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/RunFixes/Maps.json'
maps=globals().get('MAPS',['Lvl_OldVillage','Lvl_MoonlitForest','Lvl_SunkenSanctuary','Lvl_EmberQuarry'])
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
S={'i':0,'stage':'load','start':time.monotonic()};R={'passed':False,'maps':{}}
def finish():
    out.write_text(json.dumps(R,indent=2));L.editor_request_end_play()
    u.unregister_slate_post_tick_callback(S['handle']);u.SystemLibrary.quit_editor()
def tick(dt):
    if S.get('busy'):return
    S['busy']=True
    try:
        now=time.monotonic();assert now-S['start']<1000,'PIE timeout'
        name=maps[S['i']]
        if S['stage']=='load':
            assert L.load_level('/Game/Maps/'+name);L.editor_request_begin_play();S.update(stage='play',at=now);return
        if S['stage']=='ended':
            if L.is_in_play_in_editor():return
            S['i']+=1
            if S['i']==len(maps):R['passed']=True;finish();return
            S['stage']='load';return
        w=u.EditorLevelLibrary.get_game_world()
        if not w:return
        for a in u.GameplayStatics.get_all_actors_of_class(w,u.CharacterBase):
            h=a.get_component_by_class(u.HealthComponent)
            if h:h.set_damage_enabled(False)
        sp=u.GameplayStatics.get_all_actors_of_class(w,u.EnemySpawner)[0]
        if S['stage']=='play':
            if sp.get_run_time_seconds()<8:return
            enemies=u.GameplayStatics.get_all_actors_of_class(w,u.EnemyBase)
            assert len(enemies)>0,name+' enemies never spawned'
            player=u.GameplayStatics.get_player_pawn(w,0)
            # Check immediately after synchronous spawn, before movement can
            # take a capsule to a nav-edge steering position.
            moved_positions=[list(a.get_actor_location().to_tuple()) for a in enemies]
            sp.set_spawning_enabled(False)
            for enemy in enemies:enemy.destroy_actor()
            sp.set_spawning_enabled(True)
            enemies=[]
            for attempt in range(36):
                enemy=sp.spawn_enemy()
                if enemy:enemies.append(enemy)
            sp.set_spawning_enabled(False)
            assert len(enemies)>=24,'Too many safe spawn candidates were rejected: '+str(len(enemies))
            classes={a.get_class().get_name() for a in enemies}
            assert 'BP_EnemyGrunt_C' in classes,'Basic enemy population missing'
            assert 'BP_EnemyGoblinCrawler_C' in classes,'Small enemy population missing'
            reachable=0;bad=[]
            for enemy in enemies:
                p=enemy.get_actor_location()
                q=u.NavigationSystemV1.project_point_to_navigation(w,p,None,None,query_extent=u.Vector(120,120,220))
                route=u.NavigationSystemV1.find_path_to_location_synchronously(w,player.get_actor_location(),p,None,None) if q else None
                if q and route and route.is_valid() and not route.is_partial():reachable+=1
                else:bad.append({'position':list(p.to_tuple()),'projected':list(q.to_tuple()) if q else None,'partial':route.is_partial() if route else None})
            assert not bad,'Enemy off connected navigation: '+str(bad)
            chest_checked=False
            if name=='Lvl_OldVillage':
                victim=enemies[0]
                for enemy in enemies[1:]:enemy.destroy_actor()
                house=next(a for a in u.GameplayStatics.get_all_actors_of_class(w,u.StaticMeshActor) if a.get_actor_label().startswith('ThatchedCottage'))
                victim.set_actor_location(house.get_actor_location()+u.Vector(0,0,100),False,True)
                assert u.SwapVFXSetupLibrary.set_property_text(victim,'DropCategory','Elite')
                assert u.SwapVFXSetupLibrary.set_property_text(victim,'XPReward','1')
                victim.get_component_by_class(u.HealthComponent).apply_damage(1000000.)
                chests=u.GameplayStatics.get_all_actors_of_class(w,u.EliteRewardChest)
                assert chests,'Elite killed inside solid scenery lost its chest'
                for chest in chests:
                    assert sp.validate_objective_location(chest.get_actor_location(),90.,180.),'Chest spawned inside solid scenery'
                chest_checked=True
            # Remove transient crowds to test authored marker clearances only.
            sp.set_spawning_enabled(False)
            for enemy in enemies:enemy.destroy_actor()
            valid=[];blocked=[]
            for point in u.GameplayStatics.get_all_actors_of_class(w,u.ObjectiveSpawnPoint):
                found=sp.validate_objective_location(point.get_actor_location(),220.,400.)
                if found:valid.append(point.get_name())
                else:blocked.append(point.get_name())
            assert valid,name+' has no clear objective marker'
            director=u.GameplayStatics.get_all_actors_of_class(w,u.RunObjectiveDirector)[0]
            director.debug_spawn_blood_shrine()
            shrines=u.GameplayStatics.get_all_actors_of_class(w,u.BloodShrine)
            assert shrines,name+' shrine failed to spawn'
            music=[a for a in u.GameplayStatics.get_all_actors_of_class(w,u.AmbientSound) if a.actor_has_tag('OldVillageMusic')]
            assert len(music)==1
            audio=music[0].get_component_by_class(u.AudioComponent);wave=audio.get_editor_property('sound')
            assert wave.get_editor_property('looping') and audio.is_playing()
            R['maps'][name]={'enemies_reachable':reachable,'enemy_classes':sorted(classes),'run_seconds':sp.get_run_time_seconds(),'elite_chest_relocated_safely':chest_checked,
                'clear_objective_markers':valid,'blocked_markers_safely_skipped':blocked,
                'shrine_locations':[list(a.get_actor_location().to_tuple()) for a in shrines],
                'music_auto_started':True}
            audio.stop();audio.play(wave.get_editor_property('duration')-.5)
            S.update(stage='loop',at=now,audio=audio);return
        if S['stage']=='loop' and now-S['at']>3:
            assert S['audio'].is_playing(),'Music stopped at loop boundary'
            R['maps'][name]['music_crossed_loop_boundary']=True
            S['audio'].set_volume_multiplier(0.);S.update(stage='silent',at=now);return
        if S['stage']=='silent' and now-S['at']>1:
            assert S['audio'].is_playing(),'Muted music lost playback'
            R['maps'][name]['music_survives_mute']=True
            out.write_text(json.dumps(R,indent=2));L.editor_request_end_play();S.update(stage='ended')
    except Exception:
        R['error']=traceback.format_exc();u.log_error(R['error']);finish()
    finally:S['busy']=False
u.EditorPythonScripting.set_keep_python_script_alive(True)
S['handle']=u.register_slate_post_tick_callback(tick)
