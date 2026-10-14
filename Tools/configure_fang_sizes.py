"""Use the same authored size for Returning Fang and Eclipse kunai. Assets only."""
from pathlib import Path
import json
import shutil
import unreal as u

root=Path(u.Paths.project_dir())
out=root/'Saved/FangSizes'
backup=out/'Backups'
backup.mkdir(parents=True,exist_ok=True)
bp_path='/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_NinjaProjectileFang'
combo_path='/Game/HeavensDivide/Abilities/Combos/DA_Combo_HeavensDivide'
validate='-ValidateFangSizes' in u.SystemLibrary.get_command_line()
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
def objects(bp):
    result=[]
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data=u.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data,bp)
        if obj:result.append(obj)
    return result

def vector(v):return [v.x,v.y,v.z]
def snapshot(bp):
    return {x.get_name():{'scale':vector(x.get_editor_property('relative_scale3d')),'location':vector(x.get_editor_property('relative_location'))}
            for x in objects(bp) if isinstance(x,u.SceneComponent)}

bp=u.load_asset(bp_path)
combo=u.load_asset(combo_path)
assert bp and combo
before=snapshot(bp)
eclipse_before=combo.get_editor_property('fang_presentation_scale')
if not validate:
    for path in (bp_path,combo_path):
        file=root/'Content'/(path.removeprefix('/Game/')+'.uasset')
        dest=backup/file.name
        if not dest.exists():shutil.copy2(file,dest)
    # Returning Fang's actor is only presentation; native ANinjaBuildProjectile
    # owns damage and collision. Eclipse sets this same world scale explicitly.
    component=next(x for x in objects(bp) if x.get_name()=='CollisionComponent')
    component.set_editor_property('relative_scale3d',u.Vector(1,1,1))
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert 'ERROR' not in str(bp.get_editor_property('status')).upper()
    combo.set_editor_property('fang_presentation_scale',1.0)
    assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    assert u.EditorAssetLibrary.save_loaded_asset(combo,False)
after=snapshot(bp)
assert after['CollisionComponent']['scale']==[1.0,1.0,1.0],after
assert combo.get_editor_property('fang_presentation_scale')==1.0
for name,values in before.items():
    if name!='CollisionComponent':assert after[name]==values,(name,values,after[name])

# Inspect actual presentation actors after Blueprint construction. All stance,
# spectral, assist and clone Fangs share this class; Eclipse overrides its scale.
actors_sub=u.get_editor_subsystem(u.EditorActorSubsystem)
actors=[]
samples={}
try:
    for name in ('stance','eclipse'):
        actor=actors_sub.spawn_actor_from_class(bp.generated_class(),u.Vector(0,0,-10000),u.Rotator(),True)
        assert actor
        actors.append(actor)
        actor.set_actor_enable_collision(False)
        if name=='eclipse':actor.set_actor_scale3d(u.Vector(1,1,1))
        mesh=actor.get_component_by_class(u.StaticMeshComponent)
        samples[name]={'actor_scale':vector(actor.get_actor_scale3d()),'mesh_scale':vector(mesh.get_world_scale())}
    assert all(v==samples['stance'] for v in samples.values()),samples
finally:
    for actor in actors:actors_sub.destroy_actor(actor)
report={'before_components':before,'after_components':after,'eclipse_before':eclipse_before,'eclipse_after':combo.get_editor_property('fang_presentation_scale'),'spawned_presentations':samples,'saved_assets':[bp_path,combo_path],'mode':'validate' if validate else 'configure'}
(out/('ReloadValidation.json' if validate else 'Changes.json')).write_text(json.dumps(report,indent=2))
u.log('FANG_SIZES_OK '+json.dumps(samples))
