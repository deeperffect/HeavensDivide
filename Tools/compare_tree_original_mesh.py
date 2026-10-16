"""Unsaved comparison with the complete original tree LOD chain."""
import unreal as u
for a in A.get_all_level_actors():
 for c in a.get_components_by_class(u.StaticMeshComponent):
  if not c.static_mesh or not c.static_mesh.get_name().startswith('Tree_'):continue
  name='_'.join(c.static_mesh.get_name().split('_')[:3])
  m=u.load_asset('/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Trees/'+name)
  c.set_static_mesh(m);c.set_editor_property('override_materials',[])
