"""Read live versus template collision geometry without saving changes."""
import unreal as u
import json
from pathlib import Path
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Maps/Lvl_OldVillage')
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
def describe(a):
    result={'scale':str(a.get_actor_scale3d()),'location':str(a.get_actor_location()),'components':[]}
    for c in a.get_components_by_class(u.PrimitiveComponent):
        d={'name':c.get_name(),'profile':str(c.get_collision_profile_name()),'collision':str(c.get_collision_enabled()),
            'world':str(c.get_world_transform()),'relative':str(c.get_editor_property('relative_location')),
            'scale':str(c.get_editor_property('relative_scale3d'))}
        if isinstance(c,u.CapsuleComponent):d.update(radius=c.get_scaled_capsule_radius(),height=c.get_scaled_capsule_half_height())
        result['components'].append(d)
    return result
r={}
for name in ['BP_EnemyGrunt','BP_EnemyGoblinCrawler']:
    cls=u.load_class(None,'/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/'+name+'.'+name+'_C')
    a=u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(cls,u.Vector(-4122,-895,200))
    r[name]={'template':describe(u.get_default_object(cls)),'live':describe(a)}
    a.destroy_actor()
(Path(u.Paths.project_dir())/'Saved/RunFixes/GruntCollision.json').write_text(json.dumps(r,indent=2))
