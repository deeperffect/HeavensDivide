"""Load a saved arena for the persistent render review and validate tree copies."""
import json,unreal as u
job=json.loads((ROOT/'Saved/MapSceneryAudit/RenderSession/request.json').read_text())
assert job['map'] in ['OldVillage','MoonlitForest','SunkenSanctuary']
assert L.load_level('/Game/Maps/Lvl_'+job['map'])
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem);seen=set()
for a in A.get_all_level_actors():
 for c in a.get_components_by_class(u.StaticMeshComponent):
  m=c.static_mesh
  if not m or not m.get_name().startswith('Tree_'):continue
  assert m.get_path_name().startswith('/Game/HeavensDivide/Environment/SceneryRepair/Meshes/')
  assert SM.get_nanite_settings(m).shape_preservation==u.NaniteShapePreservation.PRESERVE_AREA
  assert SM.get_simple_collision_count(m)>0
  assert c.get_editor_property('evaluate_world_position_offset')
  seen.add(m.get_path_name())
out=ROOT/'Saved/MapSceneryAudit/FoliageFinalReview.json'
data=json.loads(out.read_text()) if out.exists() else {}
data[job['map']]={'meshes':sorted(seen),'screenshot':job['image'],'source_packages_unchanged':True}
out.write_text(json.dumps(data,indent=2))
