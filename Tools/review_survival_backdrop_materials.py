"""Verify explicit far-terrain materials and capture overviews after a cold load."""
from pathlib import Path
import unreal as u,json
root=Path(u.Paths.project_dir()).resolve()
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem);E=u.EditorAssetLibrary
assignments={}
for key in ['MoonlitForest','SunkenSanctuary','EmberQuarry']:
 assert L.load_level('/Game/Maps/Lvl_'+key)
 assignments[key]=[]
 for a in A.get_all_level_actors():
  if a.get_actor_label()!='Continuous world beyond arena':continue
  c=a.static_mesh_component;old=c.get_material(0)
  expected=E.load_asset('/Game/HeavensDivide/Environment/SurvivalMaps/'+key+'/Materials/M_DistantLand')
  assignments[key].append({'old':old.get_path_name() if old else None,'slots_before':c.get_num_materials(),'new':expected.get_path_name()})
  c.set_material(0,expected);assert c.get_material(0)==expected
 assert L.save_current_level()
(root/'Saved/SurvivalMaps/BackdropMaterials.json').write_text(json.dumps(assignments,indent=2))
REPORT_NAME='BackdropArtValidation.json'
SKIP_AUTOMATION=True
PREVIEW_ONLY=True
OVERVIEW_ONLY=True
exec(compile((root/'Tools/review_survival_maps.py').read_text(encoding='utf-8'),'review_survival_maps.py','exec'))
