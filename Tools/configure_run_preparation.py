"""Configure the survival map after running -run=RunPreparationReview.

Backs up changed assets under Saved/RunPreparation/Backup. Re-running updates the
same named actors; it does not add duplicate bounds or objective spawn points.
"""
import csv
import json
import math
import shutil
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
output = root / 'Saved/RunPreparation'
backup = output / 'Backup'
for relative in ('Maps/Lvl_B1_Lvl1.umap', 'HeavensDivide/Blueprints/Objectives/BP_RunObjectiveDirector.uasset'):
    source = root / 'Content' / relative
    target = backup / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        shutil.copy2(source, target)

with (output / 'LevelGround.csv').open() as stream:
    candidates = [tuple(float(row[k]) for k in ('x','y','z')) for row in csv.DictReader(stream) if row['reachable'] == '1']
assert candidates, 'Run the level collision survey before configuring the map.'

world = u.EditorLoadingAndSavingUtils.load_map('/Game/Maps/Lvl_B1_Lvl1')
assert world
editor = u.get_editor_subsystem(u.EditorActorSubsystem)
actors = editor.get_all_level_actors()
start = next(a.get_actor_location() for a in actors if isinstance(a,u.PlayerStart))
gate = next(a.get_actor_location() for a in actors if isinstance(a,u.BossToriiGate))
distance = lambda p,q: math.hypot(p[0]-q[0],p[1]-q[1])
origin = (start.x,start.y)
candidates = [p for p in candidates if 1200 <= distance(p,origin) <= 4600 and distance(p,(gate.x,gate.y)) >= 1200]
chosen = []
for i in range(8):
    angle = i * math.tau / 8
    target = (start.x + 2900 * math.cos(angle),start.y + 2900 * math.sin(angle))
    options = [p for p in candidates if all(distance(p,q)>=1000 for q in chosen)]
    assert options, 'Not enough separated, reachable positions.'
    chosen.append(min(options,key=lambda p:distance(p,target)))

bounds = [a for a in actors if isinstance(a,u.MinimapBounds)]
assert len(bounds) <= 1, 'Resolve duplicate minimap bounds before applying this script.'
area = bounds[0] if bounds else editor.spawn_actor_from_class(u.MinimapBounds,u.Vector(0,0,0))
area.set_actor_label('Bloodshift Minimap Bounds')
combat_area=next((a for a in actors if isinstance(a,u.EnemySpawnArea)),None)
if combat_area:
    box=combat_area.get_component_by_class(u.BoxComponent)
    area.set_actor_transform(box.get_world_transform(),False,False)
    area.get_editor_property('playable_area').set_box_extent(box.get_unscaled_box_extent(),False)
else:
    area.set_actor_location(u.Vector(0,0,0),False,False)
    area.get_editor_property('playable_area').set_box_extent(u.Vector(7560,7560,1200),False)
for i,p in enumerate(chosen):
    label = f'Bloodshift Objective Spawn {i+1:02d}'
    actor = next((a for a in actors if a.get_actor_label()==label),None)
    position = u.Vector(p[0],p[1],p[2]+5)
    if not actor:
        actor = editor.spawn_actor_from_class(u.ObjectiveSpawnPoint,position)
    actor.set_actor_label(label)
    actor.set_actor_location(position,False,False)

bp = u.load_asset('/Game/HeavensDivide/Blueprints/Objectives/BP_RunObjectiveDirector')
cdo = u.get_default_object(bp.generated_class())
directors = [a for a in actors if isinstance(a,u.RunObjectiveDirector)]
assert len(directors)==1
for obj in [cdo,*directors]:
    obj.set_editor_property('blood_shrine_spawn_time',180.0)
    obj.set_editor_property('second_blood_shrine_spawn_time',300.0)
    obj.set_editor_property('bEnableCharacterTrials',False)
# Technique and Twin Soul trials are retired; keep legacy class references empty.
    obj.set_editor_property('SamuraiTrialClass',None)
    obj.set_editor_property('NinjaTrialClass',None)
    obj.set_editor_property('TwinSoulTrialClass',None)
# Leave the boss-gate schedule untouched.
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
assert u.EditorLoadingAndSavingUtils.save_map(world,'/Game/Maps/Lvl_B1_Lvl1')
report = {'blood_shrines_seconds':[180,300],'character_trials_enabled':False,
          'minimap_half_extent':[7560,7560],'objective_points':chosen}
(output/'Configuration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('RUN_PREPARATION_CONFIGURED '+json.dumps(report))
