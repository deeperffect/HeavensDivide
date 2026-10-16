import json,unreal as u
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
D={}
for a in A.get_all_level_actors():
 for c in a.get_components_by_class(u.StaticMeshComponent):
  m=c.static_mesh
  if not m or not m.get_name().startswith('Tree_'):continue
  settings=SM.get_nanite_settings(m)
  D[m.get_path_name()]=str(settings)
  c.set_forced_lod_model(1)
(ROOT/'Saved/MapSceneryAudit/TreeNanite.json').write_text(json.dumps(D,indent=2))
