"""Read-only saved-map checks: collision, supplied assets, ground, and main routes."""
import json
from pathlib import Path
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/OldVillage'
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
world=globals().get('PLAY_WORLD')
if not world:
    raise RuntimeError('Run through review_old_village.py: these traces require a PIE physics world.')
actors=u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor)
floor=next(a for a in actors if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_OV_Island')
c=floor.static_mesh_component
report={'ground_channel':str(c.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND)),'ground':[],'routes':[]}
assert c.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_GROUND)==u.CollisionResponseType.ECR_BLOCK
scale=json.loads((ROOT/'Tools/old_village_layout.json').read_text()).get('layout_scale',1)
def point(e,n,z):return u.Vector(n*scale,e*scale,z)
locations=[('Spawn',-200,-1550),('West road',-2300,3300),('Pond garden',-3500,3500),('Lower garden',-4200,-1000),('Shrine court',700,3200),('East road',5800,-1000),('Training',0,-3800),('Waterfront road',-3000,-3600),('South road',1000,-5700)]
for name,e,n in locations:
    hit=u.SystemLibrary.line_trace_single(floor,point(e,n,500),point(e,n,-500),u.TraceTypeQuery.ECC_ENEMY_GROUND,False,[],u.DrawDebugTrace.NONE,False)
    report['ground'].append({'name':name,'hit':bool(hit),'result':str(hit)})
routes=[('West avenue',(-2300,1600),(-2300,6000)),('East exit',(3900,2500),(6000,4300)),('Southeast route',(500,-560),(2800,-2400)),('Workshop road',(2800,-2400),(5200,-4700)),('South loop',(-1700,-5700),(3500,-5700)),('Garden crosspath',(-5100,1300),(-2900,1300)),('Training approach',(-200,-1700),(-200,-3800))]
for name,p1,p2 in routes:
    characters=u.GameplayStatics.get_all_actors_of_class(world,u.Character)
    hit=u.SystemLibrary.capsule_trace_single_by_profile(floor,point(*p1,110),point(*p2,110),60,90,'Pawn',False,characters,u.DrawDebugTrace.NONE,False)
    report['routes'].append({'name':name,'clear':not bool(hit),'result':str(hit)})
report['passed']=all(x['hit'] for x in report['ground']) and all(x['clear'] for x in report['routes'])
report['boundaries']=[]
for name,p1,p2 in [('West',(-6000,0),(-7000,0)),('North',(0,6000),(0,7000)),('East',(6000,0),(7000,0)),('South',(0,-6000),(0,-7000)),('Waterfront',(-4000,-4000),(-5000,-5000)),('Pond',(-4600,3000),(-4600,3600))]:
    hit=u.SystemLibrary.capsule_trace_single_by_profile(floor,point(*p1,110),point(*p2,110),60,90,'Pawn',False,characters,u.DrawDebugTrace.NONE,False)
    report['boundaries'].append({'name':name,'blocked':bool(hit)})
report['passed']=report['passed'] and all(x['blocked'] for x in report['boundaries'])
(OUT/'CollisionValidation.json').write_text(json.dumps(report,indent=2))
u.log('OLD_VILLAGE_COLLISION_REPORT '+str(report['passed']))
