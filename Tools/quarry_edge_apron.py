"""Nonblocking ground colour continuity outside the sealed play boundary."""
from pathlib import Path
import unreal as u
def install():
 from import_old_village import import_file
 root=Path(u.Paths.project_dir()).resolve();E=u.EditorAssetLibrary;A=u.get_editor_subsystem(u.EditorActorSubsystem)
 path='/Game/HeavensDivide/Environment/SurvivalMaps/EmberQuarry/Meshes/SM_Quarry_EdgeApron'
 mesh=E.load_asset(path) if E.does_asset_exist(path) else import_file(root/'Art/Environment/SurvivalMaps/Terrain/SM_Quarry_EdgeApron.fbx',path.rsplit('/',1)[0],'SM_Quarry_EdgeApron',True)
 a=next((a for a in A.get_all_level_actors() if a.get_actor_label()=='Quarry edge ground apron'),None)
 if not a:a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,-4))
 a.set_actor_label('Quarry edge ground apron');a.set_folder_path('95_Backdrop');a.set_editor_property('tags',['SurvivalMapBackdrop'])
 c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_material(0,E.load_asset('/Game/HeavensDivide/Environment/SurvivalMaps/EmberQuarry/Materials/M_Ground'))
 c.set_collision_profile_name('NoCollision');c.set_editor_property('can_ever_affect_navigation',False);c.set_cast_shadow(False);c.set_editor_property('affect_distance_field_lighting',False)
 return a
